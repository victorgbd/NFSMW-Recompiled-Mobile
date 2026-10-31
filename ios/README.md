# NFSMW Recompiled para iOS

**Estado** (2026-10-23, iPhone 13 Pro con iOS 26.6.2 y Apple ID gratuita):

- **El juego corre en el iPhone** con el renderizador nativo sobre MoltenVK: vídeos, menús, carreras,
  sonido y mando por Bluetooth (fase 1).
- **Los controles táctiles de la app de Android** funcionan: menús con el dedo, carreras y pausa
  (fase 2).
- **Escritos y compilados, sin probar todavía en el iPhone**:
  - el lanzador;
  - la ISO leída sin extraerla;
  - los 10 idiomas;
  - la vibración del móvil;
  - el sonido de baja latencia con `AVAudioEngine`;
  - el contador de fps;
  - el multijugador LAN, incluido el iPhone de anfitrión;
  - los arreglos de los gráficos con MoltenVK (los cuadrados de colores y los destellos).

  El servidor del anfitrión sí está probado sin teléfono, contra el de Python (ver *Pruebas*).

El port reutiliza el motor nativo de Android tal cual (`../nativo/`: el SDK, el código del juego
recompilado y el renderizador). Lo que cambia en el SDK y en la app va con `#if` de iOS
(`REX_PLATFORM_IOS`), y Android no cambia. Lo propio de iOS está en [motor/](motor/), en
Objective-C++ y C++: no hace falta Swift ni Xcode.

## Por qué es posible

- **No hace falta JIT.** El juego está recompilado a C++ antes de compilar, así que en iOS es una app
  normal. Es lo que impide a la mayoría de emuladores.
- **El SDK ya conoce Apple.** `REX_PLATFORM_MAC` (en iOS también vale, porque `TARGET_OS_MAC` es 1),
  la capa POSIX, la memoria con `shm_open` (`nativo/sdk/src/core/memory_posix.cpp`) y MoltenVK
  (`nativo/sdk/src/ui/vulkan/vulkan_moltenvk.cpp`) para Vulkan sobre Metal.
- **SDL3 funciona en iOS** (ventana, mandos y audio).

## Lo que no se puede

- **El backend GLES no sirve.** iOS se quedó en OpenGL ES 3.0 y el nuestro pide 3.1/3.2: en iOS el
  renderizador es el de Vulkan, a través de MoltenVK.
- **No puede ir a la App Store**, porque lleva código del juego. Cada persona compila e instala la
  suya, con su ISO.
- **Con Apple ID gratuita**, la app caduca a los 7 días y hay que volver a instalarla. Los permisos
  de memoria (`increased-memory-limit`, `extended-virtual-addressing`) firman pero no se aplican:
  iOS deja unos 3 GB y la reserva del juego cabe sin ellos (ver la fase 0). Sin el espacio de
  direcciones ampliado, una ISO de 7-8 GB no cabe mapeada: en iOS se lee con `pread`
  (`nativo/sdk/src/filesystem/devices/disc_image_device.cpp`).
- **Volver al lanzador es cerrar la app** y abrirla otra vez: el motor no se puede apagar y
  arrancar de nuevo en el mismo proceso. El botón «Salir» del menú principal (o BACK) la cierra
  (`_exit`, que Apple no admitiría en la App Store), o se cierra desde el selector de apps.
- **Buscar servidores por broadcast** pide un permiso de Apple (`multicast`) que una Apple ID
  gratuita no tiene: la búsqueda pregunta a cada IP de la red, como ya hacía Android cuando el router
  se comía los broadcast.
- **De anfitrión, el iPhone sirve mientras la app está delante.** En segundo plano iOS la suspende y
  los demás pierden la conexión.

## Herramientas: xtool en WSL

Se compila y se firma desde Windows con [xtool](https://github.com/xtool-org/xtool), sin Mac. xtool
corre en WSL, no en Windows directamente. Hay que instalar, una vez:

1. **WSL** (PowerShell como administrador): `wsl --install`, reiniciar, y crear el usuario de Ubuntu.
2. **El iPhone dentro de WSL**: [usbipd-win](https://learn.microsoft.com/windows/wsl/connect-usb)
   (`winget install usbipd`, y luego `usbipd bind` y `usbipd attach --wsl` con el iPhone).
3. En Ubuntu: **Swift 6.4** (de swift.org/install/linux) y `sudo apt-get install usbmuxd
   libimobiledevice-utils`.
4. **El `Xcode.xip`** (Xcode 27), bajado con el navegador desde
   https://developer.apple.com/download/all/?q=Xcode con tu Apple ID. Con `curl` no vale.
5. **xtool**: el AppImage de sus releases en `/usr/local/bin/xtool`, y `xtool setup`. Con la Apple ID
   gratuita se elige el modo **1: Password**, que usa APIs privadas de Apple. Su guía recomienda una
   Apple ID de usar y tirar. Después se le da la ruta del `Xcode.xip`.

Para comprobarlo: `swift sdk list` tiene que decir `darwin`, e `ideviceinfo` tiene que ver el iPhone.

### Instalar se cuelga en `Connecting`

Por usbip, `xtool dev` firma pero se queda en `Connecting`, y después todo da `Mux error (-8)`. El
log (`sudo journalctl -u usbmuxd`) dice `message was too large` o `expected 49152 bytes, received
58008`: usbip pierde el paquete vacío (ZLP) que cierra una transferencia de tamaño múltiplo de 512
bytes, y el iPhone junta dos mensajes. No lo arregla actualizar usbmuxd; hay que compilarlo de
[git](https://github.com/libimobiledevice/usbmuxd) con un parche en `device_client_process`
(`src/device.c`), que manda en dos segmentos TCP (n−1 y 1 byte) los datos cuyo mensaje mux daría
múltiplo de 512:

```c
int mux_header_size = ((conn->dev->version < 2) ? 8 : sizeof(struct mux_header));
int first = size;
if(size > 1 && (mux_header_size + (int)sizeof(struct tcphdr) + size) % USB_PACKET_SIZE == 0)
	first = size - 1;
res = send_tcp(conn, TH_ACK, conn->ob_buf, first);
/* ... y si first < size, otro send_tcp con el último byte; tx_seq avanza cada vez */
```

Partir la transferencia USB en vez del mensaje no vale: el iPhone exige un mensaje entero por
transferencia. Si el iPhone se cuelga, usbipd lo suelta (`Shared`) y hay que volver a hacer
`usbipd attach --wsl`. Un `apt upgrade` de usbmuxd quita el parche.

## Compilar e instalar

En WSL, desde la copia de Windows:

```sh
bash /mnt/c/.../NFSMW-Recompiled/ios/motor/compilar.sh [--dependencias] [--limpio] [--instalar]
```

- Copia las fuentes a `~/nfsmw`, porque compilar en `/mnt/c` es varias veces más lento.
- Trae las dependencias del SDK y el código generado del checkout principal
  (`NFSMW_PRINCIPAL`, por defecto `../NFSMW-Recompiled`: el mismo checkout, si se compila desde
  él). Lo hace la primera vez y con `--dependencias`.
- Si falta, baja MoltenVK de su release de Khronos.
- Compila con el clang de Swift y el kit de iOS que instala xtool.
- Deja `~/nfsmw/build-ios/NFSMW.ipa`. Con `--instalar`, la firma y la instala con xtool. El iPhone
  tiene que estar conectado a WSL (`usbipd attach --wsl --busid <id>`).

Dentro de la app van, del checkout principal:

- `textos.json`: los textos de Android en sus 10 idiomas y los propios de iOS, los de
  [motor/textos](motor/textos/), que junta [motor/textos.py](motor/textos.py);
- `edicion.json`: la edición del código generado (`nativo/out`);
- `nfsmw_shaders.nfsp`: su biblioteca de shaders;
- `nfsmw.toml`: los ajustes del motor que lleva el APK de Android
  (`nativo/app/nfsmw_movil.toml`). Al pulsar Jugar, el lanzador lo copia a
  `Documents`, como `MotorNativo.instalarAjustes` en Android: si falta, o si la app trae otro y el de
  `Documents` no se ha cambiado.

Como el código generado, salen de tu copia del juego: la app no se comparte.

## Gráficos con MoltenVK

El renderizador es el de Android, y en el iPhone tiene que dibujar igual. Tres diferencias
salieron como cuadrados de colores y destellos blancos en la mitad de abajo:

- **Los ajustes del motor.** Sin el `nfsmw.toml` de Android, el motor se quedaba con los valores del
  código, que para varios son los de PC. El principal: la escena en **tres franjas de 1280x256 con
  MSAA 4x**, el modo de la Xbox 360, que en Android no se usa nunca. Ahora va el toml, y además el
  lanzador pasa `--nfsmw_render_sin_mosaico=true`, que manda aunque el toml de `Documents` sea uno
  guardado desde el menú del juego.
- **Los abanicos de triángulos.** Metal no los tiene. Por cada uno, MoltenVK cierra el pase de Metal
  (guarda todos los adjuntos), convierte los índices con un compute y lo abre otra vez (los carga).
  Son abanicos el cuadrado de la consulta de oclusión del sol y los halos de las luces. Con
  MoltenVK, el renderizador los dibuja como listas (`nfsmw_nativo_abanicos_a_listas`, que en
  Android sirve para probarlo).
- **La matemática rápida de Metal.** MoltenVK compila con ella los shaders que no piden conservar
  NaN e infinitos, y la biblioteca nativa no lo pide: el compilador puede quitar un `isnan()` o
  aproximar divisiones, senos y cosenos. Va apagada (`moltenvk_matematica_rapida`, por
  `VK_EXT_layer_settings`), como las cuentas en Android. Se enciende en *Avanzado* para comparar.

El registro dice qué se usó: `[compatibilidad] abanicos de triangulos: ...` y
`Vulkan: matematica rapida de Metal ...`.

## El juego en el iPhone

La app sale en Archivos como **NFSMW**, con su carpeta `Documents`. El lanzador busca el
juego en este orden:

1. **La ISO elegida con «Elegir ISO…»**, en cualquier sitio de Archivos. Se lee donde está, sin
   copiarla, con un marcador de seguridad.
2. **El juego extraído** en `Documents/game_root` (`default.xex`, `NFS/`, `Movies/`).
3. **La primera `.iso` de `Documents`**, copiada con Archivos, con el Finder o con
   `afcclient --container <id> put`.

El lanzador mira de qué edición es (el SHA-256 de su `default.xex`) y si le falta el final, como
`IsoXex` en Android.

Lo que escribe la app, en `Documents`:

- `logs/`: `nfsmw_NNN.log`, `contexto.txt`, `menus.txt` y `servidor_lan.log`;
- `datos/`: partidas, caché y el `stats.json` del anfitrión;
- `nfsmw.toml`.

Se sacan por USB con `afcclient --container XTL-<equipo>.io.github.victorgbd.nfsmw` y
`get Documents/logs/...`. El registro del sistema se ve con `idevicesyslog -p nfsmw`.

## Pruebas

[motor/pruebas](motor/pruebas/) prueba sin teléfono la red del lanzador y el servidor del
anfitrión, compilados para Linux (en WSL):

- **`prueba_servidor_ea.py`** pasa una carrera LAN de dos jugadores por el servidor de Python
  (`ea_server.py` de nfsmw-server-lan) y por el de iOS, y compara cada mensaje. Los campos son los
  del juego, sacados del `logs/server.log` de una partida real. El 2026-10-23 salen iguales los 123.
- **`prueba_red.cpp`** prueba la parte cliente contra ese servidor: direcciones, gamertag y
  búsqueda.

Cómo compilarlas está en la cabecera de `prueba_servidor_ea.py`.

## Fases

0. **La prueba de memoria**: una app sin nada del juego que midió en el iPhone lo que el SDK
   necesita. Ya no está en el repositorio; queda en el historial, en `ios/PruebaMemoria` del commit
   `be92e6a` (se compilaba con `xtool dev` en esa carpeta). Medía:
   - una zona compartida de 4,5 GB (`shm_open` o un fichero);
   - la reserva de 4,5 GB de espacio de direcciones, con las vistas en espejo de `MapViewsMac` en
     `nativo/sdk/src/system/xmemory.cpp` (en iOS entra por esa rama);
   - los fallos de página capturados del `mmio_handler`;
   - cuánta memoria deja iOS antes de cerrar la app.

   **Resultado** (iPhone 13 Pro, 6 GB, páginas de 16 KB, Apple ID gratuita): la zona compartida y
   la reserva de 4,5 GB caben, el espejo funciona y los fallos de página se capturan. iOS deja
   unos 3063 MB. Para la fase 1:
   - `shm_open` da `EPERM`: la zona compartida tiene que ser un fichero en tmp.
   - Una reserva de 8,5 GB (la de Linux) no cabe; la de 4,5 GB, sí.
   - Páginas de 16 KB: revisar la vista de `0xE0000000`, que apunta a `0x100001000`.
1. **El motor nativo compilado para iOS** ([motor/](motor/)). **Hecho y probado.** Va todo en un
   ejecutable estático, con CMake y [ios.toolchain.cmake](motor/ios.toolchain.cmake), y MoltenVK
   enlazado dentro.
   - Solo el renderizador nativo: sin la emulación de Xenos.
   - SDL3 con su escena de UIKit.
   - La zona compartida de memoria es un fichero en `tmp`, porque `shm_open` da `EPERM`.
   - El registro también va a `os_log`.
2. **Controles táctiles y lanzador.**
   - **Controles táctiles: hechos y probados.** Son `TouchControllerView` en UIKit
     ([controles_tactiles.mm](motor/controles_tactiles.mm)), con su editor en la partida. Lo
     nativo es el mismo `nativo_android.cpp` de Android, compilado sin JNI.
   - **Lanzador: escrito, sin probar.** Es `SetupActivity` en UIKit
     ([lanzador.mm](motor/lanzador.mm)). El `SDL_main` del SDK lo enseña antes del juego, en el
     mismo proceso, y le pasa la línea de comandos. Tiene las secciones de Android menos lo que
     iOS no tiene: resolución de pantalla, API gráfica, AAudio, hilos, driver de GPU, sonda y modo
     de compatibilidad.
3. **El resto: escrito, sin probar en el iPhone.**
   - Los 10 idiomas de la app, también en los controles táctiles.
   - Los preajustes gráficos.
   - La vibración del móvil con la del juego (Core Haptics) y el contador de fps.
   - El sonido de baja latencia ([audio_ios.mm](motor/audio_ios.mm)): el driver AAudio de Android con
     `AVAudioEngine`, en lugar del de SDL, que en iOS va por `AudioQueue`. El búfer de E/S es de un
     bloque del juego, 5,33 ms. Las películas siguen por SDL, sobre un dispositivo suyo que queda
     abierto todo el juego para que no toque la sesión de audio. Cada 10 s el registro dice cuántos
     fotogramas se quedaron sin datos (`[audio ios] en 10 s`).
   - El multijugador LAN: los servidores guardados, buscar en la red y comprobar el gamertag
     ([red_lan.cpp](motor/red_lan.cpp)).
   - El iPhone de anfitrión: `ServidorEa` en C++ ([servidor_ea.cpp](motor/servidor_ea.cpp)),
     probado contra el de Python.
   - Los arreglos de los gráficos con MoltenVK (ver *Gráficos con MoltenVK*).
