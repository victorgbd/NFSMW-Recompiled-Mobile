/*
 * OpenGL ES presenter (see rex/ui/gles/presenter.h).
 */

#include <atomic>

#include <rex/logging.h>
#include <rex/ui/gles/presenter.h>
#include <rex/ui/gles/provider.h>
#include <rex/ui/surface_android.h>

#include "gles_presenter_efectos.h"

namespace rex::ui::gles {

namespace {

// Full-screen triangle sampling the guest output. Row 0 of the texture is the top of the image and row 0
// of the window is the bottom, hence uv.y = 1 - p.y.
constexpr const char* kVertices = R"(#version 300 es
out vec2 uv;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
  uv = vec2(p.x, 1.0 - p.y);
}
)";

constexpr const char* kPixeles = R"(#version 300 es
precision mediump float;
uniform sampler2D salida;
in vec2 uv;
out vec4 color;
void main() {
  color = vec4(texture(salida, uv).rgb, 1.0);
}
)";

// The effects' vertex stage: a triangle covering the viewport, which is the effect's output rectangle. The pixel
// shaders work out their pixel from gl_FragCoord.
constexpr const char* kVerticesEfecto = R"(#version 310 es
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

GLuint Compilar(GLenum tipo, const char* fuente) {
  GLuint shader = glCreateShader(tipo);
  glShaderSource(shader, 1, &fuente, nullptr);
  glCompileShader(shader);
  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok != GL_TRUE) {
    char registro[1024] = {};
    glGetShaderInfoLog(shader, sizeof(registro) - 1, nullptr, registro);
    REXLOG_ERROR("GlesPresenter: el shader de pintado no compila: {}", registro);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

Presenter::PaintResult GlesPresenter::ResultadoDeError(EGLint error) {
  switch (error) {
    case EGL_CONTEXT_LOST:
      return PaintResult::kGpuLostResponsible;
    case EGL_BAD_SURFACE:
    case EGL_BAD_NATIVE_WINDOW:
    case EGL_BAD_CURRENT_SURFACE:
      return PaintResult::kNotPresentedConnectionOutdated;
    default:
      return PaintResult::kNotPresented;
  }
}

std::unique_ptr<GlesPresenter> GlesPresenter::Create(HostGpuLossCallback host_gpu_loss_callback,
                                                     const GlesProvider* provider) {
  auto presenter = std::unique_ptr<GlesPresenter>(new GlesPresenter(host_gpu_loss_callback, provider));
  if (!presenter->InicializarIndependienteDeSuperficie()) {
    return nullptr;
  }
  return presenter;
}

bool GlesPresenter::InicializarIndependienteDeSuperficie() {
  contexto_pintar_ = provider_->CrearContextoCompartido();
  if (contexto_pintar_ == EGL_NO_CONTEXT) {
    return false;
  }
  return InitializeCommonSurfaceIndependent();
}

GlesPresenter::~GlesPresenter() {
  // First: the presenter's own thread calls PaintAndPresentImpl (see VulkanPresenter::~VulkanPresenter).
  ShutdownPaintThread();
  // The objects of the paint context die with it. The guest output textures and sync objects are shared:
  // they are freed with the share group, when the provider destroys the root context right after this.
  if (superficie_ != EGL_NO_SURFACE) {
    eglDestroySurface(provider_->display(), superficie_);
    superficie_ = EGL_NO_SURFACE;
  }
  if (contexto_pintar_ != EGL_NO_CONTEXT) {
    eglDestroyContext(provider_->display(), contexto_pintar_);
    contexto_pintar_ = EGL_NO_CONTEXT;
  }
}

Surface::TypeFlags GlesPresenter::GetSupportedSurfaceTypes() const {
  return Surface::kTypeFlag_AndroidNativeWindow;
}

bool GlesPresenter::CaptureGuestOutput(RawImage& image_out) {
  (void)image_out;
  static std::atomic<bool> avisado{false};
  if (!avisado.exchange(true)) {
    REXLOG_WARN("GlesPresenter: la captura de la salida todavia no esta hecha");
  }
  return false;
}

Presenter::SurfacePaintConnectResult GlesPresenter::ConnectOrReconnectPaintingToSurfaceFromUIThread(
    Surface& new_surface, uint32_t new_surface_width, uint32_t new_surface_height, bool was_paintable,
    bool& is_vsync_implicit_out) {
  if (new_surface.GetType() != Surface::kTypeIndex_AndroidNativeWindow) {
    REXLOG_ERROR("GlesPresenter: solo se puede pintar en una ventana nativa de Android");
    return SurfacePaintConnectResult::kFailureSurfaceUnusable;
  }
  ANativeWindow* ventana = static_cast<AndroidNativeWindowSurface&>(new_surface).window();
  // With eglSwapInterval(1), eglSwapBuffers waits for a free buffer: the vertical sync is implicit.
  is_vsync_implicit_out = true;
  if (superficie_ != EGL_NO_SURFACE && ventana == ventana_) {
    // Same window: EGL follows its size by itself.
    const bool cambio = new_surface_width != superficie_ancho_ || new_surface_height != superficie_alto_;
    superficie_ancho_ = new_surface_width;
    superficie_alto_ = new_surface_height;
    return (was_paintable && !cambio) ? SurfacePaintConnectResult::kSuccessUnchanged
                                      : SurfacePaintConnectResult::kSuccess;
  }
  DisconnectPaintingFromSurfaceFromUIThreadImpl();

  const EGLDisplay display = provider_->display();
  EGLint formato = 0;
  if (eglGetConfigAttrib(display, provider_->config(), EGL_NATIVE_VISUAL_ID, &formato) == EGL_TRUE) {
    ANativeWindow_setBuffersGeometry(ventana, 0, 0, formato);
  }
  superficie_ = eglCreateWindowSurface(display, provider_->config(), ventana, nullptr);
  if (superficie_ == EGL_NO_SURFACE) {
    const EGLint error = eglGetError();
    REXLOG_ERROR("GlesPresenter: eglCreateWindowSurface fallo ({:#x})", error);
    // EGL_BAD_NATIVE_WINDOW: the window is gone or another API is connected to it.
    return error == EGL_BAD_NATIVE_WINDOW ? SurfacePaintConnectResult::kFailureSurfaceUnusable
                                          : SurfacePaintConnectResult::kFailure;
  }
  ventana_ = ventana;
  superficie_ancho_ = new_surface_width;
  superficie_alto_ = new_surface_height;
  intervalo_puesto_ = false;
  REXLOG_INFO("GlesPresenter: conectado a la ventana ({}x{})", new_surface_width, new_surface_height);
  return SurfacePaintConnectResult::kSuccess;
}

void GlesPresenter::DisconnectPaintingFromSurfaceFromUIThreadImpl() {
  // The paint context is released after every paint, so the surface is current on no thread here.
  if (superficie_ != EGL_NO_SURFACE) {
    eglDestroySurface(provider_->display(), superficie_);
    superficie_ = EGL_NO_SURFACE;
  }
  ventana_ = nullptr;
  superficie_ancho_ = 0;
  superficie_alto_ = 0;
}

bool GlesPresenter::RefreshGuestOutputImpl(uint32_t mailbox_index, uint32_t frontbuffer_width,
                                           uint32_t frontbuffer_height,
                                           std::function<bool(GuestOutputRefreshContext& context)> refresher,
                                           bool& is_8bpc_out_ref) {
  if (eglGetCurrentContext() == EGL_NO_CONTEXT) {
    static std::atomic<bool> avisado{false};
    if (!avisado.exchange(true)) {
      REXLOG_ERROR("GlesPresenter: se refresca la salida desde un hilo sin contexto de GL (ContextoHilo)");
    }
    return false;
  }
  const auto maximo = uint32_t(provider_->capacidades().max_texture_size);
  if (frontbuffer_width > maximo || frontbuffer_height > maximo) {
    return false;
  }
  ImagenSalida& salida = salidas_[mailbox_index];
  {
    // The last paint that sampled this texture has to finish before it is written again.
    std::lock_guard<std::mutex> cerrojo(sincronizacion_);
    if (salida.leida) {
      glWaitSync(salida.leida, 0, GL_TIMEOUT_IGNORED);
      glDeleteSync(salida.leida);
      salida.leida = nullptr;
    }
  }
  if (salida.textura && (salida.ancho != frontbuffer_width || salida.alto != frontbuffer_height)) {
    glDeleteTextures(1, &salida.textura);
    salida.textura = 0;
  }
  if (!salida.textura) {
    glGenTextures(1, &salida.textura);
    glBindTexture(GL_TEXTURE_2D, salida.textura);
    glTexStorage2D(GL_TEXTURE_2D, 1, kGuestOutputFormat, GLsizei(frontbuffer_width), GLsizei(frontbuffer_height));
    glBindTexture(GL_TEXTURE_2D, 0);
    salida.ancho = frontbuffer_width;
    salida.alto = frontbuffer_height;
    salida.version = siguiente_version_++;
    salida.escrita_alguna_vez = false;
  }
  GlesGuestOutputRefreshContext contexto(is_8bpc_out_ref, salida.textura, salida.ancho, salida.alto,
                                         salida.version);
  const bool ok = refresher(contexto);
  if (ok) {
    salida.escrita_alguna_vez = true;
  }
  // As in VulkanPresenter: even if the refresher failed it may have issued commands on the texture.
  GLsync escrita = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
  glFlush();
  {
    std::lock_guard<std::mutex> cerrojo(sincronizacion_);
    if (salida.escrita) {
      glDeleteSync(salida.escrita);
    }
    salida.escrita = escrita;
  }
  return ok;
}

bool GlesPresenter::PrepararPintado() {
  if (programa_) {
    return true;
  }
  const GLuint vs = Compilar(GL_VERTEX_SHADER, kVertices);
  const GLuint ps = Compilar(GL_FRAGMENT_SHADER, kPixeles);
  if (!vs || !ps) {
    glDeleteShader(vs);
    glDeleteShader(ps);
    return false;
  }
  programa_ = glCreateProgram();
  glAttachShader(programa_, vs);
  glAttachShader(programa_, ps);
  glLinkProgram(programa_);
  glDeleteShader(vs);
  glDeleteShader(ps);
  GLint ok = GL_FALSE;
  glGetProgramiv(programa_, GL_LINK_STATUS, &ok);
  if (ok != GL_TRUE) {
    char registro[1024] = {};
    glGetProgramInfoLog(programa_, sizeof(registro) - 1, nullptr, registro);
    REXLOG_ERROR("GlesPresenter: el programa de pintado no enlaza: {}", registro);
    glDeleteProgram(programa_);
    programa_ = 0;
    return false;
  }
  glUseProgram(programa_);
  glUniform1i(glGetUniformLocation(programa_, "salida"), 0);
  glUseProgram(0);
  glGenVertexArrays(1, &vao_);
  glGenSamplers(1, &sampler_);
  glSamplerParameteri(sampler_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(sampler_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(sampler_, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return true;
}

GLuint GlesPresenter::ProgramaEfecto(size_t efecto, bool final) {
  ProgramaEfectoGles& p = programas_efecto_[efecto][final ? 1 : 0];
  if (p.programa || p.fallido) {
    return p.programa;
  }
  p.fallido = true;
  static_assert(std::size(efectos::kEfectos) == size_t(GuestOutputPaintEffect::kCount),
                "gles_presenter_efectos.h does not have every GuestOutputPaintEffect");
  const efectos::Efecto& e = efectos::kEfectos[efecto];
  if (!vertices_efecto_) {
    vertices_efecto_ = Compilar(GL_VERTEX_SHADER, kVerticesEfecto);
    if (!vertices_efecto_) {
      return 0;
    }
  }
  const GLuint ps = Compilar(GL_FRAGMENT_SHADER, final ? e.final : e.intermedio);
  if (!ps) {
    REXLOG_ERROR("GlesPresenter: el efecto {} no compila", e.nombre);
    return 0;
  }
  GLuint programa = glCreateProgram();
  glAttachShader(programa, vertices_efecto_);
  glAttachShader(programa, ps);
  glLinkProgram(programa);
  glDetachShader(programa, vertices_efecto_);
  glDeleteShader(ps);
  GLint ok = GL_FALSE;
  glGetProgramiv(programa, GL_LINK_STATUS, &ok);
  if (ok != GL_TRUE) {
    char registro[1024] = {};
    glGetProgramInfoLog(programa, sizeof(registro) - 1, nullptr, registro);
    REXLOG_ERROR("GlesPresenter: el efecto {} no enlaza: {}", e.nombre, registro);
    glDeleteProgram(programa);
    return 0;
  }
  static constexpr const char* kMiembros[3] = {"constantes.m0", "constantes.m1", "constantes.m2"};
  for (uint32_t i = 0; i < e.miembros && i < 3; ++i) {
    p.constantes[i] = glGetUniformLocation(programa, kMiembros[i]);
  }
  p.alto = glGetUniformLocation(programa, "nfsmw_alto");
  glUseProgram(programa);
  glUniform1i(glGetUniformLocation(programa, "origen"), 0);
  glUseProgram(0);
  p.programa = programa;
  p.fallido = false;
  REXLOG_INFO("GlesPresenter: efecto {} ({}) listo", e.nombre, final ? "a la ventana" : "a una textura");
  return programa;
}

bool GlesPresenter::PintarEfectos(const GuestOutputPaintFlow& flujo, const GuestOutputPaintConfig& configuracion,
                                  GLuint textura, EGLint ancho_ventana, EGLint alto_ventana) {
  (void)ancho_ventana;
  // All the programs first: if one is missing nothing is drawn and the caller falls back to bilinear.
  for (size_t i = 0; i < flujo.effect_count; ++i) {
    if (!ProgramaEfecto(size_t(flujo.effects[i]), i + 1 == flujo.effect_count)) {
      return false;
    }
  }
  glBindVertexArray(vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindSampler(0, sampler_);
  GLuint entrada = textura;
  for (size_t i = 0; i < flujo.effect_count; ++i) {
    const bool final = i + 1 == flujo.effect_count;
    const auto efecto = flujo.effects[i];
    const auto [ancho, alto] = flujo.effect_output_sizes[i];
    if (final) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      // The flow measures from the top-left corner; GL's viewport from the bottom-left one.
      glViewport(flujo.output_x, alto_ventana - flujo.output_y - GLint(alto), GLsizei(ancho), GLsizei(alto));
    } else {
      Intermedia& m = intermedias_[i];
      if (m.textura && (m.ancho != ancho || m.alto != alto)) {
        glDeleteFramebuffers(1, &m.fbo);
        glDeleteTextures(1, &m.textura);
        m = Intermedia{};
      }
      if (!m.textura) {
        glGenTextures(1, &m.textura);
        glBindTexture(GL_TEXTURE_2D, m.textura);
        glTexStorage2D(GL_TEXTURE_2D, 1, kGuestOutputFormat, GLsizei(ancho), GLsizei(alto));
        glGenFramebuffers(1, &m.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m.textura, 0);
        m.ancho = ancho;
        m.alto = alto;
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
          REXLOG_ERROR("GlesPresenter: la textura intermedia {}x{} del efecto {} no se puede dibujar", ancho, alto, i);
          glBindFramebuffer(GL_FRAMEBUFFER, 0);
          return false;
        }
      }
      glBindFramebuffer(GL_FRAMEBUFFER, m.fbo);
      glViewport(0, 0, GLsizei(ancho), GLsizei(alto));
    }
    const ProgramaEfectoGles& p = programas_efecto_[size_t(efecto)][final ? 1 : 0];
    glUseProgram(p.programa);
    switch (efecto) {
      case GuestOutputPaintEffect::kBilinear:
      case GuestOutputPaintEffect::kBilinearDither: {
        BilinearConstants c;
        c.Initialize(flujo, i);
        glUniform2i(p.constantes[0], c.output_offset[0], c.output_offset[1]);
        glUniform2f(p.constantes[1], c.output_size_inv[0], c.output_size_inv[1]);
      } break;
#if defined(REX_HAS_FIDELITYFX_SDK)
      case GuestOutputPaintEffect::kCasSharpen:
      case GuestOutputPaintEffect::kCasSharpenDither: {
        CasSharpenConstants c;
        c.Initialize(flujo, i, configuracion);
        glUniform2i(p.constantes[0], c.output_offset[0], c.output_offset[1]);
        glUniform1f(p.constantes[1], c.sharpness_post_setup);
      } break;
      case GuestOutputPaintEffect::kCasResample:
      case GuestOutputPaintEffect::kCasResampleDither: {
        CasResampleConstants c;
        c.Initialize(flujo, i, configuracion);
        glUniform2i(p.constantes[0], c.output_offset[0], c.output_offset[1]);
        glUniform2f(p.constantes[1], c.input_output_size_ratio[0], c.input_output_size_ratio[1]);
        glUniform1f(p.constantes[2], c.sharpness_post_setup);
      } break;
      case GuestOutputPaintEffect::kFsrEasu: {
        FsrEasuConstants c;
        c.Initialize(flujo, i);
        glUniform2f(p.constantes[0], c.input_output_size_ratio[0], c.input_output_size_ratio[1]);
        glUniform2f(p.constantes[1], c.input_size_inv[0], c.input_size_inv[1]);
      } break;
      case GuestOutputPaintEffect::kFsrRcas:
      case GuestOutputPaintEffect::kFsrRcasDither: {
        FsrRcasConstants c;
        c.Initialize(flujo, i, configuracion);
        glUniform2i(p.constantes[0], c.output_offset[0], c.output_offset[1]);
        glUniform1f(p.constantes[1], c.sharpness_post_setup);
      } break;
      case GuestOutputPaintEffect::kSgsr1: {
        GsrConstants c;
        c.Initialize(flujo, i);
        glUniform2i(p.constantes[0], c.output_offset[0], c.output_offset[1]);
        glUniform2f(p.constantes[1], c.output_size_inv[0], c.output_size_inv[1]);
        glUniform4f(p.constantes[2], c.input_size_inv[0], c.input_size_inv[1], c.input_size[0], c.input_size[1]);
      } break;
#endif
      default:
        break;
    }
    if (final) {
      glUniform1f(p.alto, float(alto_ventana));
    }
    glBindTexture(GL_TEXTURE_2D, entrada);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    if (!final) {
      entrada = intermedias_[i].textura;
    }
  }
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindSampler(0, 0);
  glBindVertexArray(0);
  glUseProgram(0);
  return true;
}

Presenter::PaintResult GlesPresenter::PaintAndPresentImpl(bool execute_ui_drawers) {
  if (superficie_ == EGL_NO_SURFACE) {
    return PaintResult::kNotPresentedConnectionOutdated;
  }
  const EGLDisplay display = provider_->display();
  /*
   * In the "paint right away from the game thread" mode the paint happens on the thread that refreshes the
   * guest output, which has its own context current (the native renderer's ring thread). That context is
   * restored on every exit, instead of leaving the thread with none: the first version released it, and
   * from the second frame on the ring thread had no context and the screen stayed on the first image.
   * With no previous context, the paint context is released: the next paint may happen on another thread.
   */
  struct Restaurar {
    EGLDisplay display;
    EGLContext contexto = eglGetCurrentContext();
    EGLSurface dibujo = eglGetCurrentSurface(EGL_DRAW);
    EGLSurface lectura = eglGetCurrentSurface(EGL_READ);
    ~Restaurar() { eglMakeCurrent(display, dibujo, lectura, contexto); }
  } restaurar{display};
  if (eglMakeCurrent(display, superficie_, superficie_, contexto_pintar_) != EGL_TRUE) {
    const EGLint error = eglGetError();
    REXLOG_ERROR("GlesPresenter: eglMakeCurrent para pintar fallo ({:#x})", error);
    return ResultadoDeError(error);
  }

  if (!intervalo_puesto_) {
    eglSwapInterval(display, 1);
    intervalo_puesto_ = true;
  }
  if (!PrepararPintado()) {
    return PaintResult::kNotPresented;
  }
  EGLint ancho = 0, alto = 0;
  eglQuerySurface(display, superficie_, EGL_WIDTH, &ancho);
  eglQuerySurface(display, superficie_, EGL_HEIGHT, &alto);
  if (ancho <= 0 || alto <= 0) {
    return PaintResult::kNotPresentedConnectionOutdated;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glViewport(0, 0, ancho, alto);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  GuestOutputProperties propiedades;
  GuestOutputPaintConfig configuracion;
  uint32_t indice = UINT32_MAX;
  GLuint textura = 0;
  {
    std::unique_lock<std::mutex> consumidor(ConsumeGuestOutput(indice, &propiedades, &configuracion));
    if (indice != UINT32_MAX) {
      textura = salidas_[indice].textura;
      std::lock_guard<std::mutex> cerrojo(sincronizacion_);
      if (salidas_[indice].escrita) {
        glWaitSync(salidas_[indice].escrita, 0, GL_TIMEOUT_IGNORED);
      }
    }
  }
  if (textura) {
    if (efectos_fallidos_) {
      configuracion.SetEffect(GuestOutputPaintConfig::Effect::kBilinear);
    }
    const auto maximo = uint32_t(provider_->capacidades().max_texture_size);
    GuestOutputPaintFlow flujo =
        GetGuestOutputPaintFlow(propiedades, uint32_t(ancho), uint32_t(alto), maximo, maximo, configuracion);
    // A single bilinear pass (with or without dithering) keeps the presenter's own simple path; anything else
    // (FSR, CAS) goes through the chain of effects. If that fails once, bilinear for the rest of the session.
    bool pintado = false;
    const bool solo_bilineal = flujo.effect_count == 1 && (flujo.effects[0] == GuestOutputPaintEffect::kBilinear ||
                                                           flujo.effects[0] == GuestOutputPaintEffect::kBilinearDither);
    if (flujo.effect_count && !solo_bilineal && !efectos_fallidos_) {
      pintado = PintarEfectos(flujo, configuracion, textura, ancho, alto);
      if (!pintado) {
        efectos_fallidos_ = true;
        REXLOG_WARN("GlesPresenter: los efectos de escalado no estan disponibles: se pinta con bilineal");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, ancho, alto);
        configuracion.SetEffect(GuestOutputPaintConfig::Effect::kBilinear);
        flujo = GetGuestOutputPaintFlow(propiedades, uint32_t(ancho), uint32_t(alto), maximo, maximo, configuracion);
      }
    }
    if (pintado) {
      GLsync leida = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      std::lock_guard<std::mutex> cerrojo(sincronizacion_);
      if (salidas_[indice].leida) {
        glDeleteSync(salidas_[indice].leida);
      }
      salidas_[indice].leida = leida;
    } else if (flujo.effect_count) {
      const auto [salida_ancho, salida_alto] = flujo.effect_output_sizes[flujo.effect_count - 1];
      // The flow measures from the top-left corner; GL's viewport from the bottom-left one.
      glViewport(flujo.output_x, alto - flujo.output_y - GLint(salida_alto), GLsizei(salida_ancho),
                 GLsizei(salida_alto));
      glUseProgram(programa_);
      glBindVertexArray(vao_);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, textura);
      glBindSampler(0, sampler_);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glBindSampler(0, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
      glBindVertexArray(0);
      glUseProgram(0);
      GLsync leida = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      std::lock_guard<std::mutex> cerrojo(sincronizacion_);
      if (salidas_[indice].leida) {
        glDeleteSync(salidas_[indice].leida);
      }
      salidas_[indice].leida = leida;
    }
  }

  if (execute_ui_drawers) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, ancho, alto);
    GlesUIDrawContext contexto_ui(*this, uint32_t(ancho), uint32_t(alto));
    ExecuteUIDrawersFromUIThread(contexto_ui);
  }

  if (eglSwapBuffers(display, superficie_) != EGL_TRUE) {
    const EGLint error = eglGetError();
    REXLOG_WARN("GlesPresenter: eglSwapBuffers fallo ({:#x})", error);
    return ResultadoDeError(error);
  }
  return PaintResult::kPresented;
}

}  // namespace rex::ui::gles
