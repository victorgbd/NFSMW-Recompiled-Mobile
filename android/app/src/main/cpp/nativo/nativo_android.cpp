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
//      cambiar los controles tactiles en las carreras de aceleracion.
//   9. La vibracion del mando tactil: el movil vibra con lo que el juego le
//      pide al mando (GameActivity.vibrarMando).

#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
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

#include "../afinidad.h"
#include "../audio/aaudio_driver.h"

REXCVAR_DEFINE_BOOL(nfsmw_audio_aaudio, true, "Audio",
                    "Android: sacar el sonido por AAudio (el driver de NFSMW Recompiled) en vez de por "
                    "el de SDL del SDK")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

int NfsmwSondaVulkan(int argc, char** argv);  // ../sonda_vulkan.cpp

// El mando tactil del SDK del motor nativo (sdk/src/input/sdl/sdl_input_driver.cpp):
// el estado va directo al mando del juego, sin pasar por un mando virtual de SDL.
extern "C" void rex_sdl_set_touch_gamepad_state(uint16_t buttons, int16_t left_x, int16_t left_y,
                                                 int16_t right_x, int16_t right_y,
                                                 uint8_t left_trigger, uint8_t right_trigger);

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

}  // namespace

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
extern "C" JNIEXPORT void JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_pausarJuego(
    JNIEnv*, jclass, jboolean pausa) {
  if (pausa) {
    Pausar(kPausaEditor);
  } else {
    Reanudar(kPausaEditor);
  }
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
  // -1 es "mando oculto o desconectado": todo suelto.
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

extern "C" JNIEXPORT jfloat JNICALL Java_io_github_nfsmwrecomp_GameActivity_nativeFps(JNIEnv*,
                                                                                    jclass) {
  return MedirFps();
}

extern "C" JNIEXPORT jfloat JNICALL
Java_io_github_nfsmwrecomp_GameActivity_nativeMsPorFotograma(JNIEnv*, jclass) {
  return g_fps > 0.0f ? 1000.0f / g_fps : -1.0f;
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
// Las direcciones las pone su app (parche_nativo.py, g_nfsmw_android_contexto),
// traducidas a la edicion del juego. Los desplazamientos son los del 360, y
// antes de fiarse de ellos se comprueban las instrucciones del juego de las que
// salen. Cada lectura mira antes que la pagina sea legible: Java pregunta desde
// su hilo mientras el juego cambia de escena, y un puntero de una carrera que
// se acaba de descargar no puede tumbar la app.

// Su app (nfsmw_recortes_carrera.cpp, con parche_nativo.py). Debil: sin el
// parche, el contexto es desconocido y los controles son los de siempre.
extern "C" const uint32_t g_nfsmw_android_contexto[9] __attribute__((weak));

namespace {

// Los valores de TouchControllerBridge.contexto().
constexpr jint kContextoDesconocido = -1;
constexpr jint kContextoMenus = 0;       // menus y pantallas de carga
constexpr jint kContextoMundo = 1;       // conduciendo: libre o en otra carrera
constexpr jint kContextoAceleracion = 2; // carrera de aceleracion
constexpr jint kContextoPausa = 3;       // en el mundo, con el juego en pausa (su menu, un mensaje...)

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
extern "C" JNIEXPORT jint JNICALL Java_io_github_nfsmwrecomp_TouchControllerBridge_contexto(JNIEnv*,
                                                                                         jclass) {
  Lectura l;
  const jint contexto = LeerContexto(l);
  if (contexto != g_ultimo_contexto) {
    static constexpr const char* kNombres[] = {"desconocido", "menus", "conduciendo", "aceleracion", "pausa"};
    REXLOG_INFO("[contexto] {} (estado {}, tipo {} por {})", kNombres[contexto + 1], l.estado, l.tipo,
                l.via);
    g_ultimo_contexto = contexto;
  }
  return contexto;
}

// Diagnostico: lo leido, en una linea. TouchControllerView la guarda en
// files/logs/contexto.txt cada vez que cambia (el APK release no escribe el
// log del juego).
extern "C" JNIEXPORT jstring JNICALL
Java_io_github_nfsmwrecomp_TouchControllerBridge_contextoDiagnostico(JNIEnv* env, jclass) {
  Lectura l;
  const jint contexto = LeerContexto(l);
  char linea[256];
  std::snprintf(linea, sizeof(linea),
                "contexto=%d instr=%d estado=%u pausas=%u fObj=%08X parms=%08X idx=%08X col=%08X tipo=%d (%s \"%s\")",
                int(contexto), g_instrucciones_bien, l.estado, l.pausas, l.carrera, l.parametros, l.indice,
                l.coleccion, l.tipo, l.via, l.nombre);
  return env->NewStringUTF(linea);
}

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
