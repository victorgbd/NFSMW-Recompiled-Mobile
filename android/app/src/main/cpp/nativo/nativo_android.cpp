// NFSMW Recompiled - lo que la app de Android le pide al MOTOR NATIVO
//
// Con el motor nativo (-Pnfsmw.motor=nativo) libmain.so es la app de
// nfsmw-android: su codigo generado, su renderizador y su SDK. La entrada
// (SDL_main) la pone ese SDK. Este fichero es lo unico nuestro que entra en
// libmain.so, con sonda_vulkan.cpp y afinidad.cpp: lo que con el motor de
// Xenos hace android_main.cpp.
//
//   1. El mando tactil (TouchControllerBridge.setState).
//   2. Los fps del rotulo (GameActivity.nativeFps / nativeMsPorFotograma).
//   3. Abrir la ISO por su URI content:// (GameActivity.openContentFd), para
//      no copiarla ni pedir acceso a todos los archivos.
//   4. La sonda de Vulkan (--nfsmw_sonda), antes de arrancar el juego.
//   5. Los hilos fijados a nucleos (cvar thread_affinity).
//   6. La salida de audio por nuestro driver AAudio (cvar nfsmw_audio_aaudio)
//      en vez de por el de SDL de su SDK.
//   7. Parar el juego y el audio mientras la app esta en segundo plano.
//   8. En que parte del juego se esta (TouchControllerBridge.contexto), para
//      cambiar los controles tactiles en las carreras de aceleracion y saltar
//      las cinematicas con un toque.
//   9. La vibracion del mando tactil: el movil vibra con lo que el juego le
//      pide al mando (GameActivity.vibrarMando).
//  10. Los menus con el dedo: que hay debajo de un toque (las indicaciones de
//      botones y las opciones), para TouchControllerView.
//  11. El boton Salir del menu principal: si se esta en el, donde va, y las
//      pulsaciones de BACK del mando fisico.
//
// iOS (ios/motor/controles_tactiles.mm, el TouchControllerView de alli) compila
// tambien este fichero, sin lo que es de Android (JNI, AAudio, afinidad, el URI
// content://, la sonda, los fps y la vibracion): el mando tactil (1), la pausa
// (7), el contexto (8) y los menus con el dedo (10) son los mismos. Lo que hacian
// las funciones de JNI esta en las NfsmwTactil*, que llaman las de JNI y iOS.

#if defined(__ANDROID__)
#include <jni.h>
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_system.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/audio/audio_system.h>
#include <rex/memory.h>
#include <rex/runtime.h>

#if defined(__ANDROID__)
#include "../afinidad.h"
#include "../audio/aaudio_driver.h"

REXCVAR_DEFINE_BOOL(nfsmw_audio_aaudio, true, "Audio",
                    "Android: sacar el sonido por AAudio (el driver de NFSMW Recompiled) en vez de por "
                    "el de SDL del SDK")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

int NfsmwSondaVulkan(int argc, char** argv);  // ../sonda_vulkan.cpp
#else
// Sin JNI: las secciones 8 y 10 usan jint como tipo, el entero de 32 bits de JNI.
using jint = int32_t;
#endif

// Lo que usan las funciones de JNI de abajo y, en iOS, controles_tactiles.mm.
extern "C" void NfsmwTactilEstado(int32_t botones, float izq_x, float izq_y, float der_x, float der_y,
                                  float gatillo_izq, float gatillo_der);
extern "C" void NfsmwTactilPausarJuego(bool pausa);
extern "C" void NfsmwVigilarSegundoPlano();
extern "C" int32_t NfsmwTactilContexto();
extern "C" void NfsmwTactilContextoDiagnostico(char* linea, size_t tam);
extern "C" int32_t NfsmwTactilMenuTocar(float u, float v);
extern "C" bool NfsmwTactilMenuRotar();
extern "C" bool NfsmwTactilMenuEsLista();
extern "C" int32_t NfsmwTactilMenuRumbo();
std::string NfsmwTactilMenuDiagnostico();
extern "C" bool NfsmwTactilMenuPrincipal(float* salida);
extern "C" uint32_t NfsmwTactilBackPulsados();

// El mando tactil del SDK del motor nativo (sdk/src/input/sdl/sdl_input_driver.cpp):
// el estado va directo al mando del juego, sin pasar por un mando virtual de SDL.
extern "C" void rex_sdl_set_touch_gamepad_state(uint16_t buttons, int16_t left_x, int16_t left_y,
                                                 int16_t right_x, int16_t right_y,
                                                 uint8_t left_trigger, uint8_t right_trigger);
// Y las veces que se ha pulsado BACK en un mando fisico (seccion 11).
extern "C" uint32_t rex_sdl_back_pulsados();

// Fotogramas que ha presentado el juego (app/src/nfsmw_d3d_trace.cpp, en su gancho del Swap).
extern std::atomic<uint64_t> g_nfsmw_fotogramas_juego;

namespace {

// ---------------------------------------------------------------------------
//  1. Mando tactil
// ---------------------------------------------------------------------------

// TouchControllerView manda los botones con el indice de SDL_GamepadButton;
// el SDK los quiere con la mascara de XInput.
constexpr uint16_t kBotonXInput[15] = {
    0x1000,  //  0 A
    0x2000,  //  1 B
    0x4000,  //  2 X
    0x8000,  //  3 Y
    0x0020,  //  4 BACK
    0x0000,  //  5 GUIDE: no existe en el mando del juego
    0x0010,  //  6 START
    0x0040,  //  7 stick izquierdo
    0x0080,  //  8 stick derecho
    0x0100,  //  9 LB
    0x0200,  // 10 RB
    0x0001,  // 11 cruceta arriba
    0x0002,  // 12 cruceta abajo
    0x0004,  // 13 cruceta izquierda
    0x0008,  // 14 cruceta derecha
};

int16_t Eje(float v) {
  return static_cast<int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
}

uint8_t Gatillo(float v) {
  return static_cast<uint8_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f);
}

#if defined(__ANDROID__)
// ---------------------------------------------------------------------------
//  2. Fps
// ---------------------------------------------------------------------------

float g_fps = -1.0f;

// Java pregunta cada medio segundo: los fotogramas presentados desde la vez
// anterior, entre el tiempo que ha pasado.
float MedirFps() {
  using reloj = std::chrono::steady_clock;
  static reloj::time_point antes;
  static uint64_t fotogramas_antes = 0;
  static bool hay_anterior = false;

  const auto ahora = reloj::now();
  const uint64_t fotogramas = g_nfsmw_fotogramas_juego.load(std::memory_order_relaxed);
  if (!hay_anterior) {
    hay_anterior = true;
    antes = ahora;
    fotogramas_antes = fotogramas;
    return g_fps;
  }
  const double segundos = std::chrono::duration<double>(ahora - antes).count();
  if (segundos < 0.25) {
    return g_fps;
  }
  g_fps = static_cast<float>(double(fotogramas - fotogramas_antes) / segundos);
  antes = ahora;
  fotogramas_antes = fotogramas;
  return g_fps;
}

// ---------------------------------------------------------------------------
//  3. URI content://
// ---------------------------------------------------------------------------

// Lo llama el SDK al mapear la imagen de disco (tools/parche_iso.py y
// tools/android/parche_nativo.py). Devuelve un descriptor que pasa a
// ser del SDK, o -1.
int AbrirContent(const char* uri, const char* modo) {
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  auto actividad = static_cast<jobject>(SDL_GetAndroidActivity());
  if (!env || !actividad) {
    return -1;
  }
  int fd = -1;
  // FindClass desde un hilo nativo no ve las clases de la app: se llega a la
  // de la actividad por el objeto.
  jclass clase = env->GetObjectClass(actividad);
  jmethodID abrir = env->GetStaticMethodID(
      clase, "openContentFd",
      "(Ljava/lang/String;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;");
  if (abrir) {
    jstring juri = env->NewStringUTF(uri);
    jstring jmodo = env->NewStringUTF(modo ? modo : "r");
    jobject pfd = env->CallStaticObjectMethod(clase, abrir, juri, jmodo);
    if (!env->ExceptionCheck() && pfd) {
      jclass clase_pfd = env->GetObjectClass(pfd);
      jmethodID soltar = env->GetMethodID(clase_pfd, "detachFd", "()I");
      if (soltar) {
        fd = env->CallIntMethod(pfd, soltar);
      }
      env->DeleteLocalRef(clase_pfd);
    }
    if (pfd) {
      env->DeleteLocalRef(pfd);
    }
    env->DeleteLocalRef(juri);
    env->DeleteLocalRef(jmodo);
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    fd = -1;
  }
  env->DeleteLocalRef(clase);
  env->DeleteLocalRef(actividad);
  return fd;
}

// Al cargar libmain.so, antes de que arranque nada.
const struct ApuntarAbrirContent {
  ApuntarAbrirContent() { rex::filesystem::SetAndroidContentOpener(&AbrirContent); }
} g_apuntar_abrir_content;
#endif  // __ANDROID__: secciones 2 y 3

}  // namespace

#if defined(__ANDROID__)
// ---------------------------------------------------------------------------
//  4. La sonda de Vulkan
// ---------------------------------------------------------------------------

// La llama SDL_main del SDK antes de arrancar el juego (parche_nativo.py). -1 =
// no se pide la sonda: que siga el juego.
extern "C" int NfsmwSondaSiSePide(int argc, char** argv) {
  constexpr const char kArgSonda[] = "--nfsmw_sonda";
  bool sonda = false;
  std::vector<char*> args;
  args.reserve(static_cast<size_t>(argc));
  for (int i = 0; i < argc; ++i) {
    // Los mandos de Xbox por USB. El kernel de algunos moviles (su xpad sin
    // CONFIG_JOYSTICK_XPAD_FF) no le da a Android sus motores: por ahi no vibran. El
    // driver HIDAPI de SDL los abre por USB y les manda la vibracion el mismo, pero en
    // Android viene apagado. Se enciende antes de que SDL abra los mandos, solo con la
    // vibracion puesta: al conectar uno pide permiso de USB. Por Bluetooth siguen por
    // Android, que ahi si tiene sus motores.
    if (std::strcmp(argv[i], "--input_vibracion=true") == 0) {
      SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "1");
    }
    // No es un cvar: se quita antes de que cvar::Init proteste.
    if (std::strcmp(argv[i], kArgSonda) == 0) {
      sonda = true;
    } else {
      args.push_back(argv[i]);
    }
  }
  if (!sonda) {
    return -1;
  }
  return NfsmwSondaVulkan(static_cast<int>(args.size()), args.data());
}

// ---------------------------------------------------------------------------
//  5. Hilos fijados a nucleos
// ---------------------------------------------------------------------------

namespace {
nfsmw::afinidad::VigilantePtr g_vigilante;
}  // namespace
#endif  // __ANDROID__: secciones 4 y 5

// ---------------------------------------------------------------------------
//  7. Segundo plano
// ---------------------------------------------------------------------------
//
// Con el motor de Xenos, al minimizar se callaba el sonido y el juego se
// quedaba quieto. Con el nativo seguia corriendo: su renderizador no espera a
// nadie para presentar. Al irse a segundo plano:
//
//   - se pausa el sistema de audio del SDK (su hilo de audio y el XMA), y con
//     el, la salida AAudio;
//   - el hilo del anillo se para en el siguiente cambio de fotograma
//     (NfsmwAndroidPausaEnSwap, que llama su renderizador: parche_nativo.py), y
//     el juego se queda esperando sitio en el anillo, donde ya sabe esperar;
//   - el reloj del rescate de su servidor de audio (nfsmw_audio_servidor.cpp)
//     se para: mide "250 ms sin un fin de paquete", y con el audio en pausa y
//     el reloj de verdad entraria en rescate y liberaria paquetes que la voz
//     todavia tiene. Medido: cientos por pausa, y el sonido corrupto al volver.
//
// Suspender los hilos del juego a la fuerza (XThread::Suspend) se probo y se
// quito: los paraba en cualquier punto, y al volver el juego se quedaba parado
// de 5 a 17 s.

// El reloj del rescate (su nfsmw_audio_servidor.cpp). Es un puntero a funcion
// para poder cambiarlo en las pruebas.
namespace nfsmw::audio_servidor {
extern int64_t (*g_reloj_ms)();
}  // namespace nfsmw::audio_servidor

namespace {

std::mutex g_pausa_mutex;
std::condition_variable g_pausa_cv;
bool g_en_pausa = false;
bool g_audio_pausado = false;

// Su reloj, sin el tiempo pasado en segundo plano: parado durante la pausa.
int64_t (*g_reloj_real)() = nullptr;
std::atomic<int64_t> g_pausa_desde_ms{-1};
std::atomic<int64_t> g_pausado_ms{0};

int64_t RelojSinPausas() {
  const int64_t desde = g_pausa_desde_ms.load(std::memory_order_acquire);
  const int64_t ahora = desde >= 0 ? desde : g_reloj_real();
  return ahora - g_pausado_ms.load(std::memory_order_relaxed);
}

void PararReloj() {
  if (g_reloj_real && g_pausa_desde_ms.load(std::memory_order_relaxed) < 0) {
    g_pausa_desde_ms.store(g_reloj_real(), std::memory_order_release);
  }
}

void SeguirReloj() {
  const int64_t desde = g_pausa_desde_ms.load(std::memory_order_relaxed);
  if (g_reloj_real && desde >= 0) {
    g_pausado_ms.fetch_add(g_reloj_real() - desde, std::memory_order_relaxed);
    g_pausa_desde_ms.store(-1, std::memory_order_release);
  }
}

// Todos los sistemas de audio del SDK (el de SDL, el nuestro de AAudio)
// heredan de AudioSystem.
rex::audio::AudioSystem* SistemaDeAudio() {
  auto* runtime = rex::Runtime::instance();
  auto* audio = runtime ? runtime->audio_system() : nullptr;
  return audio ? static_cast<rex::audio::AudioSystem*>(audio) : nullptr;
}

// Por que esta en pausa: la app en segundo plano, o el editor del mando abierto
// en la partida (TouchControllerView). Se pausa con el primero y se reanuda
// cuando no queda ninguno: cerrar el editor con la app minimizada no puede
// poner el juego en marcha.
constexpr uint32_t kPausaSegundoPlano = 1;
constexpr uint32_t kPausaEditor = 2;
uint32_t g_motivos_pausa = 0;  // con g_pausa_mutex

void Pausar(uint32_t motivo) {
  {
    std::lock_guard<std::mutex> lock(g_pausa_mutex);
    const bool ya = g_motivos_pausa != 0;
    g_motivos_pausa |= motivo;
    if (ya) {
      return;
    }
    g_en_pausa = true;
  }
  PararReloj();
  if (auto* audio = SistemaDeAudio(); audio && !audio->is_paused()) {
    audio->Pause();
    g_audio_pausado = true;
  }
  REXLOG_INFO("[pausa] juego y audio en pausa ({})",
              motivo == kPausaEditor ? "editor del mando" : "segundo plano");
}

void Reanudar(uint32_t motivo) {
  {
    std::lock_guard<std::mutex> lock(g_pausa_mutex);
    if (!(g_motivos_pausa & motivo)) {
      return;
    }
    g_motivos_pausa &= ~motivo;
    if (g_motivos_pausa) {
      return;  // queda otro motivo
    }
    g_en_pausa = false;
  }
  g_pausa_cv.notify_all();
  if (g_audio_pausado) {
    if (auto* audio = SistemaDeAudio()) {
      audio->Resume();
    }
    g_audio_pausado = false;
  }
  SeguirReloj();
  REXLOG_INFO("[pausa] juego y audio en marcha");
}

// En el hilo de SDL, dentro de su aviso de ciclo de vida: WILL_ENTER_BACKGROUND
// llega antes de que Android quite la superficie, y WILL_ENTER_FOREGROUND es lo
// primero al volver.
bool SDLCALL EventosSegundoPlano(void*, SDL_Event* evento) {
  if (evento->type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
    Pausar(kPausaSegundoPlano);
  } else if (evento->type == SDL_EVENT_WILL_ENTER_FOREGROUND) {
    Reanudar(kPausaSegundoPlano);
  }
  return true;
}

}  // namespace

// La llama el hilo del anillo de su renderizador en cada cambio de fotograma,
// antes de presentarlo (parche_nativo.py). En marcha no cuesta mas que mirar un
// bool con el cerrojo; en pausa espera hasta que la app vuelve.
extern "C" void NfsmwAndroidPausaEnSwap() {
  std::unique_lock<std::mutex> lock(g_pausa_mutex);
  if (g_en_pausa) {
    REXLOG_INFO("[pausa] el anillo se para en el cambio de fotograma");
    g_pausa_cv.wait(lock, [] { return !g_en_pausa; });
  }
}

// El editor del mando en la partida (TouchControllerView): el juego se queda
// quieto mientras se mueven los controles.
extern "C" void NfsmwTactilPausarJuego(bool pausa) {
  if (pausa) {
    Pausar(kPausaEditor);
  } else {
    Reanudar(kPausaEditor);
  }
}

// Parar el juego y el audio en segundo plano. Vive lo que el proceso. En iOS la
// llama su nfsmw_app.h en OnPostInitLogging, como a la de abajo en Android.
extern "C" void NfsmwVigilarSegundoPlano() {
  static bool vigilando_segundo_plano = false;
  if (!vigilando_segundo_plano) {
    vigilando_segundo_plano = SDL_AddEventWatch(EventosSegundoPlano, nullptr);
    // Antes de que arranque el juego, y con el, el hilo servidor de audio.
    if (!g_reloj_real && nfsmw::audio_servidor::g_reloj_ms) {
      g_reloj_real = nfsmw::audio_servidor::g_reloj_ms;
      nfsmw::audio_servidor::g_reloj_ms = &RelojSinPausas;
    }
  }
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT void JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_pausarJuego(
    JNIEnv*, jclass, jboolean pausa) {
  NfsmwTactilPausarJuego(pausa);
}

// Su nfsmw_app.h la llama en OnPostInitLogging, cuando ya estan leidos los
// cvars y el log escribe en el fichero, y antes de que el juego cree sus hilos.
// En su app es android_rendimiento.cpp, que deja los hilos fuera de los nucleos
// lentos; aqui hace lo nuestro:
//   - el vigilante de afinidad (afinidad.cpp), que con "auto" pone el hilo del
//     anillo y el principal del juego en los nucleos prime;
//   - parar el juego y el audio en segundo plano.
// Los dos viven lo que el proceso, que GameActivity mata al salir.
extern "C" void nfsmw_android_nucleos_grandes_aplicar() {
  if (!g_vigilante) {
    g_vigilante = nfsmw::afinidad::Arrancar();
  }
  NfsmwVigilarSegundoPlano();
}

// ---------------------------------------------------------------------------
//  6. Audio por AAudio
// ---------------------------------------------------------------------------

// La llama su NfsmwApp::OnPreSetup (parche_nativo.py), con la factoria de
// audio por defecto (SDL) ya puesta. Con el motor de Xenos lo hace juego.cpp.
void NfsmwAndroidAudio(rex::RuntimeConfig& config) {
  if (REXCVAR_GET(nfsmw_audio_aaudio)) {
    config.audio_factory = REX_AUDIO_BACKEND(rex::audio::android::AndroidAAudioSystem);
  }
}

extern "C" JNIEXPORT void JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_setState(
    JNIEnv*, jclass, jint buttons, jfloat left_x, jfloat left_y, jfloat right_x, jfloat right_y,
    jfloat left_trigger, jfloat right_trigger) {
  NfsmwTactilEstado(buttons, left_x, left_y, right_x, right_y, left_trigger, right_trigger);
}

extern "C" JNIEXPORT jfloat JNICALL Java_io_github_nfsmwrecomp_GameActivity_nativeFps(JNIEnv*,
                                                                                    jclass) {
  return MedirFps();
}

extern "C" JNIEXPORT jfloat JNICALL
Java_io_github_nfsmwrecomp_GameActivity_nativeMsPorFotograma(JNIEnv*, jclass) {
  return g_fps > 0.0f ? 1000.0f / g_fps : -1.0f;
}
#endif  // __ANDROID__: JNI de la pausa, seccion 6 y fps

// Los botones con el indice de SDL_GamepadButton (TouchControllerView), los sticks y los
// gatillos de 0 a 1. -1 en los botones es "mando oculto o desconectado": todo suelto.
extern "C" void NfsmwTactilEstado(int32_t buttons, float left_x, float left_y, float right_x,
                                  float right_y, float left_trigger, float right_trigger) {
  if (buttons < 0) {
    buttons = 0;
  }
  uint16_t mascara = 0;
  for (int i = 0; i < 15; ++i) {
    if (buttons & (1 << i)) {
      mascara |= kBotonXInput[i];
    }
  }
  // Los sticks llegan como en SDL, con la Y positiva hacia abajo; en el mando
  // de la Xbox 360 es al reves.
  rex_sdl_set_touch_gamepad_state(mascara, Eje(left_x), Eje(-left_y), Eje(right_x), Eje(-right_y),
                                  Gatillo(left_trigger), Gatillo(right_trigger));
}

// ---------------------------------------------------------------------------
//  8. En que parte del juego se esta
// ---------------------------------------------------------------------------
//
// Para los controles tactiles: en las carreras de aceleracion RT y LT son
// pedales, el stick derecho una palanca de cambios, y L3 y R3 no estan. Se lee
// de la memoria del juego, con lo que da su descompilacion
// (github.com/dbalatoni13/nfsmw) y lo que hace su codigo en el 360:
//
//   TheGameFlowManager.CurrentGameFlowState   3 = menus, 6 = en el mundo
//   GRaceStatus::fObj -> +0x1A30 mRaceParms   la carrera en curso
//
// y el tipo de esa carrera como lo saca GRaceParameters::GetRaceType:
//
//   - de su entrada en la base de datos de carreras: +4 mIndex -> +0x2B;
//   - si no la tiene, del atributo "racetype" de la carrera, que es un texto
//     ("drag") que se busca en la tabla de los once tipos. Es el caso de las
//     carreras rapidas: son una copia (GRaceCustom) sin mIndex. Medido: la
//     primera version solo miraba mIndex, y en una carrera rapida de
//     aceleracion los controles no cambiaron.
//
// El atributo esta en la coleccion de la carrera (Attrib): +8 mRaceRecord ->
// +4 la coleccion, y si no esta en su tabla, en la de su padre (+0x10), que
// es la carrera original; o, si no, en su parte fija (+0x18), con el sitio
// que da la definicion de su clase (+0x14 -> +8 -> +0xC). Las tablas son
// pequenas: se recorren nodo a nodo en vez de copiar su hash, y con claves
// unicas da lo mismo. Se calcula una vez por carrera.
//
// Y si hay una cinematica del motor (NIS: la presentacion antes de cada
// carrera, las escenas de la historia, las de una persecucion), por el
// INIS::mInstance del juego, con NISActivity::mState en +684 de ese INIS
// (5 = NISACTIVITY_PLAYING). El 360 la salta con HUDACTION_SKIPNIS, que en el
// mapa "hud" del mando (attributes.bin) es A: con los menus con el dedo, un
// toque en la pantalla pulsa A. La direccion de INIS::mInstance no esta en las
// tablas de las ediciones (va en .bss); sale de las instrucciones de
// NISActivity::JoyHandle, que la lee para saltarla, como la de las pausas.
//
// Las direcciones las pone su app (parche_nativo.py, g_nfsmw_android_contexto),
// traducidas a la edicion del juego. Los desplazamientos son los del 360, y
// antes de fiarse de ellos se comprueban las instrucciones del juego de las que
// salen. Cada lectura mira antes que la pagina sea legible: Java pregunta desde
// su hilo mientras el juego cambia de escena, y un puntero de una carrera que
// se acaba de descargar no puede tumbar la app.

// Su app (nfsmw_recortes_carrera.cpp, con parche_nativo.py). Debil: sin el
// parche, el contexto es desconocido y los controles son los de siempre.
extern "C" const uint32_t g_nfsmw_android_contexto[11] __attribute__((weak));

namespace {

// Los valores de TouchControllerBridge.contexto().
constexpr jint kContextoDesconocido = -1;
constexpr jint kContextoMenus = 0;       // menus y pantallas de carga
constexpr jint kContextoMundo = 1;       // conduciendo: libre o en otra carrera
constexpr jint kContextoAceleracion = 2; // carrera de aceleracion
constexpr jint kContextoPausa = 3;       // en el mundo, con el juego en pausa (su menu, un mensaje...)
constexpr jint kContextoCinematica = 4;  // en el mundo, con una cinematica (NIS) en marcha

// Los indices de g_nfsmw_android_contexto.
enum Direccion {
  kEstado = 0,           // TheGameFlowManager.CurrentGameFlowState
  kCarrera = 1,          // GRaceStatus::fObj
  kTipoDeCarrera = 2,    // GRaceStatus::GetRaceType
  kTipoDeParametros = 3, // GRaceParameters::GetRaceType
  kNombresDeTipo = 4,    // {const char* nombre, int tipo}[11]
  kBuscarAtributo = 5,   // Attrib: buscar en la coleccion y en sus padres
  kDatoDeNodo = 6,       // Attrib: el valor de un nodo
  kPausas = 7,           // FEManager::mPauseRequest
  kPedirPausa = 8,       // FEManager::RequestPauseSimulation
  kCinematicaMando = 9,  // NISActivity::JoyHandle: INIS::mInstance y HUDACTION_SKIPNIS
  kSaltarCinematica = 10, // NISActivity::SkipOverNIS (desde INIS): mState
};

constexpr uint32_t kEstadoEnElMundo = 6;     // GAMEFLOW_STATE_RACING
constexpr int kTipoAceleracion = 2;          // GRace::kRaceType_Drag
constexpr int kTipoDesconocido = -1;
constexpr uint32_t kTiposDeCarrera = 11;
// GRaceStatus y GRaceParameters (360).
constexpr uint32_t kOffParametros = 0x1A30;  // GRaceStatus::mRaceParms
constexpr uint32_t kOffIndice = 4;           // GRaceParameters::mIndex
constexpr uint32_t kOffRegistro = 8;         // GRaceParameters::mRaceRecord
constexpr uint32_t kOffTipo = 0x2B;          // GRaceIndexData::mRaceType
// Attrib.
constexpr uint32_t kClaveTipo = 0x0F6BCDE1;  // el atributo "racetype"
constexpr uint32_t kOffColeccion = 4;        // la instancia -> su coleccion
constexpr uint32_t kOffNodos = 0;            // tabla: los nodos
constexpr uint32_t kOffTamano = 4;           //        cuantos caben
constexpr uint32_t kOffPadre = 0x10;         // coleccion: la de la que hereda
constexpr uint32_t kOffClase = 0x14;
constexpr uint32_t kOffFija = 0x18;          //             su parte fija
constexpr uint32_t kOffDefiniciones = 8;     // clase -> +8 -> +0xC, la tabla de definiciones
constexpr uint32_t kOffTablaDefiniciones = 0xC;
constexpr uint32_t kBytesNodo = 12;          // clave, dato, ..., banderas en +11
constexpr uint32_t kOffDatoNodo = 4;
constexpr uint32_t kOffBanderasNodo = 11;
constexpr uint8_t kNodoLista = 0x02;         // el dato es una lista
constexpr uint8_t kNodoEnFija = 0x10;        // el dato es un desplazamiento en la parte fija
constexpr uint8_t kNodoDentro = 0x20;        // el valor va en el propio nodo
constexpr uint32_t kMaxNodos = 4096;         // una tabla mas grande no es lo que se espera
constexpr uint32_t kMaxPadres = 8;
// NISActivity, desde su INIS (360).
constexpr uint32_t kOffEstadoCinematica = 684;  // mState
constexpr uint32_t kCinematicaEnCurso = 5;      // NISACTIVITY_PLAYING

// Las instrucciones de las que salen esos desplazamientos (iguales en las tres
// ediciones): si no estan, el ejecutable no es el que se espera y no se lee nada.
struct Instruccion {
  Direccion funcion;
  uint32_t desplazamiento;
  uint32_t palabra;
};
constexpr Instruccion kInstrucciones[] = {
    {kTipoDeCarrera, 0x00, 0x80631A30},     // lwz r3,0x1A30(r3)    mRaceParms
    {kTipoDeParametros, 0x14, 0x817F0004},  // lwz r11,4(r31)       mIndex
    {kTipoDeParametros, 0x20, 0x896B002B},  // lbz r11,0x2B(r11)    mRaceType
    {kTipoDeParametros, 0x44, 0x817F0008},  // lwz r11,8(r31)       mRaceRecord
    {kTipoDeParametros, 0x48, 0x806B0004},  // lwz r3,4(r11)        la coleccion
    {kTipoDeParametros, 0x54, 0x3C800F6B},  // lis r4,0x0F6B        "racetype"
    {kTipoDeParametros, 0x5C, 0x6084CDE1},  // ori r4,r4,0xCDE1
    {kBuscarAtributo, 0x38, 0x817F0004},    // lwz r11,4(r31)       tamano de la tabla
    {kBuscarAtributo, 0x48, 0x815F0000},    // lwz r10,0(r31)       sus nodos
    {kBuscarAtributo, 0x5C, 0x890B000B},    // lbz r8,11(r11)       banderas del nodo
    {kBuscarAtributo, 0xCC, 0x83FF0010},    // lwz r31,16(r31)      el padre
    {kBuscarAtributo, 0xD8, 0x80DD0018},    // lwz r6,24(r29)       la parte fija
    {kBuscarAtributo, 0xE4, 0x80BD0014},    // lwz r5,20(r29)       la clase
    {kBuscarAtributo, 0xEC, 0x81650008},    // lwz r11,8(r5)
    {kBuscarAtributo, 0xF0, 0x386B000C},    // addi r3,r11,12       sus definiciones
    {kDatoDeNodo, 0x00, 0x8963000B},        // lbz r11,11(r3)       banderas
    {kDatoDeNodo, 0x24, 0x38630004},        // addi r3,r3,4         valor dentro del nodo
};

// Las de la cinematica, aparte: si no estan, lo demas del contexto sigue valiendo.
constexpr Instruccion kInstruccionesCinematica[] = {
    {kCinematicaMando, 0x164, 0x2F0B0032},  // cmpwi cr6,r11,50     HUDACTION_SKIPNIS
    {kSaltarCinematica, 0x58, 0x811F02AC},  // lwz r8,684(r31)      mState
    {kSaltarCinematica, 0x5C, 0x2F080005},  // cmpwi cr6,r8,5       NISACTIVITY_PLAYING
};
// En NISActivity::JoyHandle, lis r30,ALTA y lwz r3,BAJA(r30): INIS::mInstance.
constexpr uint32_t kOffAltaCinematica = 0x11C;
constexpr uint32_t kOffBajaCinematica = 0x130;

bool Legible(rex::memory::Memory* memoria, uint32_t direccion, uint32_t bytes) {
  if (direccion == 0 || bytes == 0 || uint64_t(direccion) + bytes > 0x100000000ull) {
    return false;
  }
  // Pagina a pagina (4 KB, la mas pequena del juego).
  for (uint32_t pagina = direccion & ~0xFFFu; pagina < direccion + bytes; pagina += 0x1000) {
    auto* heap = memoria->LookupHeap(pagina);
    uint32_t proteccion = 0;
    if (!heap || !heap->QueryProtect(std::max(pagina, direccion), &proteccion) ||
        !(proteccion & rex::memory::kMemoryProtectRead)) {
      return false;
    }
  }
  return true;
}

// Sin comprobar: solo tras Legible() del rango.
uint32_t Leido32(rex::memory::Memory* memoria, uint32_t direccion) {
  uint32_t v;
  std::memcpy(&v, memoria->TranslateVirtual<const uint8_t*>(direccion), sizeof(v));
  return __builtin_bswap32(v);
}

uint16_t Leido16(rex::memory::Memory* memoria, uint32_t direccion) {
  uint16_t v;
  std::memcpy(&v, memoria->TranslateVirtual<const uint8_t*>(direccion), sizeof(v));
  return __builtin_bswap16(v);
}

uint8_t Leido8(rex::memory::Memory* memoria, uint32_t direccion) {
  return *memoria->TranslateVirtual<const uint8_t*>(direccion);
}

bool Leer32(rex::memory::Memory* memoria, uint32_t direccion, uint32_t* valor) {
  if ((direccion & 3) || !Legible(memoria, direccion, 4)) {
    return false;
  }
  *valor = Leido32(memoria, direccion);
  return true;
}

bool Leer8(rex::memory::Memory* memoria, uint32_t direccion, uint8_t* valor) {
  if (!Legible(memoria, direccion, 1)) {
    return false;
  }
  *valor = Leido8(memoria, direccion);
  return true;
}

// Un texto del juego, de como mucho `max` letras (sin el cero).
bool LeerTexto(rex::memory::Memory* memoria, uint32_t direccion, char* texto, uint32_t max) {
  if (!Legible(memoria, direccion, 1)) {
    return false;
  }
  for (uint32_t i = 0; i <= max; ++i) {
    if (((direccion + i) & 0xFFF) == 0 && !Legible(memoria, direccion + i, 1)) {
      return false;
    }
    texto[i] = char(Leido8(memoria, direccion + i));
    if (texto[i] == '\0') {
      return true;
    }
  }
  return false;  // mas largo de lo que se espera
}

bool MismoTexto(const char* a, const char* b) {  // sin mayusculas, como el juego
  for (; *a && *b; ++a, ++b) {
    if (std::tolower(uint8_t(*a)) != std::tolower(uint8_t(*b))) {
      return false;
    }
  }
  return *a == *b;
}

// El nodo de `clave` en una tabla de Attrib, o 0. Un hueco libre tiene el dato
// apuntando al propio nodo y sin la bandera kNodoEnFija (sub_821464C0).
uint32_t BuscarNodo(rex::memory::Memory* memoria, uint32_t tabla, uint32_t clave) {
  uint32_t nodos = 0, tamano = 0;
  if (!Leer32(memoria, tabla + kOffNodos, &nodos) || !Leer32(memoria, tabla + kOffTamano, &tamano) ||
      tamano == 0 || tamano > kMaxNodos || (nodos & 3) || !Legible(memoria, nodos, tamano * kBytesNodo)) {
    return 0;
  }
  for (uint32_t i = 0; i < tamano; ++i) {
    const uint32_t nodo = nodos + i * kBytesNodo;
    const uint32_t dato = Leido32(memoria, nodo + kOffDatoNodo);
    const bool ocupado = (Leido8(memoria, nodo + kOffBanderasNodo) & kNodoEnFija) || dato != nodo;
    if (ocupado && Leido32(memoria, nodo) == clave) {
      return nodo;
    }
  }
  return 0;
}

// Donde esta el valor de "racetype" de una coleccion, como sub_82148818 con el
// indice 0, o 0.
uint32_t ValorDelAtributo(rex::memory::Memory* memoria, uint32_t coleccion, uint32_t clave) {
  uint32_t nodo = 0, duena = 0;
  uint32_t c = coleccion;
  for (uint32_t n = 0; c && n < kMaxPadres && !nodo; ++n) {
    nodo = BuscarNodo(memoria, c, clave);
    if (nodo) {
      duena = c;
    } else if (!Leer32(memoria, c + kOffPadre, &c)) {
      return 0;
    }
  }
  if (!nodo) {
    // En la parte fija: la definicion de la clase dice donde.
    uint32_t fija = 0, clase = 0, definiciones = 0;
    if (!Leer32(memoria, coleccion + kOffFija, &fija) || !fija ||
        !Leer32(memoria, coleccion + kOffClase, &clase) ||
        !Leer32(memoria, clase + kOffDefiniciones, &definiciones)) {
      return 0;
    }
    nodo = BuscarNodo(memoria, definiciones + kOffTablaDefiniciones, clave);
    duena = coleccion;
  }
  uint8_t banderas = 0;
  uint32_t dato = 0, fija = 0;
  if (!nodo || !Leer8(memoria, nodo + kOffBanderasNodo, &banderas) ||
      !Leer32(memoria, nodo + kOffDatoNodo, &dato) || !Leer32(memoria, duena + kOffFija, &fija)) {
    return 0;
  }
  if (banderas & kNodoLista) {
    // sub_82145428 con el indice 0: cuantos (+2), tamano de cada uno (+4; 0 =
    // guardados como punteros) y +8 de cabecera, mas 8 con el bit alto de +6.
    const uint32_t lista = (banderas & kNodoEnFija) ? fija + dato : dato;
    if (!Legible(memoria, lista, 8) || Leido16(memoria, lista + 2) == 0) {
      return 0;
    }
    const uint32_t primero = lista + 8 + ((Leido16(memoria, lista + 6) & 0x8000) ? 8 : 0);
    uint32_t valor = 0;
    if (Leido16(memoria, lista + 4) != 0) {
      return primero;
    }
    return Leer32(memoria, primero, &valor) ? valor : 0;
  }
  if (banderas & kNodoDentro) {
    return nodo + kOffDatoNodo;
  }
  return (banderas & kNodoEnFija) ? fija + dato : dato;
}

// El tipo segun su nombre en la tabla del juego, o kTipoDesconocido.
int TipoPorNombre(rex::memory::Memory* memoria, const uint32_t* direcciones, const char* nombre) {
  for (uint32_t i = 0; i < kTiposDeCarrera; ++i) {
    uint32_t texto = 0, tipo = 0;
    char suyo[24];
    if (Leer32(memoria, direcciones[kNombresDeTipo] + i * 8, &texto) &&
        Leer32(memoria, direcciones[kNombresDeTipo] + i * 8 + 4, &tipo) &&
        LeerTexto(memoria, texto, suyo, sizeof(suyo) - 1) && MismoTexto(nombre, suyo)) {
      return int(int32_t(tipo));
    }
  }
  return kTipoDesconocido;
}

// Lo que se ha leido, para el diagnostico.
struct Lectura {
  uint32_t estado = 0;
  uint32_t pausas = 0;
  uint32_t carrera = 0;
  uint32_t parametros = 0;
  uint32_t indice = 0;
  uint32_t coleccion = 0;
  char nombre[24] = "";
  int tipo = kTipoDesconocido;
  const char* via = "";
  uint32_t cinematica = 0;         // INIS::mInstance
  uint32_t estado_cinematica = 0;  // su mState
};

// El tipo de una carrera, como GRaceParameters::GetRaceType.
int TipoDeCarrera(rex::memory::Memory* memoria, const uint32_t* direcciones, uint32_t parametros,
                  Lectura& l) {
  uint8_t tipo = 0;
  if (Leer32(memoria, parametros + kOffIndice, &l.indice) && l.indice &&
      Leer8(memoria, l.indice + kOffTipo, &tipo)) {
    l.via = "indice";
    return int8_t(tipo);
  }
  uint32_t registro = 0, valor = 0, texto = 0;
  if (!Leer32(memoria, parametros + kOffRegistro, &registro) ||
      !Leer32(memoria, registro + kOffColeccion, &l.coleccion) || !l.coleccion) {
    return kTipoDesconocido;
  }
  valor = ValorDelAtributo(memoria, l.coleccion, kClaveTipo);
  if (!valor || !Leer32(memoria, valor, &texto) ||
      !LeerTexto(memoria, texto, l.nombre, sizeof(l.nombre) - 1)) {
    return kTipoDesconocido;
  }
  l.via = "atributo";
  return TipoPorNombre(memoria, direcciones, l.nombre);
}

// -1 sin comprobar, 0 no coinciden, 1 bien. Solo lo toca el hilo de Java que pregunta.
int g_instrucciones_bien = -1;
jint g_ultimo_contexto = kContextoDesconocido - 1;
// INIS::mInstance: 0 sin comprobar aun, 1 si no se puede (otro ejecutable).
uint32_t g_dir_cinematica = 0;
// El tipo, una vez por carrera: la misma carrera tiene los mismos parametros y la misma coleccion.
uint32_t g_cache_parametros = 0;
uint32_t g_cache_coleccion = 0;
Lectura g_cache;

bool InstruccionesBien(rex::memory::Memory* memoria, const uint32_t* direcciones) {
  if (g_instrucciones_bien < 0) {
    // Si la app pregunta antes de que el ejecutable este en memoria, ahi hay
    // ceros: no se decide nada y se vuelve a mirar en la siguiente pregunta.
    for (const auto& i : kInstrucciones) {
      uint32_t palabra = 0;
      if (!Leer32(memoria, direcciones[i.funcion] + i.desplazamiento, &palabra) || palabra == 0) {
        return false;
      }
    }
    g_instrucciones_bien = 1;
    for (const auto& i : kInstrucciones) {
      uint32_t palabra = 0;
      const uint32_t direccion = direcciones[i.funcion] + i.desplazamiento;
      if (!Leer32(memoria, direccion, &palabra) || palabra != i.palabra) {
        REXLOG_WARN("[contexto] en {:08X} esperaba {:08X} y hay {:08X}: sin controles por contexto",
                    direccion, i.palabra, palabra);
        g_instrucciones_bien = 0;
        break;
      }
    }
    // FEManager::RequestPauseSimulation empieza con lis r10,ALTA y en +0xC lee
    // el contador, lwz r11,BAJA(r10): que sea el de la tabla. No vale comparar
    // palabras fijas, porque en la japonesa .data se mueve y la parte baja cambia.
    if (g_instrucciones_bien) {
      uint32_t lis = 0, lwz = 0;
      const uint32_t f = direcciones[kPedirPausa];
      const bool forma = Leer32(memoria, f, &lis) && Leer32(memoria, f + 0xC, &lwz) &&
                         (lis & 0xFFFF0000) == 0x3D400000 && (lwz & 0xFFFF0000) == 0x816A0000;
      const uint32_t calculada = (lis << 16) + uint32_t(int32_t(int16_t(lwz & 0xFFFF)));
      if (!forma || calculada != direcciones[kPausas]) {
        REXLOG_WARN("[contexto] la pausa del juego no esta en {:08X} ({:08X} {:08X}): sin controles por contexto",
                    direcciones[kPausas], lis, lwz);
        g_instrucciones_bien = 0;
      }
    }
    // Y la tabla de nombres tiene "drag" con su tipo. La rellena el juego al
    // arrancar: mientras no este, se vuelve a mirar en la siguiente pregunta.
    if (g_instrucciones_bien && TipoPorNombre(memoria, direcciones, "drag") != kTipoAceleracion) {
      static bool avisado = false;
      if (!avisado) {
        avisado = true;
        REXLOG_INFO("[contexto] la tabla de tipos de carrera aun no tiene \"drag\": se vuelve a mirar");
      }
      g_instrucciones_bien = -1;
      return false;
    }
  }
  return g_instrucciones_bien == 1;
}

// Donde esta INIS::mInstance, o 0 si las instrucciones no son las que se esperan.
uint32_t DireccionCinematica(rex::memory::Memory* memoria, const uint32_t* direcciones) {
  if (g_dir_cinematica == 0) {
    uint32_t palabra = 0;
    for (const auto& i : kInstruccionesCinematica) {
      if (!Leer32(memoria, direcciones[i.funcion] + i.desplazamiento, &palabra) || palabra == 0) {
        return 0;  // el ejecutable aun no esta en memoria
      }
    }
    g_dir_cinematica = 1;
    bool bien = true;
    for (const auto& i : kInstruccionesCinematica) {
      const uint32_t direccion = direcciones[i.funcion] + i.desplazamiento;
      if (!Leer32(memoria, direccion, &palabra) || palabra != i.palabra) {
        REXLOG_WARN("[contexto] en {:08X} esperaba {:08X} y hay {:08X}: sin saltar cinematicas",
                    direccion, i.palabra, palabra);
        bien = false;
        break;
      }
    }
    // Como la pausa: la parte alta y la baja, de sus instrucciones (en la
    // japonesa .bss se mueve).
    uint32_t lis = 0, lwz = 0;
    const uint32_t f = direcciones[kCinematicaMando];
    if (bien && Leer32(memoria, f + kOffAltaCinematica, &lis) && Leer32(memoria, f + kOffBajaCinematica, &lwz) &&
        (lis & 0xFFFF0000) == 0x3FC00000 && (lwz & 0xFFFF0000) == 0x807E0000) {
      g_dir_cinematica = (lis << 16) + uint32_t(int32_t(int16_t(lwz & 0xFFFF)));
      REXLOG_INFO("[contexto] INIS::mInstance en {:08X}", g_dir_cinematica);
    } else if (bien) {
      REXLOG_WARN("[contexto] INIS::mInstance no esta donde se espera ({:08X} {:08X}): sin saltar cinematicas",
                  lis, lwz);
    }
  }
  return g_dir_cinematica > 1 ? g_dir_cinematica : 0;
}

const uint32_t* Direcciones() {
  // Simbolo debil: sin el parche su direccion es nula, y el compilador no
  // tiene que darla por buena.
  const uint32_t* direcciones = g_nfsmw_android_contexto;
  asm volatile("" : "+r"(direcciones));
  return direcciones;
}

jint LeerContexto(Lectura& l) {
  auto* runtime = rex::Runtime::instance();
  auto* memoria = runtime ? runtime->memory() : nullptr;
  const uint32_t* direcciones = Direcciones();
  if (!direcciones || !memoria || !InstruccionesBien(memoria, direcciones) ||
      !Leer32(memoria, direcciones[kEstado], &l.estado)) {
    return kContextoDesconocido;
  }
  if (l.estado != kEstadoEnElMundo) {
    return kContextoMenus;
  }
  // En pausa (su menu, un mensaje del movil del juego...): los controles de los
  // menus, sin girar inclinando. Un numero raro es que no es eso: se ignora.
  if (Leer32(memoria, direcciones[kPausas], &l.pausas) && l.pausas > 0 && l.pausas <= 8) {
    return kContextoPausa;
  }
  // Una cinematica en marcha: un toque la salta.
  const uint32_t cinematica = DireccionCinematica(memoria, direcciones);
  if (cinematica && Leer32(memoria, cinematica, &l.cinematica) && l.cinematica &&
      Leer32(memoria, l.cinematica + kOffEstadoCinematica, &l.estado_cinematica) &&
      l.estado_cinematica == kCinematicaEnCurso) {
    return kContextoCinematica;
  }
  // En el mundo. Sin carrera (o sin poder leerla), conduciendo libre.
  if (!Leer32(memoria, direcciones[kCarrera], &l.carrera) ||
      !Leer32(memoria, l.carrera + kOffParametros, &l.parametros) || !l.parametros) {
    return kContextoMundo;
  }
  uint32_t registro = 0, coleccion = 0;
  if (Leer32(memoria, l.parametros + kOffRegistro, &registro)) {
    Leer32(memoria, registro + kOffColeccion, &coleccion);
  }
  if (l.parametros != g_cache_parametros || coleccion != g_cache_coleccion) {
    g_cache = Lectura{};
    g_cache.tipo = TipoDeCarrera(memoria, direcciones, l.parametros, g_cache);
    g_cache_parametros = l.parametros;
    g_cache_coleccion = coleccion;
  }
  l.indice = g_cache.indice;
  l.coleccion = g_cache.coleccion;
  std::memcpy(l.nombre, g_cache.nombre, sizeof(l.nombre));
  l.tipo = g_cache.tipo;
  l.via = g_cache.via;
  return l.tipo == kTipoAceleracion ? kContextoAceleracion : kContextoMundo;
}

}  // namespace

// TouchControllerView pregunta cada 250 ms mientras se ve. Cada cambio queda en
// el log, para comprobarlo con una partida.
extern "C" int32_t NfsmwTactilContexto() {
  Lectura l;
  const jint contexto = LeerContexto(l);
  if (contexto != g_ultimo_contexto) {
    static constexpr const char* kNombres[] = {"desconocido", "menus", "conduciendo",
                                               "aceleracion", "pausa", "cinematica"};
    REXLOG_INFO("[contexto] {} (estado {}, tipo {} por {})", kNombres[contexto + 1], l.estado, l.tipo,
                l.via);
    g_ultimo_contexto = contexto;
  }
  return contexto;
}

// Diagnostico: lo leido, en una linea. TouchControllerView la guarda en
// files/logs/contexto.txt cada vez que cambia (el APK release no escribe el
// log del juego).
extern "C" void NfsmwTactilContextoDiagnostico(char* linea, size_t tam) {
  Lectura l;
  const jint contexto = LeerContexto(l);
  std::snprintf(linea, tam,
                "contexto=%d instr=%d estado=%u pausas=%u nis=%08X/%u fObj=%08X parms=%08X idx=%08X col=%08X "
                "tipo=%d (%s \"%s\")",
                int(contexto), g_instrucciones_bien, l.estado, l.pausas, l.cinematica, l.estado_cinematica,
                l.carrera, l.parametros, l.indice, l.coleccion, l.tipo, l.via, l.nombre);
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT jint JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_contexto(JNIEnv*,
                                                                                         jclass) {
  return NfsmwTactilContexto();
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_nfsmwrecomp_TouchControllerBridge_contextoDiagnostico(JNIEnv* env, jclass) {
  char linea[256];
  NfsmwTactilContextoDiagnostico(linea, sizeof(linea));
  return env->NewStringUTF(linea);
}
#endif

#if defined(__ANDROID__)
// ---------------------------------------------------------------------------
//  9. La vibracion del mando tactil
// ---------------------------------------------------------------------------
//
// Su SDK le pasa a la app lo que el juego le pide al mando tactil
// (parche_nativo.py): la fuerza de los dos motores del mando de la Xbox 360,
// de 0 a 65535. El movil tiene uno: se usa el mas fuerte, en 16 niveles, y solo
// se llama a Java cuando cambia de nivel (el juego lo repite a menudo). La
// llama el hilo del juego; Java vibra con el Vibrator del sistema
// (GameActivity.vibrarMando), que dura hasta que el juego la cambie, como el
// mando de la Xbox 360. Los mandos fisicos vibran por su lado (SDL).

namespace {

std::atomic<int> g_nivel_vibracion{-1};

}  // namespace

extern "C" void NfsmwAndroidVibrar(uint16_t izquierdo, uint16_t derecho) {
  const int fuerza = std::max<int>(izquierdo, derecho);
  const int nivel = (fuerza + 4095) / 4096;  // 0..16
  if (g_nivel_vibracion.exchange(nivel) == nivel) {
    return;
  }
  auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
  auto actividad = static_cast<jobject>(SDL_GetAndroidActivity());
  if (!env || !actividad) {
    return;
  }
  jclass clase = env->GetObjectClass(actividad);
  jmethodID vibrar = env->GetStaticMethodID(clase, "vibrarMando", "(I)V");
  if (vibrar) {
    env->CallStaticVoidMethod(clase, vibrar, jint(std::min(255, nivel * 16)));
  }
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
  env->DeleteLocalRef(clase);
  env->DeleteLocalRef(actividad);
}
#endif  // __ANDROID__: seccion 9

// ---------------------------------------------------------------------------
//  10. Los menus con el dedo
// ---------------------------------------------------------------------------
//
// En los menus, TouchControllerView quita el mando y la pantalla entera es del
// dedo: deslizar mueve por el menu (la cruceta) y un toque va a lo que hay
// debajo. Lo que hay en pantalla se lee del motor de menus del juego (FEng), con
// lo que da su descompilacion (github.com/dbalatoni13/nfsmw) y lo que hace su
// codigo en el 360:
//
//   cFEng::mInstance -> +0 FEngine -> +0xE0 el primer paquete, +4 el siguiente
//   paquete (FEPackage): +0xC su nombre, +0x20 los mandos que lo controlan,
//       +0x28 si atiende al mando, +0x4C su primer objeto, +0x8C el boton
//       resaltado (el de los botones del propio FEng)
//   objeto (FEObject): +4 el siguiente, +0x18 el tipo, +0x1C las banderas (bit
//       0: oculto), +0x24 la textura (imagenes) o la fuente (textos), +0x2C sus
//       datos (+0xC el alfa, +0x1C la posicion, +0x38 el tamano), +0x4C su
//       primer guion, +0x54 el guion puesto. Un grupo, +0x64 su primer hijo, con
//       la posicion relativa a la del grupo; un texto, +0x64 sus letras (UTF-16)
//       y +0x6C como va alineado
//   guion (FEScript): +0x18 el que le sigue al acabar, +0x30 su nombre (hash)
//   fuente (FindFont): +0x14 su identificador, +0x24 su altura
//   textos del idioma: una tabla de {hash de la etiqueta, texto en bytes}, de
//       menor a mayor hash, y cuantos hay
//
// Son los de GameCube menos 0xC en el paquete (alli los bool ocupan 4 bytes).
// Antes de fiarse de ellos se comprueban las instrucciones del juego de las que
// salen (kInstruccionesMenus), como en el contexto, y cada lectura mira antes
// que la pagina sea legible: Java pregunta desde su hilo mientras el juego
// carga y descarga paquetes.
//
// Lo que se reconoce:
//   - las indicaciones de abajo, "(A) Aceptar", "(B) Atras". En casi todas las
//     pantallas son un icono y, a su derecha, un texto con una etiqueta de las de
//     siempre (kEtiquetasIndicacion: "Atras" es 61D596D7 en todo el juego); su zona
//     es el texto y el icono. El texto de un objeto es el de su etiqueta (+0x60),
//     buscado en la tabla de textos del juego como hace el (kBuscarTexto), o el
//     suyo propio (+0x64) si tiene la bandera 2. Otras formas: un texto con el
//     icono dentro, "$NOMBRE$ Atras", que el juego pinta con la textura NOMBRE
//     (FEngFont::HandleJoyEventTexture; el boton sale del nombre, BotonDeIcono), y
//     una imagen suelta con la textura 1468 (LT) o 1666 (RT), las que les pone cada
//     pantalla al crearse (FEMenuScreen::FEMenuScreen). El icono de la izquierda
//     tambien cuenta solo, por su textura (kTexturasIndicacion). Lo que no se conoce
//     sale en el diagnostico.
//   - las opciones: los objetos con el guion HIGHLIGHT y los botones del FEng,
//     de los paquetes que atienden al mando. La resaltada es la que tiene puesto
//     ese guion (o uno de los que le siguen), o el boton resaltado del paquete.
//
// Ir a otra opcion es pulsar la cruceta hacia ella hasta que el juego la
// resalte: lo hace TouchControllerView, una pulsacion cada vez, preguntando
// aqui el rumbo despues de cada una (menuRumbo). Asi el juego navega como
// siempre, con sus sonidos y sus guiones, y nada se escribe en su memoria.
// Tocar una opcion la elige; tocar la ya elegida, o la indicacion de A, acepta. Lo
// pidio el usuario: un toque cualquiera no puede ir aceptando cosas, pero si tocar
// lo que se ve elegido. Donde no hay indicacion de A (videos, "Presionar START",
// cargas: ninguna indicacion), un toque acepta. Los dialogos ("atencion", Si / No)
// igual: el primer toque elige la respuesta y el segundo la acepta (se probo
// aceptarla al primero y el usuario lo prefirio asi).
//
// Las flechas de la opcion resaltada (en las opciones, a los lados del valor; en
// el carrusel del menu principal, sus extremos) son imagenes con la textura
// kTexturasFlecha: tocarlas pulsa la cruceta hacia ese lado. Igual los "-" y "+" de
// los extremos de una barra (los reglajes de prestaciones: son textos sueltos). Y en una lista de
// opciones (menuEsLista) deslizar en horizontal cambia el valor en el sentido del
// dedo, no como al pasar paginas.
//
// El menu se pinta en 640x480 con el centro en (0, 0) (cFEngRender suma 320,
// 240), y en el 16:9 de la Xbox 360 ocupa el 4:3 del centro: el icono redondo
// de "(B) Atras" sale redondo en una captura sin estirar, y las indicaciones
// caen en el margen seguro de un 4:3 (x = -256).

// Su app (nfsmw_recortes_carrera.cpp, con parche_nativo.py), como
// g_nfsmw_android_contexto. Debil: sin el parche, los toques solo aceptan.
extern "C" const uint32_t g_nfsmw_android_menus[16] __attribute__((weak));

namespace {

// Los indices de g_nfsmw_android_menus.
enum FuncionMenus : uint8_t {
  kBuscarImagen = 0,      // FEngFindImage: cFEng::mInstance
  kMensajeAPaquete = 1,   // cFEng::QueuePackageMessage: mFEng y la lista de paquetes
  kActualizarFEng = 2,    // FEngine::Update
  kRatonPaquete = 3,      // FEngine::ProcessMouseForPackage
  kBotonActual = 4,       // FEngGetCurrentButton
  kTodosLosObjetos = 5,   // FEPackage::ForAllObjects
  kObjetosDeGrupo = 6,    // lo mismo dentro de un grupo
  kPonerEsquina = 7,      // FEngSetTopLeft
  kPonerGuion = 8,        // FEngSetScript
  kGuionActual = 9,       // FEObject::SetCurrentScript
  kAvanzarGuion = 10,     // FEPackage: el guion que sigue a uno que acaba
  kPantallaMenu = 11,     // FEMenuScreen::FEMenuScreen
  kBuscarFuente = 12,     // FindFont
  kAltoTexto = 13,        // FEngFont::GetTextHeight
  kBuscarTexto = 14,      // el texto de una etiqueta en el idioma del juego
  kPonerEtiqueta = 15,    // FEngSetLanguageHash
};

struct InstruccionMenus {
  FuncionMenus funcion;
  uint32_t desplazamiento;
  uint32_t palabra;
};
constexpr InstruccionMenus kInstruccionesMenus[] = {
    {kMensajeAPaquete, 0x58, 0x817E0000},  // lwz r11,0(r30)        cFEng::mFEng
    {kMensajeAPaquete, 0x5C, 0x396B00D8},  // addi r11,r11,0xD8     FEngine::PackList
    {kMensajeAPaquete, 0x68, 0x83EB0008},  // lwz r31,8(r11)        su primer paquete
    {kMensajeAPaquete, 0x90, 0x83FF0004},  // lwz r31,4(r31)        el siguiente
    {kActualizarFEng, 0xD4, 0x83DF00E0},   // lwz r30,0xE0(r31)     el primer paquete
    {kActualizarFEng, 0xE0, 0x88FE0028},   // lbz r7,0x28(r30)      atiende al mando
    {kActualizarFEng, 0x128, 0x83DE0004},  // lwz r30,4(r30)        el siguiente
    {kRatonPaquete, 0x1C, 0x817D0020},     // lwz r11,0x20(r29)     los mandos que lo controlan
    {kBotonActual, 0x3C, 0x806B008C},      // lwz r3,0x8C(r11)      el boton resaltado
    {kTodosLosObjetos, 0x14, 0x83FD004C},  // lwz r31,0x4C(r29)     el primer objeto
    {kTodosLosObjetos, 0x44, 0x811F0018},  // lwz r8,0x18(r31)      su tipo
    {kTodosLosObjetos, 0x6C, 0x83FF0004},  // lwz r31,4(r31)        el siguiente
    {kObjetosDeGrupo, 0x0C, 0x83E40064},   // lwz r31,0x64(r4)      el primer hijo de un grupo
    {kPonerEsquina, 0x30, 0x817F002C},     // lwz r11,0x2C(r31)     sus datos
    {kPonerEsquina, 0x34, 0x815F0018},     // lwz r10,0x18(r31)     el tipo
    {kPonerEsquina, 0x3C, 0xC00B001C},     // lfs f0,0x1C(r11)      la posicion
    {kPonerEsquina, 0x44, 0xC1AB0020},     // lfs f13,0x20(r11)
    {kPonerEsquina, 0x4C, 0xC3EB0038},     // lfs f31,0x38(r11)     el tamano
    {kPonerEsquina, 0x50, 0xC3CB003C},     // lfs f30,0x3C(r11)
    {kPonerEsquina, 0xCC, 0x807F0024},     // lwz r3,0x24(r31)      la fuente de un texto
    {kPonerEsquina, 0xE4, 0x809F0064},     // lwz r4,0x64(r31)      sus letras
    {kPonerEsquina, 0x10C, 0x817F006C},    // lwz r11,0x6C(r31)     como va alineado
    {kPonerGuion, 0x20, 0x83E3004C},       // lwz r31,0x4C(r3)      el primer guion
    {kPonerGuion, 0x2C, 0x817F0030},       // lwz r11,0x30(r31)     su nombre
    {kGuionActual, 0x14, 0x817F0054},      // lwz r11,0x54(r31)     el guion puesto
    {kAvanzarGuion, 0x114, 0x83FF0018},    // lwz r31,0x18(r31)     el que le sigue
    {kAvanzarGuion, 0x11C, 0x93FD0054},    // stw r31,0x54(r29)     pasa a ser el puesto
    {kPantallaMenu, 0x10C, 0x81630018},    // lwz r11,0x18(r3)      el tipo
    {kPantallaMenu, 0x118, 0x81430024},    // lwz r10,0x24(r3)      la textura
    {kPantallaMenu, 0x11C, 0x2B0A05BC},    // cmplwi cr6,r10,1468   la de A
    {kPantallaMenu, 0x160, 0x2B050682},    // cmplwi cr6,r5,1666    la de B
    {kBuscarFuente, 0x20, 0x814B0014},     // lwz r10,0x14(r11)     el identificador de la fuente
    {kBuscarFuente, 0x2C, 0x816B0000},     // lwz r11,0(r11)        la siguiente
    {kAltoTexto, 0x140, 0xC15D0024},       // lfs f10,0x24(r29)     su altura
    {kBuscarTexto, 0x08, 0x2B090000},      // cmplwi cr6,r9,0       sin tabla, nada
    {kBuscarTexto, 0x28, 0x7D4A482E},      // lwzx r10,r10,r9       el hash de una entrada (de 8 bytes)
    {kBuscarTexto, 0xA8, 0x80690004},      // lwz r3,4(r9)          su texto
    {kPonerEtiqueta, 0x20, 0x909F0060},    // stw r4,0x60(r31)      la etiqueta de un texto
    {kPonerEtiqueta, 0x58, 0x50C7B7BC},    // rlwimi r7,r6,22,30,30 sin la bandera 2: el de la etiqueta
};

// FEng del 360.
constexpr uint32_t kOffMotor = 0x0;            // cFEng::mFEng
constexpr uint32_t kOffPaquetes = 0xE0;        // FEngine::PackList, su primer paquete
constexpr uint32_t kOffSiguiente = 0x4;        // de un paquete, un objeto o un guion
constexpr uint32_t kOffNombrePaquete = 0xC;
constexpr uint32_t kOffMandosPaquete = 0x20;
constexpr uint32_t kOffAtiendePaquete = 0x28;
constexpr uint32_t kOffObjetos = 0x4C;
constexpr uint32_t kOffBotonResaltado = 0x8C;
constexpr uint32_t kOffTipoObjeto = 0x18;
constexpr uint32_t kOffBanderas = 0x1C;
constexpr uint32_t kOffTextura = 0x24;         // en un texto, su fuente
constexpr uint32_t kOffDatos = 0x2C;
constexpr uint32_t kOffGuiones = 0x4C;
constexpr uint32_t kOffGuionPuesto = 0x54;
constexpr uint32_t kOffHijos = 0x64;           // FEGroup
constexpr uint32_t kOffEtiqueta = 0x60;        // FEString
constexpr uint32_t kOffLetras = 0x64;          // FEString
constexpr uint32_t kOffFormato = 0x6C;         // FEString
constexpr uint32_t kOffAlfa = 0x0C;            // FEObjData: Col.a
constexpr uint32_t kOffPosicion = 0x1C;
constexpr uint32_t kOffTamanoObjeto = 0x38;
constexpr uint32_t kBytesDatos = 0x44;
constexpr uint32_t kOffGuionSiguiente = 0x18;  // FEScript::pChainTo
constexpr uint32_t kOffNombreGuion = 0x30;
constexpr uint32_t kOffIdFuente = 0x14;
constexpr uint32_t kOffAltoFuente = 0x24;

constexpr uint32_t kTipoImagen = 1, kTipoTexto = 2, kTipoGrupo = 5, kTipoPelicula = 7, kTipoImagenColor = 9,
                   kTipoImagenAnimada = 10, kTipoImagenSimple = 11, kTipoMultiImagen = 12;
constexpr uint32_t kBanderaOculto = 1u;
constexpr uint32_t kBanderaTextoPropio = 2u;    // un texto: el suyo, no el de su etiqueta
constexpr uint32_t kBanderaBoton = 1u << 28;    // FF_IsButton
constexpr uint32_t kBanderaNoBoton = 1u << 26;  // FF_IgnoreButton
constexpr uint32_t kFormatoCentrado = 1, kFormatoDerecha = 2, kFormatoCentradoV = 4, kFormatoAbajo = 8;
constexpr uint32_t kGuionResaltar = 0x249DB7B7;  // HIGHLIGHT
constexpr uint32_t kGuionApagar = 0x7AB5521A;    // UNHIGHLIGHT
// Las que pone FEMenuScreen::FEMenuScreen a las imagenes 0x6B364F8B y 0x79354351. En la
// descompilacion de GameCube parecian A y B; en el 360 son los iconos de LT y RT (EA Trax, arriba,
// a los lados del modo de reproduccion: visto en el movil).
constexpr uint32_t kTexturaGatilloLT = 1468;
constexpr uint32_t kTexturaGatilloRT = 1666;

// Los botones, con el indice de SDL_GamepadButton, como los manda TouchControllerView.
constexpr int kBotonA = 0, kBotonB = 1, kBotonX = 2, kBotonY = 3, kBotonStart = 6, kBotonLB = 9, kBotonRB = 10,
              kCrucetaArriba = 11, kCrucetaAbajo = 12, kCrucetaIzquierda = 13, kCrucetaDerecha = 14;
// Los gatillos no son botones del mando: TouchControllerView entiende estos dos como LT y RT
// pisados del todo.
constexpr int kGatilloLT = 16, kGatilloRT = 17;

// Lo que devuelven menuTocar y menuRumbo (TouchControllerBridge).
constexpr jint kMenusDesconocido = -1;    // no se puede leer el menu
constexpr jint kMenusNada = 0;            // ahi no hay nada que se conozca
constexpr jint kMenusIndicacion = 0x100;  // | el boton de la indicacion tocada
constexpr jint kMenusResaltada = 0x200;   // la opcion ya resaltada (aceptar es tocar la indicacion de A)
constexpr jint kMenusIr = 0x300;          // | la cruceta hacia la opcion tocada
constexpr jint kMenusSinRumbo = 0x400;    // una opcion, pero no se sabe llegar
constexpr jint kMenusLlegada = 0x500;     // el camino a la opcion tocada ha llegado: ya esta elegida
constexpr jint kMenusAceptar = 0x600;     // pulsar A: no hay indicacion de A con la que hacerlo

// Las etiquetas de los textos de las indicaciones, y su boton. Sacadas de files/logs/menus.txt
// en el movil (PAL Espana, 2026-10-11) y de las etiquetas que da la descompilacion
// (LANGUAGE_COMMON_*). Las etiquetas son hashes de nombres: las mismas en cualquier idioma.
struct EtiquetaIndicacion {
  uint32_t etiqueta;
  int boton;
};
constexpr EtiquetaIndicacion kEtiquetasIndicacion[] = {
    {0x159D2F37, kBotonA},  // "Aceptar": LANGUAGE_COMMON_ACCEPT
    {0x2B65A216, kBotonA},  // "Aceptar" (la que pone el codigo con FEngSetLanguageHash)
    {0x61D596D7, kBotonB},  // "Atras"
    {0x8CD567B8, kBotonB},  // LANGUAGE_COMMON_BACK
    {0x7F7340AF, kBotonY},  // "Estadisticas": LANGUAGE_COMMON_GAME_STATS, con Y en el menu principal
    // "Tarjeta de jugador", con RB, en la sala de juego y en el punto de encuentro del online
    // (OL_GameRoom, OL_LobbyRoom; 2026-10-16): su icono no es el RB de kTexturasIndicacion.
    {0x92463C0C, kBotonRB},
};

// Las texturas de los iconos de boton de las indicaciones (imagenes de 32x32 a la izquierda
// de su texto), sacadas igual del movil: cada boton tiene la suya en todas las pantallas.
struct TexturaIndicacion {
  uint32_t textura;
  int boton;
};
constexpr TexturaIndicacion kTexturasIndicacion[] = {
    {0x02959349, kBotonA},
    {0x6851AAF5, kBotonB},
    {0x54A68051, kBotonY},
    {0x79AD657F, kBotonRB},  // "Visualizar" al personalizar (visto en una captura)
    {0x7A8B6BD0, kBotonX},   // "Escuchar" en EA Trax (visto en una captura)
    {0x03B7F86D, kBotonStart},  // "Ver carrito" al personalizar: el triangulo de START (visto en una captura)
};
// "Rotar" en la barra de abajo al visualizar un coche: ahi TouchControllerView gira el
// coche mientras se arrastra el dedo (menuRotar). Su icono cambia con la edicion: la
// cruceta en la PAL espanola y el stick derecho (RS) en la italiana ("Ruota"), en la misma
// pantalla y en el mismo sitio. Su texto lleva la misma etiqueta en las dos (es la clave
// del texto, no el texto): vale para cualquier edicion, traiga el icono que traiga.
constexpr uint32_t kTexturasRotar[] = {
    0x9E6F09C3,  // la cruceta (PAL espanola)
    0xA5FC31AF,  // el stick derecho (PAL italiana)
};
constexpr uint32_t kEtiquetaRotar = 0x53EFBCB5;

// Las flechas a los lados de la opcion resaltada: la de los menus (y el carrusel del menu
// principal) y la de los de la partida (los reglajes de prestaciones, en la pausa).
constexpr uint32_t kTexturasFlecha[] = {0x1A6D3D2C, 0x02716E8A};
constexpr float kMargenFlecha = 12.0f;
// Lo que se ve del menu: lo que queda fuera es de un guion que lo saca de pantalla (las
// opciones de un carrusel que no caben, aparcadas debajo).
constexpr float kMedioAnchoVisible = 430.0f, kMedioAltoVisible = 240.0f;

// Un icono de indicacion que no se conoce: de ese tamano, en la barra de abajo, con su texto
// justo a la derecha. Tocarlo no hace nada (aceptar podria hacer otra cosa) y sale en el
// diagnostico con su textura.
constexpr float kLadoIconoMin = 26.0f, kLadoIconoMax = 40.0f, kBarraAbajo = 170.0f;

constexpr float kAnchoMenu = 640.0f;
constexpr float kAltoMenu = 480.0f;
constexpr float kFraccionAncho = 0.75f;  // del ancho del 16:9 que ocupa el 4:3 del menu
constexpr float kMargen = 4.0f;          // lo que se da de mas alrededor de cada caja, en unidades del menu
constexpr float kMargenIndicacion = 8.0f;  // y alrededor de una indicacion, que es pequena
constexpr uint32_t kMaxTexto = 96;
constexpr uint32_t kMaxPaquetes = 32;
constexpr int kMaxObjetos = 6000;
constexpr int kMaxProfundidad = 12;

struct Caja {
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  bool Vacia() const { return !(x1 > x0 && y1 > y0); }
  bool Contiene(float x, float y, float margen) const {
    return !Vacia() && x >= x0 - margen && x <= x1 + margen && y >= y0 - margen && y <= y1 + margen;
  }
  float Area() const { return Vacia() ? 0.0f : (x1 - x0) * (y1 - y0); }
  float Cx() const { return (x0 + x1) * 0.5f; }
  float Cy() const { return (y0 + y1) * 0.5f; }
  void Unir(const Caja& o) {
    if (o.Vacia()) {
      return;
    }
    x0 = std::min(x0, o.x0);
    y0 = std::min(y0, o.y0);
    x1 = std::max(x1, o.x1);
    y1 = std::max(y1, o.y1);
  }
};

struct Opcion {
  uint32_t objeto;
  uint32_t paquete;
  bool boton;  // un boton del FEng
  bool resaltada;
  Caja caja;
  uint32_t grupo;  // el grupo en que esta (0, ninguno): las opciones de un mismo grupo son una
};

struct Indicacion {
  int boton;
  Caja icono;
  Caja zona;      // el icono y su texto
  bool extender;  // un icono suelto: su texto es otro objeto, a la derecha (ZonaDeIndicacion)
};

// Un texto en pantalla: para la zona de las indicaciones con el icono aparte, y para el
// diagnostico.
struct Texto {
  Caja caja;
  uint32_t etiqueta;
  bool propio;  // el suyo, no el de la etiqueta
  char letras[kMaxTexto];
};

struct Menu {
  struct Paquete {
    uint32_t direccion;
    uint32_t mandos;
    uint32_t boton;
    bool atiende;
    int objetos;
    char nombre[40];
  };
  std::vector<Paquete> paquetes;
  std::vector<Opcion> opciones;
  std::vector<Indicacion> indicaciones;
  // Iconos que pueden ser de una indicacion de un boton que no se conoce (boton -1): lo son si
  // llevan un texto justo a la derecha (LeerMenu).
  std::vector<Indicacion> posibles;
  std::vector<Texto> textos;
  std::vector<Caja> flechas;
  // Los "-" y "+" de las barras: textos de un solo signo. Con su lado (true, el "-").
  std::vector<std::pair<Caja, bool>> signos;
  bool rotar = false;  // la barra de abajo tiene la indicacion "Rotar" (kTexturasRotar, kEtiquetaRotar)
  // Iconos de boton dentro de un texto que no se sabe que boton son (diagnostico).
  std::vector<std::string> iconos_sin_boton;
  // Imagenes pequenas, con su textura: entre ellas, los iconos de las indicaciones
  // (diagnostico, para reconocer las que aun no se conocen).
  std::vector<std::pair<uint32_t, Caja>> iconos;
};

int g_menus_bien = -1;          // -1 sin comprobar, 0 no coinciden, 1 bien. Solo lo toca el hilo de Java.
uint32_t g_instancia_feng = 0;  // donde esta cFEng::mInstance
uint32_t g_lista_fuentes = 0;   // la lista de fuentes de FindFont (su nodo centinela)
uint32_t g_alias_fuentes = 0;   // su tabla de alias: una pareja {de, a}
uint32_t g_tabla_textos = 0;    // donde esta el puntero a la tabla de textos del idioma
uint32_t g_cuantos_textos = 0;  // y cuantos tiene

// La opcion tocada a la que se va, y como va el camino.
uint32_t g_objetivo = 0;
uint32_t g_ultima_resaltada = 0;
int g_sin_moverse = 0;

// El ultimo toque, para el diagnostico.
float g_toque_x = 0.0f, g_toque_y = 0.0f;
jint g_toque_resultado = kMenusDesconocido;

const uint32_t* DireccionesMenus() {
  const uint32_t* direcciones = g_nfsmw_android_menus;
  asm volatile("" : "+r"(direcciones));
  return direcciones;
}

float LeidoF32(rex::memory::Memory* memoria, uint32_t direccion) {
  const uint32_t v = Leido32(memoria, direccion);
  float f;
  std::memcpy(&f, &v, sizeof(f));
  return f;
}

bool Razonable(float v) {
  return std::isfinite(v) && std::fabs(v) < 100000.0f;
}

// Las lecturas de un menu: como Leer32 y compania, pero mirando cada pagina una sola vez.
// Un menu son miles de lecturas en unas pocas paginas, y Legible busca la suya en el
// monton cada vez. Vale para una lectura del menu, no mas: entre una y otra el juego
// libera y reserva.
struct Lector {
  rex::memory::Memory* memoria;
  uint32_t vistas[64] = {};  // pagina + 1 de las ya legibles, por (pagina >> 12) & 63
  int objetos = 0;           // los mirados, con un tope para todos los paquetes

  bool Ver(uint32_t direccion, uint32_t bytes) {
    if (direccion == 0 || bytes == 0 || uint64_t(direccion) + bytes > 0x100000000ull) {
      return false;
    }
    for (uint32_t pagina = direccion & ~0xFFFu; pagina < direccion + bytes; pagina += 0x1000) {
      uint32_t& vista = vistas[(pagina >> 12) & 63];
      if (vista == pagina + 1) {
        continue;
      }
      if (!Legible(memoria, std::max(pagina, direccion), 1)) {
        return false;
      }
      vista = pagina + 1;
    }
    return true;
  }
  bool L32(uint32_t direccion, uint32_t* valor) {
    if ((direccion & 3) || !Ver(direccion, 4)) {
      return false;
    }
    *valor = Leido32(memoria, direccion);
    return true;
  }
  // Sin comprobar: solo tras Ver() del rango.
  uint32_t U32(uint32_t direccion) { return Leido32(memoria, direccion); }
  uint16_t U16(uint32_t direccion) { return Leido16(memoria, direccion); }
  float F32(uint32_t direccion) { return LeidoF32(memoria, direccion); }
};

// lis rD,ALTA y luego (addi|lwz) rX,BAJA(rD): la direccion que forman. `forma_*`, la
// parte alta de cada instruccion (el codigo y los registros).
bool DireccionDelCodigo(rex::memory::Memory* memoria, uint32_t lis, uint32_t baja, uint32_t forma_lis,
                        uint32_t forma_baja, uint32_t* direccion) {
  uint32_t a = 0, b = 0;
  if (!Leer32(memoria, lis, &a) || !Leer32(memoria, baja, &b) || (a & 0xFFFF0000) != forma_lis ||
      (b & 0xFFFF0000) != forma_baja) {
    return false;
  }
  *direccion = (a << 16) + uint32_t(int32_t(int16_t(b & 0xFFFF)));
  return true;
}

bool MenusBien(rex::memory::Memory* memoria, const uint32_t* f) {
  if (g_menus_bien < 0) {
    // Antes de que el ejecutable este en memoria ahi hay ceros: se vuelve a mirar.
    for (const auto& i : kInstruccionesMenus) {
      uint32_t palabra = 0;
      if (!Leer32(memoria, f[i.funcion] + i.desplazamiento, &palabra) || palabra == 0) {
        return false;
      }
    }
    g_menus_bien = 1;
    for (const auto& i : kInstruccionesMenus) {
      uint32_t palabra = 0;
      const uint32_t direccion = f[i.funcion] + i.desplazamiento;
      if (!Leer32(memoria, direccion, &palabra) || palabra != i.palabra) {
        REXLOG_WARN("[menus] en {:08X} esperaba {:08X} y hay {:08X}: los toques solo aceptan", direccion,
                    i.palabra, palabra);
        g_menus_bien = 0;
        break;
      }
    }
    // Las direcciones de datos se leen del codigo que las usa (en la japonesa .data se
    // mueve): FEngFindImage carga cFEng::mInstance con lis r11 + lwz r3; FindFont, su tabla
    // de alias con lis r11 + addi r8 y su lista con lis r11 + addi r9; la busqueda de
    // textos, la tabla con lis r11 + lwz r9 y cuantos tiene con lis r11 + lwz r11.
    if (g_menus_bien &&
        !(DireccionDelCodigo(memoria, f[kBuscarImagen] + 0x1C, f[kBuscarImagen] + 0x24, 0x3D600000, 0x806B0000,
                             &g_instancia_feng) &&
          DireccionDelCodigo(memoria, f[kBuscarFuente] + 0x0, f[kBuscarFuente] + 0x4, 0x3D600000, 0x390B0000,
                             &g_alias_fuentes) &&
          DireccionDelCodigo(memoria, f[kBuscarFuente] + 0x8, f[kBuscarFuente] + 0xC, 0x3D600000, 0x392B0000,
                             &g_lista_fuentes) &&
          DireccionDelCodigo(memoria, f[kBuscarTexto] + 0x0, f[kBuscarTexto] + 0x4, 0x3D600000, 0x812B0000,
                             &g_tabla_textos) &&
          DireccionDelCodigo(memoria, f[kBuscarTexto] + 0x10, f[kBuscarTexto] + 0x18, 0x3D600000, 0x816B0000,
                             &g_cuantos_textos))) {
      REXLOG_WARN("[menus] FEngFindImage, FindFont o la busqueda de textos no tienen la forma esperada: los toques "
                  "solo aceptan");
      g_menus_bien = 0;
    }
    if (g_menus_bien) {
      REXLOG_INFO("[menus] cFEng::mInstance en {:08X}, fuentes en {:08X} (alias en {:08X})", g_instancia_feng,
                  g_lista_fuentes, g_alias_fuentes);
    }
  }
  return g_menus_bien == 1;
}

// La altura de una fuente, como FindFont: su lista y una pareja de alias. 0 si no esta.
float AlturaFuente(Lector& l, uint32_t id) {
  for (int vuelta = 0; vuelta < 3; ++vuelta) {
    uint32_t nodo = 0;
    if (!l.L32(g_lista_fuentes, &nodo)) {
      return 0.0f;
    }
    for (int n = 0; n < 64 && nodo && nodo != g_lista_fuentes; ++n) {
      uint32_t suyo = 0, alto = 0;
      if (!l.L32(nodo + kOffIdFuente, &suyo)) {
        return 0.0f;
      }
      if (suyo == id) {
        if (!l.L32(nodo + kOffAltoFuente, &alto)) {
          return 0.0f;
        }
        float f;
        std::memcpy(&f, &alto, sizeof(f));
        return Razonable(f) && f > 0.0f && f < 200.0f ? f : 0.0f;
      }
      if (!l.L32(nodo, &nodo)) {
        return 0.0f;
      }
    }
    uint32_t de = 0, a = 0;
    if (!l.L32(g_alias_fuentes, &de) || !l.L32(g_alias_fuentes + 4, &a) || de != id ||
        a == id) {
      return 0.0f;
    }
    id = a;
  }
  return 0.0f;
}

// Unas letras del juego en `texto`, de como mucho `max` - 1, en ASCII (lo demas, '?'): los
// iconos de boton van en ASCII, y para medir basta. `anchas`: UTF-16 (el texto propio de un
// objeto) o bytes (los del idioma).
bool CopiarLetras(Lector& l, uint32_t direccion, bool anchas, char* texto, uint32_t max) {
  const uint32_t paso = anchas ? 2 : 1;
  uint32_t n = 0;
  if (direccion && !(anchas && (direccion & 1))) {
    for (; n + 1 < max; ++n) {
      const uint32_t d = direccion + n * paso;
      if ((n == 0 || (d & 0xFFF) == 0) && !l.Ver(d, paso)) {
        break;
      }
      const uint32_t c = anchas ? l.U16(d) : Leido8(l.memoria, d);
      if (c == 0) {
        break;
      }
      texto[n] = c == '\n' || (c >= 0x20 && c < 0x7F) ? char(c) : '?';
    }
  }
  texto[n] = '\0';
  return n > 0;
}

// El texto de una etiqueta, como lo busca el juego (kBuscarTexto): en su tabla de {hash,
// texto}, de menor a mayor hash.
bool TextoDeEtiqueta(Lector& l, uint32_t etiqueta, char* texto, uint32_t max) {
  uint32_t tabla = 0, cuantos = 0;
  if (!l.L32(g_tabla_textos, &tabla) || !l.L32(g_cuantos_textos, &cuantos) || !tabla || !cuantos ||
      cuantos > 1000000) {
    return false;
  }
  uint32_t desde = 0, hasta = cuantos;
  while (desde < hasta) {
    const uint32_t medio = desde + (hasta - desde) / 2;
    uint32_t hash = 0;
    if (!l.L32(tabla + medio * 8, &hash)) {
      return false;
    }
    if (hash < etiqueta) {
      desde = medio + 1;
    } else {
      hasta = medio;
    }
  }
  uint32_t hash = 0, letras = 0;
  return desde < cuantos && l.L32(tabla + desde * 8, &hash) && hash == etiqueta &&
         l.L32(tabla + desde * 8 + 4, &letras) && CopiarLetras(l, letras, false, texto, max);
}

// Lo que pinta un texto: el de su etiqueta, o el suyo si tiene la bandera 2 (FEngGetCenter
// hace lo mismo). Si la etiqueta no esta en la tabla, el suyo.
void TextoDeObjeto(Lector& l, uint32_t objeto, uint32_t banderas, Texto& t) {
  uint32_t letras = 0;
  l.L32(objeto + kOffEtiqueta, &t.etiqueta);
  l.L32(objeto + kOffLetras, &letras);
  t.propio = (banderas & kBanderaTextoPropio) || !TextoDeEtiqueta(l, t.etiqueta, t.letras, kMaxTexto);
  if (t.propio) {
    CopiarLetras(l, letras, true, t.letras, kMaxTexto);
  }
}

// El boton de un icono de boton, por el nombre de su textura. -1 si no se sabe (sale en el
// diagnostico, para anadirlo aqui).
int BotonDeIcono(const char* nombre) {
  char n[64];
  size_t largo = 0;
  for (; nombre[largo] && largo + 1 < sizeof(n); ++largo) {
    n[largo] = char(std::toupper(uint8_t(nombre[largo])));
  }
  n[largo] = '\0';
  auto tiene = [&](const char* s) { return std::strstr(n, s) != nullptr; };
  auto acaba = [&](const char* s) {
    const size_t b = std::strlen(s);
    return largo >= b && std::strcmp(n + largo - b, s) == 0;
  };
  if (tiene("SELECT") || tiene("ACCEPT") || acaba("_A")) {
    return kBotonA;
  }
  if (tiene("BACK") || tiene("CANCEL") || acaba("_B")) {
    return kBotonB;
  }
  if (acaba("_X")) {
    return kBotonX;
  }
  if (acaba("_Y")) {
    return kBotonY;
  }
  if (tiene("START")) {
    return kBotonStart;
  }
  if (acaba("_LB") || tiene("LEFT_SHOULDER") || tiene("LSHOULDER")) {
    return kBotonLB;
  }
  if (acaba("_RB") || tiene("RIGHT_SHOULDER") || tiene("RSHOULDER")) {
    return kBotonRB;
  }
  return -1;
}

// Lo que mide un texto, en letras de la linea mas larga y lineas: cada icono "$NOMBRE$",
// como dos letras (es cuadrado, de la altura de la fuente). El primer icono, en `icono`, y si
// no hay mas letras que el.
void MedirTexto(const char* t, int* largo, int* lineas, char* icono, size_t max_icono, bool* solo_icono) {
  int linea = 0, letras = 0;
  *largo = 0;
  *lineas = 1;
  icono[0] = '\0';
  for (const char* c = t; *c; ++c) {
    const char* fin = *c == '$' ? std::strchr(c + 1, '$') : nullptr;
    if (fin && fin > c + 1) {
      if (!icono[0]) {
        const size_t n = std::min(size_t(fin - c - 1), max_icono - 1);
        std::memcpy(icono, c + 1, n);
        icono[n] = '\0';
      }
      linea += 2;
      c = fin;
    } else if (*c == '\n') {
      ++*lineas;
      linea = 0;
      continue;
    } else {
      ++linea;
      if (*c != ' ') {
        ++letras;
      }
    }
    *largo = std::max(*largo, linea);
  }
  *solo_icono = icono[0] && letras == 0;
}

// La caja de un texto, como FEngGetCenter y FEngSetTopLeft: el ancho y el alto de sus letras por su
// tamano, y donde queda segun como va alineado. El ancho de cada letra no se sabe sin las
// tablas de la fuente: se cuenta media altura, lo que mide una letra normal.
Caja CajaDeTexto(Lector& l, uint32_t objeto, const char* letras, float px, float py, float sx, float sy) {
  uint32_t fuente = 0, formato = 0;
  l.L32(objeto + kOffTextura, &fuente);
  l.L32(objeto + kOffFormato, &formato);
  int largo = 0, lineas = 1;
  char icono[48];
  bool solo_icono = false;
  MedirTexto(letras, &largo, &lineas, icono, sizeof(icono), &solo_icono);
  if (largo == 0) {
    largo = 8;  // no se ha podido leer: lo que suele medir
  }
  float alto = AlturaFuente(l, fuente);
  if (alto <= 0.0f) {
    alto = 20.0f;
  }
  const float ancho = float(largo) * alto * 0.5f * std::fabs(sx);
  const float h = float(lineas) * alto * std::fabs(sy);
  Caja c;
  c.x0 = (formato & kFormatoCentrado) ? px - ancho * 0.5f : (formato & kFormatoDerecha) ? px - ancho : px;
  c.y0 = (formato & kFormatoCentradoV) ? py - h * 0.5f : (formato & kFormatoAbajo) ? py - h : py;
  c.x1 = c.x0 + ancho;
  c.y1 = c.y0 + h;
  return c;
}

// Si `buscado` es `guion` o uno de los que le siguen al acabar.
bool EnCadena(Lector& l, uint32_t guion, uint32_t buscado) {
  for (int n = 0; n < 8 && guion; ++n) {
    if (guion == buscado) {
      return true;
    }
    if (!l.L32(guion + kOffGuionSiguiente, &guion)) {
      return false;
    }
  }
  return false;
}

// Si el guion puesto es el de resaltar o uno de los que le siguen (y no uno de los de
// apagar). `tiene`: si el objeto tiene guion de resaltar.
bool Resaltada(Lector& l, uint32_t objeto, bool* tiene) {
  uint32_t guion = 0, puesto = 0, resaltar = 0, apagar = 0;
  *tiene = false;
  if (!l.L32(objeto + kOffGuiones, &guion) || !l.L32(objeto + kOffGuionPuesto, &puesto)) {
    return false;
  }
  for (int n = 0; n < 48 && guion; ++n) {
    uint32_t id = 0;
    if (!l.L32(guion + kOffNombreGuion, &id)) {
      return false;
    }
    if (id == kGuionResaltar) {
      resaltar = guion;
    } else if (id == kGuionApagar) {
      apagar = guion;
    }
    if (!l.L32(guion + kOffSiguiente, &guion)) {
      break;
    }
  }
  *tiene = resaltar != 0;
  return resaltar && puesto && EnCadena(l, resaltar, puesto) && !(apagar && EnCadena(l, apagar, puesto));
}

struct Recorrido {
  Lector* lector;
  Menu* menu;
  uint32_t paquete;
  bool atiende;  // el paquete atiende al mando: sus opciones cuentan
  uint32_t boton_resaltado;
  int objetos;
  uint32_t grupo;  // el grupo que se recorre
};

Caja RecorrerLista(Recorrido& r, uint32_t objeto, float ox, float oy, int profundidad);

// La caja de un objeto (la de lo que lleva dentro, si es un grupo), y lo que se reconoce
// en el.
Caja RecorrerObjeto(Recorrido& r, uint32_t objeto, float ox, float oy, int profundidad) {
  Lector& l = *r.lector;
  Caja caja;
  uint32_t tipo = 0, banderas = 0, datos = 0;
  if (!l.L32(objeto + kOffTipoObjeto, &tipo) || !l.L32(objeto + kOffBanderas, &banderas) ||
      !l.L32(objeto + kOffDatos, &datos) || (datos & 3) || !l.Ver(datos, kBytesDatos)) {
    return caja;
  }
  // Oculto, o transparente del todo (los guiones apagan asi lo que no se usa).
  if ((banderas & kBanderaOculto) || int32_t(l.U32(datos + kOffAlfa)) <= 0) {
    return caja;
  }
  const float px = ox + l.F32(datos + kOffPosicion);
  const float py = oy + l.F32(datos + kOffPosicion + 4);
  const float sx = l.F32(datos + kOffTamanoObjeto);
  const float sy = l.F32(datos + kOffTamanoObjeto + 4);
  if (!Razonable(px) || !Razonable(py) || !Razonable(sx) || !Razonable(sy)) {
    return caja;
  }
  switch (tipo) {
    case kTipoGrupo: {
      uint32_t hijos = 0;
      if (profundidad < kMaxProfundidad && l.L32(objeto + kOffHijos, &hijos)) {
        const uint32_t fuera = r.grupo;
        r.grupo = objeto;
        caja = RecorrerLista(r, hijos, px, py, profundidad + 1);
        r.grupo = fuera;
      }
      break;
    }
    case kTipoTexto: {
      Texto t{};
      TextoDeObjeto(l, objeto, banderas, t);
      caja = CajaDeTexto(l, objeto, t.letras, px, py, sx, sy);
      t.caja = caja;
      // Un icono de boton dentro: una indicacion. Si no lleva mas que el icono, su texto va
      // en otro objeto, a la derecha (ZonaDeIndicacion, como con las imagenes).
      int largo = 0, lineas = 0;
      char icono[48];
      bool solo_icono = false;
      MedirTexto(t.letras, &largo, &lineas, icono, sizeof(icono), &solo_icono);
      if (t.etiqueta == kEtiquetaRotar && !caja.Vacia() && caja.Cy() > kBarraAbajo) {
        r.menu->rotar = true;
      }
      // Un texto de indicacion: el icono va a su izquierda, un poco mas alto que el texto.
      for (const auto& e : kEtiquetasIndicacion) {
        if (e.etiqueta == t.etiqueta && !caja.Vacia()) {
          Caja zona = caja;
          const float alto = caja.y1 - caja.y0;
          zona.x0 -= alto * 2.0f;
          zona.y0 -= alto * 0.25f;
          zona.y1 += alto * 0.25f;
          r.menu->indicaciones.push_back({e.boton, zona, zona, false});
          break;
        }
      }
      if (icono[0]) {
        const int boton = BotonDeIcono(icono);
        if (boton >= 0) {
          r.menu->indicaciones.push_back({boton, caja, caja, solo_icono});
        } else if (r.menu->iconos_sin_boton.size() < 16) {
          r.menu->iconos_sin_boton.emplace_back(icono);
        }
      }
      if ((std::strcmp(t.letras, "-") == 0 || std::strcmp(t.letras, "+") == 0) && !caja.Vacia()) {
        r.menu->signos.emplace_back(caja, t.letras[0] == '-');
      }
      r.menu->textos.push_back(t);
      break;
    }
    case kTipoImagen:
    case kTipoPelicula:
    case kTipoImagenColor:
    case kTipoImagenAnimada:
    case kTipoImagenSimple:
    case kTipoMultiImagen: {
      caja.x0 = px - std::fabs(sx) * 0.5f;
      caja.y0 = py - std::fabs(sy) * 0.5f;
      caja.x1 = px + std::fabs(sx) * 0.5f;
      caja.y1 = py + std::fabs(sy) * 0.5f;
      uint32_t textura = 0;
      l.L32(objeto + kOffTextura, &textura);
      for (const uint32_t flecha : kTexturasFlecha) {
        if (textura == flecha) {
          r.menu->flechas.push_back(caja);
        }
      }
      for (const uint32_t rotar : kTexturasRotar) {
        if (textura == rotar && caja.Cy() > kBarraAbajo) {
          r.menu->rotar = true;
        }
      }
      int boton = textura == kTexturaGatilloLT ? kGatilloLT : textura == kTexturaGatilloRT ? kGatilloRT : -1;
      for (const auto& t : kTexturasIndicacion) {
        if (t.textura == textura) {
          boton = t.boton;
        }
      }
      const float ancho = caja.x1 - caja.x0, alto = caja.y1 - caja.y0;
      if (boton < 0 && caja.Cy() > kBarraAbajo && ancho >= kLadoIconoMin && ancho <= kLadoIconoMax &&
          alto >= kLadoIconoMin && alto <= kLadoIconoMax) {
        r.menu->posibles.push_back({-1, caja, caja, true});
      }
      if (boton >= 0) {
        r.menu->indicaciones.push_back({boton, caja, caja, true});
      } else if (std::fabs(sx) <= 64.0f && std::fabs(sy) <= 64.0f && r.menu->iconos.size() < 64) {
        r.menu->iconos.emplace_back(textura, caja);
      }
      break;
    }
    default:
      break;
  }
  if (r.atiende && !caja.Vacia() && std::fabs(caja.Cx()) < kMedioAnchoVisible &&
      std::fabs(caja.Cy()) < kMedioAltoVisible) {
    const bool boton = (banderas & kBanderaBoton) && !(banderas & kBanderaNoBoton);
    bool tiene = false;
    const bool resaltada = Resaltada(l, objeto, &tiene);
    if (boton || tiene) {
      r.menu->opciones.push_back({objeto, r.paquete, boton, resaltada || objeto == r.boton_resaltado, caja, r.grupo});
    }
  }
  return caja;
}

Caja RecorrerLista(Recorrido& r, uint32_t objeto, float ox, float oy, int profundidad) {
  Caja todo;
  while (objeto && r.lector->objetos < kMaxObjetos) {
    ++r.lector->objetos;
    ++r.objetos;
    todo.Unir(RecorrerObjeto(r, objeto, ox, oy, profundidad));
    if (!r.lector->L32(objeto + kOffSiguiente, &objeto)) {
      break;
    }
  }
  return todo;
}

// La zona de una indicacion: el icono y el texto que empieza justo a su derecha, a su
// altura. Sin ese texto, lo que suele ocupar uno corto. Si lo tenia.
bool ZonaDeIndicacion(Indicacion& ind, const std::vector<Texto>& textos) {
  if (!ind.extender) {
    return true;  // un texto con el icono y lo que dice: ya es su zona
  }
  const float lado = std::max(ind.icono.x1 - ind.icono.x0, ind.icono.y1 - ind.icono.y0);
  const float cy = ind.icono.Cy();
  const Caja* mejor = nullptr;
  for (const Texto& texto : textos) {
    const Caja& t = texto.caja;
    if (t.Vacia() || cy < t.y0 - lado * 0.25f || cy > t.y1 + lado * 0.25f) {
      continue;
    }
    if (t.x0 < ind.icono.x1 - lado * 0.5f || t.x0 > ind.icono.x1 + lado * 1.5f) {
      continue;
    }
    if (!mejor || t.x0 < mejor->x0) {
      mejor = &t;
    }
  }
  if (mejor) {
    ind.zona.Unir(*mejor);
  } else {
    ind.zona.x1 += lado * 3.0f;
  }
  return mejor != nullptr;
}

// Las indicaciones de la barra de abajo van en filas a ~25 unidades (OL_GameRoom: "(A) Cambiar
// coche" encima de "(B) Atras"), unos 4 mm en un movil, y el dedo daba en la de al lado. Cada
// una crece hacia el hueco que tiene: hasta el centro de la fila de encima y de la de debajo
// (donde dos se pisan, Tocar elige la fila mas cercana al dedo); la primera fila, hacia arriba
// sin pisar ninguna opcion; la ultima, hasta el borde de abajo; y la primera de cada fila,
// hasta el borde izquierdo de la imagen.
constexpr float kMismaFila = 12.0f;     // dos iconos con el centro a menos de esto, misma fila
constexpr float kBarraDesde = 150.0f;   // las filas de la barra de abajo empiezan por aqui
constexpr float kCrecerArriba = 24.0f;  // lo que sube la primera fila, si no hay una opcion antes

void CrecerBarra(Menu& menu) {
  // Un icono sin boton conocido justo donde empieza una indicacion conocida (por la etiqueta
  // de su texto, kEtiquetasIndicacion): es el suyo. Fuera: empieza un poco mas a la derecha
  // y MejorEnFila lo preferiria, y tocarlo no hace nada.
  auto tapado = [&menu](const Indicacion& i) {
    if (i.boton >= 0) {
      return false;
    }
    for (const Indicacion& o : menu.indicaciones) {
      if (o.boton >= 0 && std::fabs(o.icono.Cy() - i.icono.Cy()) < kMismaFila &&
          i.icono.x0 >= o.zona.x0 - 4.0f && i.icono.x0 <= o.zona.x0 + 40.0f) {
        return true;
      }
    }
    return false;
  };
  std::vector<Indicacion> quedan;
  for (const Indicacion& i : menu.indicaciones) {
    if (!tapado(i)) {
      quedan.push_back(i);
    }
  }
  menu.indicaciones = std::move(quedan);
  std::vector<float> filas;  // el centro de cada fila de la barra, de arriba abajo
  for (const Indicacion& ind : menu.indicaciones) {
    const float cy = ind.icono.Cy();
    bool nueva = cy >= kBarraDesde;
    for (const float f : filas) {
      nueva = nueva && std::fabs(f - cy) >= kMismaFila;
    }
    if (nueva) {
      filas.push_back(cy);
    }
  }
  std::sort(filas.begin(), filas.end());
  for (Indicacion& ind : menu.indicaciones) {
    const float cy = ind.icono.Cy();
    size_t k = 0;
    while (k < filas.size() && std::fabs(filas[k] - cy) >= kMismaFila) {
      ++k;
    }
    if (cy < kBarraDesde || k == filas.size()) {
      continue;
    }
    bool primera = true;
    for (const Indicacion& otra : menu.indicaciones) {
      primera = primera && !(std::fabs(otra.icono.Cy() - cy) < kMismaFila && otra.icono.x0 < ind.icono.x0 - 1.0f);
    }
    if (primera) {
      ind.zona.x0 = -kMedioAnchoVisible;
    }
    float arriba = k > 0 ? filas[k - 1] : ind.zona.y0 - kCrecerArriba;
    if (k == 0) {
      for (const Opcion& o : menu.opciones) {
        if (o.caja.y1 <= ind.zona.y0 && o.caja.x0 < ind.zona.x1 && ind.zona.x0 < o.caja.x1) {
          arriba = std::max(arriba, o.caja.y1 + 2.0f);
        }
      }
    }
    const float abajo = k + 1 < filas.size() ? filas[k + 1] : kMedioAltoVisible;
    ind.zona.y0 = std::min(ind.zona.y0, arriba);
    ind.zona.y1 = std::max(ind.zona.y1, abajo);
  }
}

// Entre dos indicaciones de una misma fila que caen bajo el dedo: la que empieza mas a la
// derecha sin pasarse de el (el texto de una se estima y puede pisar la siguiente); si el dedo
// esta a la izquierda de todas (la zona de la primera llega al borde, CrecerBarra), la primera.
bool MejorEnFila(const Indicacion& a, const Indicacion& b, float x) {
  const bool va = a.icono.x0 <= x + kMargenIndicacion, vb = b.icono.x0 <= x + kMargenIndicacion;
  if (va != vb) {
    return va;
  }
  return va ? a.icono.x0 > b.icono.x0 : a.icono.x0 < b.icono.x0;
}

bool LeerMenu(Menu& menu) {
  auto* runtime = rex::Runtime::instance();
  auto* memoria = runtime ? runtime->memory() : nullptr;
  const uint32_t* f = DireccionesMenus();
  uint32_t feng = 0, motor = 0, paquete = 0;
  if (!f || !memoria || !MenusBien(memoria, f) || !Leer32(memoria, g_instancia_feng, &feng) ||
      !Leer32(memoria, feng + kOffMotor, &motor) || !Leer32(memoria, motor + kOffPaquetes, &paquete)) {
    return false;
  }
  Lector lector{memoria};
  for (uint32_t n = 0; n < kMaxPaquetes && paquete; ++n) {
    Menu::Paquete p{};
    p.direccion = paquete;
    uint32_t nombre = 0, primero = 0;
    uint8_t atiende = 0;
    Leer32(memoria, paquete + kOffMandosPaquete, &p.mandos);
    Leer8(memoria, paquete + kOffAtiendePaquete, &atiende);
    Leer32(memoria, paquete + kOffBotonResaltado, &p.boton);
    if (!Leer32(memoria, paquete + kOffNombrePaquete, &nombre) ||
        !LeerTexto(memoria, nombre, p.nombre, sizeof(p.nombre) - 1)) {
      std::strcpy(p.nombre, "?");
    }
    // Va al diagnostico por NewStringUTF, que no admite cualquier byte.
    for (char* c = p.nombre; *c; ++c) {
      if (*c < 0x20 || *c > 0x7E) {
        *c = '?';
      }
    }
    p.atiende = atiende && p.mandos;
    Recorrido r{&lector, &menu, paquete, p.atiende, p.boton, 0, 0};
    if (Leer32(memoria, paquete + kOffObjetos, &primero)) {
      RecorrerLista(r, primero, 0.0f, 0.0f, 0);
    }
    p.objetos = r.objetos;
    menu.paquetes.push_back(p);
    if (!Leer32(memoria, paquete + kOffSiguiente, &paquete)) {
      break;
    }
  }
  for (Indicacion& ind : menu.indicaciones) {
    ZonaDeIndicacion(ind, menu.textos);
  }
  for (Indicacion ind : menu.posibles) {
    if (ZonaDeIndicacion(ind, menu.textos)) {
      menu.indicaciones.push_back(ind);
    }
  }
  // El ancho de los textos es una estimacion, y se quedaba corta: tocar el final de "Atras"
  // caia fuera. Cada indicacion llega hasta el icono de la siguiente de su fila (las de abajo
  // van seguidas, en una barra), o un poco mas si es la ultima.
  for (Indicacion& ind : menu.indicaciones) {
    float siguiente = 1e9f;
    for (const Indicacion& otra : menu.indicaciones) {
      if (otra.icono.x0 > ind.icono.x0 + 1.0f && std::fabs(otra.icono.Cy() - ind.icono.Cy()) < 12.0f) {
        siguiente = std::min(siguiente, otra.icono.x0);
      }
    }
    if (siguiente - ind.zona.x1 < 200.0f) {
      ind.zona.x1 = std::max(ind.zona.x1, siguiente - 2.0f);
    } else {
      ind.zona.x1 += 30.0f;
    }
  }
  CrecerBarra(menu);
  return true;
}

const Opcion* BuscarOpcion(const Menu& menu, uint32_t objeto) {
  for (const Opcion& o : menu.opciones) {
    if (o.objeto == objeto) {
      return &o;
    }
  }
  return nullptr;
}

// Si a y b son partes de una misma opcion: estan en el mismo grupo, y ese grupo se resalta
// entero (dos o mas partes a la vez: una cancion de EA Trax resalta el artista, el disco y su
// "Menu", pero no el titulo). En un grupo con varias opciones sueltas (Si y No de un dialogo)
// solo se resalta una, y cada una va por su cuenta.
bool MismaOpcion(const Menu& menu, const Opcion& a, const Opcion& b) {
  if (!a.grupo || a.grupo != b.grupo) {
    return false;
  }
  int resaltadas = 0;
  for (const Opcion& o : menu.opciones) {
    resaltadas += o.grupo == a.grupo && o.resaltada;
  }
  return resaltadas >= 2;
}

// Si la opcion esta elegida: ella, o una parte de su misma opcion.
bool Elegida(const Menu& menu, const Opcion& opcion) {
  if (opcion.resaltada) {
    return true;
  }
  for (const Opcion& o : menu.opciones) {
    if (o.resaltada && MismaOpcion(menu, o, opcion)) {
      return true;
    }
  }
  return false;
}

// La opcion resaltada desde la que se va al objetivo, del mismo paquete: antes un boton
// del FEng, luego la que esta en su fila o su columna (lo que se resalta por su cuenta,
// un titulo que late, no suele estarlo), y si no, cualquiera.
const Opcion* ResaltadaHacia(const Menu& menu, const Opcion& objetivo) {
  const Opcion* mejor = nullptr;
  int puntos_mejor = -1;
  for (const Opcion& o : menu.opciones) {
    if (o.paquete != objetivo.paquete || !o.resaltada || o.objeto == objetivo.objeto ||
        MismaOpcion(menu, o, objetivo)) {
      continue;
    }
    const bool fila = o.caja.y0 < objetivo.caja.y1 && objetivo.caja.y0 < o.caja.y1;
    const bool columna = o.caja.x0 < objetivo.caja.x1 && objetivo.caja.x0 < o.caja.x1;
    const int puntos = (o.boton ? 2 : 0) + (fila || columna ? 1 : 0);
    if (puntos > puntos_mejor) {
      mejor = &o;
      puntos_mejor = puntos;
    }
  }
  return mejor;
}

// Hacia donde pulsar la cruceta para llegar al objetivo, o que ya esta.
jint Rumbo(const Menu& menu) {
  const Opcion* objetivo = BuscarOpcion(menu, g_objetivo);
  if (!objetivo) {
    g_objetivo = 0;
    return kMenusSinRumbo;
  }
  if (Elegida(menu, *objetivo)) {
    g_objetivo = 0;
    return kMenusLlegada;
  }
  const Opcion* actual = ResaltadaHacia(menu, *objetivo);
  if (!actual) {
    g_objetivo = 0;
    return kMenusSinRumbo;
  }
  // Si tras dos pulsaciones la resaltada sigue siendo la misma, el juego no se mueve asi
  // (una lista que se desplaza bajo un marco fijo, una opcion que no se puede elegir).
  if (actual->objeto == g_ultima_resaltada) {
    if (++g_sin_moverse >= 2) {
      g_objetivo = 0;
      return kMenusSinRumbo;
    }
  } else {
    g_sin_moverse = 0;
  }
  g_ultima_resaltada = actual->objeto;
  // En la misma columna (una lista: en las opciones van alineadas a la derecha, y con el
  // centro la diagonal salia hacia un lado, que cambiaba el valor), arriba o abajo; en la
  // misma fila, a un lado. Si no, hacia donde mas lejos este.
  const Caja& a = actual->caja;
  const Caja& o = objetivo->caja;
  const bool fila = a.y0 < o.y1 && o.y0 < a.y1;
  const bool columna = a.x0 < o.x1 && o.x0 < a.x1;
  const float dx = o.Cx() - a.Cx();
  const float dy = o.Cy() - a.Cy();
  const bool vertical = columna && !fila ? true : fila && !columna ? false : std::fabs(dy) >= std::fabs(dx);
  if (!vertical) {
    return kMenusIr | (dx > 0 ? kCrucetaDerecha : kCrucetaIzquierda);
  }
  return kMenusIr | (dy > 0 ? kCrucetaAbajo : kCrucetaArriba);
}

// Si las opciones que se ven forman una lista: tres o mas, una debajo de otra. Ahi deslizar
// en horizontal cambia el valor de la resaltada (TouchControllerView).
bool EsLista(const Menu& menu) {
  for (const Opcion& a : menu.opciones) {
    int debajo = 0;
    for (const Opcion& b : menu.opciones) {
      const bool columna = a.caja.x0 < b.caja.x1 && b.caja.x0 < a.caja.x1;
      const bool fila = a.caja.y0 < b.caja.y1 && b.caja.y0 < a.caja.y1;
      if (a.paquete == b.paquete && columna && !fila) {
        ++debajo;
      }
    }
    if (debajo >= 2) {
      return true;
    }
  }
  return false;
}

// El ultimo menu leido, en un toque o en un paso del camino: el diagnostico lo cuenta.
Menu g_menu;
bool g_menu_leido = false;

jint Tocar(float u, float v) {
  g_objetivo = 0;
  // De la imagen del juego (0..1 en cada eje) al menu.
  const float x = (u - 0.5f) * kAnchoMenu / kFraccionAncho;
  const float y = (v - 0.5f) * kAltoMenu;
  g_toque_x = x;
  g_toque_y = y;
  g_menu = Menu{};
  g_menu_leido = LeerMenu(g_menu);
  if (!g_menu_leido) {
    return kMenusDesconocido;
  }
  const Menu& menu = g_menu;
  // Una indicacion. Si caen varias: la de la fila con el centro mas cerca del dedo (las de la
  // barra crecen hasta pisarse, CrecerBarra) y, en esa fila, MejorEnFila.
  const Indicacion* indicacion = nullptr;
  for (const Indicacion& ind : menu.indicaciones) {
    if (!ind.zona.Contiene(x, y, kMargenIndicacion)) {
      continue;
    }
    if (!indicacion) {
      indicacion = &ind;
    } else if (std::fabs(ind.icono.Cy() - indicacion->icono.Cy()) < kMismaFila) {
      if (MejorEnFila(ind, *indicacion, x)) {
        indicacion = &ind;
      }
    } else if (std::fabs(ind.icono.Cy() - y) < std::fabs(indicacion->icono.Cy() - y)) {
      indicacion = &ind;
    }
  }
  if (indicacion) {
    // Sin boton conocido, nada: mejor que aceptar.
    return indicacion->boton >= 0 ? kMenusIndicacion | indicacion->boton : kMenusSinRumbo;
  }
  // Una flecha: la cruceta hacia su lado. Las de los lados de un valor tienen otra en su fila,
  // lejos (las que van de dos en dos, una sobre otra, estan pegadas): la izquierda tiene mas a su
  // derecha. Las de subir y bajar una lista (EA Trax, a la derecha), otra encima o debajo y
  // ninguna en su fila. Una de los lados de una fila que no es la resaltada (EA Trax pone
  // flechas en todas) no cuenta: el toque elige esa fila, mas abajo.
  for (const Caja& f : menu.flechas) {
    if (!f.Contiene(x, y, kMargenFlecha)) {
      continue;
    }
    int a_la_derecha = 0, a_la_izquierda = 0, encima = 0, debajo = 0;
    for (const Caja& otra : menu.flechas) {
      const float dx = otra.Cx() - f.Cx(), dy = otra.Cy() - f.Cy();
      if (std::fabs(dy) < 8.0f && std::fabs(dx) > 40.0f) {
        ++(dx > 0 ? a_la_derecha : a_la_izquierda);
      } else if (std::fabs(dx) < 20.0f && std::fabs(dy) > 40.0f) {
        ++(dy > 0 ? debajo : encima);
      }
    }
    if (a_la_derecha + a_la_izquierda == 0 && encima + debajo > 0) {
      return kMenusIndicacion | (debajo > encima ? kCrucetaArriba : kCrucetaAbajo);
    }
    bool en_la_resaltada = false;
    for (const Opcion& o : menu.opciones) {
      en_la_resaltada = en_la_resaltada || (o.resaltada && f.y0 < o.caja.y1 + 12.0f && o.caja.y0 - 12.0f < f.y1);
    }
    if (!en_la_resaltada) {
      continue;
    }
    const bool izquierda = a_la_derecha != a_la_izquierda ? a_la_derecha > a_la_izquierda : f.Cx() < 0.0f;
    return kMenusIndicacion | (izquierda ? kCrucetaIzquierda : kCrucetaDerecha);
  }
  // Un "-" o un "+" de la barra de la opcion resaltada: bajar o subir su valor. En otra fila, nada
  // aqui: el toque elige esa fila, mas abajo.
  for (const auto& [signo, menos] : menu.signos) {
    if (!signo.Contiene(x, y, kMargenFlecha)) {
      continue;
    }
    for (const Opcion& o : menu.opciones) {
      if (o.resaltada && signo.y0 < o.caja.y1 + 6.0f && o.caja.y0 - 6.0f < signo.y1) {
        return kMenusIndicacion | (menos ? kCrucetaIzquierda : kCrucetaDerecha);
      }
    }
  }
  // Aceptar es tocar la indicacion de A o lo ya elegido. Donde no hay ni una indicacion (videos,
  // "Presionar START", cargas), un toque acepta; donde hay barra pero sin A (visualizar un coche),
  // no: tocar el coche no puede aceptar.
  bool hay_aceptar = false;
  for (const Indicacion& ind : menu.indicaciones) {
    hay_aceptar = hay_aceptar || ind.boton >= 0;
  }
  // Una opcion: la mas pequena que lo contenga (un texto antes que el grupo que lo lleva).
  const Opcion* tocada = nullptr;
  for (const Opcion& o : menu.opciones) {
    if (o.caja.Contiene(x, y, kMargen) && (!tocada || o.caja.Area() < tocada->caja.Area())) {
      tocada = &o;
    }
  }
  // En una lista, la fila entera: el nombre de cada opcion es lo que se lee, pero su valor (la
  // barra del volumen, el "Si") va a su derecha, en la misma fila.
  bool por_fila = false;
  if (!tocada && EsLista(menu)) {
    por_fila = true;
    for (const Opcion& o : menu.opciones) {
      if (y >= o.caja.y0 - 2.0f && y <= o.caja.y1 + 2.0f && x >= o.caja.x0 - kMargen &&
          (!tocada || std::fabs(o.caja.Cy() - y) < std::fabs(tocada->caja.Cy() - y))) {
        tocada = &o;
      }
    }
  }
  if (!tocada) {
    return hay_aceptar ? kMenusNada : kMenusAceptar;
  }
  const bool ya_elegida = Elegida(menu, *tocada);
  // La que ya esta elegida: aceptarla (el icono de "Modo carrera" en el menu principal, una fila
  // ya resaltada). Es tocar a proposito lo que se ve elegido.
  if (ya_elegida) {
    // Por la fila (su barra, su valor), no: ahi se toca para cambiar el valor, y aceptar podria
    // cerrar la pantalla (en los reglajes, A acepta todo).
    return por_fila ? kMenusNada : kMenusAceptar;
  }
  g_objetivo = tocada->objeto;
  g_ultima_resaltada = 0;
  g_sin_moverse = 0;
  return Rumbo(menu);
}

std::string TextoCaja(const Caja& c) {
  char t[64];
  std::snprintf(t, sizeof(t), "[%.0f %.0f %.0f %.0f]", c.x0, c.y0, c.x1, c.y1);
  return t;
}

}  // namespace

// Que hay en (u, v), la posicion del toque en la imagen del juego (0..1). Si es una
// opcion por resaltar, se queda como objetivo de menuRumbo.
extern "C" int32_t NfsmwTactilMenuTocar(float u, float v) {
  g_toque_resultado = Tocar(u, v);
  return g_toque_resultado;
}

// Si la pantalla se maneja con la cruceta mantenida ("Rotar" al visualizar un coche):
// TouchControllerView la pisa mientras se arrastra el dedo, en vez de dar pasos.
extern "C" bool NfsmwTactilMenuRotar() {
  Menu menu;
  return LeerMenu(menu) && menu.rotar;
}

// Si lo que hay en pantalla es una lista de opciones (EsLista): TouchControllerView lo
// pregunta al empezar a deslizar en horizontal.
extern "C" bool NfsmwTactilMenuEsLista() {
  Menu menu;
  return LeerMenu(menu) && EsLista(menu);
}

// Despues de cada pulsacion de la cruceta hacia el objetivo: la siguiente, o que ya ha
// llegado, o que no se sabe llegar.
extern "C" int32_t NfsmwTactilMenuRumbo() {
  if (!g_objetivo) {
    return kMenusSinRumbo;
  }
  g_menu = Menu{};
  g_menu_leido = LeerMenu(g_menu);
  if (!g_menu_leido) {
    g_objetivo = 0;
    return kMenusSinRumbo;
  }
  return Rumbo(g_menu);
}

// Diagnostico: el ultimo toque, en coordenadas del menu, y el ultimo menu leido (no se
// vuelve a leer). TouchControllerView lo guarda en files/logs/menus.txt con cada toque (el
// APK release no escribe el log del juego).
std::string NfsmwTactilMenuDiagnostico() {
  const Menu& menu = g_menu;
  const bool leido = g_menu_leido;
  std::string s;
  char linea[256];
  std::snprintf(linea, sizeof(linea), "toque (%.0f, %.0f) -> 0x%X; comprobado=%d leido=%d\n", g_toque_x,
                g_toque_y, unsigned(g_toque_resultado), g_menus_bien, int(leido));
  s += linea;
  for (const auto& p : menu.paquetes) {
    std::snprintf(linea, sizeof(linea), "  paquete %08X %s mandos=%X atiende=%d boton=%08X objetos=%d\n",
                  p.direccion, p.nombre, p.mandos, int(p.atiende), p.boton, p.objetos);
    s += linea;
  }
  for (const auto& i : menu.indicaciones) {
    std::snprintf(linea, sizeof(linea), "  indicacion boton=%d icono %s zona %s\n", i.boton,
                  TextoCaja(i.icono).c_str(), TextoCaja(i.zona).c_str());
    s += linea;
  }
  size_t n = 0;
  for (const auto& o : menu.opciones) {
    if (++n > 80) {
      std::snprintf(linea, sizeof(linea), "  ... y %zu opciones mas\n", menu.opciones.size() - 80);
      s += linea;
      break;
    }
    std::snprintf(linea, sizeof(linea), "  opcion %08X paquete %08X grupo %08X %s%s%s\n", o.objeto, o.paquete,
                  o.grupo, TextoCaja(o.caja).c_str(), o.boton ? " boton" : "", o.resaltada ? " RESALTADA" : "");
    s += linea;
  }
  n = 0;
  for (const auto& t : menu.textos) {
    if (++n > 60) {
      break;
    }
    std::snprintf(linea, sizeof(linea), "  texto %s etiqueta=%08X%s \"%s\"\n", TextoCaja(t.caja).c_str(), t.etiqueta,
                  t.propio ? " propio" : "", t.letras);
    s += linea;
  }
  for (const auto& f : menu.flechas) {
    std::snprintf(linea, sizeof(linea), "  flecha %s\n", TextoCaja(f).c_str());
    s += linea;
  }
  for (const auto& [signo, menos] : menu.signos) {
    std::snprintf(linea, sizeof(linea), "  signo %s %s\n", menos ? "-" : "+", TextoCaja(signo).c_str());
    s += linea;
  }
  for (const auto& i : menu.iconos_sin_boton) {
    std::snprintf(linea, sizeof(linea), "  icono sin boton conocido: %s\n", i.c_str());
    s += linea;
  }
  n = 0;
  for (const auto& i : menu.iconos) {
    if (++n > 40) {
      break;
    }
    std::snprintf(linea, sizeof(linea), "  icono textura=%08X %s\n", i.first, TextoCaja(i.second).c_str());
    s += linea;
  }
  return s;
}

// ---------------------------------------------------------------------------
//  11. El boton Salir del menu principal
// ---------------------------------------------------------------------------
//
// El 360 no tiene como salir del juego: en el movil, TouchControllerView pone en el menu
// principal (MainMenu.fng) una indicacion mas, "Salir", con el estilo de las del juego, a la
// derecha del recuadro del perfil y encima de "Estadisticas". Va donde la pone el juego:
// medida desde el icono de A de la barra de abajo, que no se mueve con el idioma (el de Y si:
// va detras de "Aceptar"). Medido en una captura del RedMagic (PAL Espana, 2026-10-29): el
// circulo de A centrado en (-258, 192) y el recuadro del perfil acabando en x = -122, a la
// altura y = 147.
constexpr char kPaqueteMenuPrincipal[] = "MainMenu.fng";
constexpr float kSalirDesdeA_x = 140.0f;   // su borde izquierdo, desde el centro del icono de A
constexpr float kSalirDesdeA_y = -45.0f;   // su centro

// Si se esta en el menu principal, sin nada encima (un dialogo, un submenu): solo lo atiende
// MainMenu.fng y estan sus indicaciones de A e Y. Entonces deja en salida, sobre la imagen del
// juego (0..1): el borde izquierdo del boton, su centro en vertical, y lo que mide una unidad
// del menu en cada eje (TouchControllerView dibuja en unidades del menu, estiradas como el).
extern "C" bool NfsmwTactilMenuPrincipal(float* salida) {
  Menu menu;
  if (!LeerMenu(menu)) {
    return false;
  }
  bool principal = false;
  for (const auto& p : menu.paquetes) {
    if (!p.atiende) {
      continue;
    }
    if (std::strcmp(p.nombre, kPaqueteMenuPrincipal) != 0) {
      return false;
    }
    principal = true;
  }
  const Indicacion* a = nullptr;
  bool y = false;
  for (const Indicacion& ind : menu.indicaciones) {
    if (ind.icono.Cy() < kBarraAbajo) {
      continue;
    }
    if (ind.boton == kBotonA && !a) {
      a = &ind;
    }
    y = y || ind.boton == kBotonY;
  }
  if (!principal || !a || !y) {
    return false;
  }
  const float x = a->icono.Cx() + kSalirDesdeA_x;
  const float cy = a->icono.Cy() + kSalirDesdeA_y;
  salida[0] = x * kFraccionAncho / kAnchoMenu + 0.5f;
  salida[1] = cy / kAltoMenu + 0.5f;
  salida[2] = kFraccionAncho / kAnchoMenu;
  salida[3] = 1.0f / kAltoMenu;
  return true;
}

// Las veces que se ha pulsado BACK en un mando fisico: con el mando, BACK es "Salir".
extern "C" uint32_t NfsmwTactilBackPulsados() {
  return rex_sdl_back_pulsados();
}

#if defined(__ANDROID__)
extern "C" JNIEXPORT jboolean JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_menuPrincipal(
    JNIEnv* env, jclass, jfloatArray salida) {
  float valores[4];
  if (!salida || env->GetArrayLength(salida) < 4 || !NfsmwTactilMenuPrincipal(valores)) {
    return JNI_FALSE;
  }
  env->SetFloatArrayRegion(salida, 0, 4, valores);
  return JNI_TRUE;
}

extern "C" JNIEXPORT jint JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_backPulsados(JNIEnv*, jclass) {
  return jint(NfsmwTactilBackPulsados());
}

extern "C" JNIEXPORT jint JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_menuTocar(JNIEnv*, jclass,
                                                                                          jfloat u, jfloat v) {
  return NfsmwTactilMenuTocar(u, v);
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_menuRotar(JNIEnv*, jclass) {
  return NfsmwTactilMenuRotar();
}

extern "C" JNIEXPORT jboolean JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_menuEsLista(JNIEnv*, jclass) {
  return NfsmwTactilMenuEsLista();
}

extern "C" JNIEXPORT jint JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_menuRumbo(JNIEnv*, jclass) {
  return NfsmwTactilMenuRumbo();
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_nfsmwrecomp_TouchControllerBridge_menuDiagnostico(JNIEnv* env, jclass) {
  return env->NewStringUTF(NfsmwTactilMenuDiagnostico().c_str());
}
#endif
