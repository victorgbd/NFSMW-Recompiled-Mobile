#!/usr/bin/env python3
"""
Motor nativo: lo que nuestra app de Android necesita del SDK de nfsmw-android.

    python tools/android/parche_nativo.py            aplicar
    python tools/android/parche_nativo.py --estado
    python tools/android/parche_nativo.py --revertir
    python tools/android/parche_nativo.py --arbol D:\\otra\\ruta\\nfsmw-android

Toca tres ficheros del SDK del motor nativo (..\\nfsmw-android\\sdk) y tres de su
app (..\\nfsmw-android\\app):

    sdk/include/rex/filesystem.h            SetAndroidContentOpener
    sdk/src/core/filesystem_posix.cpp       OpenAndroidContentFileDescriptor
    sdk/src/ui/windowed_app_main_sdl.cpp    la sonda de Vulkan, antes del juego
    app/src/nfsmw_app.h                     la salida de audio de la app (AAudio)
    app/src/nfsmw_nativo_sistema.cpp        parar el anillo en segundo plano
    app/src/nfsmw_ajustes_graficos.cpp      fps sin limite
    app/src/nfsmw_recortes_carrera.cpp      las direcciones del contexto del juego
    sdk/src/input/sdl/sdl_input_driver.cpp  la vibracion del mando tactil

Los demas que necesita ese SDK son los mismos que el nuestro y se aplican tal
cual con NFSMW_SDK apuntando a el: parche_iso.py, parche_gamertag.py y
parche_turnip.py. tools/android/preparar_nativo.py los pone todos.


1. ABRIR UNA URI content://
===========================

La app no copia la ISO: la elige con el selector del sistema y se queda con su
URI content://. tools/parche_iso.py ya hace que el SDK monte una imagen de
disco y que, al mapearla, pida el descriptor con
OpenAndroidContentFileDescriptor. En el SDK de ReXGlue esa funcion existe, pero
en el port a Android de nfsmw-android esta vacia (devuelve ENOSYS): su app
importa el juego a una carpeta y no la necesita.

Abrir la URI es cosa de Java (ContentResolver), y el SDK no sabe de la
actividad. Asi que el SDK solo guarda un puntero a quien sabe abrirla, y la app
lo apunta al cargar libmain.so
(android/app/src/main/cpp/nativo/nativo_android.cpp), que llama a
GameActivity.openContentFd por JNI.


2. LA SONDA DE VULKAN
=====================

Con el motor de Xenos, SDL_main es nuestro (android_main.cpp) y mira si le
piden --nfsmw_sonda antes de arrancar el juego. Con el nativo SDL_main es de su
SDK (windowed_app_main_sdl.cpp). Se le anade la misma pregunta: si la app
define NfsmwSondaSiSePide (nativo_android.cpp, en la misma libmain.so), se la
llama primero, y si devuelve un resultado no se arranca el juego.

3. EL AUDIO POR AAUDIO
======================

Con el motor de Xenos, juego.cpp hereda de NfsmwApp y en OnPreSetup cambia la
factoria de audio de SDL por la de nuestro driver AAudio
(android/app/src/main/cpp/audio/aaudio_driver.cpp). Con el nativo la app es la
suya. En su OnPreSetup se llama a NfsmwAndroidAudio si la app la define
(nativo_android.cpp, en la misma libmain.so), que pone la de AAudio.


4. PARAR EL JUEGO EN SEGUNDO PLANO
==================================

Al minimizar, el sonido se para (el audio del SDK se pausa) pero el juego
seguia: su renderizador no espera a nadie para presentar. Suspender los hilos
del juego a la fuerza (XThread::Suspend, por senal) lo paraba, pero los dejaba
en cualquier punto: medido, tras volver el juego se quedaba parado 5-17 s, y un
hilo que el juego tenia a medio arrancar no volvia.

Asi que se para donde el juego ya sabe esperar: el hilo del anillo se detiene
en el siguiente cambio de fotograma (PM4_XE_SWAP), antes de presentarlo, y el
juego se queda esperando sitio en el anillo, como con una GPU lenta. La app
define NfsmwAndroidPausaEnSwap (nativo_android.cpp), que espera ahi mientras
la app esta en segundo plano.


5. FPS SIN LIMITE
=================

El motor nativo no limita los fps por su cuenta: su hilo de vblank dispara la
interrupcion del juego nfsmw_limite_fps veces por segundo (30, 60, 90 o 120), y
el juego espera a un vblank para presentar. Ese es el tope, y ademas redondea:
un fotograma que tarda 14 ms espera al vblank siguiente.

"sin_limite" pone el vblank a 240 Hz, el maximo que admite el SDK
(video_mode_refresh_rate va de 24 a 240): el juego espera como mucho ~4 ms y va
tan rapido como de el movil. Lo elige la pantalla de inicio (Limite de fps ->
Sin limite).


6. EN QUE PARTE DEL JUEGO SE ESTA
=================================

Los controles tactiles cambian en las carreras de aceleracion (pedales, palanca
de cambios). nativo_android.cpp lo lee de la memoria del juego, con lo que da
la descompilacion del juego (github.com/dbalatoni13/nfsmw):

  TheGameFlowManager.CurrentGameFlowState   3 = menus, 6 = en el mundo
  GRaceStatus::fObj -> +0x1A30 mRaceParms -> +4 mIndex -> +0x2B el tipo
                                            (GRace::Type, 2 = aceleracion)

Las carreras rapidas no tienen mIndex: el tipo sale entonces de su atributo
"racetype" (un texto, "drag"), buscado en sus colecciones de Attrib, y de la
tabla del juego que pasa ese texto a tipo.

Los desplazamientos son los del 360 (en la de GameCube, mRaceParms va en
+0x1AAC). Las direcciones se ponen en su app, en un array, para que
crear_arbol.py las lleve a la USA y a la japonesa (en la japonesa fObj se mueve
+0x5A0) y su comprobacion compare las funciones de las que salen los
desplazamientos.


7. LA VIBRACION DEL MANDO TACTIL
===============================

Su SDK le dice al juego que el mando tactil no tiene motores (caps.vibration a
0) y, si aun asi le pide vibrar, no hace nada. Los mandos fisicos si vibran,
por SDL. Aqui el tactil dice que tiene los dos motores y le pasa lo que pide el
juego a la app (NfsmwAndroidVibrar, nativo_android.cpp), que hace vibrar el
movil. Sin la app, como antes.


8. GPU CON SOLO 4 CONJUNTOS DE DESCRIPTORES
===========================================

Sus shaders usan 5 conjuntos de descriptores: los montones de texturas 2D (0),
3D (1) y cubos (2), el de samplers (3) y los UBO de constantes (4). Vulkan solo
garantiza 4 (maxBoundDescriptorSets), y es lo que dan los Mali Valhall (G57,
G68...): su driver se cae dentro de vkCreatePipelineLayout con 5. Con menos de
5, el monton de cubos va en el conjunto del 3D (enlace 1), los samplers en el 2
y los UBO en el 3, y CrearModulo cambia igual las decoraciones de cada shader
(JuntarConjuntos). Con 5 o mas, todo como estaba.


9. LA COLA DE ORDENES ENTRE LOS DOS HILOS DEL JUEGO, CON BARRERAS
================================================================

El hilo que prepara cada fotograma anade ordenes a una lista (0x82909650) con
sub_823C8378: copia sus datos, escribe en la entrada la funcion y el tamano,
mueve el final y sube el contador. Sin barreras. El "Main XThread" las va
ejecutando a la vez (sub_823C83F8): lee el contador y llama a la funcion de
cada entrada. En ARM otro nucleo puede ver el contador nuevo antes que la
entrada: lee una funcion 0 y el juego se cierra ("Call to invalid or
unregistered function at guest address 0x00000000"). En el Samsung A22
(Dimensity 700) pasaba casi siempre al empezar una carrera. Aqui sub_823C8378
escribe lo mismo, pero con una barrera de liberacion antes de publicar el
final y el contador, y el ejecutor solo hace las ordenes ya publicadas (el
contador leido con adquisicion).


10. BC4 Y BC5 EN LA CPU EN LAS XCLIPSE
=====================================

Las Xclipse de Samsung (Exynos 2200 en adelante, AMD RDNA) dicen tener todas las
BC, pero en su driver solo BC1-3 estan completas: BC4-7 las convierte el propio
driver en cada subida (tirones) por un camino a medias (analisis de
XclipseDecomp; ExynosTools y el emulador Eden las esquivan). El juego usa BC4
(DXT5A) y BC5 (DXN, mapas de normales): en una Xclipse van por la conversion en
CPU del renderizador, la misma que en las GPU sin BC. Sin probar en un movil
con Xclipse.


11. MAPA DE SOMBRAS MAS GRANDE QUE EL DE LA XBOX 360
====================================================

El juego pide dos mapas de sombras de 1600x1600 (2000 de las 2048 baldosas de
la EDRAM), y ahi se dibujan tambien los coches: su sombra sale con poca
definicion. El renderizador ya sabe dibujar el mapa a otro tamano
(nfsmw_nativo_sombras_escala), pero solo por debajo de 100 %. Por encima vale
el mismo camino: la textura resuelta se crea al tamano del mapa, la copia es
1 a 1 y la escena la muestrea con coordenadas normalizadas, asi que sale con
mas detalle. Aqui se deja subir hasta 250 % (4000x4000) y se arregla lo unico
que suponia un mapa mas pequeno: el alto util que recorta las restauraciones
venia en pixeles del juego (1600) y con un mapa mas grande dejaba la parte de
abajo sin restaurar. Ahora va en pixeles de la imagen.


12. LAS ESTELAS DE LUZ DEL NITRO, DEL LARGO DE LA XBOX 360 A CUALQUIER FPS
=========================================================================

VehicleRenderConn::RenderFlares (sub_824E63D8) guarda, en cada fotograma de la
vista del jugador, la posicion y la matriz del coche en un anillo de 3 y, con
el nitro, dibuja destellos entre esas tres: la estela es lo que avanza el coche
en dos fotogramas. En la Xbox 360, a 30 fps, son 66 ms; a 60, la mitad de larga
(comprobado en el movil: con el limite a 30 sale como en Xenia). Aqui, antes de
la original, los dos huecos del anillo que no va a escribir se llenan con la
posicion de hace 1/30 s y 2/30 s, interpoladas de un historial propio por
coche, y la original pone la de ahora en el tercero: el mismo largo a 30, 60,
90, 120 fps o sin limite, y pegada al coche. nfsmw_estelas_nitro_30 = false lo
quita.
"""

import argparse
import pathlib
import sys

RAIZ = pathlib.Path(__file__).resolve().parents[2]

CABECERA_ANCLA = '''bool IsAndroidContentUri(const std::string_view source);
int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode);
'''

CABECERA_NUEVO = '''bool IsAndroidContentUri(const std::string_view source);
int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode);
// PARCHE LOCAL - URI content://
//
// Quien abre una URI content:// y devuelve su descriptor (o -1). Lo pone la
// app, que es la que tiene la actividad de Java; sin el, abrir una URI falla.
using AndroidContentOpener = int (*)(const char* uri, const char* mode);
void SetAndroidContentOpener(AndroidContentOpener opener);
'''

FUENTE_ANCLA = '''int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode) {
  // SAF access requires a Java ContentResolver and an app-owned JNI bridge.
  // The Android app currently imports selected trees into private POSIX storage.
  (void)uri;
  (void)mode;
  errno = ENOSYS;
  return -1;
}
'''

FUENTE_NUEVO = '''// PARCHE LOCAL - URI content://
//
// Se apunta una vez, al cargar libmain.so y antes de que exista ningun hilo
// del juego: no hace falta que sea atomico.
static AndroidContentOpener g_android_content_opener = nullptr;

void SetAndroidContentOpener(AndroidContentOpener opener) {
  g_android_content_opener = opener;
}

int OpenAndroidContentFileDescriptor(const std::string_view uri, const char* mode) {
  if (!g_android_content_opener) {
    errno = ENOSYS;
    return -1;
  }
  const std::string copia(uri);
  return g_android_content_opener(copia.c_str(), mode);
}
'''

SONDA_ANCLA = '''#else

int main(int argc, char* argv[]) {
  return RunWindowedApp(argc, argv);
}
'''

SONDA_NUEVO = '''#else

#if REX_PLATFORM_ANDROID
// PARCHE LOCAL - sonda de Vulkan
//
// La app (nativo_android.cpp, en esta misma libreria) la define: si los
// argumentos piden la sonda, la ejecuta y devuelve su resultado; si no,
// devuelve -1 y se arranca el juego como siempre.
extern "C" int NfsmwSondaSiSePide(int argc, char** argv) __attribute__((weak));
#endif

int main(int argc, char* argv[]) {
#if REX_PLATFORM_ANDROID
  if (NfsmwSondaSiSePide) {
    const int sonda = NfsmwSondaSiSePide(argc, argv);
    if (sonda >= 0) {
      return sonda;
    }
  }
#endif
  return RunWindowedApp(argc, argv);
}
'''

AUDIO_ANCLA = '''  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (nfsmw::nativo::Activo()) {
      config.graphics = nfsmw::nativo::CrearSistemaGrafico();
    }
'''

AUDIO_NUEVO = '''  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (nfsmw::nativo::Activo()) {
      config.graphics = nfsmw::nativo::CrearSistemaGrafico();
    }
#if REX_PLATFORM_ANDROID
    // PARCHE LOCAL (NFSMW Recompiled) - la app de Android puede poner su propia
    // salida de audio (AAudio) en vez de la de SDL. Ver nativo_android.cpp.
    if (NfsmwAndroidAudio) {
      NfsmwAndroidAudio(config);
    }
#endif
'''

AUDIO_DECL_ANCLA = '''class NfsmwApp : public rex::ReXApp {
'''

AUDIO_DECL_NUEVO = '''#if REX_PLATFORM_ANDROID
// PARCHE LOCAL (NFSMW Recompiled): la define la app de Android, si quiere.
void NfsmwAndroidAudio(rex::RuntimeConfig& config) __attribute__((weak));
#endif

class NfsmwApp : public rex::ReXApp {
'''

PAUSA_DECL_ANCLA = '''#include "nfsmw_nativo_sistema.h"
'''

PAUSA_DECL_NUEVO = '''#include "nfsmw_nativo_sistema.h"

#if REX_PLATFORM_ANDROID
// PARCHE LOCAL (NFSMW Recompiled): la define la app de Android (nativo_android.cpp).
// Espera mientras la app esta en segundo plano.
extern "C" void NfsmwAndroidPausaEnSwap() __attribute__((weak));
#endif
'''

PAUSA_SWAP_ANCLA = '''        TrazaSwap();
        Presentar();
        AnotarJuegoPorDelante();  // Measurement only
'''

PAUSA_SWAP_NUEVO = '''        TrazaSwap();
#if REX_PLATFORM_ANDROID
        // PARCHE LOCAL (NFSMW Recompiled): en segundo plano el anillo se para aqui, en el
        // cambio de fotograma, y el juego se queda esperando sitio en el anillo.
        if (NfsmwAndroidPausaEnSwap) {
          NfsmwAndroidPausaEnSwap();
        }
#endif
        Presentar();
        AnotarJuegoPorDelante();  // Measurement only
'''

FPS_PERMITIDOS_ANCLA = '''    .allowed({"60", "30", "90", "120"})
'''

FPS_PERMITIDOS_NUEVO = '''    // PARCHE LOCAL (NFSMW Recompiled): "sin_limite", el vblank a 240 Hz (ver AplicarModoDeVideo).
    .allowed({"60", "30", "90", "120", "sin_limite"})
'''

FPS_VBLANK_ANCLA = '''  Poner("video_mode_refresh_rate",
        limite == "30" || limite == "90" || limite == "120" ? limite.c_str() : "60");
'''

FPS_VBLANK_NUEVO = '''  // PARCHE LOCAL (NFSMW Recompiled): "sin_limite" pone el vblank a 240 Hz, el maximo del SDK
  // (video_mode_refresh_rate va de 24 a 240). El juego espera como mucho ~4 ms a un vblank y el
  // tope pasa a ser lo que de el movil.
  Poner("video_mode_refresh_rate",
        limite == "sin_limite" ? "240"
        : limite == "30" || limite == "90" || limite == "120" ? limite.c_str() : "60");
'''

CONTEXTO_ANCLA = '''namespace nfsmw::recortes_carrera {
namespace {

constexpr uint32_t kBaseVistas = 0x82A38070;
'''

CONTEXTO_NUEVO = '''// PARCHE LOCAL (NFSMW Recompiled): las direcciones del juego que lee la app de Android
// (nativo_android.cpp, nativeContexto) para saber en que parte del juego se esta y cambiar
// los controles tactiles. Van aqui, en su app, porque crear_arbol.py las traduce a la edicion
// del juego con todo lo demas, y su comprobacion compara estas funciones en las dos ediciones.
//   [0] TheGameFlowManager.CurrentGameFlowState: 3 = menus, 6 = en el mundo
//   [1] GRaceStatus::fObj
//   [2] GRaceStatus::GetRaceType: lwz r3,0x1A30(r3), el puntero a los parametros de la carrera
//   [3] GRaceParameters::GetRaceType: el tipo, del indice de la base de datos (mIndex) o, en las
//       carreras rapidas, que no lo tienen, del atributo "racetype" de la carrera
//   [4] la tabla de los once nombres de tipo ("circuit", "p2p", "drag"...) y su valor
//   [5] Attrib: la busqueda de un atributo en una coleccion y en sus padres
//   [6] Attrib: donde esta el valor de un nodo
//   [7] FEManager::mPauseRequest: cuantas peticiones de pausa hay (menu de pausa, mensajes...)
//   [8] FEManager::RequestPauseSimulation: mPauseReason[mPauseRequest++] = motivo
// La app comprueba esas instrucciones antes de fiarse de los desplazamientos.
extern "C" const uint32_t g_nfsmw_android_contexto[9] = {0x82A39AD8, 0x82A2CB18, 0x820E5E28,
                                                         0x8233A000, 0x8290D828, 0x821485E8,
                                                         0x82145C50, 0x82A2C5CC, 0x82285B80};

namespace nfsmw::recortes_carrera {
namespace {

constexpr uint32_t kBaseVistas = 0x82A38070;
'''

VIBRAR_DECL_ANCLA = '''
namespace rex::input::sdl {

namespace {

// SDL clamps to SDL_MAX_RUMBLE_DURATION_MS, which is not a public constant.
'''

VIBRAR_DECL_NUEVO = '''
#if REX_PLATFORM_ANDROID
// PARCHE LOCAL (NFSMW Recompiled): la define la app de Android (nativo_android.cpp) y
// hace vibrar el movil con lo que el juego le pide al mando tactil (0-65535 cada motor).
extern "C" void NfsmwAndroidVibrar(uint16_t izquierdo, uint16_t derecho) __attribute__((weak));
#endif

namespace rex::input::sdl {

namespace {

// SDL clamps to SDL_MAX_RUMBLE_DURATION_MS, which is not a public constant.
'''

VIBRAR_PEDIR_ANCLA = '''  if (controller->is_touch) {
    return X_ERROR_SUCCESS;
  }

  // XInput vibration holds until the guest changes it, but SDL rumble expires,
'''

VIBRAR_PEDIR_NUEVO = '''  if (controller->is_touch) {
#if REX_PLATFORM_ANDROID
    // PARCHE LOCAL (NFSMW Recompiled): el mando tactil vibra con el movil.
    if (NfsmwAndroidVibrar) {
      NfsmwAndroidVibrar(vibration->left_motor_speed, vibration->right_motor_speed);
    }
#endif
    return X_ERROR_SUCCESS;
  }

  // XInput vibration holds until the guest changes it, but SDL rumble expires,
'''

VIBRAR_CAPS_ANCLA = '''    state.caps.vibration.left_motor_speed = 0;
    state.caps.vibration.right_motor_speed = 0;
    return;
  }
  assert(state.sdl);
'''

VIBRAR_CAPS_NUEVO = '''#if REX_PLATFORM_ANDROID
    // PARCHE LOCAL (NFSMW Recompiled): con la app que hace vibrar el movil, el
    // mando tactil tiene los dos motores.
    const uint16_t motores = NfsmwAndroidVibrar ? 0xFFFFu : 0;
#else
    const uint16_t motores = 0;
#endif
    state.caps.vibration.left_motor_speed = motores;
    state.caps.vibration.right_motor_speed = motores;
    return;
  }
  assert(state.sdl);
'''

CONJ_MIEMBROS_ANCLA = '''  std::array<VkDescriptorSetLayout, 4> layouts_{};
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, 4> sets_{};
'''

CONJ_MIEMBROS_NUEVO = '''  std::array<VkDescriptorSetLayout, 4> layouts_{};
  VkDescriptorPool pool_ = VK_NULL_HANDLE;
  std::array<VkDescriptorSet, 4> sets_{};
  // PARCHE LOCAL (NFSMW Recompiled): GPU con solo 4 conjuntos de descriptores (CrearDescriptores). Lo que se
  // enlaza de los montones (el de cubos va dentro del 1), cuantos son y en que conjunto van los UBO.
  bool cuatro_conjuntos_ = false;
  std::array<VkDescriptorSet, 4> sets_enlace_{};
  uint32_t n_sets_enlace_ = 4;
  uint32_t conjunto_ubo_ = 4;
'''

CONJ_CREAR_ANCLA = '''    for (uint32_t i = 0; i < 4; ++i) {
      VkDescriptorSetLayoutBinding enlace{};
      enlace.binding = 0;
      enlace.descriptorType = kTipos[i];
      enlace.descriptorCount = kCapacidadMonton[i];
      enlace.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      VkDescriptorSetLayoutBindingFlagsCreateInfo banderas{};
      banderas.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
      banderas.bindingCount = 1;
      banderas.pBindingFlags = &banderas_enlace;
      VkDescriptorSetLayoutCreateInfo info{};
      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      info.pNext = &banderas;
      info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
      info.bindingCount = 1;
      info.pBindings = &enlace;
      if (dfn_.vkCreateDescriptorSetLayout(device_, &info, nullptr, &layouts_[i]) != VK_SUCCESS) {
        return false;
      }
      montones_[i].capacidad = kCapacidadMonton[i];
    }
'''

CONJ_CREAR_NUEVO = '''    // PARCHE LOCAL (NFSMW Recompiled): los shaders usan 5 conjuntos de descriptores (los montones 2D, 3D,
    // cubos y samplers, y los UBO en el 4) y Vulkan solo garantiza 4 (maxBoundDescriptorSets). Los Mali
    // Valhall dan 4, y su driver se cae dentro de vkCreatePipelineLayout con 5. Con 4, el monton de cubos va
    // en el conjunto del 3D (enlace 1), los samplers en el 2 y los UBO en el 3; CrearModulo cambia los
    // shaders igual (JuntarConjuntos).
    {
      VkPhysicalDeviceProperties fisicas{};
      dispositivo_->vulkan_instance()->functions().vkGetPhysicalDeviceProperties(dispositivo_->physical_device(),
                                                                                 &fisicas);
      cuatro_conjuntos_ = fisicas.limits.maxBoundDescriptorSets < 5 || REXCVAR_GET(nfsmw_nativo_cuatro_conjuntos);
      if (cuatro_conjuntos_) {
        REXLOG_INFO("[compatibilidad] {} conjuntos de descriptores{}: el monton de cubos va en el conjunto del "
                    "3D y samplers y UBO bajan uno", fisicas.limits.maxBoundDescriptorSets,
                    fisicas.limits.maxBoundDescriptorSets < 5 ? "" : " (forzado: nfsmw_nativo_cuatro_conjuntos)");
      }
    }
    for (uint32_t i = 0; i < 4; ++i) {
      montones_[i].capacidad = kCapacidadMonton[i];
      if (cuatro_conjuntos_ && i == 2) {
        continue;  // el de cubos va en el conjunto 1
      }
      const bool con_cubo = cuatro_conjuntos_ && i == 1;
      VkDescriptorSetLayoutBinding enlaces[2]{};
      enlaces[0].binding = 0;
      enlaces[0].descriptorType = kTipos[i];
      enlaces[0].descriptorCount = kCapacidadMonton[i];
      enlaces[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      enlaces[1] = enlaces[0];
      enlaces[1].binding = 1;
      enlaces[1].descriptorType = kTipos[2];
      enlaces[1].descriptorCount = kCapacidadMonton[2];
      const VkDescriptorBindingFlags banderas_enlaces[2] = {banderas_enlace, banderas_enlace};
      VkDescriptorSetLayoutBindingFlagsCreateInfo banderas{};
      banderas.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
      banderas.bindingCount = con_cubo ? 2 : 1;
      banderas.pBindingFlags = banderas_enlaces;
      VkDescriptorSetLayoutCreateInfo info{};
      info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
      info.pNext = &banderas;
      info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
      info.bindingCount = con_cubo ? 2 : 1;
      info.pBindings = enlaces;
      if (dfn_.vkCreateDescriptorSetLayout(device_, &info, nullptr, &layouts_[i]) != VK_SUCCESS) {
        return false;
      }
    }
'''

CONJ_RESERVA_ANCLA = '''    VkDescriptorSetAllocateInfo reserva{};
    reserva.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    reserva.descriptorPool = pool_;
    reserva.descriptorSetCount = 4;
    reserva.pSetLayouts = layouts_.data();
    if (dfn_.vkAllocateDescriptorSets(device_, &reserva, sets_.data()) != VK_SUCCESS) {
      return false;
    }
'''

CONJ_RESERVA_NUEVO = '''    // PARCHE LOCAL (NFSMW Recompiled): con 4 conjuntos son 3 de montones, y sets_[2] (cubos) es el mismo
    // que sets_[1].
    std::array<VkDescriptorSetLayout, 4> layouts_reserva{};
    std::array<VkDescriptorSet, 4> reservados{};
    uint32_t n_reserva = 0;
    for (VkDescriptorSetLayout l : layouts_) {
      if (l != VK_NULL_HANDLE) {
        layouts_reserva[n_reserva++] = l;
      }
    }
    VkDescriptorSetAllocateInfo reserva{};
    reserva.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    reserva.descriptorPool = pool_;
    reserva.descriptorSetCount = n_reserva;
    reserva.pSetLayouts = layouts_reserva.data();
    if (dfn_.vkAllocateDescriptorSets(device_, &reserva, reservados.data()) != VK_SUCCESS) {
      return false;
    }
    n_sets_enlace_ = 0;
    for (uint32_t i = 0; i < 4; ++i) {
      if (layouts_[i] != VK_NULL_HANDLE) {
        sets_[i] = reservados[n_sets_enlace_];
        sets_enlace_[n_sets_enlace_++] = sets_[i];
      } else {
        sets_[i] = sets_[i - 1];
      }
    }
    conjunto_ubo_ = n_sets_enlace_;
'''

CONJ_LAYOUT_ANCLA = '''    const std::array<VkDescriptorSetLayout, 5> layouts_pipeline = {layouts_[0], layouts_[1], layouts_[2],
                                                                    layouts_[3], layout_ubo_};
    info_layout.setLayoutCount = 5;
'''

CONJ_LAYOUT_NUEVO = '''    // PARCHE LOCAL (NFSMW Recompiled): con 4 conjuntos, sin el de cubos (va en el 1) y los UBO en el 3.
    std::array<VkDescriptorSetLayout, 5> layouts_pipeline{};
    uint32_t n_layouts = 0;
    for (VkDescriptorSetLayout l : layouts_) {
      if (l != VK_NULL_HANDLE) {
        layouts_pipeline[n_layouts++] = l;
      }
    }
    layouts_pipeline[n_layouts++] = layout_ubo_;
    info_layout.setLayoutCount = n_layouts;
'''

CONJ_ENLACE1_ANCLA = '''      NFSMW_SUB(1, dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 0, 4,
                                                sets_.data(), 0, nullptr));
'''

CONJ_ENLACE1_NUEVO = '''      // PARCHE LOCAL (NFSMW Recompiled): los montones que haya (4 conjuntos: 3).
      NFSMW_SUB(1, dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 0,
                                                n_sets_enlace_, sets_enlace_.data(), 0, nullptr));
'''

CONJ_ENLACE2_ANCLA = '''      dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 0, 4,
                                   sets_.data(), 0, nullptr);
'''

CONJ_ENLACE2_NUEVO = '''      // PARCHE LOCAL (NFSMW Recompiled): los montones que haya (4 conjuntos: 3).
      dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 0, n_sets_enlace_,
                                   sets_enlace_.data(), 0, nullptr);
'''

CONJ_UBO1_ANCLA = '''NFSMW_SUB(2, dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 4, 1,'''

CONJ_UBO1_NUEVO = '''NFSMW_SUB(2, dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, conjunto_ubo_, 1,'''

CONJ_UBO2_ANCLA = '''    dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, 4, 1,
                                 &sets_ubo_[c.ranura_ubo], 3, c.offsets_ubo.data());
'''

CONJ_UBO2_NUEVO = '''    // PARCHE LOCAL (NFSMW Recompiled): los UBO van en el conjunto 3 si solo hay 4.
    dfn_.vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_pipeline_, conjunto_ubo_, 1,
                                 &sets_ubo_[c.ranura_ubo], 3, c.offsets_ubo.data());
'''

CONJ_ESCRIBIR_ANCLA = '''    escritura.dstSet = sets_[monton];
    escritura.dstBinding = 0;
'''

CONJ_ESCRIBIR_NUEVO = '''    escritura.dstSet = sets_[monton];
    // PARCHE LOCAL (NFSMW Recompiled): con 4 conjuntos, el monton de cubos es el enlace 1 del conjunto 1.
    escritura.dstBinding = cuatro_conjuntos_ && monton == 2 ? 1 : 0;
'''

CONJ_MODULO_ANCLA = '''  VkResult CrearModulo(const uint32_t* spirv, size_t bytes, VkShaderModule* modulo) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes;
    info.pCode = spirv;
    std::vector<uint32_t> convertido;
    if (spirv_13_ && nfsmw::spirv11::Necesita(spirv, bytes / 4)) {
      convertido = nfsmw::spirv11::Convertir(spirv, bytes / 4);
      if (!convertido.empty()) {
        info.codeSize = convertido.size() * sizeof(uint32_t);
        info.pCode = convertido.data();
      }
    }
    return dfn_.vkCreateShaderModule(device_, &info, nullptr, modulo);
  }
'''

CONJ_MODULO_NUEVO = '''  // PARCHE LOCAL (NFSMW Recompiled): con 4 conjuntos de descriptores (CrearDescriptores), las decoraciones
  // del shader: el monton de cubos (conjunto 2) pasa al 1 con su enlace + 1, y samplers (3) y UBO (4) bajan
  // uno. Si el SPIR-V esta roto se deja como este: que decida el driver.
  static void JuntarConjuntos(std::vector<uint32_t>& p) {
    constexpr uint32_t kOpDecorate = 71;
    constexpr uint32_t kBinding = 33;
    constexpr uint32_t kDescriptorSet = 34;
    std::unordered_set<uint32_t> cubos;
    for (size_t i = 5; i < p.size();) {
      const uint32_t n = p[i] >> 16;
      if (n == 0 || i + n > p.size()) {
        return;
      }
      if ((p[i] & 0xFFFF) == kOpDecorate && n >= 4 && p[i + 2] == kDescriptorSet) {
        if (p[i + 3] == 2) {
          cubos.insert(p[i + 1]);
          p[i + 3] = 1;
        } else if (p[i + 3] == 3 || p[i + 3] == 4) {
          --p[i + 3];
        }
      }
      i += n;
    }
    for (size_t i = 5; i < p.size();) {
      const uint32_t n = p[i] >> 16;
      if ((p[i] & 0xFFFF) == kOpDecorate && n >= 4 && p[i + 2] == kBinding && cubos.count(p[i + 1])) {
        ++p[i + 3];
      }
      i += n;
    }
  }

  VkResult CrearModulo(const uint32_t* spirv, size_t bytes, VkShaderModule* modulo) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = bytes;
    info.pCode = spirv;
    std::vector<uint32_t> convertido;
    if (spirv_13_ && nfsmw::spirv11::Necesita(spirv, bytes / 4)) {
      convertido = nfsmw::spirv11::Convertir(spirv, bytes / 4);
      if (!convertido.empty()) {
        info.codeSize = convertido.size() * sizeof(uint32_t);
        info.pCode = convertido.data();
      }
    }
    if (cuatro_conjuntos_) {
      if (convertido.empty()) {
        convertido.assign(spirv, spirv + bytes / 4);
      }
      JuntarConjuntos(convertido);
      info.codeSize = convertido.size() * sizeof(uint32_t);
      info.pCode = convertido.data();
    }
    return dfn_.vkCreateShaderModule(device_, &info, nullptr, modulo);
  }
'''

CONJ_CVAR_ANCLA = '''REXCVAR_DEFINE_BOOL(nfsmw_nativo_compartidas_cache, true, "NFSMW",
'''

CONJ_CVAR_NUEVO = '''// PARCHE LOCAL (NFSMW Recompiled): para probar en una GPU con 5 o mas conjuntos lo que hacen las de 4.
REXCVAR_DEFINE_BOOL(nfsmw_nativo_cuatro_conjuntos, false, "NFSMW",
                    "Renderizador nativo: juntar los montones de cubos y 3D en un conjunto de descriptores, como en "
                    "las GPU que solo admiten 4 (maxBoundDescriptorSets). Solo pruebas: las de 4 lo hacen solas")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(nfsmw_nativo_compartidas_cache, true, "NFSMW",
'''

ORDENES_CONST_ANCLA = '''constexpr uint32_t kRetornoOrdenes = 0x82441DC4;
'''

ORDENES_CONST_NUEVO = '''constexpr uint32_t kRetornoOrdenes = 0x82441DC4;
// PARCHE LOCAL (NFSMW Recompiled): la lista de ordenes de sub_823C8378 y sub_823C83F8 (ver abajo).
constexpr uint32_t kListaOrdenes = 0x82909650;
'''

ORDENES_HOOK_ANCLA = '''  __imp__sub_823C83F8(ctx, base);
}
'''

ORDENES_HOOK_NUEVO = '''  // PARCHE LOCAL (NFSMW Recompiled): solo las ordenes ya publicadas (ver sub_823C8378, abajo). El
  // contador, con adquisicion: lo que sub_823C8378 escribio antes de su barrera ya se ve.
  {
    const uint32_t escritas = __builtin_bswap32(
        __atomic_load_n(reinterpret_cast<const uint32_t*>(base + kListaOrdenes), __ATOMIC_ACQUIRE));
    const uint32_t publicadas = escritas - Leer32(base, kListaOrdenes + 4);
    if (ctx.r4.u32 > publicadas) {
      ctx.r4.u64 = publicadas;
    }
  }
  __imp__sub_823C83F8(ctx, base);
}

/*
 * PARCHE LOCAL (NFSMW Recompiled): anadir una orden a la lista, con barrera.
 *
 * La original copia los datos de la orden al final de la lista, escribe en la entrada la funcion (+0) y el
 * tamano redondeado a 16 (+4), mueve el final de la lista (+20) y sube su contador (+0), sin barreras. El
 * ejecutor (arriba) las va haciendo a la vez desde otro hilo, y en ARM puede ver el contador nuevo antes que la
 * entrada: llama a la funcion 0 y el juego se cierra. Aqui se escribe lo mismo y en el mismo sitio, con una
 * barrera de liberacion antes de publicar el final y el contador. Con la lista cerrada (+12 a 0) la funcion
 * se llama en el acto: eso lo sigue haciendo la original.
 */
REX_EXTERN(__imp__sub_823C8378);
REX_HOOK_RAW(sub_823C8378) {
  if (Leer32(base, kListaOrdenes + 12) == 0) {
    __imp__sub_823C8378(ctx, base);
    return;
  }
  const auto escribir = [base](uint32_t direccion, uint32_t valor) {
    valor = __builtin_bswap32(valor);
    std::memcpy(base + direccion, &valor, sizeof(valor));
  };
  const uint32_t bytes = (ctx.r6.u32 + 15) & ~15u;
  const uint32_t entrada = Leer32(base, kListaOrdenes + 20);
  std::memmove(base + entrada, base + ctx.r4.u32, bytes);
  escribir(entrada + 0, ctx.r5.u32);
  escribir(entrada + 4, bytes);
  std::atomic_thread_fence(std::memory_order_release);
  escribir(kListaOrdenes + 20, entrada + bytes);
  escribir(kListaOrdenes + 0, Leer32(base, kListaOrdenes + 0) + 1);
  ctx.r3.u64 = entrada;  // lo que deja la original: el resultado de su memmove
}
'''

XCLIPSE_BC_ANCLA = '''      bc_cpu_[i] = REXCVAR_GET(nfsmw_nativo_texturas_bc_cpu) || (fp.optimalTilingFeatures & requerido) != requerido;
'''

XCLIPSE_BC_NUEVO = '''      // PARCHE LOCAL (NFSMW Recompiled): las Xclipse de Samsung dicen tener BC4 y BC5, pero su driver solo
      // tiene completas BC1-3: BC4-7 las convierte el mismo en cada subida (tirones) por un camino a medias
      // (XclipseDecomp; ExynosTools y Eden las esquivan). Esas dos, en la CPU, como sin BC.
      const bool xclipse = propiedades.driverID == VK_DRIVER_ID_SAMSUNG_PROPRIETARY ||
                           std::strstr(propiedades.deviceName, "Xclipse") != nullptr;
      bc_cpu_[i] = REXCVAR_GET(nfsmw_nativo_texturas_bc_cpu) || (fp.optimalTilingFeatures & requerido) != requerido ||
                   (xclipse && i >= 3);
'''

SOMBRAS_CVAR_ANCLA = '''                     "mismo juego (1024). El valor se toma al crear el primer mapa y no cambia en marcha")
    .range(50, 100);
'''

SOMBRAS_CVAR_NUEVO = '''                     "mismo juego (1024). El valor se toma al crear el primer mapa y no cambia en marcha. "
                     "PARCHE LOCAL (NFSMW Recompiled): hasta 250 (4000x4000), mas definicion en las sombras "
                     "de los coches")
    .range(50, 250);
'''

SOMBRAS_TAMANO_ANCLA = '''        escala_sombras_ = uint32_t(std::clamp(REXCVAR_GET(nfsmw_nativo_sombras_escala), 50, 100));
        if (!blit_ || !profundidad_escalable_) {
          escala_sombras_ = 100;
        }
      }
      if (escala_sombras_ < 100) {
'''

SOMBRAS_TAMANO_NUEVO = '''        // PARCHE LOCAL (NFSMW Recompiled): tambien por encima de 100 %. La resuelta sigue al mapa y la
        // copia es 1 a 1 (CopiarProfundidad), igual que al reducirlo: mas grande es mas detalle.
        escala_sombras_ = uint32_t(std::clamp(REXCVAR_GET(nfsmw_nativo_sombras_escala), 50, 250));
        if (!blit_ || !profundidad_escalable_) {
          escala_sombras_ = 100;
        }
      }
      if (escala_sombras_ != 100) {
'''

SOMBRAS_AREA_ANCLA = '''  void AnotarAreaUtil(const Imagen& destino, int32_t y1) {
    if (y1 > 0) {
      auto& e = estado_destino_[&destino];
      e.alto_usado = std::max(e.alto_usado, std::min(uint32_t(y1), destino.alto));
    }
  }
'''

SOMBRAS_AREA_NUEVO = '''  void AnotarAreaUtil(const Imagen& destino, int32_t y1) {
    if (y1 > 0) {
      auto& e = estado_destino_[&destino];
      // PARCHE LOCAL (NFSMW Recompiled): y1 viene en pixeles del juego. En el mapa de sombras escalado
      // la imagen tiene otro alto: se pasa a pixeles de la imagen, o con un mapa mas grande que 1600 la
      // restauracion (RestaurarContenido) dejaba sin copiar lo de abajo.
      uint32_t alto = uint32_t(y1);
      if (destino.alto_guest && destino.alto_guest != destino.alto) {
        alto = uint32_t((uint64_t(alto) * destino.alto + destino.alto_guest - 1) / destino.alto_guest);
      }
      e.alto_usado = std::max(e.alto_usado, std::min(alto, destino.alto));
    }
  }
'''

ESTELAS_ANCLA = '''  if (!g_aviso_caras.exchange(true)) {
    REXLOG_INFO("[recortes] carrera: cubemap limitado a {} cara(s) por fotograma mas {} fija(s) "
                "(mascara 0x{:X}; el juego activaba {})",
                maximo, fijas, siempre, n + fijas);
  }
}
'''

ESTELAS_NUEVO = ESTELAS_ANCLA + '''
/*
 * PARCHE LOCAL (NFSMW Recompiled): las estelas de luz del nitro, del largo de la Xbox 360 a cualquier fps.
 *
 * VehicleRenderConn::RenderFlares (sub_824E63D8; r3 la vista, r4 el reflejo) guarda en cada fotograma de la
 * vista del jugador (id 1 o 2 en +4, sin reflejo) la posicion y la matriz del coche en un anillo de 3 de su
 * CarRenderInfo (VehicleRenderConn +68): el indice en +4752, las matrices (64 bytes) en +4512 y las
 * posiciones (16 bytes) en +4704. Con el nitro dibuja destellos entre esas tres, asi que la estela es lo que
 * avanza el coche en dos fotogramas: 66 ms en la consola, a 30 fps, y la mitad a 60. Antes de la original,
 * los dos huecos que no va a escribir (el mas viejo y el del medio) se llenan con la posicion de hace 2/30 s y
 * 1/30 s, interpoladas de un historial propio por coche hecho con lo que la original escribe; la original pone
 * la de ahora en el tercero. Solo ella lee el anillo. Sin historial suficiente (los primeros fotogramas) se
 * deja lo del juego.
 */
#include <array>
#include <deque>

REXCVAR_DEFINE_BOOL(nfsmw_estelas_nitro_30, true, "NFSMW",
                    "Estelas de luz del nitro con el largo de la Xbox 360 (30 fps) a cualquier limite de fps. "
                    "false = como el juego, que las acorta al subir los fps");

namespace nfsmw::estelas_nitro {
namespace {
constexpr uint32_t kListaCoches = 0x82C84D38;  // VehicleRenderConn::GetList(): +4 el array, +12 cuantos
constexpr uint32_t kOffInfo = 68;              // VehicleRenderConn -> CarRenderInfo
constexpr uint32_t kOffIndice = 4752;          // CarRenderInfo::matrixIndex
constexpr uint32_t kOffMatrices = 4512;        // CarRenderInfo::LastFewMatrices[3]
constexpr uint32_t kOffPosiciones = 4704;      // CarRenderInfo::LastFewPositions[3]
constexpr double kPaso = 1.0 / 30.0;           // un fotograma de la Xbox 360
constexpr double kGuardar = 0.25;              // segundos de historial
constexpr size_t kMaxMuestras = 96;
constexpr size_t kMaxCoches = 32;
constexpr uint32_t kMaxLista = 64;

struct Muestra {
  double t;
  float pos[3];
  uint8_t matriz[64];  // tal cual en la memoria del juego
};
struct Historial {
  uint32_t info = 0;
  double usado = 0;
  std::deque<Muestra> muestras;
};
std::array<Historial, kMaxCoches> g_historiales;

uint32_t Leer(const uint8_t* base, uint32_t dir) {
  uint32_t v;
  std::memcpy(&v, base + dir, sizeof(v));
  return __builtin_bswap32(v);
}
float LeerF(const uint8_t* base, uint32_t dir) {
  const uint32_t v = Leer(base, dir);
  float f;
  std::memcpy(&f, &v, sizeof(f));
  return f;
}
void EscribirF(uint8_t* base, uint32_t dir, float f) {
  uint32_t v;
  std::memcpy(&v, &f, sizeof(v));
  v = __builtin_bswap32(v);
  std::memcpy(base + dir, &v, sizeof(v));
}

Historial* Buscar(uint32_t info) {
  for (auto& h : g_historiales) {
    if (h.info == info) {
      return &h;
    }
  }
  return nullptr;
}
Historial& BuscarOCrear(uint32_t info, double ahora) {
  if (Historial* h = Buscar(info)) {
    return *h;
  }
  Historial* libre = &g_historiales[0];
  for (auto& h : g_historiales) {
    if (h.info == 0) {
      libre = &h;
      break;
    }
    if (h.usado < libre->usado) {
      libre = &h;
    }
  }
  libre->info = info;
  libre->muestras.clear();
  libre->usado = ahora;
  return *libre;
}

// La posicion en el instante t, interpolada entre las dos muestras que lo rodean, y la matriz de la mas
// cercana. false si t es anterior a la muestra mas vieja.
bool Interpolar(const Historial& h, double t, float pos[3], const uint8_t** matriz) {
  const auto& m = h.muestras;
  if (m.empty() || t < m.front().t) {
    return false;
  }
  for (size_t i = m.size(); i-- > 0;) {
    if (m[i].t <= t) {
      if (i + 1 >= m.size()) {
        std::memcpy(pos, m[i].pos, sizeof(m[i].pos));
        *matriz = m[i].matriz;
        return true;
      }
      const Muestra& a = m[i];
      const Muestra& b = m[i + 1];
      const double tramo = b.t - a.t;
      const float f = tramo > 0 ? float((t - a.t) / tramo) : 0.0f;
      for (int k = 0; k < 3; ++k) {
        pos[k] = a.pos[k] + (b.pos[k] - a.pos[k]) * f;
      }
      *matriz = f < 0.5f ? a.matriz : b.matriz;
      return true;
    }
  }
  return false;
}

void Rellenar(uint8_t* base, uint32_t info, int32_t hueco, const float pos[3], const uint8_t* matriz) {
  const uint32_t p = info + kOffPosiciones + uint32_t(hueco) * 16;
  for (int k = 0; k < 3; ++k) {
    EscribirF(base, p + uint32_t(k) * 4, pos[k]);
  }
  std::memcpy(base + info + kOffMatrices + uint32_t(hueco) * 64, matriz, 64);
}
}  // namespace
}  // namespace nfsmw::estelas_nitro

REX_EXTERN(__imp__sub_824E63D8);
REX_HOOK_RAW(sub_824E63D8) {
  using namespace nfsmw::estelas_nitro;
  const uint32_t vista = ctx.r3.u32;
  const uint32_t reflejo = ctx.r4.u32;
  const uint32_t id_vista = vista ? Leer(base, vista + 4) : 0;
  if (!REXCVAR_GET(nfsmw_estelas_nitro_30) || reflejo != 0 || (id_vista != 1 && id_vista != 2)) {
    __imp__sub_824E63D8(ctx, base);
    return;
  }
  const double ahora =
      std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  const uint32_t lista = Leer(base, kListaCoches + 4);
  const uint32_t cuantos = lista ? std::min(Leer(base, kListaCoches + 12), kMaxLista) : 0;
  std::array<uint32_t, kMaxLista> infos{};
  std::array<int32_t, kMaxLista> indices{};
  for (uint32_t i = 0; i < cuantos; ++i) {
    const uint32_t conn = Leer(base, lista + i * 4);
    const uint32_t info = conn ? Leer(base, conn + kOffInfo) : 0;
    infos[i] = info;
    if (!info) {
      continue;
    }
    const int32_t indice = int32_t(Leer(base, info + kOffIndice));
    indices[i] = indice;
    Historial* h = Buscar(info);
    if (!h || h->muestras.empty() || ahora - h->muestras.back().t > kGuardar) {
      continue;
    }
    // El hueco que va a escribir la original: indice < 0 -> 0, y luego +1 dando la vuelta a 3.
    int32_t nuevo = (indice < 0 ? 0 : indice) + 1;
    if (nuevo > 2) {
      nuevo = 0;
    }
    float pos[3];
    const uint8_t* matriz = nullptr;
    if (Interpolar(*h, ahora - kPaso, pos, &matriz)) {
      Rellenar(base, info, (nuevo + 2) % 3, pos, matriz);  // el del medio: hace un fotograma de la consola
    }
    if (Interpolar(*h, ahora - 2 * kPaso, pos, &matriz)) {
      Rellenar(base, info, (nuevo + 1) % 3, pos, matriz);  // el mas viejo: hace dos
      static std::atomic<bool> anotado{false};
      if (!anotado.exchange(true)) {
        REXLOG_INFO("[estelas] nitro: la estela va con el largo de la Xbox 360 (posiciones de hace 1/30 s "
                    "y 2/30 s, {} muestras del coche; {} coches en la lista)",
                    h->muestras.size(), cuantos);
      }
    }
  }
  __imp__sub_824E63D8(ctx, base);
  // Lo que la original acaba de guardar, al historial (solo los coches cuyo indice ha avanzado).
  for (uint32_t i = 0; i < cuantos; ++i) {
    const uint32_t info = infos[i];
    if (!info) {
      continue;
    }
    const int32_t indice = int32_t(Leer(base, info + kOffIndice));
    if (indice == indices[i] || indice < 0 || indice > 2) {
      continue;
    }
    Historial& h = BuscarOCrear(info, ahora);
    h.usado = ahora;
    Muestra m;
    m.t = ahora;
    const uint32_t p = info + kOffPosiciones + uint32_t(indice) * 16;
    for (int k = 0; k < 3; ++k) {
      m.pos[k] = LeerF(base, p + uint32_t(k) * 4);
    }
    std::memcpy(m.matriz, base + info + kOffMatrices + uint32_t(indice) * 64, sizeof(m.matriz));
    h.muestras.push_back(m);
    while (h.muestras.size() > kMaxMuestras ||
           (h.muestras.size() > 2 && ahora - h.muestras.front().t > kGuardar)) {
      h.muestras.pop_front();
    }
  }
}
'''

BLOQUES = [
    ("sdk/include/rex/filesystem.h", "declarar SetAndroidContentOpener", CABECERA_ANCLA, CABECERA_NUEVO),
    ("sdk/src/core/filesystem_posix.cpp", "abrir la URI con lo que ponga la app", FUENTE_ANCLA, FUENTE_NUEVO),
    ("sdk/src/ui/windowed_app_main_sdl.cpp", "la sonda de Vulkan antes del juego", SONDA_ANCLA, SONDA_NUEVO),
    ("app/src/nfsmw_app.h", "declarar NfsmwAndroidAudio", AUDIO_DECL_ANCLA, AUDIO_DECL_NUEVO),
    ("app/src/nfsmw_app.h", "la salida de audio de la app", AUDIO_ANCLA, AUDIO_NUEVO),
    ("app/src/nfsmw_nativo_sistema.cpp", "declarar la pausa del anillo", PAUSA_DECL_ANCLA, PAUSA_DECL_NUEVO),
    ("app/src/nfsmw_nativo_sistema.cpp", "parar el anillo en el Swap", PAUSA_SWAP_ANCLA, PAUSA_SWAP_NUEVO),
    ("app/src/nfsmw_ajustes_graficos.cpp", "fps sin limite: el valor", FPS_PERMITIDOS_ANCLA, FPS_PERMITIDOS_NUEVO),
    ("app/src/nfsmw_ajustes_graficos.cpp", "fps sin limite: el vblank a 240 Hz", FPS_VBLANK_ANCLA,
     FPS_VBLANK_NUEVO),
    ("app/src/nfsmw_recortes_carrera.cpp", "direcciones del contexto del juego", CONTEXTO_ANCLA,
     CONTEXTO_NUEVO),
    ("sdk/src/input/sdl/sdl_input_driver.cpp", "vibrar: declarar la de la app", VIBRAR_DECL_ANCLA,
     VIBRAR_DECL_NUEVO),
    ("sdk/src/input/sdl/sdl_input_driver.cpp", "vibrar: pasarle al movil lo que pide el juego",
     VIBRAR_PEDIR_ANCLA, VIBRAR_PEDIR_NUEVO),
    ("sdk/src/input/sdl/sdl_input_driver.cpp", "vibrar: el mando tactil tiene motores", VIBRAR_CAPS_ANCLA,
     VIBRAR_CAPS_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: forzarlo para probar", CONJ_CVAR_ANCLA, CONJ_CVAR_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: los miembros", CONJ_MIEMBROS_ANCLA, CONJ_MIEMBROS_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: los layouts de los montones", CONJ_CREAR_ANCLA,
     CONJ_CREAR_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: reservar los de los montones", CONJ_RESERVA_ANCLA,
     CONJ_RESERVA_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: el layout de los pipelines", CONJ_LAYOUT_ANCLA,
     CONJ_LAYOUT_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: enlazar los montones (dibujo)", CONJ_ENLACE1_ANCLA,
     CONJ_ENLACE1_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: enlazar los montones (cielo)", CONJ_ENLACE2_ANCLA,
     CONJ_ENLACE2_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: los UBO (dibujo)", CONJ_UBO1_ANCLA, CONJ_UBO1_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: los UBO (cielo)", CONJ_UBO2_ANCLA, CONJ_UBO2_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: escribir el monton de cubos", CONJ_ESCRIBIR_ANCLA,
     CONJ_ESCRIBIR_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "4 conjuntos: los shaders", CONJ_MODULO_ANCLA, CONJ_MODULO_NUEVO),
    ("app/src/nfsmw_espera_fotograma.cpp", "cola de ordenes: la direccion", ORDENES_CONST_ANCLA,
     ORDENES_CONST_NUEVO),
    ("app/src/nfsmw_espera_fotograma.cpp", "cola de ordenes: con barreras", ORDENES_HOOK_ANCLA, ORDENES_HOOK_NUEVO),
    ("app/src/nfsmw_nativo_dibujos.cpp", "Xclipse: BC4 y BC5 en la CPU", XCLIPSE_BC_ANCLA, XCLIPSE_BC_NUEVO),
    ("app/src/nfsmw_nativo_destinos.cpp", "sombras mas grandes: el ajuste", SOMBRAS_CVAR_ANCLA,
     SOMBRAS_CVAR_NUEVO),
    ("app/src/nfsmw_nativo_destinos.cpp", "sombras mas grandes: el tamano del mapa", SOMBRAS_TAMANO_ANCLA,
     SOMBRAS_TAMANO_NUEVO),
    ("app/src/nfsmw_nativo_destinos.cpp", "sombras mas grandes: el alto util en pixeles de la imagen",
     SOMBRAS_AREA_ANCLA, SOMBRAS_AREA_NUEVO),
    ("app/src/nfsmw_recortes_carrera.cpp", "estelas del nitro del largo de la Xbox 360", ESTELAS_ANCLA,
     ESTELAS_NUEVO),
]


def leer(f):
    with open(f, encoding="utf-8", newline="") as h:
        txt = h.read()
    eol = "\r\n" if "\r\n" in txt else "\n"
    return txt.replace("\r\n", "\n"), eol


def escribir(f, txt, eol):
    # Solo si cambia: reescribir un fichero igual le cambia la fecha, y Ninja
    # recompila todo lo que lo incluye (filesystem.h, medio SDK).
    nuevo = txt.replace("\n", eol)
    with open(f, encoding="utf-8", newline="") as h:
        if h.read() == nuevo:
            return
    with open(f, "w", encoding="utf-8", newline="") as h:
        h.write(nuevo)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--arbol", default=str(RAIZ.parent / "nfsmw-android"),
                   help="el arbol del motor nativo (con sdk/ y app/)")
    p.add_argument("--estado", action="store_true")
    p.add_argument("--revertir", action="store_true")
    args = p.parse_args()

    arbol = pathlib.Path(args.arbol)
    ficheros = {}
    for ruta, _, _, _ in BLOQUES:
        if ruta in ficheros:
            continue
        f = arbol / ruta
        if not f.exists():
            sys.exit(f"[ERROR] No encuentro {f}")
        ficheros[ruta] = [f, *leer(f)]

    if args.estado:
        puestos = sum(1 for r, _, _, nuevo in BLOQUES if nuevo in ficheros[r][1])
        print(f"  nativo                     {puestos} de {len(BLOQUES)} bloques aplicados")
        for ruta, nombre, _, nuevo in BLOQUES:
            print(f"      {'si' if nuevo in ficheros[ruta][1] else 'NO':>2}  {nombre}  ({ruta})")
        return 0

    if args.revertir:
        for ruta, _, ancla, nuevo in BLOQUES:
            ficheros[ruta][1] = ficheros[ruta][1].replace(nuevo, ancla)
        for f, txt, eol in ficheros.values():
            escribir(f, txt, eol)
        print("[ok] nativo: quitado")
        return 0

    faltan = [b for b in BLOQUES if b[3] not in ficheros[b[0]][1]]
    if not faltan:
        print(f"[ok] nativo: los {len(BLOQUES)} bloques ya estaban")
        return 0
    # Todos los anclajes antes de escribir nada.
    for ruta, nombre, ancla, _ in faltan:
        n = ficheros[ruta][1].count(ancla)
        if n != 1:
            sys.exit(f"[ERROR] El anclaje de '{nombre}' aparece {n} veces en {ruta}, esperaba 1. "
                     f"No he tocado nada.")
    for ruta, nombre, ancla, nuevo in faltan:
        ficheros[ruta][1] = ficheros[ruta][1].replace(ancla, nuevo)
        print(f"[ok] Aplicado: {nombre}")
    for f, txt, eol in ficheros.values():
        escribir(f, txt, eol)
    return 0


if __name__ == "__main__":
    sys.exit(main())
