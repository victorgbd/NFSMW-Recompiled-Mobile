// El mando tactil de iOS: el TouchControllerView de la app de Android
// (android/app/src/main/java/io/github/nfsmwrecomp/TouchControllerView.java) en UIKit, con
// su editor en la partida. Lo nativo (el estado del mando, en que parte del juego se esta,
// los menus con el dedo, la pausa) es el mismo codigo que en Android:
// android/app/src/main/cpp/nativo/nativo_android.cpp, que iOS compila sin JNI (NfsmwTactil*).
//
// Lo que cambia respecto a Java:
//   - Las preferencias van en NSUserDefaults, con las mismas claves que en Android
//     ("nfsmw_touch_controller.<clave>" y "ajustes.<clave>"); las de "ajustes" las pone el
//     lanzador (lanzador.mm). Los textos son los de Android, en sus 10 idiomas (ios_app.h).
//   - Los controles se colocan en la zona segura (fuera de la isla o la muesca y de las
//     esquinas redondeadas); la imagen del juego (los toques de los menus) es la de la
//     pantalla entera: el 16:9 del centro, o toda si se estira o va en pantalla ancha.
//   - Un mando fisico se ve con GameController: conectado, o usado (se mira cada 200 ms).
//   - El movil vibra con Core Haptics (GameActivity.vibrarMando), y el contador de fps
//     (GameActivity.ponerContadorFps) va en esta misma vista.
//
// La vista se coloca sola: un constructor estatico espera a que la ventana de SDL (la de
// SDL_uikitviewcontroller) sea la principal y se pone encima. Nada del motor la llama.

#import <CoreHaptics/CoreHaptics.h>
#import <CoreMotion/CoreMotion.h>
#import <CoreText/CoreText.h>
#import <GameController/GameController.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#include <os/log.h>

#include <SDL3/SDL_hints.h>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <rex/cvar.h>

#include "ios_app.h"

// android/app/src/main/cpp/nativo/nativo_android.cpp
extern "C" void NfsmwTactilEstado(int32_t botones, float izq_x, float izq_y, float der_x, float der_y,
                                  float gatillo_izq, float gatillo_der);
extern "C" void NfsmwTactilPausarJuego(bool pausa);
extern "C" int32_t NfsmwTactilContexto();
extern "C" void NfsmwTactilContextoDiagnostico(char* linea, size_t tam);
extern "C" int32_t NfsmwTactilMenuTocar(float u, float v);
extern "C" bool NfsmwTactilMenuRotar();
extern "C" bool NfsmwTactilMenuEsLista();
extern "C" int32_t NfsmwTactilMenuRumbo();
extern "C" bool NfsmwTactilMenuPrincipal(float* salida);
extern "C" uint32_t NfsmwTactilBackPulsados();
std::string NfsmwTactilMenuDiagnostico();
// Fotogramas que ha presentado el juego (nativo/app/src/nfsmw_d3d_trace.cpp, en su gancho del Swap).
extern std::atomic<uint64_t> g_nfsmw_fotogramas_juego;

namespace {

// ---------------------------------------------------------------------------------------------
//  Constantes de TouchControllerView y TouchControllerBridge
// ---------------------------------------------------------------------------------------------

// Los botones con el indice de SDL_GamepadButton (NfsmwTactilEstado los pasa a XInput).
constexpr int DPAD_UP = 1 << 11, DPAD_DOWN = 1 << 12, DPAD_LEFT = 1 << 13, DPAD_RIGHT = 1 << 14;
constexpr int START = 1 << 6, BACK = 1 << 4, L3 = 1 << 7, R3 = 1 << 8, LB = 1 << 9, RB = 1 << 10;
constexpr int A = 1 << 0, B = 1 << 1, X = 1 << 2, Y = 1 << 3;
// En una pulsacion de los menus, los gatillos: no son botones, van como LT y RT pisados.
constexpr int PULSO_LT = 1 << 16, PULSO_RT = 1 << 17;

constexpr int CONTEXTO_DESCONOCIDO = -1, CONTEXTO_MENUS = 0, CONTEXTO_CONDUCIENDO = 1,
              CONTEXTO_ACELERACION = 2, CONTEXTO_PAUSA = 3, CONTEXTO_CINEMATICA = 4;
constexpr int MENU_DESCONOCIDO = -1, MENU_INDICACION = 0x100, MENU_IR = 0x300, MENU_SIN_RUMBO = 0x400,
              MENU_ACEPTAR = 0x600, MENU_TIPO = 0xF00;

constexpr uint32_t Argb(int a, int r, int g, int b) {
  return uint32_t(a) << 24 | uint32_t(r) << 16 | uint32_t(g) << 8 | uint32_t(b);
}
// Color de acento #6F7432: botones pulsados y borde del boton HIDE/TOUCH.
constexpr uint32_t PULSADO = Argb(190, 0x6F, 0x74, 0x32);
constexpr uint32_t PULSADO_CRUCETA = Argb(180, 0x6F, 0x74, 0x32);
constexpr uint32_t BORDE_TOGGLE = Argb(220, 0x6F, 0x74, 0x32);
constexpr uint32_t ACENTO = Argb(255, 0x6F, 0x74, 0x32);
constexpr uint32_t RELLENO = Argb(105, 12, 12, 15);
constexpr uint32_t BORDE = Argb(205, 255, 255, 255);
constexpr uint32_t FONDO_BOTON = Argb(175, 15, 15, 18);
constexpr uint32_t BLANCO = Argb(255, 255, 255, 255);

// El boton Salir del menu principal (TouchControllerView, SALIR_*): las medidas de la indicacion
// "(A) Aceptar" del juego, en unidades del menu (una captura del RedMagic, 2026-10-29).
constexpr double SALIR_CONFIRMAR_MS = 3000;
constexpr float SALIR_ALTO = 23.7f, SALIR_MARGEN = 4.4f, SALIR_RADIO = 10.f, SALIR_TRAZO = 1.6f,
                SALIR_HUECO = 7.3f, SALIR_MAYUSCULA = 9.8f, SALIR_ANCHO_ACEPTAR = 58.4f, SALIR_COLA = 18.f;
constexpr float SALIR_RAYA_PASO = 14.f, SALIR_RAYA_ANCHO = 7.f;
constexpr uint32_t SALIR_CREMA = Argb(255, 0xF0, 0xF0, 0xD3);
constexpr uint32_t SALIR_FONDO = Argb(140, 0, 0, 0);
constexpr uint32_t SALIR_RAYA = Argb(30, 255, 255, 255);

constexpr double TOGGLE_MS = 4000;
constexpr float PASO = .1f, ESCALA_MIN = .5f, ESCALA_MAX = 2.f, OPACIDAD_MIN = .2f;

constexpr int NORMAL = 0, ACELERACION = 1, CONDUCIENDO = 2, DISPOSICIONES = 3;
const char* const PREFIJOS[] = {"", "a_", "c_"};
const char* const NOMBRES_DISP[] = {"normal", "aceleracion", "conduciendo"};
constexpr int EN_NORMAL = 1 << NORMAL, EN_ACELERACION = 1 << ACELERACION, EN_CONDUCIENDO = 1 << CONDUCIENDO;
constexpr int EN_TODAS = EN_NORMAL | EN_ACELERACION | EN_CONDUCIENDO;

constexpr double CONTEXTO_MS = 250;
constexpr int SIN_ICONO = 0, ICONO_CAMARA = 1, ICONO_RETROVISOR = 2, ICONO_POSICIONES = 3, ICONO_NITRO = 4,
              ICONO_FRENO_MANO = 5, ICONO_RELOJ = 6;
const char* const PUESTOS[] = {"1", "2", "3"};

// A tope de golpe, como el teclado del juego en PC (TouchControllerView.java en Android).
constexpr float GIRO_FLECHA_INICIAL = 1.f;
constexpr double GIRO_FLECHA_MS = 250;
constexpr float ZONA_MUERTA_GIRO = .03f;

constexpr float PELLIZCO_DP = 14, PELLIZCO_PASO_DP = 4;
constexpr double ZOOM_QUIETO_MS = 140;
constexpr bool ROTAR_CON_EL_DEDO = true;
constexpr float VELOCIDAD_STICK_DP = .3f, STICK_MINIMO = .35f;
constexpr double ARRASTRE_QUIETO_MS = 60, FRENADA_MS = 25;
constexpr float FRENADA = .82f;
constexpr int SIN_EJE = 0, EJE_X = 1, EJE_Y = 2;
constexpr float MOVER_DP = 12, PRIMER_PASO_DP = 28, PASO_DESLIZAR_DP = 56, PASO_DESLIZAR_MARCO = .07f;
constexpr double TOQUE_MS = 450, DOS_DEDOS_MS = 600;
constexpr double PULSO_MS = 90, ENTRE_PULSOS_MS = 90;
constexpr size_t MAX_PULSOS = 6;
constexpr int TRAS_NADA = 0, TRAS_RUMBO = 1, MAX_PASOS_RUMBO = 10;
constexpr long MAX_DIAGNOSTICO_MENUS = 1 << 20;

float limitar(float v, float min, float max) { return std::max(min, std::min(max, v)); }
float signum(float v) { return v > 0 ? 1.f : (v < 0 ? -1.f : 0.f); }
// En puntos: un dp de Android es casi un punto de iOS (160 frente a 163 por pulgada).
float dp(float v) { return v; }
double ahoraMs() { return CACurrentMediaTime() * 1000.0; }

// RectF de Android: [left, right) x [top, bottom).
struct Rect {
  float left = 0, top = 0, right = 0, bottom = 0;
  void set(float l, float t, float r, float b) { left = l; top = t; right = r; bottom = b; }
  float width() const { return right - left; }
  float height() const { return bottom - top; }
  float centerX() const { return (left + right) * .5f; }
  float centerY() const { return (top + bottom) * .5f; }
  bool contains(float x, float y) const { return left < right && top < bottom && x >= left && x < right && y >= top && y < bottom; }
  void inset(float dx, float dy) { left += dx; top += dy; right -= dx; bottom -= dy; }
  CGRect cg() const { return CGRectMake(left, top, right - left, bottom - top); }
};

// Los textos de la app de Android (res/values*/strings.xml), en el idioma de la app.
struct Textos {
  NSString *ocultar, *mostrar, *mando, *gestos, *tamano, *opacidad, *restaurar, *cancelar, *listo, *ayuda;
  NSString *salir, *salirSeguro;
  NSString* disp[3];
};

Textos CargarTextos() {
  Textos t;
  t.ocultar = NfsmwTexto(@"tactil_ocultar");
  t.mostrar = NfsmwTexto(@"tactil_mostrar");
  t.mando = NfsmwTexto(@"tactil_mando");
  t.gestos = NfsmwTexto(@"tactil_gestos");
  t.tamano = NfsmwTexto(@"editor_tamano");
  t.opacidad = NfsmwTexto(@"editor_opacidad");
  t.restaurar = NfsmwTexto(@"editor_restaurar");
  t.cancelar = NfsmwTexto(@"editor_cancelar");
  t.listo = NfsmwTexto(@"editor_listo");
  t.ayuda = NfsmwTexto(@"editor_ayuda");
  t.salir = NfsmwTexto(@"salir_juego");
  t.salirSeguro = NfsmwTexto(@"salir_juego_seguro");
  t.disp[NORMAL] = NfsmwTexto(@"editor_disposicion_normal");
  t.disp[ACELERACION] = NfsmwTexto(@"editor_disposicion_aceleracion");
  t.disp[CONDUCIENDO] = NfsmwTexto(@"editor_disposicion_conduciendo");
  return t;
}

// La fuente del boton Salir: Michroma (OFL), la de res/font de Android, que compilar.sh mete en la
// app. Sin ella, la negrita del sistema.
UIFont* FuenteSalir(CGFloat tamano) {
  static CTFontDescriptorRef descriptor = [] {
    CTFontDescriptorRef d = nullptr;
    if (NSURL* url = [NSBundle.mainBundle URLForResource:@"michroma" withExtension:@"ttf"]) {
      CFArrayRef lista = CTFontManagerCreateFontDescriptorsFromURL((__bridge CFURLRef)url);
      if (lista && CFArrayGetCount(lista) > 0) {
        d = (CTFontDescriptorRef)CFRetain(CFArrayGetValueAtIndex(lista, 0));
      }
      if (lista) CFRelease(lista);
    }
    return d;
  }();
  if (!descriptor) return [UIFont boldSystemFontOfSize:tamano];
  return (__bridge_transfer UIFont*)CTFontCreateWithFontDescriptor(descriptor, tamano, nullptr);
}

// ---------------------------------------------------------------------------------------------
//  Preferencias: SharedPreferences "nfsmw_touch_controller" y "ajustes" de Android
// ---------------------------------------------------------------------------------------------

NSString* Clave(const char* fichero, const std::string& k) {
  return [NSString stringWithFormat:@"%s.%s", fichero, k.c_str()];
}
bool Hay(const char* f, const std::string& k) {
  return [NSUserDefaults.standardUserDefaults objectForKey:Clave(f, k)] != nil;
}
float LeerFloat(const char* f, const std::string& k, float def) {
  id v = [NSUserDefaults.standardUserDefaults objectForKey:Clave(f, k)];
  return v ? [v floatValue] : def;
}
bool LeerBool(const char* f, const std::string& k, bool def) {
  id v = [NSUserDefaults.standardUserDefaults objectForKey:Clave(f, k)];
  return v ? [v boolValue] : def;
}
int LeerInt(const char* f, const std::string& k, int def) {
  id v = [NSUserDefaults.standardUserDefaults objectForKey:Clave(f, k)];
  return v ? [v intValue] : def;
}
std::string LeerTexto(const char* f, const std::string& k, const std::string& def) {
  NSString* v = [NSUserDefaults.standardUserDefaults stringForKey:Clave(f, k)];
  return v ? std::string(v.UTF8String) : def;
}
void PonerFloat(const char* f, const std::string& k, float v) {
  [NSUserDefaults.standardUserDefaults setFloat:v forKey:Clave(f, k)];
}
void PonerBool(const char* f, const std::string& k, bool v) {
  [NSUserDefaults.standardUserDefaults setBool:v forKey:Clave(f, k)];
}
void Quitar(const char* f, const std::string& k) {
  [NSUserDefaults.standardUserDefaults removeObjectForKey:Clave(f, k)];
}

constexpr const char* PREFS = "nfsmw_touch_controller";
constexpr const char* AJUSTES = "ajustes";

// ---------------------------------------------------------------------------------------------
//  La vibracion del movil (GameActivity.vibrarMando y callarVibracion)
// ---------------------------------------------------------------------------------------------
//
// El juego pide una fuerza de 0 a 255 desde su hilo (NfsmwAndroidVibrar, abajo) y dura hasta que
// la cambie, como en el mando de la Xbox 360: un evento continuo de Core Haptics que se repite, y
// su intensidad cambia sobre la marcha. Se calla (sin perder lo pedido) en segundo plano, con el
// editor abierto y con un mando fisico, que vibra el (por SDL).

constexpr int CALLADA_SEGUNDO_PLANO = 1, CALLADA_EDITOR = 2, CALLADA_MANDO = 4;

std::mutex g_vibracion_mutex;
int g_vibracion_pedida = 0, g_vibracion_puesta = 0, g_vibracion_callada = 0;
std::atomic<int> g_nivel_vibracion{-1};

// El motor de hapticos y lo que suena, solo desde su cola.
dispatch_queue_t ColaVibracion() {
  static dispatch_queue_t cola = dispatch_queue_create("nfsmw.vibracion", DISPATCH_QUEUE_SERIAL);
  return cola;
}
CHHapticEngine* g_hapticos = nil;
bool g_hapticos_en_marcha = false;
id<CHHapticAdvancedPatternPlayer> g_vibrando = nil;

bool ArrancarHapticos() {
  if (!g_hapticos) {
    if (!CHHapticEngine.capabilitiesForHardware.supportsHaptics) return false;
    g_hapticos = [[CHHapticEngine alloc] initAndReturnError:nil];
    if (!g_hapticos) return false;
    g_hapticos.playsHapticsOnly = YES;
    // iOS lo para (segundo plano, otra app con hapticos): lo que sonaba se pierde.
    g_hapticos.stoppedHandler = ^(CHHapticEngineStoppedReason) {
      dispatch_async(ColaVibracion(), ^{
        g_hapticos_en_marcha = false;
        g_vibrando = nil;
      });
    };
    g_hapticos.resetHandler = ^{
      dispatch_async(ColaVibracion(), ^{
        g_hapticos_en_marcha = false;
        g_vibrando = nil;
      });
    };
  }
  if (!g_hapticos_en_marcha) {
    g_hapticos_en_marcha = [g_hapticos startAndReturnError:nil];
  }
  return g_hapticos_en_marcha;
}

// En la cola: la fuerza de 0 a 255.
void PonerVibracion(int fuerza) {
  if (fuerza <= 0) {
    [g_vibrando stopAtTime:CHHapticTimeImmediate error:nil];
    g_vibrando = nil;
    return;
  }
  if (!ArrancarHapticos()) return;
  if (!g_vibrando) {
    CHHapticEvent* e = [[CHHapticEvent alloc]
        initWithEventType:CHHapticEventTypeHapticContinuous
               parameters:@[[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1],
                            [[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticSharpness value:.3f]]
             relativeTime:0
                 duration:30];
    CHHapticPattern* patron = [[CHHapticPattern alloc] initWithEvents:@[e] parameters:@[] error:nil];
    g_vibrando = patron ? [g_hapticos createAdvancedPlayerWithPattern:patron error:nil] : nil;
    g_vibrando.loopEnabled = YES;
    [g_vibrando startAtTime:CHHapticTimeImmediate error:nil];
  }
  CHHapticDynamicParameter* intensidad =
      [[CHHapticDynamicParameter alloc] initWithParameterID:CHHapticDynamicParameterIDHapticIntensityControl
                                                      value:fuerza / 255.f
                                               relativeTime:0];
  [g_vibrando sendParameters:@[intensidad] atTime:CHHapticTimeImmediate error:nil];
}

void AplicarVibracion() {
  const bool activa = LeerBool(AJUSTES, "vibracion", true);
  int fuerza;
  {
    std::lock_guard<std::mutex> l(g_vibracion_mutex);
    fuerza = activa && g_vibracion_callada == 0 ? std::min(255, g_vibracion_pedida) : 0;
    if (fuerza == g_vibracion_puesta) return;
    g_vibracion_puesta = fuerza;
  }
  dispatch_async(ColaVibracion(), ^{
    PonerVibracion(fuerza);
  });
}

void CallarVibracion(int motivo, bool callar) {
  {
    std::lock_guard<std::mutex> l(g_vibracion_mutex);
    g_vibracion_callada = callar ? g_vibracion_callada | motivo : g_vibracion_callada & ~motivo;
  }
  AplicarVibracion();
}

// Ajustes.anguloGiro: los grados para girar del todo, de 45 (1) a 6 (20).
float AnguloGiro(int sensibilidad) {
  const int s = std::max(1, std::min(20, sensibilidad));
  return float(45.0 * std::pow(6.0 / 45.0, (s - 1) / 19.0));
}

// Documents/logs, donde el motor deja su log (GetExecutableFolder en iOS).
std::string CarpetaLogs() {
  const char* home = std::getenv("HOME");
  std::string carpeta = std::string(home ? home : "") + "/Documents/logs";
  mkdir(carpeta.c_str(), 0755);
  return carpeta;
}

std::string Hora() {
  NSDateFormatter* f = [[NSDateFormatter alloc] init];
  f.dateFormat = @"HH:mm:ss.SSS";
  f.locale = [NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"];
  return [f stringFromDate:NSDate.date].UTF8String;
}

// ---------------------------------------------------------------------------------------------
//  Tareas con retraso: postDelayed y removeCallbacks
// ---------------------------------------------------------------------------------------------

struct Tarea {
  uint64_t generacion = 0;
};

void Programar(Tarea& t, double ms, std::function<void()> f) {
  const uint64_t g = ++t.generacion;
  Tarea* p = &t;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t(ms * NSEC_PER_MSEC)), dispatch_get_main_queue(), ^{
    if (p->generacion == g) {
      f();
    }
  });
}

void Cancelar(Tarea& t) { ++t.generacion; }

// ---------------------------------------------------------------------------------------------
//  Un evento de toque como el MotionEvent de Android
// ---------------------------------------------------------------------------------------------

enum Accion { ACTION_DOWN, ACTION_POINTER_DOWN, ACTION_MOVE, ACTION_UP, ACTION_POINTER_UP, ACTION_CANCEL };

struct Dedo {
  int id;
  float x, y;
};

struct Evento {
  Accion accion;
  int indice;
  double tiempo;
  std::vector<Dedo> dedos;
  int getPointerCount() const { return int(dedos.size()); }
  int getPointerId(int i) const { return dedos[size_t(i)].id; }
  float getX(int i) const { return dedos[size_t(i)].x; }
  float getY(int i) const { return dedos[size_t(i)].y; }
  int findPointerIndex(int id) const {
    for (size_t i = 0; i < dedos.size(); ++i) {
      if (dedos[i].id == id) return int(i);
    }
    return -1;
  }
};

class Tactil;
struct Stick;
struct Flecha;
struct Trigger;

// ---------------------------------------------------------------------------------------------
//  Los controles (las clases internas de TouchControllerView)
// ---------------------------------------------------------------------------------------------

struct Control {
  Tactil* t;
  std::string id;
  float defX, defY;
  float cx, cy;
  float escala = 1.f;
  bool personalizado = false;
  bool inUse = false;
  float px[3] = {}, py[3] = {}, ps[3] = {1, 1, 1};
  bool propia[3] = {};
  float fx[3], fy[3], fs[3] = {1, 1, 1};
  int visibleEn = EN_TODAS;

  Control(Tactil* tactil, std::string i, float x, float y) : t(tactil), id(std::move(i)), defX(x), defY(y), cx(x), cy(y) {
    for (int d = 0; d < 3; ++d) {
      fx[d] = x;
      fy[d] = y;
    }
  }
  virtual ~Control() = default;
  void porDefecto(int d, float x, float y, float s) {
    fx[d] = x;
    fy[d] = y;
    fs[d] = s;
  }
  void usar(int d) {
    if (propia[d]) {
      cx = px[d];
      cy = py[d];
      escala = ps[d];
    } else {
      cx = fx[d];
      cy = fy[d];
      escala = fs[d];
    }
    personalizado = propia[d];
    colocar();
  }
  void guardarEn(int d) {
    px[d] = cx;
    py[d] = cy;
    ps[d] = escala;
    propia[d] = personalizado;
  }
  virtual void colocar() = 0;
  void situar(float x, float y) {
    cx = x;
    cy = y;
    colocar();
  }
  virtual void ocupa(Rect& r) = 0;
  virtual bool contains(float x, float y) = 0;
  virtual void press(float x, float y) {
    inUse = true;
    move(x, y);
  }
  virtual void move(float, float) {}
  virtual void release() { inUse = false; }
  virtual int buttons() { return 0; }
  bool oculto();
  virtual void draw(CGContextRef ctx) = 0;
};

class Tactil {
 public:
  explicit Tactil(UIView* vista);

  // La vista
  void tamano(float ancho, float alto, CGRect pantalla);
  void dibujar(CGContextRef ctx);
  void tocar(const Evento& e);
  void mandoUsado();
  void revisarMandos();
  void inclinacion(CMAcceleration g, UIInterfaceOrientation orientacion);
  bool escuchandoInclinacion() const { return inclinar; }
  void empezar();
  void clearInput();

  // Para los controles
  int disposicion() const { return editando ? dispEditada : dispJuego; }
  bool formasDeConducir() const { return disposicion() != NORMAL; }
  float opacidad = 1.f;
  float lastWidth = 0, lastHeight = 0;
  void flechaCambiada();
  uint32_t conOpacidad(uint32_t color) const;
  void drawCircle(CGContextRef c, float x, float y, float radius, bool pressed);
  void drawLabel(CGContextRef c, NSString* text, float x, float y, float size, float alfa);
  void rellenarRedondeado(CGContextRef c, const Rect& r, float radio, uint32_t color);
  void trazarRedondeado(CGContextRef c, const Rect& r, float radio, uint32_t color, float grosor = 2);
  void linea(CGContextRef c, float x0, float y0, float x1, float y1, uint32_t color, float grosor = 2);
  void aro(CGContextRef c, float x, float y, float r, uint32_t color, float grosor = 2);
  void dibujarPedal(CGContextRef c, const Rect& r, bool freno, bool pisadoAhora);
  void dibujarCamara(CGContextRef c, const Rect& caja);
  void dibujarPosiciones(CGContextRef c, const Rect& caja);
  void dibujarRetrovisor(CGContextRef c, const Rect& caja);
  void dibujarNitro(CGContextRef c, float x, float y, float r);
  void dibujarFrenoMano(CGContextRef c, float x, float y, float r);
  void dibujarReloj(CGContextRef c, float x, float y, float r);

 private:
  void invalidate() { [vista_ setNeedsDisplay]; }
  void buildControls(float width, float height);
  void cargar(Control* c);
  std::string clave(Control* c, int d) const;
  void usarDisposicion(int d);
  void calcularMarco();
  NSString* textoToggle() const;
  bool menusConDedo() const { return menusTactiles && enMenu && !editando; }
  bool conDedo() const { return menusConDedo() && controlsVisible && !mandoEnMenus; }
  void colocarBotones(float width, float height, float unit);
  void drawEngranaje(CGContextRef c);
  void drawEditor(CGContextRef c);
  void drawBoton(CGContextRef c, const Rect& r, NSString* texto, float tamano);
  void drawTexto(CGContextRef c, NSString* texto, float x, float y, float tamano);
  float medir(NSString* texto, float tamano) const;

  void entrarModoMando();
  void salirModoMando();
  void mostrarToggle();
  bool inclinacionActiva() const;
  void mirarContexto();
  void anotarDiagnostico();

  void mirarSalir(bool enMenus);
  bool salirALaVista() const { return salirVisible && !editando; }
  // Con el mando tactil a la vista solo se ve: para salir esta su BACK.
  bool salirTocable() const { return salirALaVista() && (!controlsVisible || conDedo()); }
  bool tocaSalir(float x, float y) const;
  void pulsarSalir();
  void salir();
  NSString* textoSalirActual() const { return salirConfirmarHasta > 0 ? textos_.salirSeguro : textos_.salir; }
  NSDictionary* atributosSalir() const;
  void colocarSalir();
  void drawSalir(CGContextRef c);
  void anotarSalir();

  bool tocarMenu(const Evento& e);
  bool tocarCinematica(const Evento& e);
  int gatilloBajo(float x, float y);
  void arrastrarStick(float x, float y, double t);
  static float curvaStick(float v);
  void soltarStick();
  void soltarArrastre();
  static float distancia(const Evento& e);
  void pellizcar(const Evento& e);
  void soltarGatillo(bool delPellizco);
  float pasoDeslizar() const;
  void darPaso(bool positivo);
  void tocarEn(float x, float y);
  void seguirRumbo();
  void pulsar(int mascara, int tras);
  void siguientePulso();
  void pararRumbo();
  void cancelarMenu();
  void cambiarMandoEnMenus();
  void anotarMenus(float u, float v);

  bool tocarEditor(const Evento& e);
  void cambiarEscala(float paso);
  void escalar(Control* c, float paso);
  void cambiarOpacidad(float paso);
  void restaurarDisposicion();
  void cambiarDisposicionEditada();
  void abrirEditor();
  void terminarEdicion();
  void guardarEdicion();
  void setControlsVisible(bool visible);
  void rehacer();
  bool avanzarGiroFlechas();
  void pasoRampaFlechas();
  void sendState();

  __weak UIView* vista_;
  Textos textos_;
  std::vector<std::unique_ptr<Control>> controls;
  std::unordered_map<int, Control*> pointers;
  bool controlsVisible = true;
  bool mandoFisico = false;
  bool toggleVisible = true;
  Tarea ocultarToggle_;
  Rect toggle, engranaje;
  CGRect pantalla_ = CGRectZero;  // la pantalla entera, en las coordenadas de los controles

  // Editor
  bool editando = false;
  bool visiblesAntesDeEditar = true;
  Control* elegido = nullptr;
  int dedoEditor = -1;
  float agarreX = 0, agarreY = 0;
  Rect tamMenos, tamMas, opaMenos, opaMas, restaurar, cancelar, listo, disposicionBoton, fondoAyuda;
  int dispEditada = NORMAL;
  float textoTamX = 0, textoOpaX = 0, tamanoAyuda = 12;

  Stick* leftStick = nullptr;
  Stick* rightStick = nullptr;
  Flecha* carrilIzq = nullptr;
  Flecha* carrilDer = nullptr;
  Trigger* leftTrigger = nullptr;
  Trigger* rightTrigger = nullptr;

  // Segun la parte del juego
  int dispJuego = NORMAL;
  bool enPausa = false;
  NSTimer* mirarContexto_ = nil;
  std::string ultimoDiagnostico;
  bool hayDiagnostico = false;

  // Girar con flechas
  bool flechasAlConducir;
  float giroFlechas = 0;
  double pasoFlechas = 0;
  Tarea rampaFlechas_;

  // Girar inclinando
  bool inclinar;
  float senoGiroCompleto;
  float giroInclinado = 0;

  // Los menus con el dedo
  bool menusTactiles;
  bool deslizarInvertido = false;
  bool enMenu = false;
  bool enCinematica = false;
  bool mandoEnMenus;
  bool estirar;
  Rect imagenJuego;
  int dedoMenu = -1;
  float menuX0 = 0, menuY0 = 0;
  double menuT0 = 0;
  int ejeMenu = SIN_EJE;
  float contadoMenu = 0;
  int pasosMenu = 0;
  bool dosDedos = false;
  bool deslizarDirecto = false;
  int mantenido = 0;
  int gatilloDedo = 0;
  float distanciaPellizco = 0;
  bool pellizco = false;
  int gatilloPellizco = 0;
  Tarea soltarPellizco_;
  bool arrastrar = false;
  float stickArrastreX = 0, stickArrastreY = 0;
  float arrastreX = 0, arrastreY = 0;
  double arrastreT = 0;
  Tarea soltarArrastre_;
  struct Pulso {
    int mascara, tras;
  };
  std::deque<Pulso> pulsos;
  int pulso = 0;
  int trasPulso = TRAS_NADA;
  bool pulsando = false;
  int pasosRumbo = 0;
  Tarea siguientePulso_;
  Tarea soltarPulso_;
  bool menusAnotados = false;

  // Salir del juego desde el menu principal (nativo_android.cpp, seccion 11): una indicacion
  // mas, "Salir", con el estilo de las del juego. Se ve siempre; se toca solo sin el mando
  // tactil a la vista (con el, su BACK hace lo mismo, como el del mando fisico). Toca (o BACK)
  // una vez y pide confirmar; otra antes de SALIR_CONFIRMAR_MS, cierra la app.
  float menuPrincipal[4] = {};
  bool salirVisible = false;
  Rect salirCaja;
  double salirConfirmarHasta = 0;
  int backVistos = -1;
  // Si el BACK del mando tactil estaba pisado en el ultimo sendState.
  bool backTactil = false;
  bool salirAnotado = false;
  // Lo que mide una unidad del menu del juego en cada eje: el menu va estirado a lo ancho, y el
  // boton con el. El texto, a la altura de las mayusculas del juego y estirado como "Aceptar".
  float salirUx = 0, salirUy = 0;
  UIFont* salirFuente = nil;
  float salirEscalaX = 1, salirGrosor = 0;
  Tarea finConfirmarSalir_;
};

bool Control::oculto() { return (visibleEn & (1 << t->disposicion())) == 0; }

struct ButtonControl : Control {
  NSString* text;
  float baseRadius;
  int mask;
  float radius = 0;
  int icono = SIN_ICONO;
  ButtonControl(Tactil* t, std::string id, NSString* text, float x, float y, float radius, int mask)
      : Control(t, std::move(id), x, y), text(text), baseRadius(radius), mask(mask) {
    colocar();
  }
  void colocar() override { radius = baseRadius * escala; }
  void ocupa(Rect& r) override { r.set(cx - radius, cy - radius, cx + radius, cy + radius); }
  bool contains(float px, float py) override {
    const float dx = px - cx, dy = py - cy;
    return dx * dx + dy * dy <= radius * radius * 1.35f;
  }
  int buttons() override { return inUse ? mask : 0; }
  void draw(CGContextRef c) override {
    t->drawCircle(c, cx, cy, radius, inUse);
    if (icono == ICONO_NITRO && t->formasDeConducir()) {
      t->dibujarNitro(c, cx, cy, radius);
    } else if (icono == ICONO_FRENO_MANO && t->formasDeConducir()) {
      t->dibujarFrenoMano(c, cx, cy, radius);
    } else if (icono == ICONO_RELOJ && t->formasDeConducir()) {
      t->dibujarReloj(c, cx, cy, radius);
    } else {
      t->drawLabel(c, text, cx, cy, radius * .72f, t->opacidad);
    }
  }
};

struct RectButton : Control {
  NSString* text;
  float baseWidth, baseHeight;
  int mask;
  Rect bounds;
  int icono = SIN_ICONO;
  RectButton(Tactil* t, std::string id, NSString* text, float x, float y, float width, float height, int mask)
      : Control(t, std::move(id), x, y), text(text), baseWidth(width), baseHeight(height), mask(mask) {
    RectButton::colocar();
  }
  void colocar() override {
    const float w = baseWidth * escala * .5f, h = baseHeight * escala * .5f;
    bounds.set(cx - w, cy - h, cx + w, cy + h);
  }
  void ocupa(Rect& r) override { r = bounds; }
  bool contains(float x, float y) override { return bounds.contains(x, y); }
  int buttons() override { return inUse ? mask : 0; }
  void draw(CGContextRef c) override {
    const float r = std::min(dp(10), bounds.height() * .5f);
    t->rellenarRedondeado(c, bounds, r, t->conOpacidad(inUse ? PULSADO : RELLENO));
    t->trazarRedondeado(c, bounds, r, t->conOpacidad(BORDE));
    if (icono == ICONO_CAMARA && t->formasDeConducir()) {
      t->dibujarCamara(c, bounds);
    } else if (icono == ICONO_RETROVISOR && t->formasDeConducir()) {
      t->dibujarRetrovisor(c, bounds);
    } else if (icono == ICONO_POSICIONES && t->formasDeConducir()) {
      t->dibujarPosiciones(c, bounds);
    } else {
      t->drawLabel(c, text, bounds.centerX(), bounds.centerY(), dp(11) * escala, t->opacidad);
    }
  }
};

// LT y RT. Conduciendo y en las de aceleracion, pedales de pie.
struct Trigger : RectButton {
  bool freno;
  Rect pedal;
  Trigger(Tactil* t, std::string id, NSString* text, float x, float y, float width, float height, bool freno)
      : RectButton(t, std::move(id), text, x, y, width, height, 0), freno(freno) {
    colocar();
  }
  void colocar() override {
    RectButton::colocar();
    const float ancho = baseWidth * escala * (freno ? .74f : .6f);
    const float alto = baseWidth * escala * (freno ? .9f : 1.15f);
    const float x = limitar(cx, ancho * .5f + dp(4), t->lastWidth - ancho * .5f - dp(4));
    const float y = limitar(cy, alto * .5f + dp(4), t->lastHeight - alto * .5f - dp(4));
    pedal.set(x - ancho * .5f, y - alto * .5f, x + ancho * .5f, y + alto * .5f);
  }
  bool contains(float x, float y) override {
    return t->formasDeConducir() ? pedal.contains(x, y) : RectButton::contains(x, y);
  }
  void ocupa(Rect& r) override {
    if (t->formasDeConducir()) {
      r = pedal;
    } else {
      RectButton::ocupa(r);
    }
  }
  void draw(CGContextRef c) override {
    if (t->formasDeConducir()) {
      t->dibujarPedal(c, pedal, freno, inUse);
    } else {
      RectButton::draw(c);
    }
  }
};

struct Stick : Control {
  NSString* text;
  float baseRadius;
  float radius = 0;
  float xValue = 0, yValue = 0;
  bool palanca = false;  // el derecho: en las de aceleracion, la palanca de cambios
  Stick(Tactil* t, std::string id, NSString* text, float x, float y, float radius)
      : Control(t, std::move(id), x, y), text(text), baseRadius(radius) {
    colocar();
  }
  void colocar() override { radius = baseRadius * escala; }
  bool esPalanca() const { return palanca && t->disposicion() == ACELERACION; }
  void ocupa(Rect& r) override {
    const float ancho = esPalanca() ? radius * .8f : radius;
    const float alto = esPalanca() ? radius * 1.3f : radius;
    r.set(cx - ancho, cy - alto, cx + ancho, cy + alto);
  }
  bool contains(float x, float y) override {
    if (esPalanca()) {
      return std::fabs(x - cx) <= radius * .8f && std::fabs(y - cy) <= radius * 1.3f;
    }
    const float dx = x - cx, dy = y - cy;
    return dx * dx + dy * dy <= radius * radius * 1.55f;
  }
  void move(float x, float y) override {
    if (esPalanca()) {
      // Arriba sube marcha y abajo la baja: entero o en el centro.
      const float dy = (y - cy) / radius;
      xValue = 0;
      yValue = dy < -.3f ? -1 : (dy > .3f ? 1 : 0);
      return;
    }
    float dx = (x - cx) / radius;
    float dy = (y - cy) / radius;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length > 1) {
      dx /= length;
      dy /= length;
    }
    const float deadzone = .08f;
    // Y como SDL: positivo hacia abajo (NfsmwTactilEstado le da la vuelta).
    xValue = std::fabs(dx) < deadzone ? 0 : dx;
    yValue = std::fabs(dy) < deadzone ? 0 : dy;
  }
  void release() override {
    Control::release();
    xValue = 0;
    yValue = 0;
  }
  void draw(CGContextRef c) override {
    if (esPalanca()) {
      dibujarPalanca(c);
      return;
    }
    t->drawCircle(c, cx, cy, radius, false);
    const float knobX = cx + xValue * radius * .58f;
    const float knobY = cy + yValue * radius * .58f;
    t->drawCircle(c, knobX, knobY, radius * .43f, inUse);
    t->drawLabel(c, text, knobX, knobY, radius * .34f, t->opacidad);
  }
  void dibujarPalanca(CGContextRef c) {
    const float ancho = radius * .36f;
    Rect ranura;
    ranura.set(cx - ancho * .5f, cy - radius, cx + ancho * .5f, cy + radius);
    t->rellenarRedondeado(c, ranura, ancho * .5f, t->conOpacidad(RELLENO));
    t->trazarRedondeado(c, ranura, ancho * .5f, t->conOpacidad(BORDE));
    t->drawLabel(c, @"+", cx + radius * .55f, cy - radius * .7f, radius * .42f, t->opacidad);
    t->drawLabel(c, @"−", cx + radius * .55f, cy + radius * .7f, radius * .42f, t->opacidad);
    t->drawCircle(c, cx, cy + yValue * radius * .72f, radius * .36f, inUse);
  }
};

// Cambiar de carril en las de aceleracion (y girar, con Ajustes.modoGiro flechas).
struct Flecha : Control {
  float baseLado;
  int lado;  // -1 izquierda, 1 derecha
  Rect caja;
  Flecha(Tactil* t, std::string id, float x, float y, float baseLado, int lado)
      : Control(t, std::move(id), x, y), baseLado(baseLado), lado(lado) {
    visibleEn = EN_ACELERACION;
    colocar();
  }
  void colocar() override {
    const float m = baseLado * escala * .5f;
    caja.set(cx - m, cy - m, cx + m, cy + m);
  }
  void ocupa(Rect& r) override { r = caja; }
  bool contains(float x, float y) override { return caja.contains(x, y); }
  void press(float x, float y) override {
    Control::press(x, y);
    t->flechaCambiada();
  }
  void release() override {
    Control::release();
    t->flechaCambiada();
  }
  void draw(CGContextRef c) override {
    const float r = std::min(dp(10), caja.height() * .2f);
    t->rellenarRedondeado(c, caja, r, t->conOpacidad(inUse ? PULSADO : RELLENO));
    t->trazarRedondeado(c, caja, r, t->conOpacidad(BORDE));
    const float m = baseLado * escala * .5f;
    const float tt = m * .42f;
    CGContextBeginPath(c);
    CGContextMoveToPoint(c, cx + lado * tt, cy);
    CGContextAddLineToPoint(c, cx - lado * tt * .75f, cy - tt);
    CGContextAddLineToPoint(c, cx - lado * tt * .75f, cy + tt);
    CGContextClosePath(c);
    const uint32_t col = t->conOpacidad(BORDE);
    CGContextSetRGBFillColor(c, ((col >> 16) & 0xFF) / 255.0, ((col >> 8) & 0xFF) / 255.0, (col & 0xFF) / 255.0,
                             (col >> 24) / 255.0);
    CGContextFillPath(c);
  }
};

struct Dpad : Control {
  float baseRadius;
  float radius = 0;
  Rect vertical, horizontal;
  int mask = 0;
  Dpad(Tactil* t, std::string id, float x, float y, float radius) : Control(t, std::move(id), x, y), baseRadius(radius) {
    colocar();
  }
  void colocar() override {
    radius = baseRadius * escala;
    const float arm = radius * .36f;
    vertical.set(cx - arm, cy - radius, cx + arm, cy + radius);
    horizontal.set(cx - radius, cy - arm, cx + radius, cy + arm);
  }
  void ocupa(Rect& r) override { r.set(cx - radius, cy - radius, cx + radius, cy + radius); }
  bool contains(float x, float y) override { return std::fabs(x - cx) <= radius && std::fabs(y - cy) <= radius; }
  void move(float x, float y) override {
    const float dx = x - cx, dy = y - cy;
    const float threshold = radius * .20f;
    mask = 0;
    if (dx < -threshold) mask |= DPAD_LEFT;
    if (dx > threshold) mask |= DPAD_RIGHT;
    if (dy < -threshold) mask |= DPAD_UP;
    if (dy > threshold) mask |= DPAD_DOWN;
  }
  void release() override {
    Control::release();
    mask = 0;
  }
  int buttons() override { return inUse ? mask : 0; }
  void draw(CGContextRef c) override {
    const uint32_t relleno = t->conOpacidad(inUse ? PULSADO_CRUCETA : RELLENO);
    const float r = dp(6);
    t->rellenarRedondeado(c, vertical, r, relleno);
    t->rellenarRedondeado(c, horizontal, r, relleno);
    t->trazarRedondeado(c, vertical, r, t->conOpacidad(BORDE));
    t->trazarRedondeado(c, horizontal, r, t->conOpacidad(BORDE));
  }
};

// ---------------------------------------------------------------------------------------------
//  Tactil: TouchControllerView
// ---------------------------------------------------------------------------------------------

Tactil::Tactil(UIView* vista) : vista_(vista), textos_(CargarTextos()) {
  inclinar = LeerBool(AJUSTES, "girar_inclinando", false);
  flechasAlConducir = !inclinar && LeerTexto(AJUSTES, "giro_carrera", "stick") == "flechas";
  senoGiroCompleto = float(std::sin(AnguloGiro(LeerInt(AJUSTES, "sensibilidad_giro", 10)) * M_PI / 180.0));
  menusTactiles = LeerBool(AJUSTES, "menus_tactiles", true);
  // Con la pantalla ancha la imagen tambien llena la pantalla (GameActivity: estirar || panoramico).
  estirar = LeerBool(AJUSTES, "estirar", false) || LeerBool(AJUSTES, "panoramico", false);
  opacidad = LeerFloat(PREFS, "opacidad", 1.f);
  mandoEnMenus = LeerBool(PREFS, "mando_en_menus", false);
  mandoFisico = GCController.controllers.count > 0;
  CallarVibracion(CALLADA_MANDO, mandoFisico);
  controlsVisible = !mandoFisico && LeerBool(PREFS, "visible", true);
  toggleVisible = !mandoFisico;
}

void Tactil::empezar() {
  // La imagen del juego: con el 16:9 del centro, las bandas las pone el presentador (en
  // Android las pone la vista, con present_letterbox=false).
  rex::cvar::SetFlagByName("present_letterbox", estirar ? "false" : "true");
  rex::cvar::SetFlagByName("present_allow_overscan_cutoff", "false");
  if (toggleVisible) mostrarToggle();
  sendState();
  mirarContexto_ = [NSTimer scheduledTimerWithTimeInterval:CONTEXTO_MS / 1000.0
                                                   repeats:YES
                                                     block:^(NSTimer*) {
                                                       this->mirarContexto();
                                                     }];
}

void Tactil::mandoUsado() {
  if (editando) return;
  if (mandoFisico && !controlsVisible && !toggleVisible) return;
  entrarModoMando();
}

void Tactil::entrarModoMando() {
  if (editando) return;
  mandoFisico = true;
  // Con mando fisico vibra el (por SDL), no el movil.
  CallarVibracion(CALLADA_MANDO, true);
  Cancelar(ocultarToggle_);
  toggleVisible = false;
  if (controlsVisible) {
    controlsVisible = false;
    rehacer();
  } else {
    invalidate();
  }
}

void Tactil::salirModoMando() {
  if (editando) return;
  mandoFisico = false;
  CallarVibracion(CALLADA_MANDO, false);
  controlsVisible = LeerBool(PREFS, "visible", true);
  rehacer();
  mostrarToggle();
}

void Tactil::revisarMandos() {
  const bool hay = GCController.controllers.count > 0;
  if (hay && !mandoFisico) {
    entrarModoMando();
  } else if (!hay && mandoFisico) {
    salirModoMando();
  }
}

void Tactil::mostrarToggle() {
  toggleVisible = true;
  Programar(ocultarToggle_, TOGGLE_MS, [this] {
    toggleVisible = false;
    invalidate();
  });
  invalidate();
}

void Tactil::clearInput() {
  cancelarMenu();
  for (auto& c : controls) c->release();
  pointers.clear();
  sendState();
  invalidate();
}

void Tactil::tamano(float ancho, float alto, CGRect pantalla) {
  if (ancho <= 0 || alto <= 0) return;
  pantalla_ = pantalla;
  if (ancho == lastWidth && alto == lastHeight) {
    calcularMarco();
    return;
  }
  buildControls(ancho, alto);
}

// La disposicion de fabrica (las tres) y encima lo guardado con el editor. Igual que
// TouchControllerView.buildControls.
void Tactil::buildControls(float width, float height) {
  lastWidth = width;
  lastHeight = height;
  calcularMarco();
  controls.clear();
  const float unit = std::min(width / 16.f, height / 9.f);
  const float stickRadius = unit * .78f;
  const float buttonRadius = unit * .38f;

  auto anadir = [this](Control* c) {
    controls.emplace_back(c);
    return c;
  };

  leftStick = static_cast<Stick*>(anadir(new Stick(this, "ls", @"L", width * .16f, height * .73f, stickRadius)));
  rightStick = static_cast<Stick*>(anadir(new Stick(this, "rs", @"R", width * .67f, height * .77f, stickRadius)));
  leftStick->visibleEn = inclinar || flechasAlConducir ? EN_NORMAL : EN_NORMAL | EN_CONDUCIENDO;
  const float ladoFlecha = unit * 1.15f;
  carrilIzq = new Flecha(this, "carril_izq", width * .16f, height * .73f, ladoFlecha, -1);
  carrilDer = new Flecha(this, "carril_der", width * .16f, height * .73f, ladoFlecha, 1);
  if (flechasAlConducir) {
    carrilIzq->visibleEn |= EN_CONDUCIENDO;
    carrilDer->visibleEn |= EN_CONDUCIENDO;
  }
  anadir(carrilIzq);
  anadir(carrilDer);

  auto* dpad = static_cast<Dpad*>(anadir(new Dpad(this, "dpad", width * .32f, height * .72f, unit * .98f)));
  dpad->visibleEn = EN_NORMAL | EN_CONDUCIENDO;
  const float ladoPosiciones = unit * .8f;
  auto* posiciones = new RectButton(this, "posiciones", @"1 2 3", width * .32f, height * .72f, ladoPosiciones,
                                    ladoPosiciones, DPAD_UP);
  posiciones->icono = ICONO_POSICIONES;
  posiciones->visibleEn = EN_ACELERACION;
  anadir(posiciones);

  const float faceX = width * .85f;
  const float faceY = height * .68f;
  auto* a = new ButtonControl(this, "a", @"A", faceX, faceY + unit * .68f, buttonRadius, A);
  auto* b = new ButtonControl(this, "b", @"B", faceX + unit * .68f, faceY, buttonRadius, B);
  auto* x = new ButtonControl(this, "x", @"X", faceX - unit * .68f, faceY, buttonRadius, X);
  auto* y = new ButtonControl(this, "y", @"Y", faceX, faceY - unit * .68f, buttonRadius, Y);
  anadir(a);
  anadir(b);
  anadir(x);
  anadir(y);
  b->icono = ICONO_NITRO;
  a->icono = ICONO_FRENO_MANO;
  x->icono = ICONO_RELOJ;
  y->visibleEn = EN_NORMAL;

  const float shoulderW = unit * 1.25f;
  const float shoulderH = unit * .55f;
  leftTrigger = new Trigger(this, "lt", @"LT", width * .07f, height * .10f, shoulderW, shoulderH, true);
  rightTrigger = new Trigger(this, "rt", @"RT", width * .93f, height * .10f, shoulderW, shoulderH, false);
  rightStick->palanca = true;
  anadir(leftTrigger);
  auto* lb = new RectButton(this, "lb", @"LB", width * .20f, height * .10f, shoulderW, shoulderH, LB);
  auto* rb = new RectButton(this, "rb", @"RB", width * .80f, height * .10f, shoulderW, shoulderH, RB);
  lb->icono = ICONO_RETROVISOR;
  rb->icono = ICONO_CAMARA;
  anadir(lb);
  anadir(rb);
  anadir(rightTrigger);

  auto* back = new RectButton(this, "back", @"BACK", width * .44f, height * .79f, unit * .82f, unit * .45f, BACK);
  auto* start = new RectButton(this, "start", @"START", width * .55f, height * .79f, unit * .92f, unit * .45f, START);
  anadir(back);
  anadir(start);
  auto* l3 = new ButtonControl(this, "l3", @"L3", width * .07f, height * .48f, buttonRadius * .82f, L3);
  auto* r3 = new ButtonControl(this, "r3", @"R3", width * .73f, height * .48f, buttonRadius * .82f, R3);
  l3->visibleEn = EN_NORMAL;
  r3->visibleEn = EN_NORMAL;
  anadir(l3);
  anadir(r3);

  // --- La de fabrica de las carreras de aceleracion (TouchControllerView).
  const float pie = height * .885f;
  const float pedal = 1.5f;
  const float tamPedal = 2.f;
  const float acelX = width * .885f;
  const float frenoX = acelX - (shoulderW * pedal * (.6f + .74f) * .5f + unit * .35f);
  const float frenoGrandeX = acelX - (shoulderW * tamPedal * (.6f + .74f) * .5f + unit * .35f);
  rightTrigger->porDefecto(ACELERACION, acelX, pie - shoulderW * tamPedal * 1.15f * .5f, tamPedal);
  leftTrigger->porDefecto(ACELERACION, frenoGrandeX, pie - shoulderW * tamPedal * .9f * .5f, tamPedal);
  const float rombo = unit * .82f;
  const float romboX = width * .858f;
  const float romboY = height * .44f;
  y->porDefecto(ACELERACION, romboX, romboY - rombo, 1.f);
  const float filaRejilla = height * .15f;
  const float camaraX = width * .13f;
  rb->porDefecto(ACELERACION, camaraX, filaRejilla, 1.f);
  lb->porDefecto(ACELERACION, camaraX, filaRejilla + unit, 1.f);
  posiciones->porDefecto(ACELERACION, camaraX - shoulderW * .5f - unit * .3f - ladoPosiciones * .5f, filaRejilla, 1.f);
  leftStick->porDefecto(ACELERACION, width * .155f, height * .57f, 1.f);
  dpad->porDefecto(ACELERACION, width * .155f, height * .82f, 1.f);
  const float filaArriba = std::max(dp(18), height * .035f) + unit * .23f;
  back->porDefecto(ACELERACION, width * .41f, filaArriba, 1.f);
  start->porDefecto(ACELERACION, width * .622f, filaArriba, 1.f);

  // --- La de fabrica conduciendo.
  for (Control* c : std::initializer_list<Control*>{rightTrigger, leftTrigger, y, rb, lb, back, start}) {
    c->porDefecto(CONDUCIENDO, c->fx[ACELERACION], c->fy[ACELERACION], c->fs[ACELERACION]);
  }
  const float pedalesIzq = frenoX - shoulderW * pedal * .74f * .5f;
  const float pedalesDer = acelX + shoulderW * pedal * .6f * .5f;
  const float stickIzqX = width - (pedalesIzq + pedalesDer) * .5f;
  leftStick->porDefecto(CONDUCIENDO, stickIzqX, pie - stickRadius, 1.5f);
  dpad->porDefecto(CONDUCIENDO, width - romboX, romboY, 1.f);

  // --- Igual en las dos.
  const float escalaFlecha = 1.5f;
  const float sobrePieFlechas = pie - ladoFlecha * escalaFlecha * .5f;
  for (int d : {ACELERACION, CONDUCIENDO}) {
    carrilIzq->porDefecto(d, stickIzqX - unit * 1.1f, sobrePieFlechas, escalaFlecha);
    carrilDer->porDefecto(d, stickIzqX + unit * 1.1f, sobrePieFlechas, escalaFlecha);
    rightStick->porDefecto(d, stickIzqX + unit * 3.58f, pie - unit * .31f, 1.f);
    x->porDefecto(d, stickIzqX + unit * 1.88f, pie - unit * 3.51f, 1.f);
    a->porDefecto(d, stickIzqX + unit * 3.28f, pie - unit * 3.13f, 1.f);
    b->porDefecto(d, stickIzqX + unit * 4.12f, pie - unit * 2.08f, 1.f);
  }
  if (inclinar) {
    dpad->porDefecto(CONDUCIENDO, width - romboX, pie - unit, 1.f);
    rightStick->porDefecto(CONDUCIENDO, stickIzqX + unit * 2.88f, pie - unit * .31f, 1.f);
    x->porDefecto(CONDUCIENDO, stickIzqX + unit * 1.06f, pie - unit * 3.52f, 1.f);
    a->porDefecto(CONDUCIENDO, stickIzqX + unit * 2.34f, pie - unit * 2.81f, 1.f);
    b->porDefecto(CONDUCIENDO, stickIzqX + unit * 3.38f, pie - unit * 1.95f, 1.f);
  }

  for (auto& c : controls) cargar(c.get());
  usarDisposicion(disposicion());
  colocarBotones(width, height, unit);
  sendState();
  invalidate();
}

void Tactil::cargar(Control* c) {
  for (int d = 0; d < DISPOSICIONES; ++d) {
    const std::string k = clave(c, d);
    if (!Hay(PREFS, k + "_x")) continue;
    c->px[d] = LeerFloat(PREFS, k + "_x", 0) * lastWidth;
    c->py[d] = LeerFloat(PREFS, k + "_y", 0) * lastHeight;
    c->ps[d] = LeerFloat(PREFS, k + "_s", 1);
    c->propia[d] = true;
  }
}

std::string Tactil::clave(Control* c, int d) const {
  const std::string prefijo = inclinar && d == CONDUCIENDO ? "ci_" : PREFIJOS[d];
  return prefijo + c->id;
}

void Tactil::usarDisposicion(int d) {
  for (auto& c : controls) c->usar(d);
  invalidate();
}

// La imagen del juego en las coordenadas de los controles: la pantalla entera si se estira;
// si no, el 16:9 mas grande que cabe, centrado (lo que deja el presentador con sus bandas).
void Tactil::calcularMarco() {
  const float width = float(pantalla_.size.width), height = float(pantalla_.size.height);
  float w = width, h = height;
  if (!estirar) {
    if (width * 9 > height * 16) {
      w = height * 16 / 9.f;
    } else {
      h = width * 9 / 16.f;
    }
  }
  const float x0 = float(pantalla_.origin.x), y0 = float(pantalla_.origin.y);
  imagenJuego.set(x0 + (width - w) * .5f, y0 + (height - h) * .5f, x0 + (width + w) * .5f, y0 + (height + h) * .5f);
}

NSString* Tactil::textoToggle() const {
  if (!controlsVisible) return textos_.mostrar;
  if (menusConDedo()) return mandoEnMenus ? textos_.gestos : textos_.mando;
  return textos_.ocultar;
}

float Tactil::medir(NSString* texto, float tamano) const {
  return float([texto sizeWithAttributes:@{NSFontAttributeName : [UIFont boldSystemFontOfSize:tamano]}].width);
}

void Tactil::colocarBotones(float width, float height, float unit) {
  const float toggleWidth = std::max(controlsVisible ? unit * 1.05f : unit * 1.35f, medir(textoToggle(), dp(11)) + dp(24));
  const float toggleTop = std::max(dp(18), height * .035f);
  toggle.set(width * .5f - toggleWidth * .5f, toggleTop, width * .5f + toggleWidth * .5f, toggleTop + unit * .46f);
  engranaje.set(toggle.right + dp(8), toggle.top, toggle.right + dp(8) + toggle.height(), toggle.bottom);

  const float t = dp(12);
  const float lado = unit * .6f;
  const float hueco = dp(10);
  const float anchoTam = medir([textos_.tamano stringByAppendingString:@" 200 %"], t) + dp(16);
  const float anchoOpa = medir([textos_.opacidad stringByAppendingString:@" 100 %"], t) + dp(16);
  const float fila1 = lado * 4 + anchoTam + anchoOpa + hueco * 7;
  float x = width * .5f - fila1 * .5f;
  float y = height * .18f;
  tamMenos.set(x, y, x + lado, y + lado);
  x += lado + hueco;
  textoTamX = x + anchoTam * .5f;
  x += anchoTam + hueco;
  tamMas.set(x, y, x + lado, y + lado);
  x += lado + hueco * 3;
  opaMenos.set(x, y, x + lado, y + lado);
  x += lado + hueco;
  textoOpaX = x + anchoOpa * .5f;
  x += anchoOpa + hueco;
  opaMas.set(x, y, x + lado, y + lado);

  float anchoDis = 0;
  for (NSString* s : textos_.disp) anchoDis = std::max(anchoDis, medir(s, t));
  anchoDis += dp(28);
  const float anchoRes = medir(textos_.restaurar, t) + dp(28);
  const float anchoCan = medir(textos_.cancelar, t) + dp(28);
  const float anchoLis = medir(textos_.listo, t) + dp(28);
  const float fila2 = anchoDis + anchoRes + anchoCan + anchoLis + hueco * 3;
  x = width * .5f - fila2 * .5f;
  y += lado + hueco;
  disposicionBoton.set(x, y, x + anchoDis, y + lado);
  x += anchoDis + hueco;
  restaurar.set(x, y, x + anchoRes, y + lado);
  x += anchoRes + hueco;
  cancelar.set(x, y, x + anchoCan, y + lado);
  x += anchoCan + hueco;
  listo.set(x, y, x + anchoLis, y + lado);

  tamanoAyuda = dp(12);
  float anchoAyuda = medir(textos_.ayuda, tamanoAyuda);
  if (anchoAyuda > width * .9f) {
    tamanoAyuda *= width * .9f / anchoAyuda;
    anchoAyuda = width * .9f;
  }
  y += lado + hueco;
  fondoAyuda.set(width * .5f - anchoAyuda * .5f - dp(10), y, width * .5f + anchoAyuda * .5f + dp(10),
                 y + tamanoAyuda * 2.f);
}

// ---- Dibujo -----------------------------------------------------------------------------------

void PonerRelleno(CGContextRef c, uint32_t col) {
  CGContextSetRGBFillColor(c, ((col >> 16) & 0xFF) / 255.0, ((col >> 8) & 0xFF) / 255.0, (col & 0xFF) / 255.0,
                           (col >> 24) / 255.0);
}
void PonerTrazo(CGContextRef c, uint32_t col, float grosor) {
  CGContextSetRGBStrokeColor(c, ((col >> 16) & 0xFF) / 255.0, ((col >> 8) & 0xFF) / 255.0, (col & 0xFF) / 255.0,
                             (col >> 24) / 255.0);
  CGContextSetLineWidth(c, grosor);
}
UIBezierPath* Redondeado(const Rect& r, float radio) {
  return [UIBezierPath bezierPathWithRoundedRect:r.cg() cornerRadius:std::max(0.f, std::min(radio, std::min(r.width(), r.height()) * .5f))];
}

uint32_t Tactil::conOpacidad(uint32_t color) const {
  const uint32_t alfa = uint32_t(std::lround((color >> 24) * opacidad));
  return (color & 0x00FFFFFF) | (std::min<uint32_t>(alfa, 255) << 24);
}

void Tactil::rellenarRedondeado(CGContextRef c, const Rect& r, float radio, uint32_t color) {
  PonerRelleno(c, color);
  CGContextAddPath(c, Redondeado(r, radio).CGPath);
  CGContextFillPath(c);
}

void Tactil::trazarRedondeado(CGContextRef c, const Rect& r, float radio, uint32_t color, float grosor) {
  PonerTrazo(c, color, grosor);
  CGContextAddPath(c, Redondeado(r, radio).CGPath);
  CGContextStrokePath(c);
}

void Tactil::linea(CGContextRef c, float x0, float y0, float x1, float y1, uint32_t color, float grosor) {
  PonerTrazo(c, color, grosor);
  CGContextBeginPath(c);
  CGContextMoveToPoint(c, x0, y0);
  CGContextAddLineToPoint(c, x1, y1);
  CGContextStrokePath(c);
}

void Tactil::aro(CGContextRef c, float x, float y, float r, uint32_t color, float grosor) {
  PonerTrazo(c, color, grosor);
  CGContextStrokeEllipseInRect(c, CGRectMake(x - r, y - r, r * 2, r * 2));
}

void Tactil::drawCircle(CGContextRef c, float x, float y, float radius, bool pressed) {
  PonerRelleno(c, conOpacidad(pressed ? PULSADO : RELLENO));
  CGContextFillEllipseInRect(c, CGRectMake(x - radius, y - radius, radius * 2, radius * 2));
  aro(c, x, y, radius, conOpacidad(BORDE));
}

// Centrado en (x, y), como drawLabel de Android (la linea base a media altura de la letra).
void Tactil::drawLabel(CGContextRef, NSString* text, float x, float y, float size, float alfa) {
  if (text.length == 0 || size < 1) return;
  UIFont* fuente = [UIFont boldSystemFontOfSize:size];
  NSDictionary* atributos = @{
    NSFontAttributeName : fuente,
    NSForegroundColorAttributeName : [UIColor colorWithWhite:1 alpha:limitar(alfa, 0, 1)],
  };
  const CGSize medida = [text sizeWithAttributes:atributos];
  const CGFloat linea_base = y + (fuente.ascender + fuente.descender) / 2;
  [text drawAtPoint:CGPointMake(x - medida.width / 2, linea_base - fuente.ascender) withAttributes:atributos];
}

void Tactil::dibujar(CGContextRef c) {
  if (editando) {
    for (auto& control : controls) {
      if (!control->oculto()) control->draw(c);
    }
    if (elegido) {
      Rect r;
      elegido->ocupa(r);
      r.inset(-dp(6), -dp(6));
      PonerTrazo(c, ACENTO, dp(3));
      CGContextStrokeRect(c, r.cg());
    }
    drawEditor(c);
    return;
  }
  if (toggleVisible) {
    drawBoton(c, toggle, textoToggle(), dp(11));
    drawEngranaje(c);
  }
  if (salirALaVista()) drawSalir(c);
  if (!controlsVisible) return;
  // Con el dedo en los menus no se dibuja nada: se toca el juego.
  if (conDedo()) return;
  for (auto& control : controls) {
    if (!control->oculto()) control->draw(c);
  }
}

void Tactil::drawEngranaje(CGContextRef c) {
  drawBoton(c, engranaje, @"", dp(11));
  const float x = engranaje.centerX();
  const float y = engranaje.centerY();
  const float r = engranaje.height() * .3f;
  for (int i = 0; i < 8; ++i) {
    const double a = M_PI / 4 * i;
    const float co = float(std::cos(a)), s = float(std::sin(a));
    linea(c, x + co * r * .7f, y + s * r * .7f, x + co * r * 1.15f, y + s * r * 1.15f, BLANCO, r * .45f);
  }
  aro(c, x, y, r * .72f, BLANCO, r * .35f);
}

void Tactil::drawEditor(CGContextRef c) {
  const float t = dp(12);
  drawBoton(c, tamMenos, @"−", dp(18));
  drawBoton(c, tamMas, @"+", dp(18));
  drawBoton(c, opaMenos, @"−", dp(18));
  drawBoton(c, opaMas, @"+", dp(18));
  NSString* tam = elegido ? [NSString stringWithFormat:@"%@ %ld %%", textos_.tamano, std::lround(elegido->escala * 100)]
                          : textos_.tamano;
  drawTexto(c, tam, textoTamX, tamMenos.centerY(), t);
  drawTexto(c, [NSString stringWithFormat:@"%@ %ld %%", textos_.opacidad, std::lround(opacidad * 100)], textoOpaX,
            opaMenos.centerY(), t);
  drawBoton(c, disposicionBoton, textos_.disp[dispEditada], t);
  drawBoton(c, restaurar, textos_.restaurar, t);
  drawBoton(c, cancelar, textos_.cancelar, t);
  drawBoton(c, listo, textos_.listo, t);
  rellenarRedondeado(c, fondoAyuda, dp(8), FONDO_BOTON);
  drawLabel(c, textos_.ayuda, fondoAyuda.centerX(), fondoAyuda.centerY(), tamanoAyuda, 1.f);
}

void Tactil::drawBoton(CGContextRef c, const Rect& r, NSString* texto, float tamano) {
  rellenarRedondeado(c, r, dp(9), FONDO_BOTON);
  trazarRedondeado(c, r, dp(9), BORDE_TOGGLE);
  drawLabel(c, texto, r.centerX(), r.centerY(), tamano, 1.f);
}

void Tactil::drawTexto(CGContextRef c, NSString* texto, float x, float y, float tamano) {
  const float mitad = medir(texto, tamano) * .5f + dp(8);
  Rect r;
  r.set(x - mitad, y - tamano, x + mitad, y + tamano);
  rellenarRedondeado(c, r, dp(8), FONDO_BOTON);
  drawLabel(c, texto, x, y, tamano, 1.f);
}

void Tactil::dibujarPedal(CGContextRef c, const Rect& r, bool freno, bool pisadoAhora) {
  Rect pisado = r;
  if (pisadoAhora) pisado.inset(r.width() * .05f, r.height() * .05f);
  const float radio = pisado.width() * (freno ? .18f : .28f);
  rellenarRedondeado(c, pisado, radio, conOpacidad(pisadoAhora ? PULSADO : RELLENO));
  trazarRedondeado(c, pisado, radio, conOpacidad(BORDE));
  const int estrias = freno ? 4 : 6;
  const float margen = pisado.width() * .2f;
  const float paso = pisado.height() / float(estrias + 1);
  for (int i = 1; i <= estrias; ++i) {
    const float y = pisado.top + paso * i;
    linea(c, pisado.left + margen, y, pisado.right - margen, y, conOpacidad(BORDE));
  }
}

void Tactil::dibujarCamara(CGContextRef c, const Rect& caja) {
  const float alto = caja.height() * .5f;
  const float ancho = alto * 1.45f;
  const float x = caja.centerX();
  const float y = caja.centerY() + alto * .06f;
  const uint32_t col = conOpacidad(BORDE);
  Rect trazo;
  trazo.set(x - ancho * .16f, y - alto * .62f, x + ancho * .16f, y - alto * .4f);
  trazarRedondeado(c, trazo, dp(2), col);
  trazo.set(x - ancho * .5f, y - alto * .4f, x + ancho * .5f, y + alto * .5f);
  trazarRedondeado(c, trazo, alto * .15f, col);
  aro(c, x, y + alto * .05f, alto * .26f, col);
}

void Tactil::dibujarPosiciones(CGContextRef c, const Rect& caja) {
  const float alto = caja.height() * .56f;
  const float paso = alto / 3.f;
  const float x = caja.centerX() - alto * .5f;
  for (int i = 0; i < 3; ++i) {
    const float y = caja.centerY() - alto * .5f + paso * (i + .5f);
    drawLabel(c, @(PUESTOS[i]), x, y, paso * .85f, opacidad);
    linea(c, x + alto * .28f, y, x + alto * 1.05f, y, conOpacidad(BORDE));
  }
}

void Tactil::dibujarRetrovisor(CGContextRef c, const Rect& caja) {
  const float alto = caja.height() * .42f;
  const float ancho = std::min(caja.width() * .7f, alto * 2.6f);
  const float x = caja.centerX();
  const float y = caja.centerY() + alto * .2f;
  const uint32_t col = conOpacidad(BORDE);
  linea(c, x, y - alto * .5f, x, y - alto * 1.05f, col);
  Rect trazo;
  trazo.set(x - ancho * .5f, y - alto * .5f, x + ancho * .5f, y + alto * .5f);
  trazarRedondeado(c, trazo, alto * .5f, col);
  linea(c, x - ancho * .25f, y + alto * .2f, x - ancho * .05f, y - alto * .2f, col);
  linea(c, x - ancho * .05f, y + alto * .2f, x + ancho * .15f, y - alto * .2f, col);
}

void Tactil::dibujarNitro(CGContextRef c, float x, float y, float r) {
  const uint32_t col = conOpacidad(BORDE);
  const float ancho = r * .46f;
  const float arriba = y - r * .2f;
  Rect trazo;
  trazo.set(x - ancho * .5f, arriba, x + ancho * .5f, y + r * .52f);
  trazarRedondeado(c, trazo, ancho * .3f, col);
  trazo.set(x - r * .1f, y - r * .4f, x + r * .1f, arriba);
  PonerTrazo(c, col, 2);
  CGContextStrokeRect(c, trazo.cg());
  linea(c, x - r * .2f, y - r * .48f, x + r * .2f, y - r * .48f, col);
  linea(c, x - ancho * .5f, y + r * .12f, x + ancho * .5f, y + r * .12f, col);
}

void Tactil::dibujarFrenoMano(CGContextRef c, float x, float y, float r) {
  const uint32_t col = conOpacidad(BORDE);
  const float anillo = r * .34f;
  aro(c, x, y, anillo, col);
  drawLabel(c, @"P", x, y, anillo * 1.25f, opacidad);
  const float fuera = r * .52f;
  // drawArc de Android: grados en el sentido de las agujas, desde la derecha.
  for (float inicio : {140.f, -40.f}) {
    UIBezierPath* p = [UIBezierPath bezierPathWithArcCenter:CGPointMake(x, y)
                                                     radius:fuera
                                                 startAngle:inicio * M_PI / 180
                                                   endAngle:(inicio + 80) * M_PI / 180
                                                  clockwise:YES];
    PonerTrazo(c, col, 2);
    CGContextAddPath(c, p.CGPath);
    CGContextStrokePath(c);
  }
}

void Tactil::dibujarReloj(CGContextRef c, float x, float y, float r) {
  const uint32_t col = conOpacidad(BORDE);
  const float esfera = r * .44f;
  const float cy = y + r * .05f;
  aro(c, x, cy, esfera, col);
  linea(c, x, cy, x, cy - esfera * .65f, col);
  linea(c, x, cy, x + esfera * .5f, cy, col);
  linea(c, x, cy - esfera, x, cy - esfera - r * .12f, col);
  linea(c, x - r * .1f, cy - esfera - r * .12f, x + r * .1f, cy - esfera - r * .12f, col);
}

// ---- Segun la parte del juego -----------------------------------------------------------------

void Tactil::mirarContexto() {
  const int contexto = NfsmwTactilContexto();
  const bool pausa = contexto == CONTEXTO_PAUSA;
  const bool cinematica = contexto == CONTEXTO_CINEMATICA;
  const int ahora = cinematica ? dispJuego
                    : contexto == CONTEXTO_ACELERACION ? ACELERACION
                    : contexto == CONTEXTO_CONDUCIENDO || pausa ? CONDUCIENDO
                                                                : NORMAL;
  if (pausa != enPausa) {
    enPausa = pausa;
    sendState();
  }
  const bool menu = contexto == CONTEXTO_MENUS || pausa || cinematica;
  if (menu != enMenu || cinematica != enCinematica) {
    enMenu = menu;
    enCinematica = cinematica;
    cancelarMenu();
    rehacer();
  }
  if (ahora != dispJuego) {
    dispJuego = ahora;
    clearInput();
    if (!editando) usarDisposicion(dispJuego);
  }
  mirarSalir(contexto == CONTEXTO_MENUS);
  anotarDiagnostico();
}

// Documents/logs/contexto.txt, una linea por cambio (files/logs/contexto.txt en Android).
void Tactil::anotarDiagnostico() {
  char linea[256];
  NfsmwTactilContextoDiagnostico(linea, sizeof(linea));
  if (hayDiagnostico && ultimoDiagnostico == linea) return;
  const bool primera = !hayDiagnostico;
  hayDiagnostico = true;
  ultimoDiagnostico = linea;
  const std::string ruta = CarpetaLogs() + "/contexto.txt";
  if (FILE* f = std::fopen(ruta.c_str(), primera ? "w" : "a")) {
    std::fprintf(f, "%s ctx=%s%s %s\n", Hora().c_str(), NOMBRES_DISP[dispJuego], enPausa ? "+pausa" : "", linea);
    std::fclose(f);
  }
}

// ---- Salir del juego desde el menu principal ---------------------------------------------------

// Cada CONTEXTO_MS: si se esta en el menu principal (y donde va el boton), y si se ha pulsado
// BACK en el mando fisico desde la ultima vez.
void Tactil::mirarSalir(bool enMenus) {
  const bool visible = !editando && enMenus && NfsmwTactilMenuPrincipal(menuPrincipal);
  const int back = int(NfsmwTactilBackPulsados());
  // Un BACK de antes de llegar al menu principal no cuenta.
  const bool pulsoBack = visible && salirVisible && backVistos >= 0 && back > backVistos;
  backVistos = back;
  bool cambio = visible != salirVisible;
  if (cambio) {
    salirVisible = visible;
    salirConfirmarHasta = 0;
    Cancelar(finConfirmarSalir_);
  }
  if (visible) {
    const float izquierda = salirCaja.left, arriba = salirCaja.top;
    colocarSalir();
    cambio |= izquierda != salirCaja.left || arriba != salirCaja.top;
    if (!salirAnotado) {
      salirAnotado = true;
      anotarSalir();
    }
  }
  if (cambio) invalidate();
  if (pulsoBack) pulsarSalir();
}

bool Tactil::tocaSalir(float x, float y) const {
  const float m = dp(8);
  return x >= salirCaja.left - m && x <= salirCaja.right + m && y >= salirCaja.top - m && y <= salirCaja.bottom + m;
}

// La primera vez pide confirmar; la segunda, antes de SALIR_CONFIRMAR_MS, sale.
void Tactil::pulsarSalir() {
  if (!salirALaVista()) return;
  const double ahora = ahoraMs();
  if (salirConfirmarHasta > ahora) {
    salir();
    return;
  }
  salirConfirmarHasta = ahora + SALIR_CONFIRMAR_MS;
  Programar(finConfirmarSalir_, SALIR_CONFIRMAR_MS, [this] {
    salirConfirmarHasta = 0;
    colocarSalir();
    invalidate();
  });
  colocarSalir();
  invalidate();
}

// iOS no deja volver al lanzador sin cerrar la app (el motor no se puede apagar y arrancar otra
// vez en el mismo proceso): se cierra, como killProcess en GameActivity.onDestroy, y al abrirla
// otra vez sale el lanzador. _exit y no exit: los destructores estaticos, con los hilos del juego
// todavia en marcha, pueden colgarse o fallar; los logs se vacian antes.
void Tactil::salir() {
  salirVisible = false;
  Cancelar(finConfirmarSalir_);
  clearInput();
  os_log(OS_LOG_DEFAULT, "[tactil] salir desde el menu principal");
  std::fflush(nullptr);
  _exit(0);
}

NSDictionary* Tactil::atributosSalir() const {
  // Mas grueso, como FILL_AND_STROKE en Android: la del juego es negrita. Negativo es rellenar y
  // trazar; va en tanto por ciento del tamano de la letra.
  return @{
    NSFontAttributeName : salirFuente,
    NSForegroundColorAttributeName : [UIColor colorWithRed:((SALIR_CREMA >> 16) & 0xFF) / 255.0
                                                     green:((SALIR_CREMA >> 8) & 0xFF) / 255.0
                                                      blue:(SALIR_CREMA & 0xFF) / 255.0
                                                     alpha:1],
    NSStrokeWidthAttributeName : @(-100.0 * salirGrosor / std::max<CGFloat>(salirFuente.pointSize, 1)),
  };
}

// Las medidas del boton desde lo que da el juego: su borde izquierdo y su centro sobre la
// imagen del juego, y lo que mide una unidad del menu en cada eje.
void Tactil::colocarSalir() {
  if (imagenJuego.width() <= 0 || imagenJuego.height() <= 0) return;
  salirUx = menuPrincipal[2] * imagenJuego.width();
  salirUy = menuPrincipal[3] * imagenJuego.height();
  const float x0 = imagenJuego.left + menuPrincipal[0] * imagenJuego.width();
  const float cy = imagenJuego.top + menuPrincipal[1] * imagenJuego.height();
  UIFont* cien = FuenteSalir(100);
  if (cien.capHeight <= 0) return;
  salirFuente = FuenteSalir(100 * SALIR_MAYUSCULA * salirUy / cien.capHeight);
  const float aceptar = float([@"Aceptar" sizeWithAttributes:@{NSFontAttributeName : salirFuente}].width);
  salirEscalaX = aceptar > 0 ? SALIR_ANCHO_ACEPTAR * salirUx / aceptar : 1;
  salirGrosor = salirUy * .9f;
  const float texto = float([textoSalirActual() sizeWithAttributes:@{NSFontAttributeName : salirFuente}].width);
  const float ancho = (SALIR_MARGEN + 2 * SALIR_RADIO + SALIR_HUECO + SALIR_COLA) * salirUx + texto * salirEscalaX;
  const float alto = SALIR_ALTO * salirUy;
  salirCaja.set(x0, cy - alto * .5f, x0 + ancho, cy + alto * .5f);
}

// Como las indicaciones del juego: barra oscura con rayas, circulo con el boton y texto.
void Tactil::drawSalir(CGContextRef c) {
  const Rect& r = salirCaja;
  if (r.width() <= 0 || r.height() <= 0 || !salirFuente) return;
  PonerRelleno(c, SALIR_FONDO);
  CGContextFillRect(c, r.cg());
  CGContextSaveGState(c);
  CGContextClipToRect(c, r.cg());
  PonerRelleno(c, SALIR_RAYA);
  const float paso = SALIR_RAYA_PASO * salirUx;
  const float raya = SALIR_RAYA_ANCHO * salirUx;
  const float inclinacion = r.height();
  if (paso > 0) {
    for (float x = r.left - inclinacion; x < r.right; x += paso) {
      CGContextBeginPath(c);
      CGContextMoveToPoint(c, x, r.bottom);
      CGContextAddLineToPoint(c, x + raya, r.bottom);
      CGContextAddLineToPoint(c, x + raya + inclinacion, r.top);
      CGContextAddLineToPoint(c, x + inclinacion, r.top);
      CGContextClosePath(c);
      CGContextFillPath(c);
    }
  }
  CGContextRestoreGState(c);
  // El icono: el boton BACK del mando, un circulo con una flecha a la izquierda.
  const float cx = r.left + (SALIR_MARGEN + SALIR_RADIO) * salirUx;
  const float cy = r.centerY();
  const float rx = SALIR_RADIO * salirUx;
  const float ry = SALIR_RADIO * salirUy;
  const float t = SALIR_TRAZO * salirUy * .5f;
  PonerTrazo(c, SALIR_CREMA, SALIR_TRAZO * salirUy);
  CGContextStrokeEllipseInRect(c, CGRectMake(cx - rx + t, cy - ry + t, (rx - t) * 2, (ry - t) * 2));
  PonerRelleno(c, SALIR_CREMA);
  const float fx = rx * .45f, fy = ry * .45f;
  CGContextBeginPath(c);
  CGContextMoveToPoint(c, cx - fx, cy);
  CGContextAddLineToPoint(c, cx + fx * .75f, cy - fy);
  CGContextAddLineToPoint(c, cx + fx * .75f, cy + fy);
  CGContextClosePath(c);
  CGContextFillPath(c);
  // El texto, con las mayusculas centradas en la barra y estirado a lo ancho.
  const float base = cy + SALIR_MAYUSCULA * salirUy * .5f;
  CGContextSaveGState(c);
  CGContextTranslateCTM(c, cx + rx + SALIR_HUECO * salirUx, base);
  CGContextScaleCTM(c, salirEscalaX, 1);
  [textoSalirActual() drawAtPoint:CGPointMake(0, -salirFuente.ascender) withAttributes:atributosSalir()];
  CGContextRestoreGState(c);
}

// Diagnostico: donde se puso el boton, en Documents/logs/menus.txt (una vez por partida).
void Tactil::anotarSalir() {
  const std::string ruta = CarpetaLogs() + "/menus.txt";
  struct stat st {};
  const bool seguir = menusAnotados && stat(ruta.c_str(), &st) == 0 && st.st_size < MAX_DIAGNOSTICO_MENUS;
  menusAnotados = true;
  if (FILE* f = std::fopen(ruta.c_str(), seguir ? "a" : "w")) {
    std::fprintf(f, "%s menu principal: salir u=%.4f v=%.4f unidad=%.5f/%.5f marco=%.0fx%.0f caja=[%.0f,%.0f][%.0f,%.0f]\n",
                 Hora().c_str(), menuPrincipal[0], menuPrincipal[1], menuPrincipal[2], menuPrincipal[3],
                 imagenJuego.width(), imagenJuego.height(), salirCaja.left, salirCaja.top, salirCaja.right,
                 salirCaja.bottom);
    std::fclose(f);
  }
}

// ---- Tocar ------------------------------------------------------------------------------------

void Tactil::tocar(const Evento& event) {
  if (editando) {
    tocarEditor(event);
    return;
  }
  const Accion action = event.accion;
  const int index = event.indice;
  const int pointerId = action == ACTION_MOVE || action == ACTION_CANCEL ? -1 : event.getPointerId(index);
  const float x = action == ACTION_CANCEL ? 0 : event.getX(index);
  const float y = action == ACTION_CANCEL ? 0 : event.getY(index);
  const bool bajando = action == ACTION_DOWN || action == ACTION_POINTER_DOWN;
  if (!bajando && conDedo()) {
    tocarMenu(event);
    return;
  }

  if (bajando) {
    if (toggleVisible && engranaje.contains(x, y)) {
      abrirEditor();
      return;
    }
    const bool enToggle = toggle.contains(x, y) || engranaje.contains(x, y);
    if (toggleVisible && toggle.contains(x, y)) {
      if (controlsVisible && menusConDedo()) {
        cambiarMandoEnMenus();
      } else {
        setControlsVisible(!controlsVisible);
      }
      return;
    }
    if (salirTocable() && tocaSalir(x, y)) {
      pulsarSalir();
      return;
    }
    if (enToggle || (mandoFisico && !controlsVisible)) {
      mostrarToggle();
      return;
    }
    if (!controlsVisible) return;
    if (conDedo()) {
      tocarMenu(event);
      return;
    }
    for (int i = int(controls.size()) - 1; i >= 0; --i) {
      Control* control = controls[size_t(i)].get();
      if (!control->inUse && !control->oculto() && control->contains(x, y)) {
        pointers[pointerId] = control;
        control->press(x, y);
        sendState();
        invalidate();
        return;
      }
    }
    return;
  }

  if (action == ACTION_MOVE) {
    bool handled = false;
    for (int i = 0; i < event.getPointerCount(); ++i) {
      auto it = pointers.find(event.getPointerId(i));
      if (it != pointers.end()) {
        it->second->move(event.getX(i), event.getY(i));
        handled = true;
      }
    }
    if (handled) {
      sendState();
      invalidate();
    }
    return;
  }

  if (action == ACTION_UP || action == ACTION_POINTER_UP) {
    auto it = pointers.find(pointerId);
    if (it == pointers.end()) return;
    it->second->release();
    pointers.erase(it);
    sendState();
    invalidate();
    return;
  }

  if (action == ACTION_CANCEL) {
    clearInput();
  }
}

// ---- Los menus con el dedo --------------------------------------------------------------------

bool Tactil::tocarMenu(const Evento& e) {
  if (enCinematica) return tocarCinematica(e);
  const int i = e.indice;
  switch (e.accion) {
    case ACTION_DOWN:
      pararRumbo();
      gatilloDedo = gatilloBajo(e.getX(i), e.getY(i));
      if (gatilloDedo != 0) {
        mantenido |= gatilloDedo;
        sendState();
      }
      dedoMenu = e.getPointerId(i);
      menuX0 = e.getX(i);
      menuY0 = e.getY(i);
      menuT0 = e.tiempo;
      ejeMenu = SIN_EJE;
      contadoMenu = 0;
      pasosMenu = 0;
      dosDedos = false;
      arrastrar = false;
      return true;
    case ACTION_POINTER_DOWN:
      if (dedoMenu != -1 && ejeMenu == SIN_EJE && gatilloDedo == 0 && e.getPointerCount() >= 2) {
        dosDedos = true;
        pellizco = false;
        distanciaPellizco = distancia(e);
      }
      return true;
    case ACTION_MOVE: {
      if (dosDedos) {
        pellizcar(e);
        return true;
      }
      const int j = dedoMenu == -1 ? -1 : e.findPointerIndex(dedoMenu);
      if (j < 0 || gatilloDedo != 0) return true;
      const float dx = e.getX(j) - menuX0;
      const float dy = e.getY(j) - menuY0;
      if (ejeMenu == SIN_EJE) {
        if (std::hypot(dx, dy) < dp(MOVER_DP)) return true;
        ejeMenu = std::fabs(dx) >= std::fabs(dy) ? EJE_X : EJE_Y;
        deslizarDirecto = ejeMenu == EJE_X && NfsmwTactilMenuEsLista();
        arrastrar = NfsmwTactilMenuRotar();
        arrastreX = e.getX(j);
        arrastreY = e.getY(j);
        arrastreT = e.tiempo;
      }
      if (arrastrar) {
        arrastrarStick(e.getX(j), e.getY(j), e.tiempo);
        return true;
      }
      float d = (ejeMenu == EJE_X ? dx : dy) - contadoMenu;
      float paso = pasosMenu == 0 ? dp(PRIMER_PASO_DP) : pasoDeslizar();
      while (std::fabs(d) >= paso) {
        const float s = signum(d);
        darPaso(s > 0);
        contadoMenu += s * paso;
        d -= s * paso;
        paso = pasoDeslizar();
      }
      return true;
    }
    case ACTION_UP:
    case ACTION_POINTER_UP: {
      if (dosDedos && e.accion == ACTION_POINTER_UP) {
        soltarGatillo(true);
      }
      if (e.getPointerId(i) != dedoMenu) return true;
      dedoMenu = -1;
      const double dura = e.tiempo - menuT0;
      if (arrastrar) {
        arrastrar = false;
        Programar(soltarArrastre_, FRENADA_MS, [this] { soltarArrastre(); });
      } else if (gatilloDedo != 0) {
        soltarGatillo(false);
      } else if (dosDedos) {
        soltarGatillo(true);
        if (!pellizco && dura <= DOS_DEDOS_MS) pulsar(B, TRAS_NADA);
      } else if (ejeMenu == SIN_EJE) {
        if (dura <= TOQUE_MS) tocarEn(menuX0, menuY0);
      } else if (pasosMenu == 0) {
        if (ejeMenu == EJE_X) deslizarDirecto = NfsmwTactilMenuEsLista();
        const float d = ejeMenu == EJE_X ? e.getX(i) - menuX0 : e.getY(i) - menuY0;
        if (dura <= TOQUE_MS) darPaso(d > 0);
      }
      return true;
    }
    case ACTION_CANCEL:
      dedoMenu = -1;
      soltarGatillo(false);
      soltarGatillo(true);
      arrastrar = false;
      soltarStick();
      return true;
  }
  return true;
}

bool Tactil::tocarCinematica(const Evento& e) {
  if ((e.accion == ACTION_UP || e.accion == ACTION_POINTER_UP) && !pulsando && pulsos.empty()) {
    pulsar(A, TRAS_NADA);
  }
  return true;
}

int Tactil::gatilloBajo(float x, float y) {
  if (!imagenJuego.contains(x, y)) return 0;
  const int r = NfsmwTactilMenuTocar((x - imagenJuego.left) / imagenJuego.width(),
                                     (y - imagenJuego.top) / imagenJuego.height());
  if (r < 0 || (r & MENU_TIPO) != MENU_INDICACION) return 0;
  const int mascara = 1 << (r & 0xFF);
  return mascara == PULSO_LT || mascara == PULSO_RT ? mascara : 0;
}

void Tactil::arrastrarStick(float x, float y, double t) {
  const double dt = t - arrastreT;
  if (dt <= 0) return;
  const float lleno = dp(VELOCIDAD_STICK_DP);
  const float vx = curvaStick(float((x - arrastreX) / dt / lleno));
  const float vy = curvaStick(float((y - arrastreY) / dt / lleno));
  arrastreX = x;
  arrastreY = y;
  arrastreT = t;
  const float s = ROTAR_CON_EL_DEDO ? -1.f : 1.f;
  stickArrastreX = stickArrastreX * .6f + s * vx * .4f;
  stickArrastreY = stickArrastreY * .6f + s * vy * .4f;
  sendState();
  Programar(soltarArrastre_, ARRASTRE_QUIETO_MS, [this] { soltarArrastre(); });
}

float Tactil::curvaStick(float v) {
  const float a = std::fabs(v);
  if (a < .02f) return 0;
  return signum(v) * std::min(1.f, STICK_MINIMO + (1.f - STICK_MINIMO) * a);
}

void Tactil::soltarArrastre() {
  stickArrastreX *= FRENADA;
  stickArrastreY *= FRENADA;
  if (std::fabs(stickArrastreX) < .05f && std::fabs(stickArrastreY) < .05f) {
    soltarStick();
    return;
  }
  sendState();
  Programar(soltarArrastre_, FRENADA_MS, [this] { soltarArrastre(); });
}

void Tactil::soltarStick() {
  Cancelar(soltarArrastre_);
  if (stickArrastreX != 0 || stickArrastreY != 0) {
    stickArrastreX = 0;
    stickArrastreY = 0;
    sendState();
  }
}

float Tactil::distancia(const Evento& e) {
  return std::hypot(e.getX(0) - e.getX(1), e.getY(0) - e.getY(1));
}

void Tactil::pellizcar(const Evento& e) {
  if (e.getPointerCount() < 2) return;
  const float d = distancia(e);
  const float cambio = d - distanciaPellizco;
  if (std::fabs(cambio) < dp(pellizco ? PELLIZCO_PASO_DP : PELLIZCO_DP)) return;
  pellizco = true;
  distanciaPellizco = d;
  const int gatillo = cambio > 0 ? PULSO_RT : PULSO_LT;
  if (gatillo != gatilloPellizco) {
    mantenido = (mantenido & ~gatilloPellizco) | gatillo;
    gatilloPellizco = gatillo;
    sendState();
  }
  Programar(soltarPellizco_, ZOOM_QUIETO_MS, [this] { soltarGatillo(true); });
}

void Tactil::soltarGatillo(bool delPellizco) {
  const int gatillo = delPellizco ? gatilloPellizco : gatilloDedo;
  if (delPellizco) {
    Cancelar(soltarPellizco_);
    gatilloPellizco = 0;
  } else {
    gatilloDedo = 0;
  }
  if (gatillo != 0) {
    mantenido &= ~gatillo;
    sendState();
  }
}

float Tactil::pasoDeslizar() const { return std::max(dp(PASO_DESLIZAR_DP), imagenJuego.width() * PASO_DESLIZAR_MARCO); }

void Tactil::darPaso(bool positivo) {
  const bool directo = deslizarInvertido || (ejeMenu == EJE_X && deslizarDirecto);
  if (!directo) positivo = !positivo;
  pasosMenu++;
  pulsar(ejeMenu == EJE_X ? (positivo ? DPAD_RIGHT : DPAD_LEFT) : (positivo ? DPAD_DOWN : DPAD_UP), TRAS_NADA);
}

void Tactil::tocarEn(float x, float y) {
  if (!imagenJuego.contains(x, y)) return;
  const float u = (x - imagenJuego.left) / imagenJuego.width();
  const float v = (y - imagenJuego.top) / imagenJuego.height();
  const int r = NfsmwTactilMenuTocar(u, v);
  const int tipo = r < 0 ? r : r & MENU_TIPO;
  if (tipo == MENU_INDICACION) {
    pulsar(1 << (r & 0xFF), TRAS_NADA);
  } else if (tipo == MENU_IR) {
    pasosRumbo = 1;
    pulsar(1 << (r & 0xFF), TRAS_RUMBO);
  } else if (tipo == MENU_ACEPTAR || r == MENU_DESCONOCIDO) {
    pulsar(A, TRAS_NADA);
  }
  anotarMenus(u, v);
}

void Tactil::seguirRumbo() {
  const int r = NfsmwTactilMenuRumbo();
  if (r == MENU_ACEPTAR) {
    pulsar(A, TRAS_NADA);
  } else if (r >= 0 && (r & MENU_TIPO) == MENU_IR && pasosRumbo < MAX_PASOS_RUMBO) {
    pasosRumbo++;
    pulsar(1 << (r & 0xFF), TRAS_RUMBO);
  }
}

void Tactil::pulsar(int mascara, int tras) {
  if (pulsos.size() >= MAX_PULSOS) return;
  pulsos.push_back({mascara, tras});
  if (!pulsando) siguientePulso();
}

void Tactil::siguientePulso() {
  const int tras = trasPulso;
  trasPulso = TRAS_NADA;
  if (tras == TRAS_RUMBO) seguirRumbo();
  if (pulsos.empty()) {
    pulsando = false;
    return;
  }
  const Pulso p = pulsos.front();
  pulsos.pop_front();
  pulsando = true;
  pulso = p.mascara;
  trasPulso = p.tras;
  sendState();
  Programar(soltarPulso_, PULSO_MS, [this] {
    pulso = 0;
    sendState();
    Programar(siguientePulso_, ENTRE_PULSOS_MS, [this] { siguientePulso(); });
  });
}

void Tactil::pararRumbo() {
  trasPulso = TRAS_NADA;
  pulsos.erase(std::remove_if(pulsos.begin(), pulsos.end(), [](const Pulso& p) { return p.tras == TRAS_RUMBO; }),
               pulsos.end());
}

void Tactil::cancelarMenu() {
  Cancelar(soltarPulso_);
  Cancelar(siguientePulso_);
  pulsos.clear();
  pulsando = false;
  trasPulso = TRAS_NADA;
  dedoMenu = -1;
  Cancelar(soltarPellizco_);
  Cancelar(soltarArrastre_);
  gatilloDedo = 0;
  gatilloPellizco = 0;
  stickArrastreX = 0;
  stickArrastreY = 0;
  arrastrar = false;
  if (pulso != 0 || mantenido != 0) {
    pulso = 0;
    mantenido = 0;
    sendState();
  }
}

void Tactil::cambiarMandoEnMenus() {
  mandoEnMenus = !mandoEnMenus;
  PonerBool(PREFS, "mando_en_menus", mandoEnMenus);
  cancelarMenu();
  rehacer();
  mostrarToggle();
}

// Documents/logs/menus.txt, con cada toque lo que se leyo del menu.
void Tactil::anotarMenus(float u, float v) {
  const std::string leido = NfsmwTactilMenuDiagnostico();
  const std::string ruta = CarpetaLogs() + "/menus.txt";
  struct stat st {};
  const bool seguir = menusAnotados && stat(ruta.c_str(), &st) == 0 && st.st_size < MAX_DIAGNOSTICO_MENUS;
  menusAnotados = true;
  if (FILE* f = std::fopen(ruta.c_str(), seguir ? "a" : "w")) {
    std::fprintf(f, "%s u=%.3f v=%.3f ctx=%s%s marco=%.0fx%.0f %s\n", Hora().c_str(), u, v, NOMBRES_DISP[dispJuego],
                 enPausa ? "+pausa" : "", imagenJuego.width(), imagenJuego.height(), leido.c_str());
    std::fclose(f);
  }
}

// ---- Editor -----------------------------------------------------------------------------------

bool Tactil::tocarEditor(const Evento& e) {
  const int index = e.indice;
  switch (e.accion) {
    case ACTION_DOWN:
    case ACTION_POINTER_DOWN: {
      const float x = e.getX(index), y = e.getY(index);
      if (dedoEditor != -1) return true;
      if (tamMenos.contains(x, y)) {
        cambiarEscala(-PASO);
      } else if (tamMas.contains(x, y)) {
        cambiarEscala(PASO);
      } else if (opaMenos.contains(x, y)) {
        cambiarOpacidad(-PASO);
      } else if (opaMas.contains(x, y)) {
        cambiarOpacidad(PASO);
      } else if (disposicionBoton.contains(x, y)) {
        cambiarDisposicionEditada();
      } else if (restaurar.contains(x, y)) {
        restaurarDisposicion();
      } else if (cancelar.contains(x, y)) {
        terminarEdicion();
      } else if (listo.contains(x, y)) {
        guardarEdicion();
      } else {
        elegido = nullptr;
        for (int i = int(controls.size()) - 1; i >= 0; --i) {
          Control* c = controls[size_t(i)].get();
          if (!c->oculto() && c->contains(x, y)) {
            elegido = c;
            break;
          }
        }
        if (elegido) {
          dedoEditor = e.getPointerId(index);
          agarreX = x - elegido->cx;
          agarreY = y - elegido->cy;
        }
        invalidate();
      }
      return true;
    }
    case ACTION_MOVE: {
      const int i = dedoEditor == -1 ? -1 : e.findPointerIndex(dedoEditor);
      if (i < 0 || !elegido) return true;
      elegido->situar(limitar(e.getX(i) - agarreX, 0, lastWidth), limitar(e.getY(i) - agarreY, 0, lastHeight));
      elegido->personalizado = true;
      invalidate();
      return true;
    }
    case ACTION_UP:
    case ACTION_POINTER_UP:
      if (e.getPointerId(index) == dedoEditor) dedoEditor = -1;
      return true;
    case ACTION_CANCEL:
      dedoEditor = -1;
      return true;
  }
  return true;
}

void Tactil::cambiarEscala(float paso) {
  if (elegido) {
    escalar(elegido, paso);
  } else {
    for (auto& c : controls) {
      if (!c->oculto()) escalar(c.get(), paso);
    }
  }
  invalidate();
}

void Tactil::escalar(Control* c, float paso) {
  c->escala = limitar(std::round((c->escala + paso) * 10.f) / 10.f, ESCALA_MIN, ESCALA_MAX);
  c->personalizado = true;
  c->colocar();
}

void Tactil::cambiarOpacidad(float paso) {
  opacidad = limitar(std::round((opacidad + paso) * 10.f) / 10.f, OPACIDAD_MIN, 1.f);
  invalidate();
}

void Tactil::restaurarDisposicion() {
  for (auto& c : controls) {
    c->propia[dispEditada] = false;
    c->usar(dispEditada);
  }
  if (dispEditada == NORMAL) opacidad = 1.f;
  elegido = nullptr;
  invalidate();
}

void Tactil::cambiarDisposicionEditada() {
  for (auto& c : controls) c->guardarEn(dispEditada);
  dispEditada = dispEditada == NORMAL ? CONDUCIENDO : dispEditada == CONDUCIENDO ? ACELERACION : NORMAL;
  elegido = nullptr;
  dedoEditor = -1;
  usarDisposicion(dispEditada);
}

// En la partida: el editor sobre el juego, que se queda quieto.
void Tactil::abrirEditor() {
  clearInput();
  NfsmwTactilPausarJuego(true);
  CallarVibracion(CALLADA_EDITOR, true);
  visiblesAntesDeEditar = controlsVisible;
  Cancelar(ocultarToggle_);
  toggleVisible = false;
  controlsVisible = true;
  editando = true;
  dispEditada = dispJuego;
  elegido = nullptr;
  dedoEditor = -1;
  rehacer();
}

// LISTO y CANCELAR: se vuelve a jugar con lo guardado.
void Tactil::terminarEdicion() {
  editando = false;
  elegido = nullptr;
  dedoEditor = -1;
  opacidad = LeerFloat(PREFS, "opacidad", 1.f);
  controlsVisible = visiblesAntesDeEditar;
  rehacer();
  mostrarToggle();
  CallarVibracion(CALLADA_EDITOR, false);
  NfsmwTactilPausarJuego(false);
}

void Tactil::guardarEdicion() {
  for (auto& c : controls) c->guardarEn(dispEditada);
  for (auto& c : controls) {
    for (int d = 0; d < DISPOSICIONES; ++d) {
      const std::string k = clave(c.get(), d);
      if (c->propia[d]) {
        PonerFloat(PREFS, k + "_x", c->px[d] / lastWidth);
        PonerFloat(PREFS, k + "_y", c->py[d] / lastHeight);
        PonerFloat(PREFS, k + "_s", c->ps[d]);
      } else {
        Quitar(PREFS, k + "_x");
        Quitar(PREFS, k + "_y");
        Quitar(PREFS, k + "_s");
      }
    }
  }
  PonerFloat(PREFS, "opacidad", opacidad);
  terminarEdicion();
}

void Tactil::setControlsVisible(bool visible) {
  controlsVisible = visible;
  if (!mandoFisico) PonerBool(PREFS, "visible", visible);
  rehacer();
  mostrarToggle();
}

void Tactil::rehacer() {
  for (auto& c : controls) c->release();
  pointers.clear();
  if (lastWidth > 0 && lastHeight > 0) {
    const float w = lastWidth, h = lastHeight;
    lastWidth = 0;  // buildControls siempre
    buildControls(w, h);
  }
  sendState();
  invalidate();
}

void Tactil::flechaCambiada() {
  if (!flechasAlConducir) return;
  Cancelar(rampaFlechas_);
  pasoFlechas = ahoraMs();
  pasoRampaFlechas();
}

// postOnAnimation: un paso por fotograma mientras siga subiendo.
void Tactil::pasoRampaFlechas() {
  if (avanzarGiroFlechas()) Programar(rampaFlechas_, 1000.0 / 60.0, [this] { pasoRampaFlechas(); });
}

bool Tactil::avanzarGiroFlechas() {
  const int hacia = disposicion() != CONDUCIENDO ? 0 : (carrilDer->inUse ? 1 : 0) - (carrilIzq->inUse ? 1 : 0);
  const double ahora = ahoraMs();
  const double dt = std::min(ahora - pasoFlechas, 50.0);
  pasoFlechas = ahora;
  const float antes = giroFlechas;
  if (hacia == 0) {
    giroFlechas = 0;
  } else if (signum(giroFlechas) != float(hacia)) {
    giroFlechas = hacia * GIRO_FLECHA_INICIAL;
  } else {
    giroFlechas = hacia * std::min(1.f, std::fabs(giroFlechas) + float(dt * (1.0 - GIRO_FLECHA_INICIAL) / GIRO_FLECHA_MS));
  }
  if (giroFlechas != antes) sendState();
  return hacia != 0 && std::fabs(giroFlechas) < 1.f;
}

bool Tactil::inclinacionActiva() const {
  return inclinar && !editando && controlsVisible && dispJuego == CONDUCIENDO && !enPausa;
}

// La inclinacion (CoreMotion). TouchControllerView: bajar el lado izquierdo, como al girar un
// volante a la izquierda, es girar a la izquierda (negativo). La X de la pantalla, en las
// coordenadas del iPhone: con la isla a la izquierda (LandscapeRight) es -Y; a la derecha, +Y.
// La gravedad de CoreMotion apunta al suelo, en g.
void Tactil::inclinacion(CMAcceleration g, UIInterfaceOrientation orientacion) {
  const float sx = float(orientacion == UIInterfaceOrientationLandscapeLeft ? g.y : -g.y);
  float giro = limitar(sx / senoGiroCompleto, -1.f, 1.f);
  giro = std::fabs(giro) < ZONA_MUERTA_GIRO ? 0.f
                                           : signum(giro) * (std::fabs(giro) - ZONA_MUERTA_GIRO) / (1.f - ZONA_MUERTA_GIRO);
  if (std::fabs(giro - giroInclinado) > .005f) {
    giroInclinado = giro;
    if (inclinacionActiva()) sendState();
  }
}

void Tactil::sendState() {
  if (editando) return;
  if (!controlsVisible) {
    backTactil = false;
    NfsmwTactilEstado(-1, 0, 0, 0, 0, 0, 0);
    return;
  }
  const int pisado = pulso | mantenido;
  int buttons = pisado & ~(PULSO_LT | PULSO_RT);
  for (auto& control : controls) buttons |= control->buttons();
  // BACK en el menu principal: el boton Salir (y el juego lo recibe igual, como el fisico).
  const bool back = (buttons & BACK) != 0;
  const bool pulsoBack = back && !backTactil;
  backTactil = back;
  if (pulsoBack && salirALaVista()) {
    dispatch_async(dispatch_get_main_queue(), ^{
      this->pulsarSalir();
    });
  }
  float lx = leftStick ? leftStick->xValue : 0;
  float ly = leftStick ? leftStick->yValue : 0;
  if (carrilIzq && disposicion() == CONDUCIENDO && flechasAlConducir) {
    lx = giroFlechas;
    ly = 0;
  } else if (disposicion() == ACELERACION && carrilIzq) {
    lx = float((carrilDer->inUse ? 1 : 0) - (carrilIzq->inUse ? 1 : 0));
    ly = 0;
  } else if (inclinacionActiva()) {
    lx = giroInclinado;
    ly = 0;
  }
  const bool arrastrando = stickArrastreX != 0 || stickArrastreY != 0;
  NfsmwTactilEstado(buttons, lx, ly, arrastrando ? stickArrastreX : (rightStick ? rightStick->xValue : 0),
                    arrastrando ? stickArrastreY : (rightStick ? rightStick->yValue : 0),
                    (leftTrigger && leftTrigger->inUse) || (pisado & PULSO_LT) ? 1.f : 0.f,
                    (rightTrigger && rightTrigger->inUse) || (pisado & PULSO_RT) ? 1.f : 0.f);
}

}  // namespace

// =============================================================================================
//  La vista
// =============================================================================================

@interface NfsmwControlesTactiles : UIView
@end

@implementation NfsmwControlesTactiles {
  std::unique_ptr<Tactil> tactil_;
  std::vector<std::pair<UITouch*, int>> dedos_;
  int siguiente_dedo_;
  CMMotionManager* movimiento_;
  NSTimer* vigilante_;
  BOOL empezado_;
  // El contador de fps (GameActivity.ponerContadorFps): los fotogramas presentados desde la vez
  // anterior, entre el tiempo que ha pasado, cada medio segundo.
  UILabel* fps_;
  NSTimer* relojFps_;
  CFTimeInterval antesFps_;
  uint64_t fotogramasAntes_;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (!self) {
    return nil;
  }
  self.opaque = NO;
  self.backgroundColor = UIColor.clearColor;
  self.multipleTouchEnabled = YES;
  self.contentMode = UIViewContentModeRedraw;
  self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  siguiente_dedo_ = 0;
  tactil_ = std::make_unique<Tactil>(self);

  NSNotificationCenter* centro = NSNotificationCenter.defaultCenter;
  [centro addObserver:self selector:@selector(mandosCambiaron) name:GCControllerDidConnectNotification object:nil];
  [centro addObserver:self selector:@selector(mandosCambiaron) name:GCControllerDidDisconnectNotification object:nil];
  // En segundo plano no vibra (GameActivity.onPause); al volver sigue lo que pidiera el juego.
  [centro addObserverForName:UIApplicationDidEnterBackgroundNotification
                      object:nil
                       queue:NSOperationQueue.mainQueue
                  usingBlock:^(NSNotification*) {
                    CallarVibracion(CALLADA_SEGUNDO_PLANO, true);
                  }];
  [centro addObserverForName:UIApplicationWillEnterForegroundNotification
                      object:nil
                       queue:NSOperationQueue.mainQueue
                  usingBlock:^(NSNotification*) {
                    CallarVibracion(CALLADA_SEGUNDO_PLANO, false);
                  }];

  if (LeerBool(AJUSTES, "mostrar_fps", true)) {
    fps_ = [UILabel new];
    fps_.font = [UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightBold];
    fps_.textColor = UIColor.whiteColor;
    fps_.backgroundColor = [UIColor colorWithWhite:0 alpha:0x99 / 255.0];
    fps_.textAlignment = NSTextAlignmentCenter;
    fps_.userInteractionEnabled = NO;
    fps_.text = @"-- fps";
    [self addSubview:fps_];
    __weak NfsmwControlesTactiles* yo = self;
    relojFps_ = [NSTimer scheduledTimerWithTimeInterval:.5
                                                repeats:YES
                                                  block:^(NSTimer*) {
                                                    [yo medirFps];
                                                  }];
  }

  if (tactil_->escuchandoInclinacion()) {
    movimiento_ = [[CMMotionManager alloc] init];
    if (movimiento_.deviceMotionAvailable) {
      movimiento_.deviceMotionUpdateInterval = 1.0 / 60.0;
      __weak NfsmwControlesTactiles* yo = self;
      [movimiento_ startDeviceMotionUpdatesToQueue:NSOperationQueue.mainQueue
                                       withHandler:^(CMDeviceMotion* m, NSError*) {
                                         [yo inclinacion:m];
                                       }];
    }
  }

  // Cada 200 ms: SDL puede taparla (al darle una vista nueva a su ventana le vuelve a poner
  // el controlador raiz, y UIKit pone esa vista encima), y un mando fisico en uso la esconde
  // (GameActivity.dispatchKeyEvent llama a mandoUsado con cada evento de mando).
  __weak NfsmwControlesTactiles* yo = self;
  vigilante_ = [NSTimer scheduledTimerWithTimeInterval:.2
                                               repeats:YES
                                                 block:^(NSTimer*) {
                                                   [yo vigilar];
                                                 }];
  return self;
}

- (void)inclinacion:(CMDeviceMotion*)m {
  if (m) {
    tactil_->inclinacion(m.gravity, self.window.windowScene.interfaceOrientation);
  }
}

- (void)mandosCambiaron {
  tactil_->revisarMandos();
}

- (void)medirFps {
  const CFTimeInterval ahora = CACurrentMediaTime();
  const uint64_t fotogramas = g_nfsmw_fotogramas_juego.load(std::memory_order_relaxed);
  if (antesFps_ > 0) {
    const double segundos = ahora - antesFps_;
    const double fps = double(fotogramas - fotogramasAntes_) / std::max(segundos, 1e-3);
    NSString* texto = fps > 0 ? [NSString stringWithFormat:@"%.0f fps  %.1f ms", fps, 1000.0 / fps] : @"0 fps";
    if (![texto isEqualToString:fps_.text]) {
      fps_.text = texto;
      [self colocarFps];
    }
  }
  antesFps_ = ahora;
  fotogramasAntes_ = fotogramas;
}

// Arriba a la izquierda, dentro de la zona segura, con 6 puntos de margen alrededor del texto.
- (void)colocarFps {
  if (!fps_) return;
  const CGRect z = [self zona];
  const CGSize t = [fps_ sizeThatFits:CGSizeMake(CGFLOAT_MAX, CGFLOAT_MAX)];
  fps_.frame = CGRectMake(z.origin.x + 12, z.origin.y + 12, std::ceil(t.width) + 12, std::ceil(t.height) + 6);
}

- (void)vigilar {
  UIView* padre = self.superview;
  if (padre && padre.subviews.lastObject != self) {
    [padre bringSubviewToFront:self];
  }
  for (GCController* mando in GCController.controllers) {
    GCExtendedGamepad* g = mando.extendedGamepad;
    if (!g) continue;
    const bool algo = g.buttonA.pressed || g.buttonB.pressed || g.buttonX.pressed || g.buttonY.pressed ||
                      g.leftShoulder.pressed || g.rightShoulder.pressed || g.leftTrigger.value > .2f ||
                      g.rightTrigger.value > .2f || g.dpad.xAxis.value != 0 || g.dpad.yAxis.value != 0 ||
                      std::fabs(g.leftThumbstick.xAxis.value) > .3f || std::fabs(g.leftThumbstick.yAxis.value) > .3f ||
                      std::fabs(g.rightThumbstick.xAxis.value) > .3f || std::fabs(g.rightThumbstick.yAxis.value) > .3f ||
                      g.buttonMenu.pressed;
    if (algo) {
      tactil_->mandoUsado();
      break;
    }
  }
}

// La zona de los controles: la segura (fuera de la isla o la muesca y de las esquinas). Los
// controles trabajan en sus coordenadas; la pantalla entera, para la imagen del juego, va en
// ellas tambien.
- (CGRect)zona {
  return UIEdgeInsetsInsetRect(self.bounds, self.safeAreaInsets);
}

- (void)layoutSubviews {
  [super layoutSubviews];
  const CGRect z = [self zona];
  const CGRect pantalla = CGRectOffset(self.bounds, -z.origin.x, -z.origin.y);
  tactil_->tamano(float(z.size.width), float(z.size.height), pantalla);
  if (!empezado_ && z.size.width > 0) {
    empezado_ = YES;
    tactil_->empezar();
  }
  [self colocarFps];
  [self setNeedsDisplay];
}

- (void)drawRect:(CGRect)rect {
  CGContextRef ctx = UIGraphicsGetCurrentContext();
  const CGRect z = [self zona];
  CGContextSaveGState(ctx);
  CGContextTranslateCTM(ctx, z.origin.x, z.origin.y);
  tactil_->dibujar(ctx);
  CGContextRestoreGState(ctx);
}

- (Evento)evento:(Accion)accion indice:(int)indice tiempo:(NSTimeInterval)tiempo {
  const CGRect z = [self zona];
  Evento e{accion, indice, tiempo * 1000.0, {}};
  for (const auto& [toque, id] : dedos_) {
    const CGPoint p = [toque locationInView:self];
    e.dedos.push_back({id, float(p.x - z.origin.x), float(p.y - z.origin.y)});
  }
  return e;
}

- (int)indiceDe:(UITouch*)t {
  for (size_t i = 0; i < dedos_.size(); ++i) {
    if (dedos_[i].first == t) return int(i);
  }
  return -1;
}

// Cada dedo nuevo es un ACTION_DOWN (el primero) o un ACTION_POINTER_DOWN, como en Android.
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  for (UITouch* t in touches) {
    dedos_.push_back({t, siguiente_dedo_++});
    const int i = int(dedos_.size()) - 1;
    tactil_->tocar([self evento:(i == 0 ? ACTION_DOWN : ACTION_POINTER_DOWN) indice:i tiempo:t.timestamp]);
  }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  NSTimeInterval tiempo = 0;
  for (UITouch* t in touches) tiempo = std::max(tiempo, t.timestamp);
  tactil_->tocar([self evento:ACTION_MOVE indice:0 tiempo:tiempo]);
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  for (UITouch* t in touches) {
    const int i = [self indiceDe:t];
    if (i < 0) continue;
    tactil_->tocar([self evento:(dedos_.size() == 1 ? ACTION_UP : ACTION_POINTER_UP) indice:i tiempo:t.timestamp]);
    dedos_.erase(dedos_.begin() + i);
  }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
  if (!dedos_.empty()) {
    tactil_->tocar([self evento:ACTION_CANCEL indice:0 tiempo:CACurrentMediaTime()]);
  }
  dedos_.clear();
}

@end

// ---- Encima de la ventana de SDL ------------------------------------------------------------

namespace {

NfsmwControlesTactiles* g_controles = nil;

void Colocar(UIWindow* ventana) {
  if (g_controles || !ventana) {
    return;
  }
  Class sdl = NSClassFromString(@"SDL_uikitviewcontroller");
  if (!sdl || ![ventana.rootViewController isKindOfClass:sdl]) {
    return;  // la pantalla de arranque de SDL, u otra
  }
  g_controles = [[NfsmwControlesTactiles alloc] initWithFrame:ventana.bounds];
  [ventana addSubview:g_controles];
  os_log(OS_LOG_DEFAULT, "[tactil] mando tactil sobre la ventana de SDL (%.0f x %.0f)", ventana.bounds.size.width,
         ventana.bounds.size.height);
}

__attribute__((constructor)) void EsperarVentanaDeSdl() {
  // El indicador de inicio atenuado y los gestos del sistema a un segundo deslizamiento: un
  // pulgar en el stick, junto al borde de abajo, no puede mandar el juego a segundo plano.
  SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "2");
  [NSNotificationCenter.defaultCenter addObserverForName:UIWindowDidBecomeKeyNotification
                                                  object:nil
                                                   queue:NSOperationQueue.mainQueue
                                              usingBlock:^(NSNotification* n) {
                                                Colocar(n.object);
                                              }];
  // El primer toque del boton de probar del lanzador ya vibra: la cola de hapticos, preparada.
  ColaVibracion();
}

}  // namespace

// ---- La vibracion: lo que llaman el SDK y el lanzador -----------------------------------------

// El SDK le pasa a la app lo que el juego le pide al mando tactil (sdl_input_driver.cpp, con el
// nombre de Android): la fuerza de los dos motores del mando de la Xbox 360, de 0 a 65535. El
// movil tiene uno: se usa el mas fuerte, en 16 niveles, y solo se cambia cuando cambia de nivel
// (el juego lo repite a menudo), como en nativo_android.cpp. La llama el hilo del juego.
extern "C" void NfsmwAndroidVibrar(uint16_t izquierdo, uint16_t derecho) {
  const int fuerza = std::max<int>(izquierdo, derecho);
  const int nivel = (fuerza + 4095) / 4096;  // 0..16
  if (g_nivel_vibracion.exchange(nivel) == nivel) {
    return;
  }
  {
    std::lock_guard<std::mutex> l(g_vibracion_mutex);
    g_vibracion_pedida = std::min(255, nivel * 16);
  }
  AplicarVibracion();
}

// Medio segundo a toda fuerza (SetupActivity.vibrarUnRato), para el boton de probar.
void NfsmwVibrarUnRato() {
  dispatch_async(ColaVibracion(), ^{
    if (!ArrancarHapticos()) return;
    CHHapticEvent* e = [[CHHapticEvent alloc]
        initWithEventType:CHHapticEventTypeHapticContinuous
               parameters:@[[[CHHapticEventParameter alloc] initWithParameterID:CHHapticEventParameterIDHapticIntensity value:1]]
             relativeTime:0
                 duration:.5];
    CHHapticPattern* patron = [[CHHapticPattern alloc] initWithEvents:@[e] parameters:@[] error:nil];
    id<CHHapticPatternPlayer> uno = patron ? [g_hapticos createPlayerWithPattern:patron error:nil] : nil;
    [uno startAtTime:CHHapticTimeImmediate error:nil];
  });
}
