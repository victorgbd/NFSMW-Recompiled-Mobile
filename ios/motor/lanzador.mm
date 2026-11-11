// El lanzador de iOS: la pantalla de inicio de la app de Android (SetupActivity, con lo que hacen
// Ajustes, MotorNativo, IsoXex e Idioma) en UIKit. Sale antes que el juego, en el mismo proceso:
// el SDL_main del SDK en iOS (windowed_app_main_sdl.cpp) llama a NfsmwLanzadorIos, que pinta la
// pantalla en su propia ventana, espera a Jugar y devuelve la linea de comandos con lo elegido.
// Despues el juego arranca como siempre, con la ventana de SDL.
//
// Lo que cambia respecto a Android:
//   - El juego: la ISO elegida en Archivos (se lee donde esta, con un marcador de seguridad), una
//     ISO en la carpeta de la app (Documents, que ven Archivos y el Finder) o el juego extraido en
//     Documents/game_root. La ISO no se mapea entera: ver disc_image_device.cpp del SDK.
//   - No estan las opciones que en iOS no existen: la resolucion de la pantalla, la API grafica
//     (solo Vulkan, por MoltenVK), los hilos fijados, el driver de la GPU, la sonda de Vulkan y el
//     modo de compatibilidad (sin Xenos en iOS). El sonido por AAudio es aqui por AVAudioEngine.
//   - Multijugador: la red va en red_lan.cpp y el servidor del anfitrion en servidor_ea.cpp. Buscar
//     servidores por broadcast no sale sin un permiso de Apple que no tiene una Apple ID gratuita:
//     se pregunta a cada IP de la red. El anfitrion sigue sirviendo en segundo plano y con la
//     pantalla bloqueada (segundo_plano.mm).
//   - El editor del mando tactil es el de la partida (el engranaje junto a OCULTAR).
//   - Volver aqui es cerrar la app y abrirla: iOS no deja que una app se cierre sola.
//
// Las preferencias van en NSUserDefaults con las claves de Android ("ajustes.<clave>"), las mismas
// que lee el mando tactil. Los textos son los de la app de Android en sus 10 idiomas (textos.py).

#import <CommonCrypto/CommonDigest.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/runtime.h>
#include <os/log.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <csignal>
#include <optional>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

#include "ios_app.h"
#include "partidas.h"
#include "red_lan.h"
#include "segundo_plano.h"
#include "servidor_ea.h"

// Un toque en una vista que no es un boton (las filas de los servidores), con un bloque.
@interface NfsmwAccion : NSObject
@property(nonatomic, copy) void (^hacer)(void);
- (void)ejecutar;
@end

@implementation NfsmwAccion
- (void)ejecutar {
  if (self.hacer) self.hacer();
}
@end

namespace {

void AlTocar(UIView* v, void (^hacer)(void)) {
  NfsmwAccion* a = [NfsmwAccion new];
  a.hacer = hacer;
  [v addGestureRecognizer:[[UITapGestureRecognizer alloc] initWithTarget:a action:@selector(ejecutar)]];
  // El reconocedor no retiene a su destino: lo retiene la vista.
  objc_setAssociatedObject(v, (__bridge const void*)a, a, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

// ---------------------------------------------------------------------------------------------
//  Textos (Idioma.java): textos.json, que compilar.sh mete en la app
// ---------------------------------------------------------------------------------------------

// "" = el del telefono. Despues, como en Android, por el nombre de cada idioma en su lengua.
NSString* const CODIGOS_IDIOMA[] = {@"", @"de", @"en", @"es", @"fr", @"it", @"pl", @"pt", @"ja", @"ko", @"zh-TW"};
NSString* const NOMBRES_IDIOMA[] = {@"idioma_auto", @"idioma_de", @"idioma_en", @"idioma_es", @"idioma_fr",
                                   @"idioma_it",   @"idioma_pl", @"idioma_pt", @"idioma_ja", @"idioma_ko",
                                   @"idioma_zh_tw"};

NSDictionary<NSString*, NSDictionary<NSString*, NSString*>*>* g_textos = nil;
NSDictionary<NSString*, NSString*>* g_tabla = nil;
NSDictionary<NSString*, NSString*>* g_ingles = nil;

NSString* IdiomaElegido() {
  NSString* v = [NSUserDefaults.standardUserDefaults stringForKey:@"ajustes.idioma"];
  return v ?: @"";
}

// El codigo de la tabla para un idioma de iOS: "es-ES" es "es", "zh-Hant-TW" es "zh-TW".
NSString* TablaPara(NSString* idioma) {
  if ([idioma hasPrefix:@"zh-Hant"] || [idioma hasPrefix:@"zh-TW"] || [idioma hasPrefix:@"zh-HK"]) {
    return g_textos[@"zh-TW"] ? @"zh-TW" : nil;
  }
  NSString* base = [idioma componentsSeparatedByString:@"-"].firstObject;
  return g_textos[base] ? base : nil;
}

void CargarTextos() {
  if (!g_textos) {
    NSString* ruta = [NSBundle.mainBundle pathForResource:@"textos" ofType:@"json"];
    NSData* datos = ruta ? [NSData dataWithContentsOfFile:ruta] : nil;
    id json = datos ? [NSJSONSerialization JSONObjectWithData:datos options:0 error:nil] : nil;
    g_textos = [json isKindOfClass:NSDictionary.class] ? json : @{};
    g_ingles = g_textos[@"en"] ?: @{};
  }
  NSString* codigo = nil;
  NSString* elegido = IdiomaElegido();
  if (elegido.length) {
    codigo = TablaPara(elegido);
  } else {
    for (NSString* idioma in NSLocale.preferredLanguages) {
      if ((codigo = TablaPara(idioma))) break;
    }
  }
  g_tabla = g_textos[codigo ?: @"en"] ?: g_ingles;
}

// ---------------------------------------------------------------------------------------------
//  Colores y preferencias (SetupActivity y Ajustes.java)
// ---------------------------------------------------------------------------------------------

UIColor* Color(uint32_t argb) {
  return [UIColor colorWithRed:((argb >> 16) & 0xFF) / 255.0
                         green:((argb >> 8) & 0xFF) / 255.0
                          blue:(argb & 0xFF) / 255.0
                         alpha:((argb >> 24) & 0xFF) / 255.0];
}
constexpr uint32_t ACENTO = 0xFF6F7432;
// El acento sobre fondo oscuro, para iconos y textos: el de siempre se pierde.
constexpr uint32_t ACENTO_CLARO = 0xFFB9BF6A;
constexpr uint32_t GRIS = 0xFFA0A0A0, AVISO = 0xFFFFB74D, FONDO = 0xFF121212, FONDO_MENU = 0xFF181818;
constexpr uint32_t ELEGIDO = 0xFF2E301E, TARJETA = 0xFF1E1E1E, BOTON = 0xFF2C2C2C, BLANCO = 0xFFFFFFFF;

NSString* Clave(NSString* k) { return [@"ajustes." stringByAppendingString:k]; }
id Leer(NSString* k) { return [NSUserDefaults.standardUserDefaults objectForKey:Clave(k)]; }
bool Hay(NSString* k) { return Leer(k) != nil; }
bool LeerBool(NSString* k, bool def) {
  id v = Leer(k);
  return [v isKindOfClass:NSNumber.class] ? [v boolValue] : def;
}
int LeerInt(NSString* k, int def) {
  id v = Leer(k);
  return [v isKindOfClass:NSNumber.class] ? [v intValue] : def;
}
NSString* LeerTexto(NSString* k, NSString* def) {
  id v = Leer(k);
  return [v isKindOfClass:NSString.class] ? v : def;
}
void Poner(NSString* k, id v) { [NSUserDefaults.standardUserDefaults setObject:v forKey:Clave(k)]; }
void Quitar(NSString* k) { [NSUserDefaults.standardUserDefaults removeObjectForKey:Clave(k)]; }

// Mando tactil, en la disposicion de conducir: con que se gira (Ajustes.modoGiro). Inclinar
// sigue en su clave de antes (girar_inclinando).
NSString* const GIRO_STICK = @"stick";
NSString* const GIRO_FLECHAS = @"flechas";
NSString* const GIRO_INCLINAR = @"inclinar";
NSString* ModoGiro() {
  if (LeerBool(@"girar_inclinando", false)) return GIRO_INCLINAR;
  return [LeerTexto(@"giro_carrera", GIRO_STICK) isEqualToString:GIRO_FLECHAS] ? GIRO_FLECHAS : GIRO_STICK;
}
void ModoGiro(NSString* v) {
  Poner(@"girar_inclinando", @([v isEqualToString:GIRO_INCLINAR]));
  Poner(@"giro_carrera", v);
}
constexpr int SENSIBILIDAD_MIN = 1, SENSIBILIDAD_MAX = 20;
// Los grados que hay que inclinar el movil para girar del todo: de 45 (1) a 6 (20).
float AnguloGiro(int sensibilidad) {
  const int s = std::max(SENSIBILIDAD_MIN, std::min(SENSIBILIDAD_MAX, sensibilidad));
  return float(45.0 * std::pow(6.0 / 45.0, (s - SENSIBILIDAD_MIN) / double(SENSIBILIDAD_MAX - SENSIBILIDAD_MIN)));
}

NSString* const AA_NO = @"no";
NSString* const AA_FXAA = @"fxaa";
// El motor nativo solo tiene FXAA: lo que viniera de antes (MSAA, "sin MSAA") es sin suavizado.
NSString* Antialiasing() { return [LeerTexto(@"antialiasing", AA_NO) isEqualToString:AA_FXAA] ? AA_FXAA : AA_NO; }

NSString* const SOMBRAS_RAPIDA = @"rapida";
NSString* const SOMBRAS_SUAVE = @"suave";
NSString* const SOMBRAS_XBOX = @"xbox360";
constexpr int FPS_SIN_LIMITE = 0;

bool GamertagValido(NSString* g) {
  return g.length == 0 ||
         [[NSPredicate predicateWithFormat:@"SELF MATCHES %@", @"[A-Za-z][A-Za-z0-9 ]{0,14}"] evaluateWithObject:g];
}

// --- Multijugador (Ajustes.servidores, servidorLan, anfitrionLan)

std::string S(NSString* t) { return t ? std::string(t.UTF8String) : std::string(); }
NSString* N(const std::string& t) { return [NSString stringWithUTF8String:t.c_str()] ?: @""; }
bool DireccionValida(NSString* d) { return nfsmw::red::DireccionValida(S(d)); }

// Un servidor LAN guardado: su direccion y, si se le puso, un nombre ("Casa", "PC de Juan").
struct Servidor {
  NSString* nombre = @"";
  NSString* direccion = @"";
};

// Donde esta esa direccion en la lista (sin mirar mayusculas), o -1.
NSInteger IndiceServidor(const std::vector<Servidor>& l, NSString* direccion) {
  for (size_t i = 0; i < l.size(); ++i) {
    if ([l[i].direccion caseInsensitiveCompare:direccion] == NSOrderedSame) return NSInteger(i);
  }
  return -1;
}

// Los guardados, en el orden en que se guardaron: [{"nombre": "...", "direccion": "..."}]. La elegida
// (servidor_lan) siempre esta en la lista.
std::vector<Servidor> Servidores() {
  std::vector<Servidor> l;
  NSData* datos = [LeerTexto(@"servidores", @"[]") dataUsingEncoding:NSUTF8StringEncoding];
  NSArray* a = [NSJSONSerialization JSONObjectWithData:datos options:0 error:nil];
  if ([a isKindOfClass:NSArray.class]) {
    for (NSDictionary* o in a) {
      if (![o isKindOfClass:NSDictionary.class]) continue;
      NSString* d = [o[@"direccion"] isKindOfClass:NSString.class] ? o[@"direccion"] : o[@"ip"];
      if (![d isKindOfClass:NSString.class] || !DireccionValida(d) || IndiceServidor(l, d) >= 0) continue;
      NSString* n = [o[@"nombre"] isKindOfClass:NSString.class] ? o[@"nombre"] : @"";
      l.push_back({[n stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet], d});
    }
  }
  NSString* elegida = LeerTexto(@"servidor_lan", @"");
  if (DireccionValida(elegida) && IndiceServidor(l, elegida) < 0) l.insert(l.begin(), Servidor{@"", elegida});
  return l;
}

void Servidores(const std::vector<Servidor>& l) {
  NSMutableArray* a = [NSMutableArray array];
  for (const auto& v : l) [a addObject:@{@"nombre" : v.nombre ?: @"", @"direccion" : v.direccion}];
  NSData* datos = [NSJSONSerialization dataWithJSONObject:a options:0 error:nil];
  Poner(@"servidores", [[NSString alloc] initWithData:datos encoding:NSUTF8StringEncoding]);
}

// El servidor contra el que va el juego: este telefono si es el anfitrion, o la direccion elegida
// (IP o nombre). Vacio = ninguno.
NSString* ServidorEfectivo() {
  if (LeerBool(@"anfitrion_lan", false)) return N(nfsmw::red::IpLocal());
  return LeerTexto(@"servidor_lan", @"");
}

// Lo que ven los demas al buscar servidores: el gamertag de este telefono, o su nombre.
std::string NombreAnfitrion() {
  NSString* g = [LeerTexto(@"gamertag", @"") stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
  return S(g.length ? g : UIDevice.currentDevice.name);
}

// --- Preajustes (Ajustes.aplicarAhorro, aplicarFabrica, aplicarMisAjustes)

// Los ajustes graficos que cambian los preajustes (y "sombras_cada", que ya no se usa).
NSString* const CLAVES_GRAFICOS[] = {@"limite_fps",   @"resolucion_interna", @"escalado",     @"antialiasing",
                                     @"sin_msaa",     @"anisotropico",       @"calidad_sombras", @"sombras_escala",
                                     @"sombras_cada", @"sombras_corte",      @"reflejos_coche", @"reflejo_asfalto",
                                     @"perfil_gpu_debil"};

// Los valores guardados de CLAVES_GRAFICOS con su tipo, en el JSON de Android:
// {"clave": ["b", true], "otra": ["i", 30], ...}.
NSString* FotoGraficos() {
  NSMutableDictionary* foto = [NSMutableDictionary dictionary];
  for (NSString* clave : CLAVES_GRAFICOS) {
    id v = Leer(clave);
    if ([v isKindOfClass:NSString.class]) {
      foto[clave] = @[@"s", v];
    } else if ([v isKindOfClass:NSNumber.class]) {
      const bool booleano = CFGetTypeID((__bridge CFTypeRef)v) == CFBooleanGetTypeID();
      foto[clave] = booleano ? @[@"b", @([v boolValue])] : @[@"i", @([v intValue])];
    }
  }
  NSData* datos = [NSJSONSerialization dataWithJSONObject:foto options:NSJSONWritingSortedKeys error:nil];
  return datos ? [[NSString alloc] initWithData:datos encoding:NSUTF8StringEncoding] : @"{}";
}

// Antes de aplicar otro preajuste se guardan los ajustes del usuario, si los ha tocado desde el
// ultimo preajuste: asi "Ahorro" y luego "Fabrica" no pisan los suyos.
void GuardarMisAjustes() {
  NSString* ahora = FotoGraficos();
  if (![ahora isEqualToString:LeerTexto(@"huella_preajuste", @"")]) {
    Poner(@"mis_ajustes", ahora);
  }
}

// 30 fps, 1024x576 con FSR, sin suavizado ni anisotropico, sombras rapidas y de cerca y un solo
// reflejo del coche.
void AplicarAhorro() {
  GuardarMisAjustes();
  Poner(@"limite_fps", @30);
  Poner(@"resolucion_interna", @"1024x576");
  Poner(@"escalado", @"fsr");
  Poner(@"antialiasing", AA_NO);
  Poner(@"anisotropico", @0);
  Poner(@"calidad_sombras", SOMBRAS_RAPIDA);
  Poner(@"sombras_escala", @100);
  Poner(@"sombras_corte", @200);
  Poner(@"reflejos_coche", @1);
  Poner(@"huella_preajuste", FotoGraficos());
}

// Los de una instalacion nueva. En iOS no hay perfil de GPU debil: todos los iPhone que llegan a
// iOS 16 tienen GPU de sobra para los de fabrica.
void AplicarFabrica() {
  GuardarMisAjustes();
  for (NSString* clave : CLAVES_GRAFICOS) Quitar(clave);
  Poner(@"huella_preajuste", FotoGraficos());
}

void AplicarMisAjustes() {
  NSData* datos = [LeerTexto(@"mis_ajustes", @"") dataUsingEncoding:NSUTF8StringEncoding];
  NSDictionary* foto = datos.length ? [NSJSONSerialization JSONObjectWithData:datos options:0 error:nil] : nil;
  if (![foto isKindOfClass:NSDictionary.class]) return;  // una foto rota no cambia nada
  for (NSString* clave : CLAVES_GRAFICOS) {
    NSArray* v = foto[clave];
    if (![v isKindOfClass:NSArray.class] || v.count != 2) {
      Quitar(clave);
    } else if ([v[0] isEqual:@"b"]) {
      Poner(clave, @([v[1] boolValue]));
    } else if ([v[0] isEqual:@"i"]) {
      Poner(clave, @([v[1] intValue]));
    } else {
      Poner(clave, [v[1] description]);
    }
  }
}

// ---------------------------------------------------------------------------------------------
//  La edicion del juego de esta app (edicion.json, de la compilacion) y donde esta el juego
// ---------------------------------------------------------------------------------------------

struct Edicion {
  NSString* nombre = @"?";
  NSString* sha = @"";
  int idioma = -1;
  int pais = -1;
};

Edicion LeerEdicion() {
  Edicion e;
  NSString* ruta = [NSBundle.mainBundle pathForResource:@"edicion" ofType:@"json"];
  NSData* datos = ruta ? [NSData dataWithContentsOfFile:ruta] : nil;
  NSDictionary* j = datos ? [NSJSONSerialization JSONObjectWithData:datos options:0 error:nil] : nil;
  if (![j isKindOfClass:NSDictionary.class]) return e;
  if ([j[@"nombre"] isKindOfClass:NSString.class]) e.nombre = j[@"nombre"];
  if ([j[@"sha256"] isKindOfClass:NSString.class]) e.sha = [j[@"sha256"] lowercaseString];
  if ([j[@"idioma"] isKindOfClass:NSNumber.class]) e.idioma = [j[@"idioma"] intValue];
  if ([j[@"pais"] isKindOfClass:NSNumber.class]) e.pais = [j[@"pais"] intValue];
  return e;
}

NSString* Documentos() { return [NSHomeDirectory() stringByAppendingPathComponent:@"Documents"]; }

enum class Fuente { NINGUNA, ISO_ELEGIDA, ISO_DOCUMENTOS, CARPETA };

struct Juego {
  Fuente fuente = Fuente::NINGUNA;
  NSString* ruta = nil;
  NSString* nombre = nil;
  // Hay una ISO elegida, pero iOS ya no da acceso a ella (se movio o se borro).
  bool elegidaSinAcceso = false;
};

// La ISO elegida, con su acceso de seguridad abierto hasta que se cierre la app: el juego la lee
// mientras dura la partida.
NSURL* g_iso_accedida = nil;
bool g_iso_con_acceso = false;

NSURL* ResolverIsoElegida(bool* sinAcceso) {
  NSData* marcador = [NSUserDefaults.standardUserDefaults dataForKey:Clave(@"iso_marcador")];
  if (!marcador) return nil;
  BOOL viejo = NO;
  NSURL* url = [NSURL URLByResolvingBookmarkData:marcador options:0 relativeToURL:nil bookmarkDataIsStale:&viejo error:nil];
  if (!url) {
    *sinAcceso = true;
    return nil;
  }
  if (g_iso_accedida && ![g_iso_accedida isEqual:url]) {
    if (g_iso_con_acceso) [g_iso_accedida stopAccessingSecurityScopedResource];
    g_iso_accedida = nil;
  }
  if (!g_iso_accedida) {
    // NO tambien con las de la propia carpeta de la app, que no lo necesitan.
    g_iso_con_acceso = [url startAccessingSecurityScopedResource];
    g_iso_accedida = url;
  }
  if (access(url.fileSystemRepresentation, R_OK) != 0) {
    *sinAcceso = true;
    return nil;
  }
  if (viejo) {
    NSData* nuevo = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
    if (nuevo) Poner(@"iso_marcador", nuevo);
  }
  return url;
}

// La elegida con el boton; si no, el juego extraido de Documents/game_root; si no, la primera ISO
// de Documents.
Juego BuscarJuego() {
  Juego j;
  bool sinAcceso = false;
  if (NSURL* url = ResolverIsoElegida(&sinAcceso)) {
    j.fuente = Fuente::ISO_ELEGIDA;
    j.ruta = url.path;
    j.nombre = LeerTexto(@"iso_nombre", url.lastPathComponent);
    return j;
  }
  j.elegidaSinAcceso = sinAcceso;
  NSFileManager* fm = NSFileManager.defaultManager;
  NSString* docs = Documentos();
  NSString* carpeta = [docs stringByAppendingPathComponent:@"game_root"];
  if ([fm fileExistsAtPath:[carpeta stringByAppendingPathComponent:@"default.xex"]]) {
    j.fuente = Fuente::CARPETA;
    j.ruta = carpeta;
    j.nombre = @"game_root";
    return j;
  }
  NSArray* ficheros = [[fm contentsOfDirectoryAtPath:docs error:nil] sortedArrayUsingSelector:@selector(localizedStandardCompare:)];
  for (NSString* f in ficheros) {
    if ([f.pathExtension.lowercaseString isEqualToString:@"iso"]) {
      j.fuente = Fuente::ISO_DOCUMENTOS;
      j.ruta = [docs stringByAppendingPathComponent:f];
      j.nombre = f;
      return j;
    }
  }
  return j;
}

// La biblioteca de shaders del renderizador nativo: la de la app (compilar.sh la mete) o una
// copiada a Documents o junto al juego extraido (nfsmw_nativo_sistema.cpp mira ahi).
bool HayBiblioteca(const Juego& j) {
  NSFileManager* fm = NSFileManager.defaultManager;
  NSString* nombre = @"nfsmw_shaders.nfsp";
  if ([fm fileExistsAtPath:[NSBundle.mainBundle.bundlePath stringByAppendingPathComponent:nombre]] ||
      [fm fileExistsAtPath:[Documentos() stringByAppendingPathComponent:nombre]]) {
    return true;
  }
  return j.fuente == Fuente::CARPETA && [fm fileExistsAtPath:[j.ruta stringByAppendingPathComponent:nombre]];
}

// ---------------------------------------------------------------------------------------------
//  IsoXex: de que edicion es la ISO (el SHA-256 de su default.xex) y si esta entera
// ---------------------------------------------------------------------------------------------

// xex de una ISO que se abrio bien pero no tiene el XDVDFS de Xbox 360: otro juego, la version de
// PC o de PS2, un .zip/.7z o un fichero roto.
const std::string NO_ES_ISO = "-";

struct Mirada {
  bool hecha = false;
  // SHA-256 del default.xex, "" si no se pudo leer o NO_ES_ISO.
  std::string xex;
  // Bytes que le faltan al fichero para contener todos sus archivos; 0 si esta entero.
  uint64_t faltan = 0;
};

constexpr uint64_t SECTOR = 2048;
constexpr uint8_t DIRECTORIO = 0x10;
// Donde empieza la particion de juego; los mismos que mira el SDK (DiscImageDevice::Verify).
constexpr uint64_t BASES[] = {0, 0x0FD90000, 0x02080000, 0x18300000, 0x0000FB20, 0x00020600};
constexpr uint64_t MAX_DIRECTORIO = 1 << 20, MAX_XEX = 64 << 20;
constexpr int MAX_PROFUNDIDAD = 8;

bool LeerEn(int fd, uint64_t desde, void* destino, size_t cuantos) {
  auto* p = static_cast<uint8_t*>(destino);
  while (cuantos) {
    const ssize_t n = pread(fd, p, cuantos, off_t(desde));
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    p += n;
    desde += uint64_t(n);
    cuantos -= size_t(n);
  }
  return true;
}

uint32_t Le32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;  // arm64: little endian, como el XDVDFS
}
uint16_t Le16(const uint8_t* p) {
  uint16_t v;
  std::memcpy(&v, p, 2);
  return v;
}

std::string Sha256(const std::vector<uint8_t>& datos) {
  uint8_t resumen[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256(datos.data(), CC_LONG(datos.size()), resumen);
  static const char* const hex = "0123456789abcdef";
  std::string s;
  for (uint8_t b : resumen) {
    s += hex[b >> 4];
    s += hex[b & 15];
  }
  return s;
}

// Recorre el arbol de directorios apuntando el default.xex de la raiz y donde acaba el ultimo
// archivo. Cada entrada lleva los desplazamientos (en palabras de 4 bytes) de sus dos hijas en el
// arbol binario, el sector, el tamano, los atributos y el nombre.
struct Recorrido {
  int fd;
  uint64_t base;
  uint64_t sectorXex = 0, tamXex = 0, fin = 0;

  void directorio(uint64_t sector, uint64_t tam, int profundidad) {
    fin = std::max(fin, base + sector * SECTOR + tam);
    if (tam == 0 || tam > MAX_DIRECTORIO || profundidad > MAX_PROFUNDIDAD) return;
    std::vector<uint8_t> tabla(tam);
    if (!LeerEn(fd, base + sector * SECTOR, tabla.data(), tabla.size())) {
      return;  // el directorio cae en lo que falta: ya cuenta en `fin`
    }
    std::vector<size_t> pila{0};
    int vueltas = 0;
    while (!pila.empty() && vueltas++ < 8192) {
      const size_t off = pila.back();
      pila.pop_back();
      if (off + 14 > tabla.size()) continue;
      const uint16_t izq = Le16(&tabla[off]), der = Le16(&tabla[off + 2]);
      const uint64_t sectorHijo = Le32(&tabla[off + 4]), tamHijo = Le32(&tabla[off + 8]);
      const uint8_t atributos = tabla[off + 12], largo = tabla[off + 13];
      if (izq != 0 && izq != 0xFFFF && pila.size() < 256) pila.push_back(size_t(izq) * 4);
      if (der != 0 && der != 0xFFFF && pila.size() < 256) pila.push_back(size_t(der) * 4);
      if (largo == 0 || off + 14 + largo > tabla.size()) continue;
      if (atributos & DIRECTORIO) {
        directorio(sectorHijo, tamHijo, profundidad + 1);
        continue;
      }
      fin = std::max(fin, base + sectorHijo * SECTOR + tamHijo);
      std::string nombre(reinterpret_cast<const char*>(&tabla[off + 14]), largo);
      std::transform(nombre.begin(), nombre.end(), nombre.begin(), [](unsigned char c) { return char(std::tolower(c)); });
      if (profundidad == 0 && nombre == "default.xex") {
        sectorXex = sectorHijo;
        tamXex = tamHijo;
      }
    }
  }
};

Mirada MirarIso(const char* ruta) {
  Mirada m;
  const int fd = open(ruta, O_RDONLY);
  if (fd < 0) return m;
  struct stat datos {};
  const uint64_t tam = fstat(fd, &datos) == 0 ? uint64_t(datos.st_size) : 0;
  m.hecha = true;
  m.xex = NO_ES_ISO;
  for (uint64_t base : BASES) {
    uint8_t vd[28];
    if (!LeerEn(fd, base + 32 * SECTOR, vd, sizeof(vd)) || std::memcmp(vd, "MICROSOFT*XBOX*MEDIA", 20) != 0) continue;
    Recorrido r{fd, base};
    r.directorio(Le32(vd + 20), Le32(vd + 24), 0);
    m.xex.clear();
    if (r.tamXex > 0 && r.tamXex <= MAX_XEX) {
      std::vector<uint8_t> xex(r.tamXex);
      if (LeerEn(fd, base + r.sectorXex * SECTOR, xex.data(), xex.size())) m.xex = Sha256(xex);
    }
    m.faltan = tam > 0 && r.fin > tam ? r.fin - tam : 0;
    break;
  }
  close(fd);
  return m;
}

Mirada MirarJuego(const Juego& j) {
  if (j.fuente == Fuente::CARPETA) {
    Mirada m;
    NSData* xex = [NSData dataWithContentsOfFile:[j.ruta stringByAppendingPathComponent:@"default.xex"]];
    m.hecha = true;
    if (xex.length > 0 && xex.length <= MAX_XEX) {
      const auto* p = static_cast<const uint8_t*>(xex.bytes);
      m.xex = Sha256(std::vector<uint8_t>(p, p + xex.length));
    }
    return m;
  }
  return MirarIso(j.ruta.fileSystemRepresentation);
}

// ---------------------------------------------------------------------------------------------
//  La linea de comandos del juego (MotorNativo.argumentos)
// ---------------------------------------------------------------------------------------------

// anisotropic_override del SDK (0, 2 = 2x, 3 = 4x, 5 = 16x) a nfsmw_nativo_anisotropico.
int Anisotropico(int sdk) {
  switch (sdk) {
    case 2: return 2;
    case 3: return 4;
    case 4: return 8;
    case 5: return 16;
    default: return 0;
  }
}

// Ancho entre alto de la pantalla entera, en horizontal.
double ProporcionPantalla() {
  const CGSize s = UIScreen.mainScreen.nativeBounds.size;
  return std::max(s.width, s.height) / std::max(1.0, double(std::min(s.width, s.height)));
}

// nfsmw.toml: los ajustes del motor que el lanzador no ensena, los mismos que lleva el APK de Android
// (nativo/app/nfsmw_movil.toml, que compilar.sh mete en la app). Sin el, el motor
// se queda con los valores del codigo, que para varios son los de PC: la escena en tres franjas con
// MSAA, la cara del retrovisor del cubemap no en cada fotograma, dos mapas de sombras... Como
// MotorNativo.instalarAjustes: se pone el de la app y se renueva cuando la app trae otro, salvo que el
// de Documents se haya cambiado a mano o desde el menu del juego.
void InstalarAjustesMotor() {
  NSString* origen = [NSBundle.mainBundle pathForResource:@"nfsmw" ofType:@"toml"];
  NSData* deLaApp = origen ? [NSData dataWithContentsOfFile:origen] : nil;
  if (!deLaApp.length) {
    os_log(OS_LOG_DEFAULT, "[lanzador] la app no lleva nfsmw.toml");
    return;
  }
  const auto huella = [](NSData* d) {
    const auto* p = static_cast<const uint8_t*>(d.bytes);
    return Sha256(std::vector<uint8_t>(p, p + d.length));
  };
  const std::string huellaApp = huella(deLaApp);
  NSString* destino = [Documentos() stringByAppendingPathComponent:@"nfsmw.toml"];
  if (NSData* actual = [NSData dataWithContentsOfFile:destino]) {
    const std::string huellaActual = huella(actual);
    if (huellaActual == huellaApp || huellaActual != S(LeerTexto(@"motor_toml_sha256", @""))) return;
  }
  if ([deLaApp writeToFile:destino atomically:YES]) {
    Poner(@"motor_toml_sha256", N(huellaApp));
  } else {
    os_log(OS_LOG_DEFAULT, "[lanzador] no se pudo escribir %{public}@", destino);
  }
}

// redDns: la IP del servidor de la LAN a la que va Xbox Live del juego; nil = sin online.
std::vector<std::string> Argumentos(const Juego& j, const Edicion& edicion, const std::string& argv0, NSString* redDns) {
  std::vector<std::string> a{argv0};
  auto poner = [&a](NSString* s) { a.emplace_back(s.UTF8String); };
  auto bandera = [](bool v) { return v ? @"true" : @"false"; };
  poner([@"--game_data_root=" stringByAppendingString:j.ruta]);

  // La pantalla: con el 16:9 del centro, las bandas las pone el presentador; estirando o con la
  // pantalla ancha, la imagen llena la pantalla entera (el mando tactil lo sabe por sus ajustes).
  const bool panoramico = LeerBool(@"panoramico", false);
  const bool estirar = LeerBool(@"estirar", false) && !panoramico;
  poner([@"--present_letterbox=" stringByAppendingString:bandera(!estirar && !panoramico)]);
  poner(@"--present_allow_overscan_cutoff=false");
  poner(@"--present_safe_area_x=100");
  poner(@"--present_safe_area_y=100");
  // GSR 2 lo hace el renderizador nativo y entrega la imagen al tamano de la pantalla.
  NSString* escalado = LeerTexto(@"escalado", @"bilinear");
  if ([escalado isEqualToString:@"gsr2"]) {
    poner(@"--present_effect=bilinear");
    poner(@"--nfsmw_gsr2=true");
  } else {
    poner([@"--present_effect=" stringByAppendingString:escalado]);
  }
  if (panoramico) {
    poner([NSString stringWithFormat:@"--nfsmw_proporcion_pantalla=%.4f", ProporcionPantalla()]);
  }

  // Graficos y sonido
  poner([@"--nfsmw_resolucion_interna=" stringByAppendingString:LeerTexto(@"resolucion_interna", @"1280x720")]);
  const int fps = LeerInt(@"limite_fps", 60);
  poner(fps == FPS_SIN_LIMITE ? @"--nfsmw_limite_fps=sin_limite" : [NSString stringWithFormat:@"--nfsmw_limite_fps=%d", fps]);
  // MSAA no hay: la escena se pinta en una pasada. Solo FXAA. Tambien va en nfsmw.toml, pero aqui
  // manda aunque el de Documents sea uno guardado desde el menu del juego con el valor del codigo (tres
  // franjas con MSAA 4x, que en Android no se usa: con MoltenVK salen cuadrados y destellos).
  poner(@"--nfsmw_render_sin_mosaico=true");
  // El mapa de sombras como lo necesita la GPU de Apple: el pase que pinta los coches encima del mundo
  // carga lo que hay (no DONT_CARE) y no se intercambia al resolver (sin la copia que lo restaura). Sin
  // las dos cosas salen recuadros sin sombra alrededor del coche. El codigo ya trae esos valores en iOS,
  // pero nfsmw.toml (el del APK) pone pase_sombras_sin_load = true y los argumentos mandan sobre el.
  poner(@"--nfsmw_nativo_pase_sombras_sin_load=false");
  poner(@"--nfsmw_nativo_resolver_sin_copia=false");
  poner([Antialiasing() isEqualToString:AA_FXAA] ? @"--nfsmw_antialiasing=fxaa" : @"--nfsmw_antialiasing=apagado");
  poner([NSString stringWithFormat:@"--nfsmw_nativo_anisotropico=%d", Anisotropico(LeerInt(@"anisotropico", 3))]);
  poner([@"--nfsmw_tratamiento_visual=" stringByAppendingString:LeerTexto(@"filtro_color", @"original")]);
  poner([@"--nfsmw_nativo_sin_desenfoque=" stringByAppendingString:bandera(!LeerBool(@"desenfoque", false))]);
  poner([@"--nfsmw_posproceso=" stringByAppendingString:LeerTexto(@"filtro_imagen", @"apagado")]);
  NSString* sombras = LeerTexto(@"calidad_sombras", SOMBRAS_RAPIDA);
  poner([@"--nfsmw_nativo_pcf_barato=" stringByAppendingString:bandera([sombras isEqualToString:SOMBRAS_RAPIDA])]);
  poner([@"--nfsmw_sombras_sin_vegetacion=" stringByAppendingString:bandera(![sombras isEqualToString:SOMBRAS_XBOX])]);
  poner([NSString stringWithFormat:@"--nfsmw_nativo_sombras_escala=%d", LeerInt(@"sombras_escala", 100)]);
  // Siempre cada fotograma: cada 2 deja el mapa un fotograma por detras del coche.
  poner(@"--nfsmw_sombras_cada=1");
  poner([NSString stringWithFormat:@"--nfsmw_sombras_corte=%d", LeerInt(@"sombras_corte", 150)]);
  poner([NSString stringWithFormat:@"--nfsmw_cubemap_caras_max=%d", LeerInt(@"reflejos_coche", 6)]);
  poner([@"--nfsmw_reflejo_carretera=" stringByAppendingString:bandera(LeerBool(@"reflejo_asfalto", true))]);
  poner([@"--nfsmw_resplandor_cielo=" stringByAppendingString:LeerTexto(@"resplandor_cielo", @"natural")]);
  // El volumen del juego, siempre el original: por encima de 100 su limitador lo comprime.
  poner(@"--audio_ganancia_pct=100");
  // La vibracion del juego, a los mandos fisicos y, con el mando tactil, al movil.
  poner([@"--input_vibracion=" stringByAppendingString:bandera(LeerBool(@"vibracion", true))]);
  // La salida de audio: AVAudioEngine (audio_ios.mm) o la de SDL de su SDK.
  poner([@"--nfsmw_audio_ios=" stringByAppendingString:bandera(LeerBool(@"audio_avaudio", true))]);
  // La matematica rapida de Metal en los shaders (vulkan_instance.cpp): apagada, como en Android.
  poner([@"--moltenvk_matematica_rapida=" stringByAppendingString:bandera(LeerBool(@"metal_rapida", false))]);
  poner(@"--nfsmw_renderizador=nativo");

  // El idioma y el pais de la consola, los de la edicion de esta app.
  if (edicion.idioma >= 0) poner([NSString stringWithFormat:@"--user_language=%d", edicion.idioma]);
  if (edicion.pais >= 0) poner([NSString stringWithFormat:@"--user_country=%d", edicion.pais]);
  NSString* gamertag = LeerTexto(@"gamertag", @"");
  if (gamertag.length && GamertagValido(gamertag)) {
    // Un solo argumento aunque lleve espacios: va en el vector, sin shell.
    poner([@"--user_gamertag=" stringByAppendingString:gamertag]);
  }
  // Xbox Live del juego contra el servidor de EA de la LAN: los permisos de Live, la red de verdad,
  // la sesion de Live y todos los nombres (los servidores de EA) a esa IP.
  if (redDns.length) {
    poner(@"--grant_user_privileges=true");
    poner(@"--red_lan=true");
    poner(@"--red_live=true");
    poner([@"--red_dns=" stringByAppendingString:redDns]);
    // Diagnostico del multijugador: las llamadas de red al log.
    if (LeerBool(@"registro", false)) poner(@"--red_registro=true");
  }
  poner(LeerBool(@"registro", false) ? @"--log_level=debug" : @"--log_level=info");
  // Pruebas: Documents/extra.txt (se sube por afcclient). Cada linea "--cvar=valor" va al final de los
  // argumentos; "NOMBRE=valor" es una variable de entorno (las MVK_CONFIG_* de MoltenVK).
  NSString* docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
  NSString* extra = [NSString stringWithContentsOfFile:[docs stringByAppendingPathComponent:@"extra.txt"]
                                              encoding:NSUTF8StringEncoding error:nil];
  for (NSString* bruta in [extra componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet]) {
    NSString* l = [bruta stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet];
    if (!l.length || [l hasPrefix:@"#"]) continue;
    if ([l hasPrefix:@"--"]) {
      poner(l);
    } else {
      NSRange igual = [l rangeOfString:@"="];
      if (igual.location != NSNotFound) {
        setenv([l substringToIndex:igual.location].UTF8String, [l substringFromIndex:igual.location + 1].UTF8String, 1);
      }
    }
    NSLog(@"[extra] %@", l);
  }
  return a;
}

// ---------------------------------------------------------------------------------------------
//  Piezas de la pantalla (los equivalentes de las de SetupActivity)
// ---------------------------------------------------------------------------------------------

UILabel* Etiqueta(NSString* texto, CGFloat tam, UIFontWeight peso, uint32_t color) {
  UILabel* l = [UILabel new];
  l.text = texto;
  l.font = [UIFont systemFontOfSize:tam weight:peso];
  l.textColor = Color(color);
  l.numberOfLines = 0;
  return l;
}

UILabel* Resumen(NSString* texto) { return Etiqueta(texto, 13, UIFontWeightRegular, GRIS); }

// Una columna con margen arriba y abajo, como el padding de un LinearLayout.
UIStackView* Columna(NSArray<UIView*>* vistas, CGFloat arriba = 0, CGFloat abajo = 0, CGFloat entre = 0) {
  UIStackView* s = [[UIStackView alloc] initWithArrangedSubviews:vistas];
  s.axis = UILayoutConstraintAxisVertical;
  s.alignment = UIStackViewAlignmentFill;
  s.spacing = entre;
  s.layoutMarginsRelativeArrangement = YES;
  s.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(arriba, 0, abajo, 0);
  return s;
}

UIView* Titulo(NSString* texto) { return Columna(@[Etiqueta(texto, 16, UIFontWeightBold, BLANCO)], 12); }

UIView* Espacio(CGFloat alto) {
  UIView* v = [UIView new];
  [v.heightAnchor constraintEqualToConstant:alto].active = YES;
  return v;
}

UIView* Seccion(NSString* texto) {
  UIView* raya = [UIView new];
  raya.backgroundColor = Color(ACENTO);
  [raya.heightAnchor constraintEqualToConstant:2].active = YES;
  return Columna(@[Etiqueta(texto, 22, UIFontWeightBold, BLANCO), raya], 8, 16, 8);
}

UIView* Tarjeta(NSArray<UIView*>* vistas) {
  UIView* t = [UIView new];
  t.backgroundColor = Color(TARJETA);
  t.layer.cornerRadius = 16;
  UIStackView* s = Columna(vistas);
  s.translatesAutoresizingMaskIntoConstraints = NO;
  [t addSubview:s];
  [NSLayoutConstraint activateConstraints:@[
    [s.leadingAnchor constraintEqualToAnchor:t.leadingAnchor constant:16],
    [s.trailingAnchor constraintEqualToAnchor:t.trailingAnchor constant:-16],
    [s.topAnchor constraintEqualToAnchor:t.topAnchor constant:16],
    [s.bottomAnchor constraintEqualToAnchor:t.bottomAnchor constant:-16],
  ]];
  return t;
}

UIConfigurationTextAttributesTransformer Letra(UIFont* letra) {
  return ^NSDictionary<NSAttributedStringKey, id>*(NSDictionary<NSAttributedStringKey, id>* entrada) {
    NSMutableDictionary* d = [entrada mutableCopy];
    d[NSFontAttributeName] = letra;
    return d;
  };
}

UIButton* Boton(NSString* texto, uint32_t fondo, uint32_t letra, void (^accion)(void)) {
  UIButtonConfiguration* c = [UIButtonConfiguration filledButtonConfiguration];
  c.title = texto;
  c.baseBackgroundColor = Color(fondo);
  c.baseForegroundColor = Color(letra);
  c.background.cornerRadius = 10;
  c.cornerStyle = UIButtonConfigurationCornerStyleFixed;
  c.contentInsets = NSDirectionalEdgeInsetsMake(12, 16, 12, 16);
  c.titleTextAttributesTransformer = Letra([UIFont systemFontOfSize:15 weight:UIFontWeightMedium]);
  return [UIButton buttonWithConfiguration:c primaryAction:[UIAction actionWithHandler:^(UIAction*) { accion(); }]];
}

UIView* Interruptor(NSString* titulo, NSString* resumen, bool valor, void (^guardar)(bool),
                    UISwitch* __strong* salida = nullptr) {
  UILabel* l = Etiqueta(titulo, 16, UIFontWeightRegular, BLANCO);
  UISwitch* s = [UISwitch new];
  s.on = valor;
  s.onTintColor = Color(ACENTO);
  [s addAction:[UIAction actionWithHandler:^(UIAction* a) { guardar(((UISwitch*)a.sender).isOn); }]
      forControlEvents:UIControlEventValueChanged];
  [s setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
  [s setContentCompressionResistancePriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView* fila = [[UIStackView alloc] initWithArrangedSubviews:@[l, s]];
  fila.axis = UILayoutConstraintAxisHorizontal;
  fila.alignment = UIStackViewAlignmentCenter;
  fila.spacing = 12;
  NSMutableArray* vistas = [NSMutableArray arrayWithObject:fila];
  if (resumen.length) [vistas addObject:Resumen(resumen)];
  if (salida) *salida = s;
  return Columna(vistas, 10, 6, 2);
}

UIImage* Circulo(bool elegido) {
  UIImage* i = [UIImage systemImageNamed:elegido ? @"largecircle.fill.circle" : @"circle"];
  return [i imageWithTintColor:Color(elegido ? ACENTO_CLARO : 0xFF9E9E9E) renderingMode:UIImageRenderingModeAlwaysOriginal];
}

// Opciones a la vista: en fila (UISegmentedControl) si caben, como en Android; si no, en columna.
UIView* Selector(NSString* titulo, NSString* resumen, NSArray<NSString*>* textos, NSArray* valores, id actual,
                 bool horizontal, void (^guardar)(id)) {
  NSMutableArray* vistas = [NSMutableArray array];
  if (titulo.length) [vistas addObject:Titulo(titulo)];
  if (resumen.length) [vistas addObject:Resumen(resumen)];
  NSUInteger total = 0, mayor = 0;
  for (NSString* t in textos) {
    total += t.length;
    mayor = std::max(mayor, t.length);
  }
  const NSUInteger elegido = [valores indexOfObject:actual];
  if (horizontal && total <= 30 && mayor <= 14) {
    UISegmentedControl* sc = [[UISegmentedControl alloc] initWithItems:textos];
    sc.selectedSegmentIndex = elegido == NSNotFound ? UISegmentedControlNoSegment : NSInteger(elegido);
    sc.selectedSegmentTintColor = Color(ACENTO);
    sc.backgroundColor = Color(BOTON);
    [sc setTitleTextAttributes:@{NSForegroundColorAttributeName : UIColor.whiteColor} forState:UIControlStateNormal];
    [sc addAction:[UIAction actionWithHandler:^(UIAction* a) {
          const NSInteger i = ((UISegmentedControl*)a.sender).selectedSegmentIndex;
          if (i >= 0 && NSUInteger(i) < valores.count) guardar(valores[i]);
        }]
        forControlEvents:UIControlEventValueChanged];
    [vistas addObject:Columna(@[sc], 8)];
  } else {
    UIStackView* opciones = Columna(@[], 4);
    __weak UIStackView* columna = opciones;
    for (NSUInteger i = 0; i < textos.count; ++i) {
      UIButtonConfiguration* c = [UIButtonConfiguration plainButtonConfiguration];
      c.title = textos[i];
      c.image = Circulo(i == elegido);
      c.imagePadding = 10;
      c.baseForegroundColor = UIColor.whiteColor;
      c.contentInsets = NSDirectionalEdgeInsetsMake(7, 0, 7, 0);
      c.titleTextAttributesTransformer = Letra([UIFont systemFontOfSize:15]);
      UIButton* b = [UIButton buttonWithConfiguration:c
                                        primaryAction:[UIAction actionWithHandler:^(UIAction*) {
                                          NSArray* botones = columna.arrangedSubviews;
                                          for (NSUInteger k = 0; k < botones.count; ++k) {
                                            UIButton* o = botones[k];
                                            UIButtonConfiguration* oc = o.configuration;
                                            oc.image = Circulo(k == i);
                                            o.configuration = oc;
                                          }
                                          guardar(valores[i]);
                                        }]];
      b.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;
      [opciones addArrangedSubview:b];
    }
    [vistas addObject:opciones];
  }
  return Columna(vistas, 12, 12);
}

// Como Selector, pero en un menu desplegable: para las listas largas.
UIView* Desplegable(NSString* titulo, NSString* resumen, NSArray<NSString*>* textos, NSArray* valores, id actual,
                    void (^guardar)(id)) {
  NSMutableArray* vistas = [NSMutableArray array];
  if (titulo.length) [vistas addObject:Titulo(titulo)];
  if (resumen.length) [vistas addObject:Resumen(resumen)];
  NSUInteger elegido = [valores indexOfObject:actual];
  if (elegido == NSNotFound) elegido = 0;
  NSMutableArray<UIMenuElement*>* acciones = [NSMutableArray array];
  for (NSUInteger i = 0; i < textos.count; ++i) {
    UIAction* a = [UIAction actionWithTitle:textos[i] image:nil identifier:nil handler:^(UIAction*) { guardar(valores[i]); }];
    a.state = i == elegido ? UIMenuElementStateOn : UIMenuElementStateOff;
    [acciones addObject:a];
  }
  UIButtonConfiguration* c = [UIButtonConfiguration filledButtonConfiguration];
  c.baseBackgroundColor = Color(BOTON);
  c.baseForegroundColor = UIColor.whiteColor;
  c.indicator = UIButtonConfigurationIndicatorPopup;
  c.contentInsets = NSDirectionalEdgeInsetsMake(12, 16, 12, 16);
  c.titleTextAttributesTransformer = Letra([UIFont systemFontOfSize:15]);
  UIButton* b = [UIButton buttonWithConfiguration:c primaryAction:nil];
  b.menu = [UIMenu menuWithChildren:acciones];
  b.showsMenuAsPrimaryAction = YES;
  b.changesSelectionAsPrimaryAction = YES;
  b.contentHorizontalAlignment = UIControlContentHorizontalAlignmentFill;
  [vistas addObject:Columna(@[b], 8)];
  return Columna(vistas, 12, 12);
}

// Un deslizador de enteros, con su valor escrito debajo del titulo. Se guarda en cada paso.
UIView* Deslizador(NSString* titulo, NSString* resumen, int min, int max, int actual, NSString* (^texto)(int),
                   void (^guardar)(int)) {
  UILabel* valor = Etiqueta(texto(actual), 13, UIFontWeightRegular, BLANCO);
  UISlider* s = [UISlider new];
  s.minimumValue = min;
  s.maximumValue = max;
  s.value = std::max(min, std::min(max, actual));
  s.minimumTrackTintColor = Color(ACENTO);
  s.maximumTrackTintColor = Color(0xFF303030);
  __block int ultimo = int(s.value);
  [s addAction:[UIAction actionWithHandler:^(UIAction* a) {
       UISlider* d = (UISlider*)a.sender;
       const int n = int(std::lround(d.value));
       d.value = n;
       if (n == ultimo) return;
       ultimo = n;
       valor.text = texto(n);
       guardar(n);
     }]
      forControlEvents:UIControlEventValueChanged];
  NSMutableArray* vistas = [NSMutableArray arrayWithArray:@[Titulo(titulo), valor, Columna(@[s], 4, 4)]];
  if (resumen.length) [vistas addObject:Resumen(resumen)];
  return Columna(vistas, 6, 6);
}

UIButton* BotonIcono(NSString* simbolo, NSString* descripcion, void (^accion)(void)) {
  UIButtonConfiguration* c = [UIButtonConfiguration plainButtonConfiguration];
  c.image = [UIImage systemImageNamed:simbolo];
  c.baseForegroundColor = Color(0xFFD0D0D0);
  UIButton* b = [UIButton buttonWithConfiguration:c primaryAction:[UIAction actionWithHandler:^(UIAction*) { accion(); }]];
  b.accessibilityLabel = descripcion;
  [b.widthAnchor constraintEqualToConstant:44].active = YES;
  [b.heightAnchor constraintEqualToConstant:44].active = YES;
  return b;
}

// Botones en fila, cada uno de su tamano, pegados a la izquierda.
UIStackView* FilaBotones(NSArray<UIView*>* botones) {
  for (UIView* b in botones) {
    [b setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
  }
  UIStackView* fila = [[UIStackView alloc] initWithArrangedSubviews:[botones arrayByAddingObject:[UIView new]]];
  fila.axis = UILayoutConstraintAxisHorizontal;
  fila.spacing = 8;
  return fila;
}

// Un campo de texto oscuro con su etiqueta encima (el hint de Android).
UITextField* Campo(NSString* texto, NSString* ejemplo) {
  UITextField* c = [UITextField new];
  c.text = texto;
  c.placeholder = ejemplo;
  c.borderStyle = UITextBorderStyleRoundedRect;
  c.font = [UIFont systemFontOfSize:16];
  c.textColor = UIColor.whiteColor;
  c.autocorrectionType = UITextAutocorrectionTypeNo;
  c.spellCheckingType = UITextSpellCheckingTypeNo;
  [c.heightAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
  return c;
}

UIScrollView* Panel(NSArray<UIView*>* tarjetas) {
  UIScrollView* sv = [UIScrollView new];
  sv.translatesAutoresizingMaskIntoConstraints = NO;
  sv.keyboardDismissMode = UIScrollViewKeyboardDismissModeInteractive;
  sv.alwaysBounceVertical = YES;
  UIStackView* col = Columna(tarjetas, 8, 12, 8);
  col.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(8, 12, 12, 12);
  col.translatesAutoresizingMaskIntoConstraints = NO;
  [sv addSubview:col];
  [NSLayoutConstraint activateConstraints:@[
    [col.leadingAnchor constraintEqualToAnchor:sv.contentLayoutGuide.leadingAnchor],
    [col.trailingAnchor constraintEqualToAnchor:sv.contentLayoutGuide.trailingAnchor],
    [col.topAnchor constraintEqualToAnchor:sv.contentLayoutGuide.topAnchor],
    [col.bottomAnchor constraintEqualToAnchor:sv.contentLayoutGuide.bottomAnchor],
    [col.widthAnchor constraintEqualToAnchor:sv.frameLayoutGuide.widthAnchor],
  ]];
  return sv;
}

NSString* T(NSString* clave) { return NfsmwTexto(clave); }

// Las secciones del menu lateral, en su orden.
enum { S_JUEGO, S_PANTALLA, S_GRAFICOS, S_MULTIJUGADOR, S_CONTROLES, S_IDIOMA, S_OTRAS, S_AVANZADO, SECCIONES };
NSString* const NOMBRES_SECCION[] = {@"seccion_juego",        @"seccion_pantalla",  @"seccion_graficos",
                                     @"seccion_multijugador", @"seccion_controles", @"seccion_idioma",
                                     @"seccion_otras",        @"seccion_avanzado"};
NSString* const ICONOS_SECCION[] = {@"gamecontroller.fill", @"rectangle.inset.filled", @"paintpalette.fill",
                                    @"person.2.fill",       @"hand.tap.fill",          @"globe",
                                    @"gearshape.fill",      @"wrench.and.screwdriver.fill"};

}  // namespace

NSString* NfsmwTexto(NSString* clave) {
  if (!g_tabla) CargarTextos();
  return g_tabla[clave] ?: g_ingles[clave] ?: clave;
}

// =============================================================================================
//  La pantalla
// =============================================================================================

@interface NfsmwLanzador : UIViewController <UIDocumentPickerDelegate, UITextFieldDelegate>
@property(nonatomic, readonly) BOOL terminado;
- (instancetype)initConArgv0:(std::string)argv0;
- (std::vector<std::string>)argumentos;
@end

@implementation NfsmwLanzador {
  std::string argv0_;
  std::vector<std::string> argumentos_;
  int seccion_;
  NSMutableArray<UIView*>* paneles_;
  NSMutableArray<UIButton*>* entradas_;
  UIButton* jugar_;
  UILabel* textoIso_;
  UILabel* textoEdicion_;
  UILabel* avisoJugar_;
  UILabel* avisoAcceso_;
  UILabel* avisoBiblioteca_;
  // Partidas guardadas (partidas.h): lo que se hizo, y para que se abrio el selector de documentos
  // (la ISO, el .zip exportado que se va a guardar o el que se va a importar).
  UILabel* textoPartidas_;
  enum class Eleccion { ISO, EXPORTAR, IMPORTAR } selector_;
  NSURL* exportado_;
  nfsmw::partidas::Resultado exportadas_;
  UILabel* errorGamertag_;
  UILabel* vibracionProbada_;
  Edicion edicion_;
  Juego juego_;
  Mirada mirada_;
  int miradas_;
  UIView* contenido_;
  UIView* aviso_;
  // El gamertag: si esta libre en el servidor de la LAN (comprobarGamertag). `comprobaciones_` cuenta
  // las consultas para tirar las respuestas que llegan tarde.
  UILabel* comprobacion_;
  BOOL gamertagEscritoValido_;
  BOOL comprobandoParaJugar_;
  int comprobaciones_;
  NSTimer* comprobacionPendiente_;
  NSTimer* renovarReserva_;
  // El servidor de la LAN: la lista de guardados y, con este telefono de anfitrion, en su lugar la IP
  // que ponen los demas.
  UISwitch* interruptorAnfitrion_;
  UILabel* textoAnfitrion_;
  UIView* bloqueServidores_;
  UIStackView* listaServidores_;
  UIStackView* listaEncontrados_;
  UIView* filaBotonesServidores_;
  UIButton* botonBuscar_;
  // Lo que contesto la ultima busqueda (sin valor = no se ha buscado o ya se eligio uno), si este
  // telefono no estaba en ninguna red y si hay una en marcha.
  std::optional<std::vector<nfsmw::red::Encontrado>> encontrados_;
  BOOL busquedaSinRed_;
  BOOL buscando_;
  // El formulario de anadir o editar un servidor, metido en la lista; nil = cerrado.
  // direccionEditada_: la del que se edita, "" = uno nuevo.
  UIView* editorServidor_;
  NSString* direccionEditada_;
  __weak UITextField* ipEditor_;
  __weak UITextField* nombreEditor_;
  void (^aceptarEditor_)(void);
}

- (instancetype)initConArgv0:(std::string)argv0 {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    argv0_ = argv0;
    seccion_ = S_JUEGO;
    edicion_ = LeerEdicion();
    miradas_ = 0;
    gamertagEscritoValido_ = GamertagValido(LeerTexto(@"gamertag", @""));
    // El servidor de este telefono, si es el anfitrion: arrancarlo dos veces no hace nada.
    if (LeerBool(@"anfitrion_lan", false)) [self arrancarAnfitrion];
  }
  return self;
}

// Cada consulta que sale libre aparta el nombre para este telefono unos minutos (10 en el
// servidor); mientras esta pantalla se ve, se renueva cada minuto.
- (void)viewDidAppear:(BOOL)animado {
  [super viewDidAppear:animado];
  [renovarReserva_ invalidate];
  __weak NfsmwLanzador* yo = self;
  renovarReserva_ = [NSTimer scheduledTimerWithTimeInterval:60
                                                    repeats:YES
                                                      block:^(NSTimer*) {
                                                        [yo comprobarGamertag];
                                                      }];
}

- (std::vector<std::string>)argumentos {
  return argumentos_;
}

- (BOOL)prefersStatusBarHidden {
  return YES;
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskLandscape;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self construir];
}

// En horizontal: a la izquierda el menu de secciones con Jugar debajo, que se ve desde todas; a
// la derecha, la seccion elegida. Se rehace entera al cambiar de idioma o aplicar un preajuste.
- (void)construir {
  CargarTextos();
  for (UIView* v in self.view.subviews) [v removeFromSuperview];
  UIView* raiz = self.view;
  raiz.backgroundColor = Color(FONDO);
  UILayoutGuide* segura = raiz.safeAreaLayoutGuide;

  // --- Menu lateral
  UIView* menu = [UIView new];
  menu.backgroundColor = Color(FONDO_MENU);
  menu.translatesAutoresizingMaskIntoConstraints = NO;
  [raiz addSubview:menu];

  UILabel* titulo = Etiqueta(T(@"app_name"), 18, UIFontWeightBold, BLANCO);
  UIStackView* lista = Columna(@[], 0, 0, 2);
  entradas_ = [NSMutableArray array];
  for (int i = 0; i < SECCIONES; ++i) {
    UIButtonConfiguration* c = [UIButtonConfiguration plainButtonConfiguration];
    c.title = T(NOMBRES_SECCION[i]);
    c.image = [UIImage systemImageNamed:ICONOS_SECCION[i]
                      withConfiguration:[UIImageSymbolConfiguration configurationWithPointSize:17]];
    c.imagePadding = 14;
    c.contentInsets = NSDirectionalEdgeInsetsMake(12, 12, 12, 12);
    c.background.cornerRadius = 12;
    c.titleLineBreakMode = NSLineBreakByWordWrapping;
    __weak NfsmwLanzador* yo = self;
    UIButton* b = [UIButton buttonWithConfiguration:c
                                      primaryAction:[UIAction actionWithHandler:^(UIAction*) {
                                        [yo mostrarSeccion:i];
                                      }]];
    b.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;
    [entradas_ addObject:b];
    [lista addArrangedSubview:b];
  }
  UIScrollView* desplazarMenu = [UIScrollView new];
  desplazarMenu.showsVerticalScrollIndicator = NO;
  lista.translatesAutoresizingMaskIntoConstraints = NO;
  [desplazarMenu addSubview:lista];
  [NSLayoutConstraint activateConstraints:@[
    [lista.leadingAnchor constraintEqualToAnchor:desplazarMenu.contentLayoutGuide.leadingAnchor],
    [lista.trailingAnchor constraintEqualToAnchor:desplazarMenu.contentLayoutGuide.trailingAnchor],
    [lista.topAnchor constraintEqualToAnchor:desplazarMenu.contentLayoutGuide.topAnchor],
    [lista.bottomAnchor constraintEqualToAnchor:desplazarMenu.contentLayoutGuide.bottomAnchor],
    [lista.widthAnchor constraintEqualToAnchor:desplazarMenu.frameLayoutGuide.widthAnchor],
  ]];

  __weak NfsmwLanzador* yo = self;
  UIButtonConfiguration* cj = [UIButtonConfiguration filledButtonConfiguration];
  cj.title = T(@"jugar");
  cj.image = [UIImage systemImageNamed:@"play.fill"];
  cj.imagePadding = 8;
  cj.cornerStyle = UIButtonConfigurationCornerStyleFixed;
  cj.background.cornerRadius = 12;
  cj.contentInsets = NSDirectionalEdgeInsetsMake(16, 12, 16, 12);
  cj.titleTextAttributesTransformer = Letra([UIFont systemFontOfSize:18 weight:UIFontWeightBold]);
  jugar_ = [UIButton buttonWithConfiguration:cj primaryAction:[UIAction actionWithHandler:^(UIAction*) { [yo jugar]; }]];
  jugar_.configurationUpdateHandler = ^(UIButton* b) {
    UIButtonConfiguration* c = b.configuration;
    c.baseBackgroundColor = Color(b.enabled ? ACENTO : 0xFF3A3A3A);
    c.baseForegroundColor = Color(b.enabled ? 0xFF121212 : 0xFF808080);
    b.configuration = c;
  };

  UIStackView* columna = [[UIStackView alloc] initWithArrangedSubviews:@[Columna(@[titulo], 0, 12), desplazarMenu,
                                                                        Columna(@[jugar_], 8)]];
  columna.axis = UILayoutConstraintAxisVertical;
  columna.translatesAutoresizingMaskIntoConstraints = NO;
  [menu addSubview:columna];

  // --- La seccion elegida. Todas se construyen al empezar y solo se ve una.
  UIView* contenido = [UIView new];
  contenido_ = contenido;
  aviso_ = nil;
  contenido.translatesAutoresizingMaskIntoConstraints = NO;
  [raiz addSubview:contenido];
  paneles_ = [NSMutableArray arrayWithArray:@[
    Panel(@[[self tarjetaJuego]]), Panel(@[[self tarjetaPantalla]]), Panel(@[[self tarjetaGraficos]]),
    Panel(@[[self tarjetaPerfil], [self tarjetaServidorLan]]), Panel(@[[self tarjetaControles]]),
    Panel(@[[self tarjetaIdioma]]), Panel(@[[self tarjetaSonido]]),
    Panel(@[[self tarjetaAvanzado]])
  ]];
  for (UIView* p in paneles_) {
    [contenido addSubview:p];
    [NSLayoutConstraint activateConstraints:@[
      [p.leadingAnchor constraintEqualToAnchor:contenido.leadingAnchor],
      [p.trailingAnchor constraintEqualToAnchor:contenido.trailingAnchor],
      [p.topAnchor constraintEqualToAnchor:contenido.topAnchor],
      [p.bottomAnchor constraintEqualToAnchor:contenido.bottomAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:@[
    [menu.leadingAnchor constraintEqualToAnchor:raiz.leadingAnchor],
    [menu.topAnchor constraintEqualToAnchor:raiz.topAnchor],
    [menu.bottomAnchor constraintEqualToAnchor:raiz.bottomAnchor],
    [columna.leadingAnchor constraintEqualToAnchor:segura.leadingAnchor constant:12],
    [columna.widthAnchor constraintEqualToConstant:196],
    [menu.trailingAnchor constraintEqualToAnchor:columna.trailingAnchor constant:12],
    [columna.topAnchor constraintEqualToAnchor:segura.topAnchor constant:16],
    [columna.bottomAnchor constraintEqualToAnchor:segura.bottomAnchor constant:-12],
    [contenido.leadingAnchor constraintEqualToAnchor:menu.trailingAnchor],
    [contenido.trailingAnchor constraintEqualToAnchor:segura.trailingAnchor],
    [contenido.topAnchor constraintEqualToAnchor:segura.topAnchor],
    // El teclado (el del gamertag) solo encoge la seccion: el menu se queda debajo de el.
    [contenido.bottomAnchor constraintEqualToAnchor:raiz.keyboardLayoutGuide.topAnchor],
  ]];
  [self mostrarSeccion:seccion_];
  [self refrescar];
  [self programarComprobacion];
}

- (void)mostrarSeccion:(int)i {
  [self.view endEditing:YES];
  seccion_ = i;
  for (int j = 0; j < SECCIONES; ++j) {
    const bool esta = j == i;
    paneles_[j].hidden = !esta;
    UIButton* b = entradas_[j];
    UIButtonConfiguration* c = b.configuration;
    c.background.backgroundColor = esta ? Color(ELEGIDO) : UIColor.clearColor;
    c.baseForegroundColor = Color(esta ? BLANCO : 0xFFC8C8C8);
    c.image = [c.image imageWithTintColor:Color(esta ? ACENTO_CLARO : GRIS) renderingMode:UIImageRenderingModeAlwaysOriginal];
    c.titleTextAttributesTransformer = Letra([UIFont systemFontOfSize:15 weight:esta ? UIFontWeightBold : UIFontWeightRegular]);
    b.configuration = c;
  }
}

// ---- Juego

- (UIView*)tarjetaJuego {
  textoIso_ = Etiqueta(@"", 16, UIFontWeightRegular, BLANCO);
  textoEdicion_ = Resumen(@"");
  __weak NfsmwLanzador* yo = self;
  UIButton* elegir = Boton(T(@"elegir_iso"), BOTON, BLANCO, ^{ [yo elegirIso]; });
  elegir.contentHorizontalAlignment = UIControlContentHorizontalAlignmentLeading;
  avisoAcceso_ = Etiqueta(T(@"ios_iso_sin_acceso"), 13, UIFontWeightRegular, AVISO);
  // Por que Jugar, en el menu, no se puede pulsar.
  avisoJugar_ = Etiqueta(T(@"jugar_sin_iso"), 13, UIFontWeightRegular, AVISO);
  avisoBiblioteca_ = Etiqueta(T(@"ios_sin_biblioteca"), 13, UIFontWeightRegular, AVISO);
  UIStackView* boton = FilaBotones(@[elegir]);
  // Partidas guardadas: exportar a un .zip e importar uno (las de Android valen).
  UIButton* exportar = Boton(T(@"partidas_exportar"), BOTON, BLANCO, ^{ [yo exportarPartidas]; });
  UIButton* importar = Boton(T(@"partidas_importar"), BOTON, BLANCO, ^{ [yo importarPartidas]; });
  textoPartidas_ = Etiqueta(@"", 14, UIFontWeightRegular, BLANCO);
  textoPartidas_.hidden = YES;
  return Tarjeta(@[
    Seccion(T(@"seccion_juego")), Columna(@[Resumen(T(@"ios_app_con_juego"))], 0, 12), textoIso_,
    Columna(@[textoEdicion_], 4), Espacio(12), boton, Columna(@[Resumen(T(@"ios_ayuda_iso"))], 8),
    Columna(@[avisoAcceso_, avisoJugar_, avisoBiblioteca_], 8, 0, 6),
    Columna(@[Resumen(T(@"ios_cerrar_para_ajustes"))], 12),
    Columna(@[Titulo(T(@"seccion_partidas")), Resumen(T(@"partidas_resumen"))], 20, 0, 4), Espacio(10),
    FilaBotones(@[exportar, importar]), Columna(@[textoPartidas_], 8)
  ]);
}

// ---- Partidas guardadas (Partidas.java en Android)

// La carpeta donde guarda el motor (user_data_root, nfsmw_app.h) y la version, que va en la marca.
NSString* CarpetaPartidas() { return [Documentos() stringByAppendingPathComponent:@"datos"]; }
std::string VersionApp() {
  NSString* v = NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"];
  return v ? v.UTF8String : "";
}
NSString* Tamano(uint64_t bytes) {
  return [NSByteCountFormatter stringFromByteCount:(long long)bytes countStyle:NSByteCountFormatterCountStyleFile];
}

- (void)textoPartidas:(NSString*)texto {
  textoPartidas_.text = texto;
  textoPartidas_.hidden = texto.length == 0;
}

// El .zip se hace en tmp y luego el selector de iOS lo guarda donde diga el jugador (Archivos, iCloud).
- (void)exportarPartidas {
  const std::string carpeta = CarpetaPartidas().UTF8String;
  if (!nfsmw::partidas::Hay(carpeta)) {
    [self textoPartidas:T(@"partidas_ninguna")];
    return;
  }
  [self textoPartidas:T(@"partidas_exportando")];
  NSString* ruta = [NSTemporaryDirectory()
      stringByAppendingPathComponent:[NSString stringWithUTF8String:nfsmw::partidas::NombreSugerido().c_str()]];
  __weak NfsmwLanzador* yo = self;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    nfsmw::partidas::Resultado r;
    std::string error;
    const bool bien = nfsmw::partidas::Exportar(carpeta, ruta.UTF8String, VersionApp(), r, error);
    NSString* fallo = bien ? nil : [NSString stringWithUTF8String:error.c_str()];
    dispatch_async(dispatch_get_main_queue(), ^{
      [yo exportado:bien ? [NSURL fileURLWithPath:ruta] : nil resultado:r fallo:fallo];
    });
  });
}

- (void)exportado:(NSURL*)url resultado:(nfsmw::partidas::Resultado)r fallo:(NSString*)fallo {
  if (!url) {
    [self textoPartidas:NfsmwTextoF(@"partidas_error", fallo ?: @"")];
    return;
  }
  exportado_ = url;
  exportadas_ = r;
  selector_ = Eleccion::EXPORTAR;
  UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[url] asCopy:YES];
  p.delegate = self;
  [self presentViewController:p animated:YES completion:nil];
}

- (void)importarPartidas {
  selector_ = Eleccion::IMPORTAR;
  UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeZIP]
                                                                                                asCopy:YES];
  p.delegate = self;
  p.allowsMultipleSelection = NO;
  [self presentViewController:p animated:YES completion:nil];
}

// Las de ahora se sustituyen: se pregunta antes, diciendo donde queda la copia.
- (void)confirmarImportar:(NSURL*)url {
  // Como la ensena Archivos: la carpeta de la app lleva su nombre.
  NSString* app = NSBundle.mainBundle.infoDictionary[@"CFBundleDisplayName"] ?: @"NFSMW";
  NSString* copia = [app stringByAppendingString:@" › partidas_antes_de_importar.zip"];
  UIAlertController* a = [UIAlertController alertControllerWithTitle:T(@"partidas_importar_titulo")
                                                             message:NfsmwTextoF(@"partidas_importar_aviso", copia)
                                                      preferredStyle:UIAlertControllerStyleAlert];
  __weak NfsmwLanzador* yo = self;
  [a addAction:[UIAlertAction actionWithTitle:T(@"editor_cancelar").localizedCapitalizedString
                                        style:UIAlertActionStyleCancel
                                      handler:^(UIAlertAction*) {
                                        [NSFileManager.defaultManager removeItemAtURL:url error:nil];
                                      }]];
  [a addAction:[UIAlertAction actionWithTitle:T(@"partidas_importar")
                                        style:UIAlertActionStyleDestructive
                                      handler:^(UIAlertAction*) { [yo importar:url]; }]];
  [self presentViewController:a animated:YES completion:nil];
}

- (void)importar:(NSURL*)url {
  [self textoPartidas:T(@"partidas_importando")];
  const std::string carpeta = CarpetaPartidas().UTF8String;
  const std::string copia = [Documentos() stringByAppendingPathComponent:@"partidas_antes_de_importar.zip"].UTF8String;
  __weak NfsmwLanzador* yo = self;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    nfsmw::partidas::Resultado r;
    std::string error;
    const auto hecho = nfsmw::partidas::Importar(carpeta, url.path.UTF8String, copia, VersionApp(), r, error);
    [NSFileManager.defaultManager removeItemAtURL:url error:nil];  // la copia que hizo el selector
    NSString* texto = hecho == nfsmw::partidas::Importado::kBien
                          ? NfsmwTextoF(@"partidas_importadas", r.archivos, Tamano(r.bytes))
                      : hecho == nfsmw::partidas::Importado::kNoValido
                          ? T(@"partidas_no_valido")
                          : NfsmwTextoF(@"partidas_error", [NSString stringWithUTF8String:error.c_str()]);
    dispatch_async(dispatch_get_main_queue(), ^{ [yo textoPartidas:texto]; });
  });
}

- (void)elegirIso {
  selector_ = Eleccion::ISO;
  UIDocumentPickerViewController* p = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeData]
                                                                                                asCopy:NO];
  p.delegate = self;
  p.allowsMultipleSelection = NO;
  p.directoryURL = [NSURL fileURLWithPath:Documentos() isDirectory:YES];
  [self presentViewController:p animated:YES completion:nil];
}

// La ISO no se copia: se guarda un marcador para volver a abrirla donde esta en cada arranque.
- (void)documentPicker:(UIDocumentPickerViewController*)picker didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
  NSURL* url = urls.firstObject;
  if (selector_ == Eleccion::EXPORTAR) {  // el .zip ya esta donde lo dejo el jugador
    [NSFileManager.defaultManager removeItemAtURL:exportado_ error:nil];
    exportado_ = nil;
    [self textoPartidas:url ? NfsmwTextoF(@"partidas_exportadas", exportadas_.archivos, Tamano(exportadas_.bytes)) : @""];
    return;
  }
  if (selector_ == Eleccion::IMPORTAR) {
    if (url) [self confirmarImportar:url];
    return;
  }
  if (!url) return;
  const BOOL acceso = [url startAccessingSecurityScopedResource];
  NSData* marcador = [url bookmarkDataWithOptions:0 includingResourceValuesForKeys:nil relativeToURL:nil error:nil];
  if (acceso) [url stopAccessingSecurityScopedResource];
  if (!marcador) {
    UIAlertController* a = [UIAlertController alertControllerWithTitle:nil
                                                               message:T(@"ios_iso_sin_acceso")
                                                        preferredStyle:UIAlertControllerStyleAlert];
    [a addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];
    [self presentViewController:a animated:YES completion:nil];
    return;
  }
  Poner(@"iso_marcador", marcador);
  Poner(@"iso_nombre", url.lastPathComponent);
  [self refrescar];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)picker {
  if (selector_ == Eleccion::EXPORTAR) {
    [NSFileManager.defaultManager removeItemAtURL:exportado_ error:nil];
    exportado_ = nil;
    [self textoPartidas:@""];
  }
}

- (void)refrescar {
  juego_ = BuscarJuego();
  switch (juego_.fuente) {
    case Fuente::NINGUNA: textoIso_.text = NfsmwTextoF(@"sin_iso", edicion_.nombre); break;
    case Fuente::CARPETA: textoIso_.text = NfsmwTextoF(@"ios_juego_extraido", juego_.nombre); break;
    default: textoIso_.text = NfsmwTextoF(@"iso_elegida", juego_.nombre); break;
  }
  avisoAcceso_.hidden = !juego_.elegidaSinAcceso;
  avisoBiblioteca_.hidden = HayBiblioteca(juego_);
  const bool puedeJugar = juego_.fuente != Fuente::NINGUNA;
  jugar_.enabled = puedeJugar;
  avisoJugar_.hidden = puedeJugar;

  // De que edicion es y si esta entera: son unos megas de lectura, en otro hilo. `miradas_` tira
  // las respuestas que llegan tarde (se eligio otra ISO entretanto).
  mirada_ = Mirada();
  const int n = ++miradas_;
  if (puedeJugar) {
    const Juego juego = juego_;
    __weak NfsmwLanzador* yo = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
      const Mirada m = MirarJuego(juego);
      dispatch_async(dispatch_get_main_queue(), ^{
        [yo mirada:m numero:n];
      });
    });
  }
  [self pintarEdicion];
}

- (void)mirada:(Mirada)m numero:(int)n {
  if (n != miradas_) return;
  mirada_ = m;
  [self pintarEdicion];
}

// La app lleva el codigo de una edicion del juego: se dice cual y, si el default.xex de la ISO es
// de otra o a la ISO le falta el final, se avisa. No se impide jugar: lo que no se ha podido leer
// no demuestra nada.
- (void)pintarEdicion {
  const bool mirada = juego_.fuente != Fuente::NINGUNA && mirada_.hecha;
  const bool incompleta = mirada && mirada_.faltan > 0;
  const bool otraEdicion = mirada && !mirada_.xex.empty() && mirada_.xex != NO_ES_ISO && edicion_.sha.length &&
                           ![@(mirada_.xex.c_str()) isEqualToString:edicion_.sha];
  if (mirada && mirada_.xex == NO_ES_ISO) {
    textoEdicion_.text = T(@"iso_no_es_xbox");
  } else if (incompleta) {
    textoEdicion_.text = NfsmwTextoF(@"iso_incompleta", [NSByteCountFormatter stringFromByteCount:(long long)mirada_.faltan
                                                                                       countStyle:NSByteCountFormatterCountStyleFile]);
  } else if (otraEdicion) {
    textoEdicion_.text = NfsmwTextoF(@"ios_iso_otra_edicion", edicion_.nombre);
  } else {
    textoEdicion_.text = [NSString stringWithFormat:@"%@ %@", NfsmwTextoF(@"ios_edicion_app", edicion_.nombre), T(@"motor_nativo")];
  }
  textoEdicion_.textColor = Color(incompleta || otraEdicion || (mirada && mirada_.xex == NO_ES_ISO) ? AVISO : GRIS);
}

// ---- Pantalla

- (UIView*)tarjetaPantalla {
  // Estirar y pantalla ancha se excluyen: las dos llenan la pantalla, una deformando y la otra
  // no. Encender una apaga la otra; si venian las dos encendidas, se queda la pantalla ancha.
  if (LeerBool(@"estirar", false) && LeerBool(@"panoramico", false)) Poner(@"estirar", @NO);
  __block __weak UISwitch* estirar = nil;
  __block __weak UISwitch* panoramico = nil;
  UISwitch* s1 = nil;
  UISwitch* s2 = nil;
  UIView* filaEstirar = Interruptor(T(@"estirar"), T(@"estirar_resumen"), LeerBool(@"estirar", false), ^(bool v) {
    Poner(@"estirar", @(v));
    if (v && panoramico.on) {
      [panoramico setOn:NO animated:YES];
      Poner(@"panoramico", @NO);
    }
  }, &s1);
  UIView* filaPanoramico = Interruptor(T(@"panoramico"), T(@"panoramico_resumen"), LeerBool(@"panoramico", false), ^(bool v) {
    Poner(@"panoramico", @(v));
    if (v && estirar.on) {
      [estirar setOn:NO animated:YES];
      Poner(@"estirar", @NO);
    }
  }, &s2);
  estirar = s1;
  panoramico = s2;
  return Tarjeta(@[
    Seccion(T(@"seccion_pantalla")), filaEstirar, filaPanoramico,
    Interruptor(T(@"mostrar_fps"), T(@"mostrar_fps_resumen"), LeerBool(@"mostrar_fps", true), ^(bool v) {
      Poner(@"mostrar_fps", @(v));
    })
  ]);
}

// ---- Graficos

- (UIView*)tarjetaGraficos {
  auto guardar = ^(NSString* clave) {
    return ^(id v) { Poner(clave, v); };
  };
  return Tarjeta(@[
    Seccion(T(@"seccion_graficos")), [self filaPreajustes],
    Selector(T(@"resolucion_interna"), T(@"resolucion_interna_resumen"),
             @[T(@"res_interna_576"), T(@"res_interna_720"), T(@"res_interna_1080")],
             @[@"1024x576", @"1280x720", @"1920x1080"], LeerTexto(@"resolucion_interna", @"1280x720"), false,
             guardar(@"resolucion_interna")),
    Desplegable(T(@"limite_fps"), T(@"limite_fps_resumen"),
                @[NfsmwTextoF(@"fps_valor", 30), NfsmwTextoF(@"fps_valor", 60), NfsmwTextoF(@"fps_experimental", 90),
                  NfsmwTextoF(@"fps_experimental", 120), T(@"fps_sin_limite")],
                @[@30, @60, @90, @120, @(FPS_SIN_LIMITE)], @(LeerInt(@"limite_fps", 60)), guardar(@"limite_fps")),
    // El motor nativo pinta la escena en una pasada, sin MSAA: solo hay FXAA, y sin niveles.
    Selector(T(@"suavizado"), T(@"suavizado_resumen"), @[T(@"aa_no"), T(@"aa_fxaa")], @[AA_NO, AA_FXAA],
             Antialiasing(), true, guardar(@"antialiasing")),
    Selector(T(@"escalado"), T(@"escalado_resumen"),
             @[T(@"escalado_bilineal"), T(@"escalado_fsr"), T(@"escalado_cas"), T(@"escalado_gsr"),
               T(@"escalado_gsr2")],
             @[@"bilinear", @"fsr", @"cas", @"gsr", @"gsr2"],
             LeerTexto(@"escalado", @"bilinear"), false, guardar(@"escalado")),
    Selector(T(@"anisotropico"), T(@"anisotropico_resumen"), @[T(@"aniso_no"), T(@"aniso_2"), T(@"aniso_4"), T(@"aniso_16")],
             @[@0, @2, @3, @5], @(LeerInt(@"anisotropico", 3)), true, guardar(@"anisotropico")),
    Selector(T(@"calidad_sombras"), T(@"calidad_sombras_resumen"),
             @[T(@"sombras_rapida"), T(@"sombras_suave"), T(@"sombras_xbox360")],
             @[SOMBRAS_RAPIDA, SOMBRAS_SUAVE, SOMBRAS_XBOX], LeerTexto(@"calidad_sombras", SOMBRAS_RAPIDA), true,
             guardar(@"calidad_sombras")),
    Desplegable(T(@"resolucion_sombras"), nil,
                @[T(@"sombras_nativa"), T(@"sombras_alta"), T(@"sombras_muy_alta"), T(@"sombras_ultra")],
                @[@100, @150, @200, @250], @(LeerInt(@"sombras_escala", 100)), guardar(@"sombras_escala")),
    Selector(T(@"sombras_distancia"), T(@"sombras_distancia_resumen"),
             @[T(@"sombras_lejos"), T(@"sombras_media"), T(@"sombras_cerca")], @[@100, @150, @200],
             @(LeerInt(@"sombras_corte", 150)), true, guardar(@"sombras_corte")),
    Selector(T(@"reflejos_coche"), T(@"reflejos_coche_resumen"),
             @[T(@"calidad_alta"), T(@"calidad_media"), T(@"calidad_baja")], @[@6, @2, @1],
             @(LeerInt(@"reflejos_coche", 6)), true, guardar(@"reflejos_coche")),
    Interruptor(T(@"reflejo_asfalto"), T(@"reflejo_asfalto_resumen"), LeerBool(@"reflejo_asfalto", true),
                ^(bool v) { Poner(@"reflejo_asfalto", @(v)); }),
    Selector(T(@"resplandor_cielo"), T(@"resplandor_cielo_resumen"),
             @[T(@"cielo_natural"), T(@"cielo_original"), T(@"cielo_suave")], @[@"natural", @"original", @"suave"],
             LeerTexto(@"resplandor_cielo", @"natural"), true, guardar(@"resplandor_cielo")),
    Selector(T(@"filtro_color"), T(@"filtro_color_resumen"),
             @[T(@"filtro_color_original"), T(@"filtro_color_suave"), T(@"filtro_color_apagado")],
             @[@"original", @"suave", @"apagado"], LeerTexto(@"filtro_color", @"original"), true,
             guardar(@"filtro_color")),
    Interruptor(T(@"desenfoque"), T(@"desenfoque_resumen"), LeerBool(@"desenfoque", false),
                ^(bool v) { Poner(@"desenfoque", @(v)); }),
    Desplegable(T(@"filtro_imagen"), T(@"filtro_imagen_resumen"),
                @[T(@"filtro_imagen_no"), T(@"filtro_imagen_cine"), T(@"filtro_imagen_vivo"), T(@"filtro_imagen_calido"),
                  T(@"filtro_imagen_frio"), T(@"filtro_imagen_sepia"), T(@"filtro_imagen_noir"), T(@"filtro_imagen_crt")],
                @[@"apagado", @"cine", @"vivo", @"calido", @"frio", @"sepia", @"noir", @"crt"],
                LeerTexto(@"filtro_imagen", @"apagado"), guardar(@"filtro_imagen"))
  ]);
}

// Preajustes: cambian de golpe los ajustes graficos de abajo.
- (UIView*)filaPreajustes {
  __weak NfsmwLanzador* yo = self;
  UIButton* ahorro = Boton(T(@"preajuste_ahorro"), BOTON, BLANCO, ^{
    [yo confirmarPreajuste:@"preajuste_ahorro" detalle:@"preajuste_ahorro_detalle" aplicar:^{ AplicarAhorro(); }];
  });
  UIButton* fabrica = Boton(T(@"preajuste_fabrica"), BOTON, BLANCO, ^{
    [yo confirmarPreajuste:@"preajuste_fabrica" detalle:@"preajuste_fabrica_detalle" aplicar:^{ AplicarFabrica(); }];
  });
  UIStackView* fila = [[UIStackView alloc] initWithArrangedSubviews:@[ahorro, fabrica]];
  fila.axis = UILayoutConstraintAxisHorizontal;
  fila.distribution = UIStackViewDistributionFillEqually;
  fila.spacing = 8;
  NSMutableArray* vistas = [NSMutableArray arrayWithArray:@[Titulo(T(@"preajustes")), Resumen(T(@"preajustes_resumen")),
                                                            Columna(@[fila], 6)]];
  // Los del usuario, guardados al aplicar uno de los otros dos: solo si los hay.
  if (Hay(@"mis_ajustes")) {
    [vistas addObject:Columna(@[Boton(T(@"preajuste_mios"), BOTON, BLANCO, ^{
                                [yo confirmarPreajuste:@"preajuste_mios"
                                               detalle:@"preajuste_mios_detalle"
                                               aplicar:^{ AplicarMisAjustes(); }];
                              })],
                              4)];
  }
  return Columna(vistas, 0, 6);
}

// Pregunta antes de cambiar varios ajustes a la vez; al aplicar, la pantalla se rehace con ellos.
- (void)confirmarPreajuste:(NSString*)titulo detalle:(NSString*)detalle aplicar:(void (^)(void))aplicar {
  UIAlertController* a = [UIAlertController alertControllerWithTitle:T(titulo)
                                                             message:T(detalle)
                                                      preferredStyle:UIAlertControllerStyleAlert];
  __weak NfsmwLanzador* yo = self;
  [a addAction:[UIAlertAction actionWithTitle:T(@"editor_cancelar").localizedCapitalizedString
                                        style:UIAlertActionStyleCancel
                                      handler:nil]];
  [a addAction:[UIAlertAction actionWithTitle:T(@"preajuste_aplicar")
                                        style:UIAlertActionStyleDefault
                                      handler:^(UIAlertAction*) {
                                        aplicar();
                                        [yo construir];
                                      }]];
  [self presentViewController:a animated:YES completion:nil];
}

// ---- Multijugador: el perfil

// El gamertag se guarda segun se escribe, pero solo si es valido para Xbox 360; si no, se avisa y
// se queda el ultimo bueno. Vacio = el de fabrica, "User".
- (UIView*)tarjetaPerfil {
  UITextField* campo = [UITextField new];
  campo.text = LeerTexto(@"gamertag", @"");
  campo.placeholder = T(@"gamertag");
  campo.borderStyle = UITextBorderStyleRoundedRect;
  campo.font = [UIFont systemFontOfSize:16];
  campo.textColor = UIColor.whiteColor;
  campo.autocorrectionType = UITextAutocorrectionTypeNo;
  campo.spellCheckingType = UITextSpellCheckingTypeNo;
  campo.returnKeyType = UIReturnKeyDone;
  campo.delegate = self;
  [campo.heightAnchor constraintGreaterThanOrEqualToConstant:44].active = YES;
  errorGamertag_ = Etiqueta(T(@"gamertag_error"), 13, UIFontWeightRegular, AVISO);
  errorGamertag_.hidden = GamertagValido(campo.text);
  // Debajo, si el nombre esta libre en el servidor de la LAN.
  comprobacion_ = Resumen(@"");
  comprobacion_.hidden = YES;
  __weak NfsmwLanzador* yo = self;
  [campo addAction:[UIAction actionWithHandler:^(UIAction* a) {
           UITextField* c = (UITextField*)a.sender;
           if (c.text.length > 15) c.text = [c.text substringToIndex:15];
           [yo gamertagEscrito:[c.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet]];
         }]
      forControlEvents:UIControlEventEditingChanged];
  return Tarjeta(@[
    Seccion(T(@"seccion_perfil")), Resumen(T(@"gamertag_resumen")), Espacio(8), campo,
    Columna(@[errorGamertag_, comprobacion_], 6, 0, 4)
  ]);
}

- (void)gamertagEscrito:(NSString*)g {
  gamertagEscritoValido_ = GamertagValido(g);
  errorGamertag_.hidden = gamertagEscritoValido_;
  if (gamertagEscritoValido_) {
    Poner(@"gamertag", g);
    [self programarComprobacion];
  } else {
    // Mal escrito: manda el error de formato, no el del servidor.
    [comprobacionPendiente_ invalidate];
    comprobaciones_++;
    comprobacion_.hidden = YES;
  }
}

// La comprobacion, medio segundo despues del ultimo cambio; con este telefono de anfitrion, algo
// mas: su servidor puede estar arrancando.
- (void)programarComprobacion {
  [comprobacionPendiente_ invalidate];
  comprobaciones_++;
  __weak NfsmwLanzador* yo = self;
  comprobacionPendiente_ = [NSTimer scheduledTimerWithTimeInterval:LeerBool(@"anfitrion_lan", false) ? 1.5 : .5
                                                           repeats:NO
                                                             block:^(NSTimer*) {
                                                               [yo comprobarGamertag];
                                                             }];
}

// Pregunta al servidor de la LAN si el gamertag esta libre y lo dice debajo del campo. Sin
// servidor no hay nada que comprobar.
- (void)comprobarGamertag {
  if (!gamertagEscritoValido_ || comprobandoParaJugar_) return;
  NSString* servidor = ServidorEfectivo();
  if (!DireccionValida(servidor)) {
    comprobacion_.hidden = YES;
    return;
  }
  NSString* gamertag = LeerTexto(@"gamertag", @"");
  const int n = ++comprobaciones_;
  __weak NfsmwLanzador* yo = self;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
    const std::string ip = nfsmw::red::ResolverIpv4(S(servidor));
    const auto r = ip.empty() ? nfsmw::red::Comprobacion::kSinRespuesta : nfsmw::red::ComprobarGamertag(ip, S(gamertag));
    dispatch_async(dispatch_get_main_queue(), ^{
      [yo comprobado:r servidor:servidor gamertag:gamertag numero:n];
    });
  });
}

- (void)comprobado:(nfsmw::red::Comprobacion)r servidor:(NSString*)servidor gamertag:(NSString*)gamertag numero:(int)n {
  if (n != comprobaciones_ || !gamertagEscritoValido_) return;
  NSString* nombre = N(nfsmw::red::GamertagEfectivo(S(gamertag)));
  comprobacion_.hidden = NO;
  switch (r) {
    case nfsmw::red::Comprobacion::kLibre:
      comprobacion_.text = NfsmwTextoF(@"gamertag_libre", nombre, servidor);
      comprobacion_.textColor = Color(GRIS);
      break;
    case nfsmw::red::Comprobacion::kEnUso:
      comprobacion_.text = NfsmwTextoF(@"gamertag_en_uso", nombre, servidor);
      comprobacion_.textColor = Color(AVISO);
      break;
    default:
      comprobacion_.text = NfsmwTextoF(@"gamertag_sin_respuesta", servidor);
      comprobacion_.textColor = Color(GRIS);
      break;
  }
}

// ---- Multijugador: el servidor de la LAN (ser el anfitrion o elegir uno de los guardados)

- (UIView*)tarjetaServidorLan {
  __weak NfsmwLanzador* yo = self;
  UISwitch* interruptor = nil;
  UIView* filaAnfitrion = Interruptor(T(@"servidor_anfitrion"), T(@"servidor_anfitrion_resumen"),
                                      LeerBool(@"anfitrion_lan", false), ^(bool v) { [yo cambiarAnfitrion:v]; }, &interruptor);
  interruptorAnfitrion_ = interruptor;
  textoAnfitrion_ = Etiqueta(@"", 15, UIFontWeightRegular, BLANCO);

  // Tocar un servidor lo elige; el lapiz lo edita y la papelera lo quita. Buscar: los que contestan
  // salen debajo de los guardados, y tocar uno lo guarda y lo elige.
  listaServidores_ = Columna(@[], 0, 0, 6);
  listaEncontrados_ = Columna(@[], 6, 0, 6);
  botonBuscar_ = Boton(T(@"servidor_buscar"), BOTON, BLANCO, ^{ [yo buscarServidores]; });
  UIButton* anadir = Boton(T(@"servidor_anadir"), BOTON, BLANCO, ^{ [yo abrirEditorDe:nil nombre:nil]; });
  UIStackView* botones = FilaBotones(@[botonBuscar_, anadir]);
  filaBotonesServidores_ = Columna(@[botones], 10);
  bloqueServidores_ = Columna(@[Titulo(T(@"servidores_guardados")), Columna(@[listaServidores_], 6), listaEncontrados_,
                                filaBotonesServidores_]);
  UIView* t = Tarjeta(@[
    Seccion(T(@"servidor_lan")), Resumen(T(@"servidor_lan_resumen")), filaAnfitrion, Columna(@[textoAnfitrion_], 4),
    Espacio(4), bloqueServidores_
  ]);
  [self pintarServidores];
  [self actualizarAnfitrion];
  return t;
}

- (void)arrancarAnfitrion {
  if (nfsmw::servidor::EnMarcha()) return;
  NSString* datos = [Documentos() stringByAppendingPathComponent:@"datos/servidor_lan"];
  NSString* logs = [Documentos() stringByAppendingPathComponent:@"logs"];
  __weak NfsmwLanzador* yo = self;
  // Abrir los puertos es red: fuera del hilo principal.
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    std::string error;
    const bool bien = nfsmw::servidor::Arrancar(
        S(datos), S(logs), [] {}, [] { return NombreAnfitrion(); }, &error);
    if (bien) {
      // Que siga sirviendo con la app en segundo plano o la pantalla bloqueada.
      dispatch_async(dispatch_get_main_queue(), ^{
        if (LeerBool(@"anfitrion_lan", false)) nfsmw::segundo_plano::Mantener(true);
      });
      return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
      // Lo normal: los puertos ya los tiene otro.
      Poner(@"anfitrion_lan", @NO);
      [yo anfitrionFallo:N(error)];
    });
  });
}

- (void)anfitrionFallo:(NSString*)error {
  [interruptorAnfitrion_ setOn:NO animated:YES];
  [self actualizarAnfitrion];
  [self aviso:NfsmwTextoF(@"servidor_error", error) deshacer:nil];
}

// El interruptor de anfitrion: arranca o para el servidor de este telefono.
- (void)cambiarAnfitrion:(bool)si {
  Poner(@"anfitrion_lan", @(si));
  if (si) {
    [self arrancarAnfitrion];
  } else {
    nfsmw::servidor::Parar();
    nfsmw::segundo_plano::Mantener(false);
  }
  [self actualizarAnfitrion];
  // Otro servidor, otros jugadores.
  [self programarComprobacion];
}

// Con este telefono de anfitrion: la IP que ponen los demas (o que no hay red), y sin la lista de
// servidores. La IP se mira cada vez: cambia con la Wi-Fi.
- (void)actualizarAnfitrion {
  const bool anfitrion = LeerBool(@"anfitrion_lan", false);
  bloqueServidores_.hidden = anfitrion;
  textoAnfitrion_.hidden = !anfitrion;
  if (anfitrion) {
    const std::string ip = nfsmw::red::IpLocal();
    textoAnfitrion_.text = ip.empty() ? T(@"servidor_anfitrion_sin_red") : NfsmwTextoF(@"servidor_anfitrion_ip", N(ip));
    textoAnfitrion_.textColor = Color(ip.empty() ? AVISO : BLANCO);
  }
}

// La lista: "Sin servidor" y los guardados, con el elegido marcado.
- (void)pintarServidores {
  for (UIView* v in listaServidores_.arrangedSubviews) [v removeFromSuperview];
  NSString* elegida = LeerTexto(@"servidor_lan", @"");
  const std::vector<Servidor> guardados = Servidores();
  [listaServidores_ addArrangedSubview:[self filaServidor:nil elegido:IndiceServidor(guardados, elegida) < 0]];
  bool editorPuesto = false;
  for (const auto& v : guardados) {
    // El que se edita: el formulario en su sitio.
    if (editorServidor_ && [v.direccion isEqualToString:direccionEditada_]) {
      [listaServidores_ addArrangedSubview:editorServidor_];
      editorPuesto = true;
    } else {
      Servidor copia = v;
      [listaServidores_ addArrangedSubview:[self filaServidor:&copia elegido:[v.direccion isEqualToString:elegida]]];
    }
  }
  if (editorServidor_ && !editorPuesto) [listaServidores_ addArrangedSubview:editorServidor_];
  filaBotonesServidores_.hidden = editorServidor_ != nil;
  // "Ya guardado" depende de la lista.
  [self pintarEncontrados];
}

// Una fila de la lista; s nullptr = "Sin servidor", que no se edita ni se quita.
- (UIView*)filaServidor:(const Servidor*)s elegido:(bool)elegido {
  UIView* fila = [UIView new];
  fila.backgroundColor = Color(elegido ? ELEGIDO : 0xFF262626);
  fila.layer.cornerRadius = 12;
  fila.layer.borderWidth = elegido ? 1 : 0;
  fila.layer.borderColor = Color(ACENTO).CGColor;
  NSString* etiqueta = !s ? T(@"servidor_ninguno") : s->nombre.length ? s->nombre : s->direccion;
  NSString* detalle = !s ? T(@"servidor_ninguno_resumen") : s->nombre.length ? s->direccion : nil;
  UILabel* principal = Etiqueta(etiqueta, 16, elegido ? UIFontWeightBold : UIFontWeightRegular, BLANCO);
  UIStackView* textos = Columna(detalle ? @[principal, Resumen(detalle)] : @[principal], 0, 0, 2);
  UIImageView* marca = [[UIImageView alloc] initWithImage:Circulo(elegido)];
  [marca setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
  NSMutableArray* partes = [NSMutableArray arrayWithArray:@[marca, textos]];
  __weak NfsmwLanzador* yo = self;
  NSString* direccion = s ? s->direccion : @"";
  if (s) {
    NSString* nombre = s->nombre;
    [partes addObject:BotonIcono(@"pencil", NfsmwTextoF(@"servidor_editar", etiqueta), ^{
              [yo abrirEditorDe:direccion nombre:nombre];
            })];
    [partes addObject:BotonIcono(@"trash", NfsmwTextoF(@"servidor_eliminar", etiqueta), ^{
              [yo eliminarServidor:direccion];
            })];
  }
  UIStackView* h = [[UIStackView alloc] initWithArrangedSubviews:partes];
  h.axis = UILayoutConstraintAxisHorizontal;
  h.alignment = UIStackViewAlignmentCenter;
  h.spacing = 12;
  h.translatesAutoresizingMaskIntoConstraints = NO;
  [fila addSubview:h];
  [NSLayoutConstraint activateConstraints:@[
    [h.leadingAnchor constraintEqualToAnchor:fila.leadingAnchor constant:12],
    [h.trailingAnchor constraintEqualToAnchor:fila.trailingAnchor constant:-4],
    [h.topAnchor constraintEqualToAnchor:fila.topAnchor constant:6],
    [h.bottomAnchor constraintEqualToAnchor:fila.bottomAnchor constant:-6],
    [fila.heightAnchor constraintGreaterThanOrEqualToConstant:56],
  ]];
  AlTocar(fila, ^{
    [yo elegirServidor:direccion];
  });
  return fila;
}

// "" = sin servidor: el juego como siempre.
- (void)elegirServidor:(NSString*)direccion {
  if ([direccion isEqualToString:LeerTexto(@"servidor_lan", @"")]) return;
  Poner(@"servidor_lan", direccion);
  [self pintarServidores];
  // Otro servidor, otros jugadores: el nombre puede estar en uso alli.
  [self programarComprobacion];
}

// Anadir (direccion nil) o editar un servidor: nombre opcional e IP. En la propia lista, en el sitio
// del que se edita (o al final, si es nuevo), con los botones en la linea del titulo para que se vean
// con el teclado abierto. Guardar solo se puede con una direccion valida que no este ya en la lista.
// Uno nuevo queda elegido; al editar el elegido, sigue elegido con su direccion nueva.
- (void)abrirEditorDe:(NSString*)direccion nombre:(NSString*)nombreActual {
  [self cerrarEditor];
  const bool nuevo = direccion == nil;
  direccionEditada_ = direccion ?: @"";

  UITextField* ip = Campo(direccion ?: @"", @"10.0.0.4");
  // Una IP o un nombre de equipo: teclado de direcciones, sin autocorregir.
  ip.keyboardType = UIKeyboardTypeURL;
  ip.autocapitalizationType = UITextAutocapitalizationTypeNone;
  ip.returnKeyType = UIReturnKeyNext;
  ip.delegate = self;
  UITextField* nombre = Campo(nombreActual ?: @"", nil);
  nombre.returnKeyType = UIReturnKeyDone;
  nombre.delegate = self;
  UILabel* error = Etiqueta(@"", 13, UIFontWeightRegular, AVISO);
  error.hidden = YES;
  UIStackView* columnaIp = Columna(@[Etiqueta(T(@"servidor_lan_ip"), 13, UIFontWeightRegular, ACENTO_CLARO), ip, error], 0, 0, 4);
  UIStackView* columnaNombre = Columna(@[Etiqueta(T(@"servidor_nombre"), 13, UIFontWeightRegular, ACENTO_CLARO), nombre], 0, 0, 4);
  UIStackView* campos = [[UIStackView alloc] initWithArrangedSubviews:@[columnaIp, columnaNombre]];
  campos.axis = UILayoutConstraintAxisHorizontal;
  campos.alignment = UIStackViewAlignmentTop;
  campos.spacing = 8;
  [columnaNombre.widthAnchor constraintEqualToAnchor:columnaIp.widthAnchor multiplier:1.3].active = YES;

  __weak NfsmwLanzador* yo = self;
  UIButton* cancelar = Boton(T(@"editor_cancelar").localizedCapitalizedString, BOTON, BLANCO, ^{ [yo cerrarEditor]; });
  UIButton* guardar = Boton(T(@"guardar"), ACENTO, 0xFF121212, ^{
    NfsmwLanzador* fuerte = yo;
    if (fuerte && fuerte->aceptarEditor_) fuerte->aceptarEditor_();
  });
  UIView* titulo = Etiqueta(T(nuevo ? @"servidor_anadir" : @"servidor_editar_titulo"), 16, UIFontWeightBold, BLANCO);
  UIStackView* cabecera = [[UIStackView alloc] initWithArrangedSubviews:@[titulo, cancelar, guardar]];
  cabecera.axis = UILayoutConstraintAxisHorizontal;
  cabecera.alignment = UIStackViewAlignmentCenter;
  cabecera.spacing = 8;

  UIView* editor = [UIView new];
  editor.backgroundColor = Color(0xFF262626);
  editor.layer.cornerRadius = 12;
  editor.layer.borderWidth = 1;
  editor.layer.borderColor = Color(ACENTO).CGColor;
  UIStackView* dentro = Columna(@[cabecera, campos], 0, 0, 8);
  dentro.translatesAutoresizingMaskIntoConstraints = NO;
  [editor addSubview:dentro];
  [NSLayoutConstraint activateConstraints:@[
    [dentro.leadingAnchor constraintEqualToAnchor:editor.leadingAnchor constant:12],
    [dentro.trailingAnchor constraintEqualToAnchor:editor.trailingAnchor constant:-12],
    [dentro.topAnchor constraintEqualToAnchor:editor.topAnchor constant:8],
    [dentro.bottomAnchor constraintEqualToAnchor:editor.bottomAnchor constant:-10],
  ]];

  // La direccion que se puede guardar, o nil (y el porque, bajo el campo).
  __weak UITextField* wip = ip;
  __weak UITextField* wnombre = nombre;
  __weak UILabel* werror = error;
  __weak UIButton* wguardar = guardar;
  NSString* (^validar)(void) = ^NSString*(void) {
    NSString* t = [wip.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] ?: @"";
    const bool valida = DireccionValida(t);
    const bool repetida = valida && (nuevo || [t caseInsensitiveCompare:direccion] != NSOrderedSame) &&
                          IndiceServidor(Servidores(), t) >= 0;
    werror.text = !valida ? T(@"servidor_lan_error") : repetida ? T(@"servidor_repetido") : @"";
    werror.hidden = t.length == 0 || (valida && !repetida);
    const bool vale = valida && !repetida;
    wguardar.enabled = vale;
    return vale ? t : nil;
  };
  [ip addAction:[UIAction actionWithHandler:^(UIAction*) {
         if (wip.text.length > 253) wip.text = [wip.text substringToIndex:253];
         validar();
       }]
      forControlEvents:UIControlEventEditingChanged];
  [nombre addAction:[UIAction actionWithHandler:^(UIAction*) {
            if (wnombre.text.length > 24) wnombre.text = [wnombre.text substringToIndex:24];
          }]
      forControlEvents:UIControlEventEditingChanged];
  validar();
  aceptarEditor_ = ^{
    NSString* nueva = validar();
    if (!nueva) {
      [wip becomeFirstResponder];
      return;
    }
    std::vector<Servidor> l = Servidores();
    const Servidor v{[wnombre.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet] ?: @"", nueva};
    const NSInteger donde = nuevo ? -1 : IndiceServidor(l, direccion);
    if (donde >= 0) {
      l[size_t(donde)] = v;
    } else {
      l.push_back(v);
    }
    const bool eraElegido = !nuevo && [direccion isEqualToString:LeerTexto(@"servidor_lan", @"")];
    // Primero la lista: Servidores() mete la elegida si no esta.
    Servidores(l);
    if (nuevo || eraElegido) {
      Poner(@"servidor_lan", nueva);
      [yo programarComprobacion];
    }
    [yo cerrarEditor];
  };

  editorServidor_ = editor;
  ipEditor_ = ip;
  nombreEditor_ = nombre;
  [self pintarServidores];
  [ip becomeFirstResponder];
}

// Quita el formulario de la lista (sin guardar) y el teclado.
- (void)cerrarEditor {
  if (!editorServidor_) return;
  [self.view endEditing:YES];
  [editorServidor_ removeFromSuperview];
  editorServidor_ = nil;
  direccionEditada_ = nil;
  aceptarEditor_ = nil;
  [self pintarServidores];
}

// Quita un servidor guardado, con la opcion de deshacerlo. Si era el elegido, queda "Sin servidor".
- (void)eliminarServidor:(NSString*)direccion {
  std::vector<Servidor> l = Servidores();
  const NSInteger donde = IndiceServidor(l, direccion);
  if (donde < 0) return;
  const Servidor quitado = l[size_t(donde)];
  const bool eraElegido = [direccion isEqualToString:LeerTexto(@"servidor_lan", @"")];
  l.erase(l.begin() + donde);
  if (eraElegido) {
    Poner(@"servidor_lan", @"");
    [self programarComprobacion];
  }
  Servidores(l);
  [self pintarServidores];
  __weak NfsmwLanzador* yo = self;
  [self aviso:NfsmwTextoF(@"servidor_eliminado", quitado.nombre.length ? quitado.nombre : quitado.direccion)
      deshacer:^{
        std::vector<Servidor> ahora = Servidores();
        if (IndiceServidor(ahora, quitado.direccion) >= 0) return;
        ahora.insert(ahora.begin() + std::min<NSInteger>(donde, NSInteger(ahora.size())), quitado);
        Servidores(ahora);
        if (eraElegido && LeerTexto(@"servidor_lan", @"").length == 0) {
          Poner(@"servidor_lan", quitado.direccion);
          [yo programarComprobacion];
        }
        [yo pintarServidores];
      }];
}

// Busca servidores en la red (~1,5 s) y los pone bajo los guardados.
- (void)buscarServidores {
  if (buscando_) return;
  buscando_ = YES;
  [self pintarEncontrados];
  __weak NfsmwLanzador* yo = self;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    const auto r = nfsmw::red::BuscarServidores();
    dispatch_async(dispatch_get_main_queue(), ^{
      [yo encontrados:r];
    });
  });
}

- (void)encontrados:(std::optional<std::vector<nfsmw::red::Encontrado>>)r {
  buscando_ = NO;
  busquedaSinRed_ = !r;
  encontrados_ = r ? *r : std::vector<nfsmw::red::Encontrado>();
  [self pintarEncontrados];
}

// Lo que encontro la ultima busqueda, o por que no hay nada.
- (void)pintarEncontrados {
  botonBuscar_.enabled = !buscando_;
  UIButtonConfiguration* c = botonBuscar_.configuration;
  c.title = T(buscando_ ? @"servidor_buscando" : @"servidor_buscar");
  botonBuscar_.configuration = c;
  for (UIView* v in listaEncontrados_.arrangedSubviews) [v removeFromSuperview];
  listaEncontrados_.hidden = !encontrados_;
  if (!encontrados_) return;
  [listaEncontrados_ addArrangedSubview:Titulo(T(@"servidores_encontrados"))];
  if (encontrados_->empty()) {
    [listaEncontrados_ addArrangedSubview:Resumen(T(busquedaSinRed_ ? @"servidor_anfitrion_sin_red" : @"servidor_ninguno_encontrado"))];
    return;
  }
  const std::vector<Servidor> guardados = Servidores();
  for (const auto& e : *encontrados_) {
    [listaEncontrados_ addArrangedSubview:[self filaEncontrado:e guardado:IndiceServidor(guardados, N(e.ip)) >= 0]];
  }
}

// Un servidor que contesto a la busqueda; tocarlo lo guarda (si no lo estaba) y lo elige.
- (UIView*)filaEncontrado:(const nfsmw::red::Encontrado&)e guardado:(bool)guardado {
  UIView* fila = [UIView new];
  fila.backgroundColor = Color(0xFF262626);
  fila.layer.cornerRadius = 12;
  NSString* ip = N(e.ip);
  NSString* nombre = N(e.nombre);
  NSString* etiqueta = nombre.length ? nombre : ip;
  UIImageView* icono = [[UIImageView alloc]
      initWithImage:[[UIImage systemImageNamed:@"server.rack"] imageWithTintColor:Color(ACENTO_CLARO)
                                                                    renderingMode:UIImageRenderingModeAlwaysOriginal]];
  [icono setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
  UIStackView* textos = Columna(@[
    Etiqueta(etiqueta, 16, UIFontWeightRegular, BLANCO),
    Resumen(NfsmwTextoF(guardado ? @"servidor_encontrado_guardado" : @"servidor_encontrado_detalle", ip, e.jugadores))
  ], 0, 0, 2);
  NSMutableArray* partes = [NSMutableArray arrayWithArray:@[icono, textos]];
  __weak NfsmwLanzador* yo = self;
  const int jugadores = e.jugadores;
  void (^usar)(void) = ^{
    [yo usarEncontrado:ip nombre:nombre jugadores:jugadores];
  };
  if (!guardado) [partes addObject:BotonIcono(@"plus", NfsmwTextoF(@"servidor_encontrado_anadir", etiqueta), usar)];
  UIStackView* h = [[UIStackView alloc] initWithArrangedSubviews:partes];
  h.axis = UILayoutConstraintAxisHorizontal;
  h.alignment = UIStackViewAlignmentCenter;
  h.spacing = 12;
  h.translatesAutoresizingMaskIntoConstraints = NO;
  [fila addSubview:h];
  [NSLayoutConstraint activateConstraints:@[
    [h.leadingAnchor constraintEqualToAnchor:fila.leadingAnchor constant:12],
    [h.trailingAnchor constraintEqualToAnchor:fila.trailingAnchor constant:-4],
    [h.topAnchor constraintEqualToAnchor:fila.topAnchor constant:6],
    [h.bottomAnchor constraintEqualToAnchor:fila.bottomAnchor constant:-6],
    [fila.heightAnchor constraintGreaterThanOrEqualToConstant:56],
  ]];
  AlTocar(fila, usar);
  return fila;
}

- (void)usarEncontrado:(NSString*)ip nombre:(NSString*)nombre jugadores:(int)jugadores {
  [self cerrarEditor];
  std::vector<Servidor> l = Servidores();
  const bool nuevo = IndiceServidor(l, ip) < 0;
  if (nuevo) {
    // El nombre, con el limite del formulario.
    l.push_back({nombre.length > 24 ? [nombre substringToIndex:24] : nombre, ip});
    Servidores(l);
  }
  // Ya esta en la lista de arriba, elegido: la busqueda ha cumplido.
  encontrados_.reset();
  [self elegirServidor:ip];
  [self pintarServidores];
  if (nuevo) [self aviso:NfsmwTextoF(@"servidor_guardado", nombre.length ? nombre : ip) deshacer:nil];
}

// Un aviso abajo de la seccion, unos segundos (el Snackbar de Android), con Deshacer si se da.
- (void)aviso:(NSString*)texto deshacer:(void (^)(void))deshacer {
  if (!contenido_) return;  // la pantalla aun no esta hecha
  [aviso_ removeFromSuperview];
  UIView* caja = [UIView new];
  caja.backgroundColor = Color(0xFF323232);
  caja.layer.cornerRadius = 10;
  caja.translatesAutoresizingMaskIntoConstraints = NO;
  NSMutableArray* partes = [NSMutableArray arrayWithObject:Etiqueta(texto, 14, UIFontWeightRegular, BLANCO)];
  __weak UIView* wcaja = caja;
  if (deshacer) {
    UIButtonConfiguration* c = [UIButtonConfiguration plainButtonConfiguration];
    c.title = T(@"deshacer");
    c.baseForegroundColor = Color(ACENTO_CLARO);
    UIButton* b = [UIButton buttonWithConfiguration:c
                                      primaryAction:[UIAction actionWithHandler:^(UIAction*) {
                                        deshacer();
                                        [wcaja removeFromSuperview];
                                      }]];
    [b setContentHuggingPriority:UILayoutPriorityRequired forAxis:UILayoutConstraintAxisHorizontal];
    [partes addObject:b];
  }
  UIStackView* h = [[UIStackView alloc] initWithArrangedSubviews:partes];
  h.axis = UILayoutConstraintAxisHorizontal;
  h.alignment = UIStackViewAlignmentCenter;
  h.spacing = 8;
  h.translatesAutoresizingMaskIntoConstraints = NO;
  [caja addSubview:h];
  [contenido_ addSubview:caja];
  [NSLayoutConstraint activateConstraints:@[
    [h.leadingAnchor constraintEqualToAnchor:caja.leadingAnchor constant:14],
    [h.trailingAnchor constraintEqualToAnchor:caja.trailingAnchor constant:-8],
    [h.topAnchor constraintEqualToAnchor:caja.topAnchor constant:8],
    [h.bottomAnchor constraintEqualToAnchor:caja.bottomAnchor constant:-8],
    [caja.leadingAnchor constraintEqualToAnchor:contenido_.leadingAnchor constant:12],
    [caja.trailingAnchor constraintEqualToAnchor:contenido_.trailingAnchor constant:-12],
    [caja.bottomAnchor constraintEqualToAnchor:contenido_.bottomAnchor constant:-12],
  ]];
  aviso_ = caja;
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, int64_t((deshacer ? 5 : 2.5) * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
    [UIView animateWithDuration:.2
        animations:^{
          wcaja.alpha = 0;
        }
        completion:^(BOOL) {
          [wcaja removeFromSuperview];
        }];
  });
}

- (BOOL)textFieldShouldReturn:(UITextField*)campo {
  if (campo == ipEditor_) {
    [nombreEditor_ becomeFirstResponder];
  } else if (campo == nombreEditor_ && aceptarEditor_) {
    aceptarEditor_();
  } else {
    [campo resignFirstResponder];
  }
  return YES;
}

// ---- Controles tactiles

- (UIView*)tarjetaControles {
  vibracionProbada_ = Resumen(T(@"ios_vibracion_probada"));
  vibracionProbada_.hidden = YES;
  __weak UILabel* probada = vibracionProbada_;
  UIButton* probar = Boton(T(@"probar_vibracion"), BOTON, BLANCO, ^{
    NfsmwVibrarUnRato();
    probada.hidden = NO;
  });
  UIStackView* filaProbar = FilaBotones(@[probar]);
  return Tarjeta(@[
    Seccion(T(@"seccion_tactil")), Resumen(T(@"ios_editar_tactil_resumen")), Espacio(8),
    Interruptor(T(@"vibracion"), T(@"vibracion_resumen"), LeerBool(@"vibracion", true),
                ^(bool v) { Poner(@"vibracion", @(v)); }),
    Columna(@[filaProbar, vibracionProbada_], 2, 0, 6),
    Interruptor(T(@"menus_tactiles"), T(@"menus_tactiles_resumen"), LeerBool(@"menus_tactiles", true),
                ^(bool v) { Poner(@"menus_tactiles", @(v)); }),
    [self selectorGiro]
  ]);
}

// Con que se gira en las carreras. La sensibilidad solo cuenta inclinando el movil.
- (UIView*)selectorGiro {
  UIView* sensibilidad = Deslizador(
      T(@"sensibilidad_giro"), T(@"sensibilidad_giro_resumen"), SENSIBILIDAD_MIN, SENSIBILIDAD_MAX,
      LeerInt(@"sensibilidad_giro", 10),
      ^NSString*(int s) { return NfsmwTextoF(@"sensibilidad_valor", s, int(std::lround(AnguloGiro(s)))); },
      ^(int s) { Poner(@"sensibilidad_giro", @(s)); });
  UIStackView* conMargen = Columna(@[sensibilidad]);
  conMargen.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(0, 32, 0, 0);
  conMargen.hidden = ![ModoGiro() isEqualToString:GIRO_INCLINAR];
  __weak UIStackView* mostrar = conMargen;
  UIView* selector = Selector(T(@"giro_carrera"), T(@"giro_carrera_resumen"),
                              @[T(@"giro_stick"), T(@"giro_flechas"), T(@"giro_inclinar")],
                              @[GIRO_STICK, GIRO_FLECHAS, GIRO_INCLINAR], ModoGiro(), true, ^(id modo) {
                                ModoGiro(modo);
                                mostrar.hidden = ![modo isEqualToString:GIRO_INCLINAR];
                              });
  return Columna(@[selector, conMargen]);
}

// ---- Idioma: todos a la vista; al elegir uno, la pantalla se rehace en el.

- (UIView*)tarjetaIdioma {
  NSMutableArray* nombres = [NSMutableArray array];
  NSMutableArray* codigos = [NSMutableArray array];
  for (size_t i = 0; i < std::size(CODIGOS_IDIOMA); ++i) {
    [nombres addObject:T(NOMBRES_IDIOMA[i])];
    [codigos addObject:CODIGOS_IDIOMA[i]];
  }
  __weak NfsmwLanzador* yo = self;
  return Tarjeta(@[
    Seccion(T(@"seccion_idioma")), Selector(nil, nil, nombres, codigos, IdiomaElegido(), false, ^(id codigo) {
      if ([codigo isEqualToString:IdiomaElegido()]) return;
      [NSUserDefaults.standardUserDefaults setObject:codigo forKey:@"ajustes.idioma"];
      // Despues del toque: el boton que lo recibe es de la pantalla que se va a rehacer.
      dispatch_async(dispatch_get_main_queue(), ^{
        [yo construir];
      });
    })
  ]);
}

// ---- Otras opciones: el sonido (por donde sale el audio)

- (UIView*)tarjetaSonido {
  return Tarjeta(@[
    Seccion(T(@"seccion_sonido")),
    Interruptor(T(@"ios_audio"), T(@"ios_audio_resumen"), LeerBool(@"audio_avaudio", true),
                ^(bool v) { Poner(@"audio_avaudio", @(v)); })
  ]);
}

// ---- Avanzado

- (UIView*)tarjetaAvanzado {
  return Tarjeta(@[
    Seccion(T(@"seccion_avanzado")), Resumen(T(@"avanzado_resumen")),
    Interruptor(T(@"ios_metal_rapida"), T(@"ios_metal_rapida_resumen"), LeerBool(@"metal_rapida", false),
                ^(bool v) { Poner(@"metal_rapida", @(v)); }),
    Interruptor(T(@"registro_detallado"), T(@"registro_detallado_resumen"), LeerBool(@"registro", false),
                ^(bool v) { Poner(@"registro", @(v)); })
  ]);
}

// ---- Jugar

// Jugar. Con servidor de la LAN se pregunta otra vez antes de entrar (otro jugador ha podido
// conectarse con ese nombre desde que se escribio) y, si esta en uso, no se entra. Sin servidor, o
// si no contesta, se juega como siempre. Un servidor puesto por nombre se pasa aqui a su IP: el
// juego solo entiende IPs. Si la red no lo encuentra, se pregunta.
- (void)jugar {
  [self.view endEditing:YES];
  juego_ = BuscarJuego();
  if (juego_.fuente == Fuente::NINGUNA) {
    [self refrescar];
    return;
  }
  NSString* servidor = ServidorEfectivo();
  if (!DireccionValida(servidor)) {
    [self lanzarConRed:nil];
    return;
  }
  if (comprobandoParaJugar_) return;
  comprobandoParaJugar_ = YES;
  jugar_.enabled = NO;
  NSString* gamertag = LeerTexto(@"gamertag", @"");
  const int n = ++comprobaciones_;
  __weak NfsmwLanzador* yo = self;
  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    const std::string ip = nfsmw::red::ResolverIpv4(S(servidor));
    const auto r = ip.empty() ? nfsmw::red::Comprobacion::kSinRespuesta : nfsmw::red::ComprobarGamertag(ip, S(gamertag));
    dispatch_async(dispatch_get_main_queue(), ^{
      [yo comprobadoParaJugar:r servidor:servidor ip:N(ip) gamertag:gamertag numero:n];
    });
  });
}

- (void)comprobadoParaJugar:(nfsmw::red::Comprobacion)r
                   servidor:(NSString*)servidor
                         ip:(NSString*)ip
                   gamertag:(NSString*)gamertag
                     numero:(int)n {
  comprobandoParaJugar_ = NO;
  jugar_.enabled = juego_.fuente != Fuente::NINGUNA;
  if (n == comprobaciones_) [self comprobado:r servidor:servidor gamertag:gamertag numero:n];
  __weak NfsmwLanzador* yo = self;
  if (ip.length == 0) {
    // El nombre del servidor no da ninguna IP en esta red: o se corrige, o se juega sin online.
    [self preguntar:T(@"servidor_no_encontrado_titulo")
            mensaje:NfsmwTextoF(@"servidor_no_encontrado", servidor)
                 si:^{ [yo lanzarConRed:nil]; }];
    return;
  }
  if (r == nfsmw::red::Comprobacion::kEnUso) {
    // El gamertag lo tiene otro telefono: o se cambia, o se juega esta vez sin el modo online.
    [self preguntar:T(@"gamertag_en_uso_titulo")
            mensaje:NfsmwTextoF(@"gamertag_en_uso_pregunta", N(nfsmw::red::GamertagEfectivo(S(gamertag))), servidor)
                 si:^{ [yo lanzarConRed:nil]; }];
    return;
  }
  [self lanzarConRed:ip];
}

- (void)preguntar:(NSString*)titulo mensaje:(NSString*)mensaje si:(void (^)(void))si {
  UIAlertController* a = [UIAlertController alertControllerWithTitle:titulo
                                                             message:mensaje
                                                      preferredStyle:UIAlertControllerStyleAlert];
  [a addAction:[UIAlertAction actionWithTitle:T(@"editor_cancelar").localizedCapitalizedString
                                        style:UIAlertActionStyleCancel
                                      handler:nil]];
  [a addAction:[UIAlertAction actionWithTitle:T(@"jugar_sin_online")
                                        style:UIAlertActionStyleDefault
                                      handler:^(UIAlertAction*) { si(); }]];
  [self presentViewController:a animated:YES completion:nil];
}

// ip: la del servidor de la LAN a la que va Xbox Live del juego; nil = sin online (el servidor
// guardado no se toca).
- (void)lanzarConRed:(NSString*)ip {
  InstalarAjustesMotor();
  argumentos_ = Argumentos(juego_, edicion_, argv0_, ip);
  std::string linea;
  for (const auto& a : argumentos_) linea += " " + a;
  os_log(OS_LOG_DEFAULT, "[lanzador] jugar:%{public}s", linea.c_str());
  [comprobacionPendiente_ invalidate];
  [renovarReserva_ invalidate];
  _terminado = YES;
}

@end

// =============================================================================================
//  La entrada: el SDL_main del SDK en iOS (windowed_app_main_sdl.cpp)
// =============================================================================================

// La pantalla en su propia ventana, encima de la de arranque de SDL, hasta que se pulse Jugar. La
// llama SDL_main en el hilo principal, dentro del bucle de UIKit: aqui se le da vueltas a ese
// bucle, como hace SDL con sus mensajes. Devuelve la linea de comandos del juego; vacia = la de
// siempre (sin escena a la que poner la ventana, que no deberia pasar).
std::vector<std::string> NfsmwLanzadorIos(int argc, char** argv) {
  // Escribir en un socket que el otro lado ya cerro manda SIGPIPE, que mata la app: el juego (y el
  // servidor del anfitrion, que puede arrancar ya aqui) cuentan con el EPIPE de Winsock. El SDK pide
  // MSG_NOSIGNAL, pero en Darwin es SO_NOSIGPIPE por socket; asi vale para todos.
  signal(SIGPIPE, SIG_IGN);
  UIWindowScene* escena = nil;
  for (UIScene* s in UIApplication.sharedApplication.connectedScenes) {
    if ([s isKindOfClass:UIWindowScene.class]) {
      escena = (UIWindowScene*)s;
      break;
    }
  }
  if (!escena) {
    os_log(OS_LOG_DEFAULT, "[lanzador] sin escena: el juego arranca sin lanzador");
    return {};
  }
  NfsmwLanzador* lanzador = [[NfsmwLanzador alloc] initConArgv0:argc > 0 ? argv[0] : "nfsmw"];
  UIWindow* ventana = [[UIWindow alloc] initWithWindowScene:escena];
  ventana.windowLevel = UIWindowLevelNormal + 2;
  ventana.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
  ventana.rootViewController = lanzador;
  [ventana makeKeyAndVisible];
  while (!lanzador.terminado) {
    @autoreleasepool {
      [NSRunLoop.currentRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
    }
  }
  std::vector<std::string> argumentos = [lanzador argumentos];
  ventana.hidden = YES;
  ventana.rootViewController = nil;
  return argumentos;
}
