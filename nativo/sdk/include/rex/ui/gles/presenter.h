#pragma once
/*
 * OpenGL ES presenter (see gles/provider.h).
 *
 * The guest output is a mailbox of three GL_RGB10_A2 textures, like VulkanPresenter's
 * A2B10G10R10 images. Who refreshes it (the native renderer's ring thread) draws into the texture from its
 * own context, shared with the presenter's: that thread must have a ContextoHilo current. Synchronization
 * between the two contexts is done with sync objects, on the GPU (glWaitSync): the refresh leaves one
 * after writing, which the paint waits for before sampling, and the paint leaves one after sampling,
 * which the next refresh of that texture waits for before writing.
 *
 * Painting happens on the presenter's own thread, on the UI thread or right away on the refreshing thread,
 * never on two at once (the base class guarantees it): the paint context is made current with the window
 * surface for each paint, and afterwards the thread gets back whatever context it had (the ring thread's,
 * when it paints itself) or none, so the paint context can change threads.
 *
 * The UI drawers (ImGui, GlesImmediateDrawer) draw over the output in the paint, with its context current.
 *
 * The effects (bilinear, AMD CAS and FSR 1.0) are VulkanPresenter's pixel shaders converted to GLSL ES
 * (gles_presenter_efectos.h), chained through intermediate textures like in Vulkan. If one does not compile, the
 * presenter goes back to plain bilinear for the rest of the session.
 *
 * Not yet: guest output capture.
 */

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>

#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <android/native_window.h>

#include <rex/ui/presenter.h>

namespace rex::ui::gles {

class GlesProvider;

class GlesPresenter final : public Presenter {
 public:
  static constexpr GLenum kGuestOutputFormat = GL_RGB10_A2;

  class GlesGuestOutputRefreshContext final : public GuestOutputRefreshContext {
   public:
    GlesGuestOutputRefreshContext(bool& is_8bpc_out_ref, GLuint texture, uint32_t width, uint32_t height,
                                  uint64_t version)
        : GuestOutputRefreshContext(is_8bpc_out_ref),
          texture_(texture),
          width_(width),
          height_(height),
          version_(version) {}

    // GL_TEXTURE_2D, kGuestOutputFormat, a single level. Row 0 is the top of the image (the D3D
    // orientation the renderer keeps): the presenter flips it when painting to the window.
    GLuint texture() const { return texture_; }
    uint32_t width() const { return width_; }
    uint32_t height() const { return height_; }
    // Changes when the texture is recreated: framebuffers of the refresher's context can be cached by it.
    uint64_t version() const { return version_; }

   private:
    GLuint texture_;
    uint32_t width_;
    uint32_t height_;
    uint64_t version_;
  };

  // For GlesImmediateDrawer: the window surface is bound as the draw framebuffer, with the paint context current.
  class GlesUIDrawContext final : public UIDrawContext {
   public:
    GlesUIDrawContext(Presenter& presenter, uint32_t render_target_width, uint32_t render_target_height)
        : UIDrawContext(presenter, render_target_width, render_target_height) {}
  };

  static std::unique_ptr<GlesPresenter> Create(HostGpuLossCallback host_gpu_loss_callback,
                                               const GlesProvider* provider);
  ~GlesPresenter() override;

  Surface::TypeFlags GetSupportedSurfaceTypes() const override;
  bool CaptureGuestOutput(RawImage& image_out) override;
#if defined(REX_HAS_FIDELITYFX_SDK)
  bool SupportsGsrEffect() const override { return true; }
#endif

 protected:
  SurfacePaintConnectResult ConnectOrReconnectPaintingToSurfaceFromUIThread(
      Surface& new_surface, uint32_t new_surface_width, uint32_t new_surface_height, bool was_paintable,
      bool& is_vsync_implicit_out) override;
  void DisconnectPaintingFromSurfaceFromUIThreadImpl() override;
  bool RefreshGuestOutputImpl(uint32_t mailbox_index, uint32_t frontbuffer_width,
                              uint32_t frontbuffer_height,
                              std::function<bool(GuestOutputRefreshContext& context)> refresher,
                              bool& is_8bpc_out_ref) override;
  PaintResult PaintAndPresentImpl(bool execute_ui_drawers) override;

 private:
  struct ImagenSalida {
    GLuint textura = 0;
    uint32_t ancho = 0;
    uint32_t alto = 0;
    uint64_t version = 0;
    bool escrita_alguna_vez = false;
    // Under sincronizacion_: the producer's (after writing) and the consumer's (after sampling).
    GLsync escrita = nullptr;
    GLsync leida = nullptr;
  };

  GlesPresenter(HostGpuLossCallback host_gpu_loss_callback, const GlesProvider* provider)
      : Presenter(host_gpu_loss_callback), provider_(provider) {}
  static PaintResult ResultadoDeError(EGLint error);
  bool InicializarIndependienteDeSuperficie();
  bool PrepararPintado();  // with the paint context current
  // The chain of effects of `flujo` from `textura` to the window. false if a program is missing: nothing drawn.
  bool PintarEfectos(const GuestOutputPaintFlow& flujo, const GuestOutputPaintConfig& configuracion, GLuint textura,
                     EGLint ancho_ventana, EGLint alto_ventana);
  // Program of an effect (GuestOutputPaintEffect) into a texture or into the window; 0 if it does not compile.
  GLuint ProgramaEfecto(size_t efecto, bool final);
  void DestruirObjetosDePintado();

  const GlesProvider* provider_;
  EGLContext contexto_pintar_ = EGL_NO_CONTEXT;
  EGLSurface superficie_ = EGL_NO_SURFACE;
  ANativeWindow* ventana_ = nullptr;
  uint32_t superficie_ancho_ = 0;
  uint32_t superficie_alto_ = 0;
  bool intervalo_puesto_ = false;  // eglSwapInterval for this surface

  // Objects of the paint context.
  GLuint programa_ = 0;
  GLuint vao_ = 0;
  GLuint sampler_ = 0;
  // The effects: programs by effect and destination (0 texture, 1 window), compiled on first use.
  struct ProgramaEfectoGles {
    GLuint programa = 0;
    bool fallido = false;
    GLint constantes[3] = {-1, -1, -1};
    GLint alto = -1;
  };
  std::array<std::array<ProgramaEfectoGles, 2>, size_t(GuestOutputPaintEffect::kCount)> programas_efecto_;
  GLuint vertices_efecto_ = 0;
  struct Intermedia {
    GLuint textura = 0;
    GLuint fbo = 0;
    uint32_t ancho = 0;
    uint32_t alto = 0;
  };
  std::array<Intermedia, kMaxGuestOutputPaintEffects> intermedias_;
  bool efectos_fallidos_ = false;

  std::mutex sincronizacion_;
  std::array<ImagenSalida, kGuestOutputMailboxSize> salidas_;
  uint64_t siguiente_version_ = 1;
};

}  // namespace rex::ui::gles
