# El motor nativo

El APK de Android se puede compilar con dos motores:

| Motor | Qué hace | Cómo se pide |
|---|---|---|
| `xenos` | El SDK imita la GPU de la Xbox 360 (EDRAM, resolves, shaders traducidos al vuelo). Es el de siempre, y sigue siendo el que sale si no se dice nada. | `gradlew assembleRelease` |
| `nativo` | El renderizador nativo de [nfsmw-android](https://github.com/codepdbh/nfsmw-android), el port a Android de [nfsmw-nx](https://github.com/StevensND/nfsmw-nx): lee la lista de comandos de GPU del juego y dibuja con Vulkan, sin imitar la EDRAM. | `gradlew assembleRelease -Pnfsmw.motor=nativo` |

**Estado: funciona en el móvil** (Snapdragon 8 Elite), y va mejor que el de Xenos. Las
opciones portadas después (Turnip, sonda, afinidad, gráficos) están sin probar una a una:
ver "Qué falta".

## Por qué

Con el camino de Xenos la carrera de demostración va a 36–45 fps en un Snapdragon 8 Elite,
después de todo lo que hay en [android.md](android.md). nfsmw-android mide **62 fps** de
media en esa misma escena y el mismo SoC (Galaxy S25 Ultra), a 1280×720 y con el tope en
60. La diferencia no es un ajuste: es no imitar la GPU.

Lo que hace ese renderizador está contado en su `docs/native-renderer.md`. En corto:

- Los render targets son imágenes de Vulkan y los resolves, copias; muchas se evitan
  intercambiando las dos imágenes.
- Los shaders van traducidos de antemano en una biblioteca (`nfsmw_shaders.nfsp`), en vez
  de traducir el microcódigo al vuelo.
- Solo se devuelven al juego las imágenes de 64×64 que usa la exposición.
- Tres fotogramas en vuelo: la CPU graba el siguiente mientras la GPU dibuja el actual.
- Las funciones más calientes del juego (materiales, matrices, visibilidad, escenario)
  están reescritas en C++ y se comparan en marcha con las originales.
- Código generado con llamadas directas y ThinLTO.

## Es otro programa

El motor nativo no es un parche sobre nuestro SDK. Es **otro árbol nativo entero**, que
se queda al lado de este repositorio, en `..\nfsmw-android`:

- su SDK: un ReXGlue v0.10.0 con sus propios cambios (presentador, audio, ficheros) y su
  propio port a Android, que no es el de hells-gate;
- su app: el renderizador (`app/src/nfsmw_nativo_*`, unas 27.600 líneas), audio y vídeo;
- su código generado, con **su** generador (los registros del PowerPC como variables de
  C++ y llamadas directas entre funciones).

Nada de eso se copia aquí. De este repositorio entran en `libmain.so`
[`nativo_android.cpp`](../android/app/src/main/cpp/nativo/nativo_android.cpp), con los
puentes JNI de nuestro lanzador, y dos ficheros que ya usaba el motor de Xenos:
`sonda_vulkan.cpp` y `afinidad.cpp`. A su SDK se le aplican nuestros parches de ISO,
gamertag y Turnip, que entran tal cual.

Lo que **sí** es nuestro con los dos motores es la app de Java: la pantalla de inicio, el
mando táctil y su editor, los idiomas, elegir la ISO sin copiarla, los drivers propios y la
sonda de Vulkan.

```
android/app/src/main/cpp/CMakeLists.txt     motor xenos
android/app/src/main/cpp/nativo/nativo.cmake    motor nativo: compila ..\nfsmw-android\app
android/app/src/main/cpp/nativo/nativo_android.cpp   táctil, fps, ISO, sonda y afinidad
android/app/src/main/cpp/sonda_vulkan.cpp       la sonda de Vulkan (los dos motores)
android/app/src/main/cpp/afinidad.cpp           hilos fijados a núcleos (los dos motores)
android/app/src/main/cpp/audio/aaudio_driver.cpp   salida de audio AAudio (los dos motores)
android/app/src/main/java/.../MotorNativo.java  carpetas, ajustes y shaders del motor nativo
tools/android/preparar_nativo.py                deja ..\nfsmw-android listo
tools/android/parche_nativo.py                  parche a su SDK: abrir una URI y la sonda
```

## Compilar

Además de lo de [android.md](android.md) hace falta **Node.js**, para la biblioteca de
shaders.

```
python tools/android/preparar_nativo.py --iso "D:\dumps\NFSMW.iso"
cd android
gradlew assembleRelease -Pnfsmw.motor=nativo
```

`preparar_nativo.py`:

1. clona nfsmw-android en un commit fijo;
2. rellena los submódulos de su SDK. Si `..\rexglue-sdk-android` ya está y son los mismos
   commits (lo son: los dos parten del SDK v0.10.0), los copia de ahí; bajarlos tarda
   mucho;
3. pone libadrenotools en su `thirdparty` y le aplica a su SDK los mismos parches que al
   nuestro para la ISO, el gamertag y Turnip ([`parche_iso.py`](../tools/parche_iso.py),
   `parche_gamertag.py`, `parche_turnip.py`), más `parche_nativo.py`;
4. extrae de la ISO `default.xex` y `NFS/` (los vídeos no hacen falta para compilar);
5. compila su generador y traduce el juego;
6. genera la biblioteca de shaders con las herramientas WASM de su app;
7. apunta la edición en `out/edicion.json`.

Como con el SDK, el generador necesita `cmake`, `ninja` y `clang++` en el PATH; con
`--rexglue <exe>` se usa uno ya compilado **desde ese árbol**.

La compilación nativa va aparte de la del motor de Xenos (otra carpeta `.cxx`), así que la
primera vez son unos 30 minutos. Los dos APK tienen el mismo identificador: instalar uno
sustituye al otro. La pantalla de inicio dice cuál es ("Renderizador nativo").

`assets/`, `app*/generated/` y `out/` de ese árbol salen de tu copia del juego. El APK
lleva dentro el código traducido y la biblioteca de shaders: **no se comparte**, igual que
el del motor de Xenos.

## Otras ediciones (USA y Japón)

Su app está escrita para la PAL España: cada gancho, cada función declarada a mano
(`nfsmw.toml`, `overrides.toml`) y cada límite de función (`huecos.toml`) lleva una
dirección de ese ejecutable. La USA y la japonesa son otras compilaciones del juego, con el
código en otras direcciones; en la japonesa, además, `.data` crece 0x5A0 bytes y sus datos
se mueven dentro, a trozos. Con la ISO de una de ellas, **después de preparar la PAL**:

```
python tools/android/preparar_nativo.py --iso "D:\dumps\NFSMW (USA).iso"
cd android
gradlew assembleRelease -Pnfsmw.motor=nativo -Pnfsmw.edicion=usa      (o jpn)
```

Hace lo mismo que nfsmw-nx para sus builds USA y japonesa (en su árbol,
`docs/editions.md`), con sus herramientas donde no hace falta numpy:

1. Extrae el juego en `assets/game_root_<edición>`; la PAL de `assets/game_root` se queda.
2. Junta las 56.802 direcciones de la app PAL: las de sus fuentes y sus `.toml`, las del
   reparto de funciones del codegen y las de las funciones que su código generado sigue
   llamando con gancho. Las traduce con [`tools/ediciones/emparejar.py`](../tools/ediciones/emparejar.py)
   (el método de su `emparejar.py`, sin numpy) a `out/<edición>/tabla.tsv`:
   - el código, con anclas de 12 instrucciones normalizadas; sin la búsqueda por
     contenido, que en la USA dio una vez con otra función;
   - `.rdata`, por contenido; `.data`, en la misma dirección si la sección no cambia de
     tamaño (USA);
   - si cambia (japonesa), **por el código que calcula cada dirección**, como su
     `datos_por_referencias.py`: las parejas `lis` + parte baja que la dan en la PAL se
     llevan a la otra edición con las anclas y se lee lo que calculan allí. Sin
     referencias propias, lo que se mueven las vecinas: a menos de 0x400, o las dos más
     cercanas, una a cada lado, si se mueven lo mismo (las vistas 13 y 14 de la tabla de
     vistas, cuya base sí se calcula: +0x5A0 con 63 de 63 parejas).

   USA: 56.771 seguras; japonesa: 56.755. Las dudosas solo se usan para repartir funciones
   entre ficheros. De las parejas que nfsmw-nx corrigió a mano salen solas, con el mismo
   valor, 10 de 11 (USA) y 13 de 14 (japonesa), y sus 6 huecos japoneses dan lo mismo.
3. Comprueba las funciones que usa el código de la app (no las que solo nombran sus
   comentarios): enteras, salvo direcciones, y además cada dirección que calculan con
   `lis` + parte baja tiene que ser la traducción de la PAL (su `verificar_parejas.py`).
   Son 846 direcciones calculadas, todas bien en las dos ediciones: es lo que comprueba la
   traducción de `.data` en la japonesa. Las constantes de `.rdata` que la otra edición
   cambia de valor salen como aviso: dos (tres en la USA), en la función de la que el
   servidor de audio solo usa una dirección de retorno.
4. Crea `app_<edición>` con su `crear_arbol.py`: la app con todo traducido, sus parejas y
   huecos corregidos (`tools/editions/<edición>/parejas_corregidas.json`) y las huellas
   de los shaders que el renderizador reconoce y que cambian: resplandor y cielo en la USA;
   resplandor, cielo y la composición final en la japonesa. Están en
   [`ediciones.json`](../tools/ediciones/ediciones.json), en `"nativo"`; salieron
   comparando el microcódigo de los contenedores de las dos ediciones, como su
   `shaders_especiales.py`.
5. Codegen, llamadas directas y copias literales sobre `app_<edición>`, y la biblioteca de
   shaders y `edicion.json` en `out/<edición>`. Las dos bibliotecas coinciden byte a byte
   con las oficiales de nfsmw-nx (el SHA-256 de su manifiesto).

El codegen de las dos da los mismos cuatro avisos que el de la PAL, en las direcciones
traducidas. Con `-Pnfsmw.edicion=<edición>`, Gradle compila `app_<edición>` en vez de
`app` (`NFSMW_NATIVO_APP`), en su propia carpeta `.cxx`, y empaqueta `out/<edición>`. El
idioma y el país que se pasan al juego salen de la edición: inglés y EE. UU. (1 y 103),
japonés y Japón (2 y 53).

## La ISO, sin copiarla

nfsmw-android pide el juego extraído en una carpeta de la memoria interna y el permiso de
acceso a todos los archivos. Aquí no: se sigue eligiendo la ISO con el selector del
sistema y el juego la lee en su sitio.

- `parche_iso.py` se aplica tal cual a su SDK (los seis bloques): acepta un fichero o una
  URI como `--game_data_root` y lo monta con `DiscImageDevice`.
- En su port a Android, `OpenAndroidContentFileDescriptor` está vacía. Con
  `parche_nativo.py` el SDK guarda un puntero a quien sabe abrir la URI, y
  `nativo_android.cpp` lo apunta al cargar `libmain.so`: llama por JNI a
  `GameActivity.openContentFd`, que ya existía.

Su app solo lee por ruta la biblioteca de shaders, que busca en "la carpeta del
ejecutable". En Android esa es `REX_APP_FOLDER`, que `MotorNativo.java` pone en
`files/nativo/usuario` y donde copia la biblioteca desde el APK.

## Lo que cambia en la app

- **Carpetas.** Datos en almacenamiento interno, `files/nativo/usuario`: `nfsmw.toml`, la
  biblioteca de shaders, la caché de pipelines y las partidas. La primera vez se copia ahí
  lo que hubiera guardado el motor de Xenos (`Android/data/.../files/datos`), sin tocar el
  original.
- **Ajustes.** Lo que elige la pantalla de inicio se traduce a sus cvars
  (`MotorNativo.argumentos`):

  | Pantalla de inicio | Motor nativo |
  |---|---|
  | Resolución y Estirar | igual que antes: el búfer y el tamaño de la vista los pone `GameActivity`, y el presentador llena el búfer |
  | Resolución de render (nuevo) | `nfsmw_resolucion_interna`: 1024×576, 1280×720 o 1920×1080 |
  | Límite de fps (nuevo, desplegable) | `nfsmw_limite_fps`: 30, 60, 90, 120 o `sin_limite` |
  | Suavizado | `nfsmw_antialiasing` = `fxaa` o `apagado`. MSAA no hay: la escena va en una pasada |
  | Filtro anisótropo | `nfsmw_nativo_anisotropico`: 0, 2, 4 o 16 |
  | Sombras, distancia de sombras (nuevos) | `nfsmw_sombras_cada` (1 o 2) y `nfsmw_sombras_corte` (100, 150 o 200) |
  | Reflejos del coche, reflejo del asfalto (nuevos) | `nfsmw_cubemap_caras_max` (6, 2, 1) y `nfsmw_reflejo_carretera` |
  | Resplandor del cielo, filtro de color, desenfoque, filtro de imagen (nuevos) | `nfsmw_resplandor_cielo`, `nfsmw_tratamiento_visual`, `nfsmw_nativo_sin_desenfoque` y `nfsmw_posproceso` |
  | Escalado a la pantalla (nuevo, los dos motores) | `present_effect`: `bilinear`, `fsr` (AMD FSR 1.0) o `cas` (AMD CAS) |
  | (volumen) | `audio_ganancia_pct` siempre a 100, el original |
  | Sonido por AAudio (nuevo) | `nfsmw_audio_aaudio`: nuestro driver AAudio, o el de SDL de su SDK |
  | Fijar hilos a núcleos | `thread_affinity=auto`, nuestro `afinidad.cpp`: el hilo del anillo ("GPU anillo nativo") y el principal del juego a los núcleos prime |
  | Driver de GPU y turbo | los mismos `android_gpu_*`, con `parche_turnip.py` en su SDK |
  | Gamertag | `user_gamertag`, con el mismo parche que en nuestro SDK |
  | Modo de compatibilidad (nuevo, Avanzado) | `nfsmw_renderizador=xenos` y los ajustes de su modo de compatibilidad: su emulación de la GPU, para GPUs que no pueden con el renderizador |

  Los valores por defecto son los de su `nfsmw.toml`, que viaja en el APK y guarda lo que
  la pantalla de inicio no enseña.
- **No se enseñan** los ajustes que son solo de la GPU imitada de nuestro SDK (una pasada,
  EDRAM en shader, oclusión, exposición fiel, memexport, páginas, posprocesado, escala).
- **Afinidad.** Su `android_rendimiento.cpp` (hilos fuera de los núcleos lentos) no entra:
  en el 8 Elite no hace nada, porque no tiene núcleos lentos. `nativo_android.cpp` define
  la función que su app llama en ese momento y arranca ahí nuestro vigilante.
- **Sonda de Vulkan.** Su `SDL_main` es de su SDK; `parche_nativo.py` le hace preguntar
  primero a `NfsmwSondaSiSePide`, de `nativo_android.cpp`.
- **Segundo plano.** Al minimizar, como con Xenos: el sonido se para y el juego se queda
  quieto. Su SDK no atendía los eventos de segundo plano; se le aplica nuestro
  `parche_pausa.py`, que suelta la superficie antes de que Android la quite. Y como su
  renderizador no espera a nadie para presentar, el juego seguía corriendo. Al irse:
  se pausa el sistema de audio del SDK (su hilo de audio y el XMA); el hilo del anillo se
  para en el siguiente cambio de fotograma (`parche_nativo.py` le pone un gancho antes de
  presentar) y el juego se queda esperando sitio en el anillo; y se para el reloj del
  rescate de su servidor de audio, que si no tomaba la pausa por un atasco y liberaba
  paquetes que la voz aún tenía (sonido corrupto al volver). Suspender los hilos del juego
  a la fuerza se probó y se quitó: al volver, el juego se quedaba parado de 5 a 17 s.
- **FSR 1.0 y CAS.** Su renderizador entrega cada fotograma al presentador del SDK
  (`RefreshGuestOutput`), y es el presentador quien lo escala a la pantalla. Ese
  presentador, el de Xenia, ya trae AMD FSR 1.0 y CAS con sus shaders precompilados
  (`src/ui/shaders/vulkan_spirv/guest_output_ffx_*`), pero el SDK solo los activa
  (`REX_HAS_FIDELITYFX_SDK`) si se descarga el FidelityFX SDK de AMD, que es para
  escritorio y solo hace falta para FSR 2/3. Sin él, el código ya cae en FSR 1.0 y CAS.
  `nativo.cmake` y `CMakeLists.txt` lo definen en todo el árbol menos el código generado
  del juego: cambia la forma de las clases del presentador, y así no se recompilan sus
  265 ficheros. Vale para los dos motores.
- **Orientación.** La partida va siempre apaisada (`setOrientationBis`: SDL pedía la
  orientación según el tamaño de su ventana); la pantalla de inicio gira libre.
- **Fps sin límite.** Su motor no limita los fps por su cuenta: su hilo de vblank dispara
  la interrupción del juego `nfsmw_limite_fps` veces por segundo, y el juego espera a un
  vblank para presentar. Así que el límite es el ritmo del vblank, y además redondea hacia
  arriba: a 120 Hz, un fotograma de 14 ms espera a 16,7. `sin_limite` (`parche_nativo.py`)
  pone el vblank a 240 Hz, el máximo que admite el SDK (`video_mode_refresh_rate` va de 24
  a 240): el juego espera como mucho ~4 ms y va tan rápido como dé el móvil. Es
  experimental, como 90 y 120: no se ha medido en el móvil.
- **Audio.** Su salida es su driver de SDL (una "bomba" que pide audio cada 5,33 ms con 64
  ms de reserva). Por defecto se cambia por **nuestro driver AAudio**
  (`audio/aaudio_driver.cpp`), el que arregló los cortes con el motor de Xenos: su
  `OnPreSetup` llama a `NfsmwAndroidAudio` (`parche_nativo.py`), que pone esa factoría si
  el cvar `nfsmw_audio_aaudio` está a `true` (Sonido → "Sonido por AAudio"). Compilado con
  `NFSMW_MOTOR_NATIVO`, el driver hace además lo que hacía el de SDL de su SDK: el volumen
  (`audio_ganancia_pct`, siempre al 100 %), su limitador sobre el pliegue sin recortar, callar el juego
  mientras suena la pista propia de una película (`IsGameOutputSuppressed`), y dejar
  iniciado el audio de SDL, por el que va esa pista.
- **Mando táctil.** El mismo, pero el estado va directo al mando del juego
  (`rex_sdl_set_touch_gamepad_state` de su SDK) en vez de por un mando virtual de SDL.
- **Vibración.** Su SDK la tiene apagada por defecto (`input_vibracion = false` en
  `sdk/src/input/input_system.cpp`: `SetState` cambia lo que pide el juego por vibración
  cero), así que la app pasa `--input_vibracion=` con el ajuste Vibración (encendido por
  defecto). Los mandos físicos vibran por SDL (`SDL_RumbleGamepad`), si Android expone sus
  motores (`InputDevice.getVibratorManager`, la clase `VIBRATOR` de `dumpsys input`).
  Por USB, el kernel del RedMagic carga los mandos de Xbox con su xpad sin force feedback
  y salen sin motores; por Bluetooth (`uhid`) sí los tienen. Por eso, con la vibración
  puesta, `NfsmwSondaSiSePide` (lo primero del `main` de su SDK, antes de que SDL abra los
  mandos) enciende `SDL_HINT_JOYSTICK_HIDAPI_XBOX`, que en Android viene apagado: el
  driver HIDAPI de SDL abre los mandos de Xbox 360 y One/Series por USB
  (`HIDDeviceManager`, pidiendo permiso de USB), se los quita al kernel y les manda la
  vibración él mismo. Mientras, Android no los ve como `InputDevice`, así que
  `TouchControllerView` también cuenta como mando físico los de Xbox que hay en
  `UsbManager` (la misma interfaz que reconoce SDL) y escucha los avisos de USB. **Probar
  vibración** (pantalla de inicio) hace vibrar cada mando con su vibrador de Android y
  dice cuántos motores tiene; de un Xbox por USB avisa de que en la partida lo maneja SDL.
  El mando táctil no tiene motores: `parche_nativo.py` hace que su driver de SDL
  (`SetDeviceVibration`) llame a `NfsmwAndroidVibrar` de `nativo_android.cpp` (débil: si no
  está, el mando táctil dice que no vibra), que pasa la fuerza mayor de los dos motores a
  16 niveles y, solo cuando cambia, llama a `GameActivity.vibrarMando`, que deja el
  vibrador del móvil con esa amplitud hasta el siguiente cambio. Se calla al minimizar,
  con el editor del mando abierto y en el modo mando de `TouchControllerView` (un mando
  físico, USB o Bluetooth, conectado o usado en la partida; el mismo que esconde los
  controles táctiles): con mando físico vibra solo él. Los motivos se suman
  (`GameActivity.callarVibracion`, `CALLADA_*`), como los de la pausa.
- **Girar inclinando el móvil.** Opción de la pantalla de inicio (Controles táctiles). En la
  disposición de conducir, `TouchControllerView` lee el sensor de gravedad
  (`TYPE_GRAVITY`; si no hay, el acelerómetro con un filtro de paso bajo) a `SENSOR_DELAY_GAME`
  mientras la partida se ve. Pasa el vector a la pantalla según su rotación (en horizontal, la X
  de lo que se ve es la Y del móvil, con el signo según el lado) y usa su componente lateral:
  `x / (g · sen(ángulo))` (signo comprobado en el móvil), con el ángulo de giro completo
  según la sensibilidad (deslizador de 1 a 20: de 45 a 6 grados, cada paso un ~10 % menos;
  `Ajustes.anguloGiro`) y una zona muerta del 3 %. Como solo mira la componente lateral, vale con el móvil vertical o
  recostado. Ese valor es el eje X del stick izquierdo, que no se ve. En aceleración siguen las
  flechas de carril; en los menús y editando, nada.
- **Editar el mando en la partida.** Un engranaje junto a OCULTAR/TÁCTIL abre el editor de
  la pantalla de inicio sobre el juego. El juego se para con la misma pausa que al
  minimizar (`TouchControllerBridge.pausarJuego`): la pausa guarda sus motivos (segundo
  plano, editor) y no se reanuda hasta que no queda ninguno, así que cerrar el editor con la
  app minimizada no pone el juego en marcha. CANCELAR rehace los controles desde lo
  guardado; LISTO guarda con `commit()`, y la pantalla de inicio lo vuelve a leer
  (`MODE_MULTI_PROCESS`: el juego va en el proceso `:juego`).

  Hay tres disposiciones, cada una con la suya de fábrica: la normal (menús y cargas), la de
  conducir (el resto de carreras, la conducción libre y las persecuciones: lo de aceleración
  pero con el stick izquierdo para girar y la cruceta, el stick en el reflejo de los pedales y
  la cruceta en el de los botones en rombo) y la de las carreras de aceleración. El editor
  elige cuál se edita con su botón CONTROLES; desde la partida abre la de la parte del juego
  en que se esté. La de fábrica de aceleración sale de una hecha a
  mano en el móvil (2688×1216), hecha simétrica: abajo, sobre la misma línea, los pedales a
  la derecha y las flechas de carril a la izquierda, cada flecha en el reflejo de su pedal
  respecto al centro de la pantalla, con la palanca junto a ellas; los botones en rombo a la
  derecha; arriba a la izquierda, en rejilla, la clasificación y la cámara en una fila y el
  retrovisor bajo la cámara; y BACK y START simétricos a los lados de OCULTAR/TÁCTIL y el
  engranaje (`buildControls`, en fracciones de la pantalla y tamaños en `unit`). Lo que se mueve se guarda aparte (`a_<control>_x`, `_y`, `_s`); RESTAURAR vuelve a
  la de fábrica de la que se edita. Editándola se ven los pedales, la palanca y los iconos,
  sin L3 ni R3.
- **Controles según la parte del juego.** Cada 250 ms, mientras la partida se ve,
  `TouchControllerView` pregunta al juego dónde está (`TouchControllerBridge.contexto`,
  `nativo_android.cpp`). En las carreras de aceleración los mismos controles, en el mismo
  sitio (también el que se haya movido con el editor), cambian de forma: RT es el
  acelerador y LT el freno (pedales, que tocan en todo lo que ocupan), el stick derecho una
  palanca de cambios (arriba sube marcha y abajo la baja; va entero o al centro, porque el
  juego cambia al cruzar su umbral), RB lleva una cámara y LB un retrovisor, el stick
  izquierdo, que ahí solo cambia de carril, son dos botones cuadrados con flecha (pulsado uno,
  el eje X va entero a ese lado; los dos, al centro), la cruceta, que ahí solo abre la
  clasificación (arriba), un botón con el icono de la clasificación que pulsa arriba, y L3
  y R3 no están. Sin cruceta ni stick vertical, el menú de pausa de una carrera de
  aceleración no se puede recorrer arriba y abajo (pendiente: detectar la pausa). Al cambiar de contexto se suelta todo, para que ningún dedo se quede pulsando un
  control que ya no está. En los menús, conduciendo libre y en las demás carreras, los de
  siempre.

  Se lee de la memoria del juego, con lo que da su
  [descompilación](https://github.com/dbalatoni13/nfsmw) (versiones de GameCube, 360, PS2 y
  PC):

  | Qué | Dónde (PAL) |
  |---|---|
  | Estado del juego: 3 menús, 1-2 y 4-8 cargas, 6 en el mundo | `TheGameFlowManager.CurrentGameFlowState`, `0x82A39AD8` |
  | La carrera en curso | `GRaceStatus::fObj`, `0x82A2CB18` → `+0x1A30` `mRaceParms` |
  | Su tipo (`GRace::Type`: 0 sprint, 1 circuito, **2 aceleración**, 3 eliminación...), si está en la base de datos | `mRaceParms` → `+4` `mIndex` → `+0x2B` |
  | Si no (carreras rápidas), su atributo `racetype` (clave `0x0F6BCDE1`): un texto, `"drag"` | `mRaceParms` → `+8` → `+4`, la colección de atributos de la carrera |
  | La tabla con la que el juego pasa ese texto a tipo (`"circuit"` 1, `"p2p"` 0, `"drag"` 2...) | `0x8290D828`, once `{nombre, tipo}` |
  | En pausa (menú de pausa, mensajes...): cuántas peticiones hay | `FEManager::mPauseRequest`, `0x82A2C5CC` (`RequestPauseSimulation`, `sub_82285B80`, hace `mPauseReason[mPauseRequest++] = motivo`) |

  Las carreras rápidas son una copia de la carrera original (`GRaceCustom`) **sin**
  `mIndex`: el tipo hay que sacarlo como lo saca `GRaceParameters::GetRaceType` cuando le
  falta, del atributo. La primera versión solo miraba `mIndex`, y en una carrera rápida de
  aceleración los controles no cambiaron; el diagnóstico lo dejó claro (`idx=00000000`).
  El atributo se busca como su `Attrib`: en la tabla de la colección, luego en la de su
  padre (`+0x10`, la carrera original) y, si no, en su parte fija (`+0x18`) con el sitio que
  da la definición de su clase (`+0x14` → `+8` → `+0xC`). Las tablas tienen nodos de 12
  bytes (clave, dato, banderas en `+11`) y se recorren nodo a nodo, en vez de copiar su
  hash; con claves únicas da lo mismo. Se calcula una vez por carrera. Medido en el móvil
  (USA, dos carreras rápidas de aceleración): `tipo=2 (atributo "drag")`, los controles
  cambian al empezar la carrera (estado 6) y vuelven al descargarla (estados 7 y 8).

  Los desplazamientos son los del 360, no los de la descompilación (de GameCube: allí
  `mRaceParms` va en `+0x1AAC`). Salen de las funciones que los usan:
  `GRaceStatus::GetRaceType` (`sub_820E5E28`), `GRaceParameters::GetRaceType`
  (`sub_8233A000`) y las de `Attrib` que buscan un atributo (`sub_821485E8`) y su valor
  (`sub_82145C50`). La app comprueba 17 de sus instrucciones en memoria antes de fiarse de
  ellos, y que la tabla de nombres tenga `"drag"` = 2. Las siete direcciones van en un array
  de su app (`parche_nativo.py`), para que `crear_arbol.py` las lleve a la USA y a la
  japonesa (allí `fObj` se mueve +0x5A0 y la tabla de nombres +0x190) y su comprobación
  compare esas funciones. En pausa no se gira inclinando el móvil (movería las opciones del
  menú) y la disposición es la de conducir, que tiene la cruceta; también al pausar una
  carrera de aceleración, cuya disposición no la tiene. La dirección del contador de pausas no
  se comprueba con una palabra fija, porque en la japonesa `.data` se mueve: se rehace del `lis`
  + `lwz` de `RequestPauseSimulation` y tiene que coincidir con la traducida. Cada lectura mira
  antes que la página sea legible: Java pregunta
  mientras el juego cambia de escena. Cada cambio queda en el log (`[contexto]
  aceleracion`) y, como el APK release no escribe log, en una línea de
  `files/logs/contexto.txt` con todo lo leído. Con el motor de Xenos no hay contexto y los
  controles son siempre los de siempre.
- **Fps.** El rótulo cuenta los fotogramas que presenta el juego
  (`g_nfsmw_fotogramas_juego`).

## GPU que no son Adreno (Mali, PowerVR, Xclipse)

Desde la 0.4.0 el árbol de nfsmw-android está en el commit `6df1501`, que adapta el
renderizador a lo que falte en la GPU, solo cuando falta:

| Si la GPU... | Qué hace |
|---|---|
| no tiene texturas BC1-5 (Mali, PowerVR) | las convierte en la CPU a RGBA8, R8 o RG8 al subirlas (más memoria y más carga al entrar en una zona) |
| no tiene `shaderInt64` ni direcciones de buffer | nada: la biblioteca de shaders de Android lee las constantes siempre por UBO |
| tiene Vulkan 1.1 | pasa el SPIR-V 1.5 de la biblioteca a 1.3 al crear cada shader; el SDK activa `VK_EXT_descriptor_indexing` |
| es un Mali | no usa las consultas de oclusión (colgaban su driver): sin destello del sol |

La biblioteca de shaders se genera con el `shader_common.h` de su app de Android
(`android/app/src/main/assets/shaders`), "sin punteros", el mismo que copia
`preparar_nativo.py` al instalador. Para la PAL sale con el SHA-256 que esperan ellos
(`b84602ca…`).

Lo que añade `parche_nativo.py` encima:

- **4 conjuntos de descriptores** (sección 8). Los shaders usan 5 (montones 2D, 3D, cubos y
  samplers, y los UBO) y Vulkan solo garantiza 4. Los Mali Valhall dan 4 y su driver se cae
  dentro de `vkCreatePipelineLayout`. Con menos de 5, el montón de cubos va en el conjunto del
  3D como enlace 1, samplers y UBO bajan uno, y `CrearModulo` cambia las decoraciones de cada
  shader (`JuntarConjuntos`). `nfsmw_nativo_cuatro_conjuntos` lo fuerza en cualquier GPU.
- **La cola de órdenes con barreras** (sección 9). No es de la GPU sino de la CPU ARM: el
  cierre "Call to invalid or unregistered function at guest address 0x00000000" al empezar
  carrera en móviles lentos.
- **Xclipse: BC4 y BC5 en la CPU** (sección 10). Su driver dice tener todas las BC, pero
  BC4-7 las convierte él mismo, a medias. Sin probar en un móvil con Xclipse.

Probado en un Samsung A22 5G (Mali-G57 MC2, driver r32p1, Vulkan 1.1, 4 GB de RAM): menús a
40-60 fps, carreras a 20-22 fps a 1024x576, limitado por la GPU (55-75 ms por fotograma a
1280x720). Quedan fallos que solo salen con el driver de Mali: el retrovisor negro, una franja
negra en el horizonte de la pista del bosque y el vídeo de demostración del menú duplicado. En
el RedMagic con los mismos caminos forzados (`nfsmw_nativo_texturas_bc_cpu`,
`nfsmw_nativo_simular_vulkan11`, `nfsmw_nativo_cuatro_conjuntos`,
`nfsmw_consultas_oclusion=off`, a 1280x720) se ve bien.

**Mali nuevos (Valhall tardíos y 5.ª generación: G710, G715/Immortalis-G715, G720, G725, G925).**
No hay ninguno entre los móviles de prueba, así que nada de lo de arriba está comprobado en ellos.
Lo que sí hace el código para no depender de adivinar: los caminos de compatibilidad (BC en la CPU,
4 conjuntos de descriptores, SPIR-V 1.3) se activan por lo que **dice el driver**
(`optimalTilingFeatures`, `maxBoundDescriptorSets`, versión de Vulkan), no por el nombre de la GPU,
así que un Mali nuevo que sí tenga 5 conjuntos o Vulkan 1.3 usa el camino normal. La única decisión
por marca es la de las consultas de oclusión, que se desactivan en todo Mali (vendor `0x13B5`) y
pierden el destello del sol; en los nuevos puede que no haga falta. La sonda (pantalla de inicio)
detecta el Mali por vendor, driver o nombre, y escribe en su informe la generación, la versión del
driver y `maxBoundDescriptorSets`. **Para validar un Mali nuevo**: pasar la sonda, mirar que diga
"MOTOR NATIVO: sirve" y copiar ese informe al informar de un problema.

**PanVK** (Mesa para Mali sobre kbase, la compilación de FristOneRR para el Mali-G57) se probó
como driver propio y no sirve, por ahora: en Android 13 necesita una libdrm más nueva que la
del sistema (cargada con otro nombre), no puede crear la cadena de presentación con el gralloc
de Samsung (`VK_ERROR_INVALID_EXTERNAL_HANDLE`: pantalla negra), sus BC salen negras, y aun con
las BC en la CPU la mayoría de superficies salen negras. El retrovisor, en cambio, sí se ve.
Hipótesis sin comprobar: PanVK podría funcionar solo en Android 16 o posterior (API 36), donde
los fallos de libdrm y del gralloc podrían no darse. La sonda anota ahora la versión de Android
en su informe para compararlo.

## Qué falta

- **Probar lo portado**: Turnip, el turbo, la sonda, la afinidad, el audio por AAudio
  (sobre todo las películas: que no suenen dobles ni mudas) y cada opción gráfica.
  Compilan y están en el APK; no se han visto funcionar en el móvil.
- **Ediciones.** PAL España, USA y japonesa (USA y japonesa compiladas, sin verlas
  arrancar todavía). Las europeas (alemana, francesa, italiana) son la misma compilación
  que la española y deberían ir por `app/` tal cual, pero no se han probado; la PAL inglesa
  necesita sus parejas (nfsmw-nx las tiene en `tools/editions/`) y sus huellas de shaders
  en `ediciones.json`.
