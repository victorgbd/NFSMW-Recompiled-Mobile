# Changelog

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/).

## [Sin publicar]

Lo de este fork ([victorgbd/NFSMW-Recompiled](https://github.com/victorgbd/NFSMW-Recompiled))
sobre upstream: el port a Android y el trabajo de rendimiento del camino de Xenos. Es un
proyecto vibe-codeado: ver la sección "This fork" del [README](README.md).

### Añadido

- **Botón «Salir» en el menú principal** (0.5.6, motor nativo): el 360 no tiene cómo salir del
  juego. En el menú principal (`MainMenu.fng`, sin diálogos ni submenús encima) sale una
  indicación más, «◄ Salir», con el estilo de las del juego (barra oscura rayada, círculo con la
  flecha del botón BACK y texto crema en Michroma, fuente OFL parecida a la del juego, licencia
  en `res/raw`). Va a la derecha del recuadro del perfil, encima de «Estadísticas», medida desde
  el icono de A, que no se mueve con el idioma. Un toque pide «¿Seguro?» durante 3 s y otro
  cierra la partida y vuelve al lanzador. Con mando físico, BACK hace lo mismo: el SDK cuenta sus
  pulsaciones (`rex_sdl_back_pulsados`). Se ve siempre, también con el mando táctil encima del
  menú (sin «menús con el dedo»), pero entonces no se toca: lo hace su BACK, como el del mando
  físico. Textos en los 10 idiomas. En iOS el mismo botón (`controles_tactiles.mm`, la fuente la
  mete `compilar.sh`) cierra la app, porque allí no se puede volver al lanzador sin reiniciar el
  proceso; al abrirla otra vez sale el lanzador. Sin ver en el móvil todavía, ni compilado en iOS.
- **Exportar e importar las partidas** (sección Juego del lanzador): el motor nativo guarda en el
  almacenamiento interno de la app (`files/nativo/usuario`), donde no llegan ni el explorador ni
  el PC. «Exportar» mete en un `.zip`, donde diga el selector de Android, el perfil, las partidas,
  los logros y las copias de `saves/`. Deja fuera `nfsmw.toml`, los `.nfsp` y las cachés.
  «Importar» pregunta antes y sustituye las partidas actuales por las de un `.zip` exportado por la
  app (lleva dentro `nfsmw_partidas.txt`). Antes deja las de ahora en
  `Android/data/io.github.nfsmwrecomp/files/partidas_antes_de_importar.zip`. Con el motor de Xenos
  usa su carpeta (`Android/data/.../files/datos`). Textos en los 10 idiomas. Sin probar en el móvil.
- **Acceso a todos los archivos, para elegir la ISO** (y las partidas): en algunos móviles el
  selector de Android no enseña el almacenamiento interno. «Elegir ISO» explica el permiso y lo
  pide: el de todos los archivos (`MANAGE_EXTERNAL_STORAGE`) en Android 11+, y los de
  almacenamiento de antes en 8-10. Con él, la app busca las `.iso` de más de 100 MB en el
  almacenamiento interno y en las SD (`BuscadorIsos`: hasta 6 carpetas de profundidad, sin
  `Android/`, con tope de 8 s). Solo enseña las compatibles: lee el SHA-256 del `default.xex` de
  cada una (`IsoXex`) y descarta las de otros juegos y de ediciones que el APK no lleva. En la
  lista salen con su edición, tamaño y carpeta, y marcadas si están incompletas. Si no hay
  ninguna, dice qué ediciones valen y cuántas `.iso` descartó. La elegida se
  guarda como ruta y el juego la abre directamente: el SDK ya montaba un fichero normal como
  imagen de disco. Sin el permiso, o desde la lista, sigue a mano el selector de Android. Con el
  permiso, las partidas se exportan a `NFSMW/partidas` del almacenamiento interno, y la copia de
  antes de importar también va ahí. Importar abre siempre el selector de Android. Sin probar en el móvil.
- **Preajuste «Mis ajustes»** (0.5.5, motor nativo): al aplicar «Ahorro de batería» o «Valores de
  fábrica», antes se guardan los ajustes gráficos del usuario, y el botón nuevo vuelve a ellos.
  Si el usuario no ha tocado nada desde el último preajuste, lo guardado no se pisa (aplicar
  «Ahorro» y luego «Fábrica» no pierde los suyos). Solo aparece cuando hay algo guardado; textos
  en los 10 idiomas.
- **«Pantalla» es una sección propia del menú lateral** (0.5.5), entre Juego y Gráficos, con su
  icono: resolución, estirar, panorámico y contador de fps. Gráficos se queda con los ajustes
  gráficos.
- Herramientas para depurar Vulkan en el móvil (0.5.5): `apk_ediciones.py --capas-vulkan <carpeta>`
  (o `-Pnfsmw.capas_vulkan`) mete la capa de validación de Khronos en el APK, que se enciende con
  `--vulkan_validation_enabled=true --vulkan_log_debug_messages=true` (y en el móvil
  `setprop debug.vvl.validate_sync true` para la de sincronización). Los APK con `--registro`,
  `--depurable` o `--capas-vulkan` salen como `nfsmw-all-<versión>-depuracion-release.apk` y no
  pisan el de publicar.
- **OpenGL ES: escalado FSR 1.0 y CAS** (0.5.4): el presentador de GLES hace los mismos efectos que
  el de Vulkan (bilineal, AMD CAS de nitidez o reescalado y FSR 1.0 con EASU y RCAS, con y sin
  tramado), encadenados en texturas intermedias `GL_RGB10_A2`. Antes pintaba siempre en bilineal
  aunque el launcher pidiera FSR o CAS. Los shaders son los de Vulkan
  (`sdk/src/ui/shaders/vulkan_spirv`) pasados a GLSL ES 3.10 una vez en el PC con SPIRV-Cross
  (`nativo/tools/gles_efectos_presentador.cpp`, que escribe `gles_presenter_efectos.h`), en dos
  variantes: a una textura, tal cual, y a la ventana, con la y de `gl_FragCoord` volteada. Si uno
  no compila, bilineal el resto de la sesión.
- **OpenGL ES: programas guardados en disco** (0.5.4, `nfsmw_nativo_gles_cache_programas`): cada
  programa enlazado se guarda con `glGetProgramBinary` en `cache/nfsmw_nativo_programas_gles.bin`
  y en las partidas siguientes se carga con `glProgramBinary`, sin convertir, compilar ni enlazar:
  los tirones de 20–110 ms de la primera vez que sale cada shader solo pasan en la primera
  partida. La clave es el SPIR-V de los dos shaders, la especialización y las variantes; otro
  driver empieza de cero.
- **OpenGL ES: el destello del sol con fundido** (0.5.4): cada consulta de oclusión lleva un
  contador atómico de muestras, y sus dibujos usan una variante del programa con la prueba de
  profundidad antes del shader y un `atomicCounterIncrement` al final: sale la cuenta de la 360 y
  no solo el sí o no de `GL_ANY_SAMPLES_PASSED`. Sin contadores atómicos en los pixel shaders, o si
  el driver da 0 con la consulta visible, vuelve al sí o no.
- **OpenGL ES: las optimizaciones de Vulkan que más CPU ahorran** (0.5.4), con sus mismas cvars:
  vértices sin duplicar en el mismo fotograma (`nfsmw_nativo_dedupe_vertices`), sombras sin
  vegetación (`nfsmw_sombras_sin_vegetacion`), dibujos invisibles saltados
  (`nfsmw_nativo_saltar_invisibles`) y Z temprana (`nfsmw_nativo_z_temprana`); y otras propias:
  cambio de orden de bytes de los vértices con NEON, índices de 16 bits con NEON directos al búfer
  de subida y vértice base con `glDrawElementsBaseVertex` (`nfsmw_nativo_gles_base_vertice`), sin
  volver a apuntar los atributos ni a enlazar los bloques uniformes si no cambian, y texturas por
  registro reutilizadas dentro del fotograma (`nfsmw_nativo_gles_memo_texturas`). Falta el cielo
  aplazado. Nada de la 0.5.4 se ha probado todavía en un móvil.
- **OpenGL ES: el reflejo de la carretera bajo demanda** (0.5.4): `nfsmw_reflejo_bajo_demanda` y
  `nfsmw_reflejo_visibilidad` necesitan que el renderizador avise de cuándo se resuelve el reflejo,
  cuándo se lee y cuándo hay Swap, y con GLES no se avisaba: el guardián no veía copias, se apagaba
  y el reflejo se dibujaba en todos los fotogramas (en Vulkan son ~210 dibujos por fotograma donde
  no se ve agua). Ahora `DestinosGles` avisa, y los dibujos miden si el agua deja muestras con una
  consulta `GL_ANY_SAMPLES_PASSED_CONSERVATIVE` propia, con el testigo de la composición final,
  como Vulkan.
- **OpenGL ES: el cielo aplazado** (0.5.4, `nfsmw_nativo_cielo_aplazado`): la cúpula del cielo se
  dibuja después de los opacos de su pase, con el criterio y el guardián de Vulkan (3,4–4,3 ms de
  GPU allí). Para poder aplazar un dibujo, el final de `Dibujar` se separó en el dibujo calculado
  (`DibujoGles`) y `Emitir`; C2 cierra el pase (`TerminarPase`) antes de cada copia, Swap y
  consulta del juego.
- **OpenGL ES: informe de fases** (0.5.4, `nfsmw_nativo_gles_etapas`, apagado): el tiempo de cada
  dibujo por fases y el de las copias y Swaps de C2, cada 600 fotogramas en el log.
- **BC1 a 16 bits sin BC en la GPU** (0.5.4, `nfsmw_nativo_bc1_16_bits`): en Mali y PowerVR las BC1
  convertidas en la CPU van a 5:6:5 (o 5:5:5:1 si usan su alfa de un bit) en vez de RGBA8, en GLES
  y en Vulkan: la mitad de memoria y de ancho de banda en las texturas más comunes.
- **Media precisión en los pixel shaders** (0.5.4, `nfsmw_nativo_media_precision`, apagado):
  `RelaxedPrecision` en la matemática de color del SPIR-V (`nfsmw_spirv_precision.cpp`), para
  Vulkan y GLES; coordenadas, profundidad y lo que no sale de una textura siguen a 32 bits.
  Comprobado con `spirv-val` y glslang en el PC; sin probar en un móvil.
- **Preajustes en el launcher** (0.5.4, motor nativo): «Ahorro de batería» (30 fps, 1024×576 con
  FSR, sin suavizado ni anisotrópico, sombras rápidas cada 2 fotogramas y de cerca, un reflejo del
  coche) y «Valores de fábrica» (los de una instalación nueva en ese móvil), con confirmación; los
  textos en los 10 idiomas.
- **OpenGL ES para el motor nativo** (0.5.3, experimental): ajuste nuevo **"API gráfica"**
  (Vulkan u OpenGL ES 3.2) para móviles sin Vulkan o con el driver de Vulkan roto. Es un
  segundo backend del renderizador nativo con las mismas interfaces que el de Vulkan, que no
  cambia (detalle en [docs/motor-nativo-gles.md](docs/motor-nativo-gles.md)):
  - shaders: una biblioteca aparte (`nfsmw_shaders_gles.nfsp`, una textura fija por registro en
    vez de los montones de descriptores) que SPIRV-Cross pasa a GLSL ES en el móvil;
  - presentador EGL con un buzón de tres texturas `GL_RGB10_A2` y sincronización con `glWaitSync`
    entre el contexto del anillo y el de pintar;
  - destinos, copias, resueltas y lecturas a la memoria del guest (C2), y los dibujos (C3–C6):
    estado fijo, búfer de subida persistente de 64 MB, texturas con las BC descomprimidas en la
    CPU, el desplazamiento de profundidad de las sombras y expulsión de texturas con un límite
    según la RAM del móvil;
  - oclusión con `GL_ANY_SAMPLES_PASSED` (el destello del sol se enciende y se apaga, sin
    fundido), FXAA en la salida, tiempo de GPU con `EXT_disjoint_timer_query` y los overlays de
    ImGui (el teclado del nombre del perfil, los mensajes de XAM, depuración, logros).
  Por defecto sale OpenGL ES si el móvil no tiene Vulkan 1.1. Las seis ediciones llevan su
  biblioteca GLES; si una no la trae, se juega con Vulkan y se avisa. Probado en un Galaxy A22
  (Mali-G57): el vídeo, los menús a 30 fps y carreras a unos 22 fps. Faltan las optimizaciones
  del de Vulkan (cielo aplazado, Z temprana, poda de dibujos, caché de programas en disco: la
  primera vez que sale cada shader hay un tirón de 20–110 ms) y los efectos FidelityFX al
  escalar (solo bilineal). El límite de texturas por RAM, la oclusión, el FXAA y los overlays,
  sin probar en el móvil.
- **Buscar servidores en la red** (0.5.2): botón **"Buscar en la red"** en Multijugador. La app
  pregunta por UDP 10902 (`NFSMW_DISCOVER`) por broadcast, tres veces, y a cada IP de la /24 de
  la Wi-Fi o del punto de acceso (hay routers que se comen los broadcast), y escucha 1,5 s
  (`BuscadorServidores.java`). Contestan el servidor del PC (nfsmw-server-lan, con su nombre de
  equipo o `--name`) y un móvil anfitrión (con su gamertag), con cuántos hay en Xbox Live.
  Tocar uno lo guarda y lo elige. En Android, mandar a una IP sin nadie bloquea el socket
  mientras espera al ARP: los envíos van en hilos aparte, con un búfer de 1 MB, para no pasarse
  del plazo sin leer las respuestas (la primera versión no encontraba el servidor de Python).
  El anfitrión coge un `MulticastLock` para recibir los broadcast. El servidor del PC necesita
  la versión nueva y, en Windows, permitir UDP en el cortafuegos.
- **Lista de servidores guardados** (0.5.2): la tarjeta Multijugador guarda varios servidores
  con un nombre opcional ("Casa", "PC de Juan"), y se elige uno o "Sin servidor". Vale una IP o
  un nombre de equipo (`mi-pc`), que se resuelve al jugar. Editar, eliminar con deshacer; la IP
  de la versión anterior pasa a la lista.
- **Saltar las cinemáticas con un toque** (0.5.2, menús con el dedo): en la presentación de
  cada carrera y en las escenas de la historia no hay mando en pantalla y un toque pulsa A, que
  es lo que las salta en el 360 (`HUDACTION_SKIPNIS` en el mapa "hud" del mando, leído de
  `attributes.bin`). El contexto nuevo (`CONTEXTO_CINEMATICA`) sale de `INIS::mInstance` y su
  `mState` (5 = en marcha); la dirección de `INIS::mInstance` va en .bss y no está en las tablas
  de las ediciones, así que se saca de las instrucciones de `NISActivity::JoyHandle`, como la de
  las pausas. Comprobado en las seis ediciones (en la japonesa se mueve); sin probar jugando.
- **Girar con flechas** (0.5.2): el ajuste "Girar en carrera" elige entre el stick izquierdo,
  dos flechas (las de carril de las de aceleración, que giran del todo, poco a poco al pisarlas
  para que el coche no derrape de morro) o inclinar el móvil.
- **Pantalla ancha (prototipo)** (0.5.2, motor nativo, apagado por defecto): el juego dibuja a
  la proporción de la pantalla (1600x720 en un 20:9) y la cámara ve más por los lados (Hor+, en
  `sub_8243EB70`, `nfsmw_panoramico.cpp`), sin barras ni deformar. El HUD, los menús y los
  vídeos, de momento, se estiran. Solo visto en los menús del RedMagic.
- **Un móvil puede ser el servidor de la partida** (motor nativo, 0.5.1): ajuste nuevo
  **"Anfitrión en este móvil"** en la tarjeta "Servidor LAN". Ese móvil corre el servidor de
  salas de EA y juega a la vez; los demás ponen su IP, que la pantalla de inicio enseña. Va
  por Wi-Fi o con el punto de acceso del propio móvil (nunca coge la IP de los datos
  móviles). El servidor es un port 1:1 a Java de `ea_server.py` de nfsmw-server-lan
  (`ServidorEa.java`: mismos mensajes, mismo `stats.json`; con dos clientes de prueba los dos
  contestan exactamente lo mismo) y vive en un servicio en primer plano del proceso principal
  (`ServidorLanService.java`), así que sigue en marcha mientras el juego corre en el suyo. La
  notificación dice la IP y cuántos hay en Xbox Live, y tiene un botón para pararlo. El juego
  del anfitrión entra por su propia IP de la red local, no por 127.0.0.1, porque esa IP es la
  que el servidor reparte a los demás. Estadísticas en `files/servidor_lan/` y registro en
  `files/logs/servidor_lan.log`. Un cambio de protocolo va ahora a los dos servidores.
- **Multijugador en red local** (motor nativo, 0.5.0). La versión de 360 no tiene System
  Link: su online es "Xbox Live" contra los servidores de EA de 2005, que ya no existen. Ahora
  un PC de la misma Wi-Fi hace de ese servidor con
  [nfsmw-server-lan](https://github.com/victorgbd/nfsmw-server-lan) (Python, sin
  dependencias), y en la app basta con poner su IP en el ajuste nuevo **"Servidor LAN
  (multijugador)"**. Funciona Xbox Live → Partida rápida: crear partida, buscarla y unirse
  desde otro móvil, la sala de juego con la lista de jugadores, la carrera entre los dos
  móviles (conectados directamente por UDP, sin pasar por el PC), los resultados al acabar y
  cambiar de anfitrión sin reiniciar. Probado con un RedMagic (Snapdragon 8 Elite) y un Galaxy
  A22 (Mali-G57), con la PAL España: carreras de circuito completas. Lo que hizo falta en el
  SDK del motor nativo, todo apagado sin servidor configurado:
  - los privilegios de Xbox Live (`grant_user_privileges`), la sesión iniciada en Live
    (`red_live`), la red de verdad con la IP del móvil (`red_lan`) y todos los nombres de
    los servidores de EA hacia el PC (`red_dns`, también `XOnlineGetServiceInfo`);
  - los sockets, que venían de Windows: `select`, `FIONBIO`, `setsockopt`, errores WSA,
    orden de bytes de `sendto`/`recvfrom`, `SIGPIPE`; `WSARecvFrom` solapada y
    `WSAGetOverlappedResult` (el hilo de recepción del enlace de juego); y
    `WSAWaitForMultipleEvents`, que esperaba a *todos* los eventos en vez de a cualquiera
    y devolvía siempre 0: el anfitrión solo recibía el primer paquete del otro;
  - un gancho en la tabla `xmap` de DirtySock, que mandaba todo el tráfico de la carrera al
    servidor (en Xbox Live, la pasarela segura); ahora va al otro móvil;
  - la información de sesión de `XSessionCreate` para el anfitrión, `XSessionLeave`, QoS y
    `XNet*` mínimos.
  El gancho lleva direcciones de la PAL España y se traduce a las otras cinco ediciones con el
  resto de la app; esas cinco compilan, pero solo la PAL se ha probado en red. Detalle en
  [docs/diario/red-y-privilegios.md](docs/diario/red-y-privilegios.md).
- Gamertag único en el servidor LAN (0.5.0): al poner el servidor o el gamertag, al volver a la
  pantalla de inicio y al pulsar Jugar, la app pregunta al servidor (TCP 10902) si el nombre
  está libre. Si lo tiene otro móvil, conectado o apartado, el campo sale en rojo y Jugar
  pregunta si se quiere jugar **sin modo online** (esa vez, sin las opciones de red). Un nombre
  comprobado queda apartado 10 minutos para ese móvil, y se renueva mientras la pantalla de
  inicio está abierta. Sin servidor configurado no se comprueba nada; si no contesta, se
  avisa y se deja jugar (`ComprobacionGamertag.java`).
- Estadísticas personales, clasificación y Blacklist 15 del online, en el servidor LAN (0.5.0): el
  protocolo (`user` con `STAT`/`LWSTAT`/`LMSTAT`, `snap` con sus filas `+snp`) sacado del código
  del juego, y los resultados de cada carrera (`rank`) descifrados y sumados por jugador.
  Faltan los modos que no son circuito y qué es cada tabla de la clasificación.
- La pantalla de inicio en **diez idiomas** (0.5.0): a inglés, español y portugués se suman italiano,
  alemán, francés, polaco, japonés, coreano y chino tradicional (traducciones hechas con la
  IA, sin revisar por nativos). El idioma se elige ahora en un desplegable, con el nombre de
  cada uno en su lengua.
- Por qué se cerró el juego, sin adb (0.5.0): al volver a la pantalla de inicio, la app apunta en
  `files/logs/cierre.txt` el motivo que guarda Android (Android 11+) y, si fue un fallo nativo,
  las cadenas legibles del informe; el informe entero va al lado (`Cierres.java`). El APK
  normal no escribe el log del juego, y un móvil que se quedaba en negro no dejaba nada.
- Las demás ediciones que soporta nfsmw-nx, con el motor nativo: PAL Alemania, Italia,
  Francia y Reino Unido, la traducción polaca de fans, Corea y Asia (chino tradicional).
  Se reconocen por el SHA-256 de su `default.xex` (los del instalador de nfsmw-nx) y llevan
  su idioma y su país. **La alemana, la italiana y la inglesa ya compilan** (2026-10-13), con
  los controles táctiles y los menús con el dedo como las demás, y entran en el APK de
  varias ediciones, que con seis pasa a 118 MB; sin probar en el móvil. Las otras cuatro
  esperan su ISO.
  `preparar_nativo.py` saca solo, de la biblioteca de shaders de cada edición, las huellas
  de los tres shaders que el renderizador reconoce (de ellas salen igual las de la USA y la
  japonesa que estaban anotadas), y las PAL europeas van en su propio `app_<edición>` en vez
  de pisar la española. `apk_ediciones.py` mete todas las que estén preparadas.
- Un APK con las tres ediciones (motor nativo, 0.4.2): `python tools/android/apk_ediciones.py`
  saca `out/apk/nfsmw-ediciones-<versión>-release.apk`, que juega la ISO PAL España, USA o
  japonesa. Lleva una `libmain` por edición (`libmain.so`, `libmain_usa.so`,
  `libmain_jpn.so`) y su biblioteca de shaders, y una sola vez lo que no depende de la
  edición: el SDK, SDL, libc++ y los hooks de Turnip (el script comprueba que el SDK de
  cada una exporta lo mismo). Al jugar, el SHA-256 del `default.xex` de la ISO decide qué
  `libmain` se carga, qué shaders se instalan y qué idioma y país se le pasan al juego.
  Las partidas son las mismas para todas. El APK pasa de unos 33 MB a unos 67 MB. Los APK
  de una edición siguen saliendo como siempre. Ver
  [docs/motor-nativo.md](docs/motor-nativo.md#las-tres-en-un-apk).
- Menús con el dedo (motor nativo, 0.4.2): en los menús y en la pausa no hay mando; deslizar
  mueve como al pasar páginas (la cruceta), tocar una opción la elige (el juego va
  hasta ella con la cruceta) y tocarla otra vez, o tocar «Ⓐ Aceptar», acepta; tocar una
  indicación de abajo (Atrás, X, Y, RB, START, LT y RT) pulsa ese botón, y dos dedos van
  atrás. Donde no hay indicación de A (vídeos, «Pulsa START»), un toque acepta. En un
  diálogo Sí/No, el primer toque elige y el segundo acepta. En las listas de opciones, las
  flechas y los «−»/«+» del juego cambian el valor, y deslizar en horizontal también; en EA
  Trax, sus flechas suben y bajan. Al ver un coche, arrastrar gira la cámara (el stick
  derecho), pellizcar hace zoom y LT/RT se pisan mientras se tocan. Esa pantalla se reconoce
  por la etiqueta del texto «Rotar», que es la misma en todas las ediciones, o por su icono:
  la cruceta en la española y el stick derecho en la italiana («Ruota»), que al principio no
  se reconocía. En las carreras, los
  controles de siempre. Lo que hay en pantalla se lee del motor de menús del juego (FEng) sin tocarlo,
  con sus textos (cada indicación es un texto con el icono del botón dentro), comprobando
  antes 16 de sus funciones (iguales en PAL, USA y japonesa). El botón de
  arriba dice MANDO para volver al mando en los menús. Opciones en Controles táctiles:
  apagarlo y deslizar al revés. Cada toque deja en `files/logs/menus.txt` lo que se leyó.
- Ajustes de partida para GPU debiles (Adreno 5xx y hasta la 620, Mali hasta la G68, PowerVR; p. ej.
  Snapdragon 650, 660, 695): la primera vez que se abre la app, y solo en lo que el usuario no haya
  elegido nunca, se ponen 1024x576, 30 fps, sin suavizado, bufer de pantalla a 720p, anisotropico 2x,
  sombras cada 2 fotogramas y de cerca, un solo reflejo del coche y sin reflejo del asfalto. Despues
  manda lo que se cambie en la pantalla de inicio. Sin medir en un movil: `Gpu.debil`,
  `Ajustes.aplicarPerfilGpuDebil`.
- `thread_affinity=auto` con pocos nucleos grandes (2+4 del 650 y 2+6 del 695): solo se fijan el
  hilo del anillo y el principal; ya no se manda todo lo demas a los nucleos pequenos, que dejaba
  el audio y el driver en nucleos de menos de la mitad de capacidad. Si el kernel no da
  `cpu_capacity` (kernels antiguos), se usa la frecuencia maxima. Sin medir en un movil.
  Junto con el reparto para un solo prime (en Cambiado): con un solo prime y el resto
  pequenos, el hilo de la GPU comparte el prime en vez de bajar a un nucleo pequeno.
- Calidad de las sombras en Gráficos (motor nativo, 0.4.1): Rápida (como venía: un solo muestreo
  del mapa de sombras y sin la sombra de la vegetación), Bordes suaves (el patrón 3x3 de la
  Xbox 360, que difumina el contorno; cuesta GPU) y Como la Xbox 360 (además, árboles,
  arbustos y vallas vuelven a dar sombra; cuesta sobre todo CPU). Por defecto, Rápida. La
  resolución del mapa va aparte.
- Resolución de las sombras en Gráficos (motor nativo, 0.4.1), un desplegable: Nativa (1600, la de la
  Xbox 360), Alta (2400), Muy alta (3200) o Ultra (4000).
  El mapa de sombras es también el de los coches, y con más resolución su sombra gana
  definición. Su motor solo dejaba bajarla: `parche_nativo.py` (sección 11) le deja subir
  hasta 250 % y arregla el alto útil de las restauraciones, que con un mapa de más de 1600
  dejaba sin copiar la parte de abajo. En el RedMagic, a 3200 y 1080p, ~0,8 ms más de GPU por fotograma
  y la carrera sigue a 60 fps. Por defecto, 1600.
- Motor nativo en GPU que no son Adreno (0.4.0). El árbol de nfsmw-android pasa al commit
  `6df1501`, que trae lo que hace falta para los Mali, PowerVR y Adreno con drivers
  antiguos, y que solo se activa si a la GPU le falta algo: las texturas BC1-5 se convierten
  en la CPU (a RGBA8, R8 y RG8) en las GPU sin BC; la biblioteca de shaders de Android ya no
  usa enteros de 64 bits ni direcciones de buffer (las constantes van siempre por UBO), así
  que `shaderInt64` deja de hacer falta; en Vulkan 1.1 el SPIR-V 1.5 se pasa a 1.3 al crear
  cada shader y el SDK activa los índices de descriptores por extensión; el renderizador
  nativo ya no exige lo de la emulación de la Xenos (`vertexPipelineStoresAndAtomics`); y en
  Mali no se usan las consultas de oclusión, que colgaban su driver. La biblioteca de
  shaders de la PAL sale igual que la que esperan ellos (`b84602ca…`). Probado en un Samsung
  A22 5G (Mali-G57 MC2, Vulkan 1.1, sin BC ni `shaderInt64`): menús a 40-60 fps y carreras a
  20-22 fps a 1024x576, limitado por la GPU. Fallos que quedan solo en Mali (con los mismos
  caminos forzados en el RedMagic se ve bien, así que son de su driver): el retrovisor sale
  negro, una franja negra tapa el horizonte en la pista del bosque y el vídeo de demostración
  del menú sale duplicado.
- GPU con solo 4 conjuntos de descriptores (`maxBoundDescriptorSets`), como los Mali
  Valhall: los shaders del motor nativo usan 5 y el driver de Mali se caía dentro de
  `vkCreatePipelineLayout`. Con menos de 5, el montón de cubos va en el conjunto del 3D
  (enlace 1), samplers y UBO bajan uno, y las decoraciones de cada shader se cambian igual
  al crearlo (`parche_nativo.py`, sección 8). `nfsmw_nativo_cuatro_conjuntos` lo fuerza en
  cualquier GPU, para probar.
- Xclipse de Samsung: BC4 y BC5 se convierten en la CPU, porque su driver solo tiene
  completas BC1-3 y las demás las convierte él mismo, a medias y con tirones (según el
  análisis de XclipseDecomp; ExynosTools y Eden las esquivan). Sin probar en un móvil con
  Xclipse.
- La sonda de Vulkan, con el motor nativo, dice si su renderizador sirve en la GPU y qué
  adapta (BC en CPU, sin enteros de 64 bits, Vulkan 1.1, Mali) y, aparte, si sirve su modo
  de compatibilidad.
- La pantalla de inicio sabe qué GPU tiene el móvil (OpenGL ES, `GL_RENDERER`, una vez por
  versión del sistema): "Driver del sistema (Mali-G57 MC2)", la ayuda de Turnip solo en
  Adreno y, en otra GPU, que se usa el del sistema. El turbo de GPU solo se puede activar
  con Turnip (es de KGSL), y solo con él se pasa al juego.
- Para probar en un móvil: `gradlew assembleRelease -Pnfsmw.registro=true` deja el log del
  juego también en release, y `-Pnfsmw.depurable=true` hace el APK depurable (`adb shell
  run-as`, para sacar capturas y caches) sin cambiar el código nativo.
- Motor nativo en Android (ver [docs/motor-nativo.md](docs/motor-nativo.md)): el APK se puede
  compilar con el renderizador nativo de
  [nfsmw-android](https://github.com/codepdbh/nfsmw-android), el port a Android de
  [nfsmw-nx](https://github.com/StevensND/nfsmw-nx), en vez de con la GPU de Xbox 360 imitada
  del SDK: `gradlew assembleRelease -Pnfsmw.motor=nativo`. Es otro árbol nativo entero (su
  SDK, su app y su código generado) que `tools/android/preparar_nativo.py` deja preparado
  al lado del repositorio; de aquí solo entra `nativo_android.cpp`, los puentes JNI. La app
  de Java es la misma: pantalla de inicio, mando táctil y editor, idiomas, y la ISO elegida
  con el selector del sistema y leída sin copiarla (`parche_iso.py` se aplica tal cual a su
  SDK, más `tools/android/parche_nativo.py`), los drivers propios (Turnip, con
  `parche_turnip.py` en su SDK), la sonda de Vulkan, los hilos fijados a núcleos y la salida de audio por nuestro driver
  AAudio (con el volumen, el limitador y el silencio durante las películas de su SDK). Ajustes
  nuevos para ese motor: resolución de render, límite de fps, sombras, reflejos, resplandor
  del cielo, filtros de color e imagen, desenfoque y modo de compatibilidad. El
  motor de Xenos sigue siendo el que sale por defecto. Funciona en el móvil y va mejor que
  el de Xenos; lo portado después (Turnip, sonda, afinidad y opciones) falta probarlo una a
  una.
- Motor nativo para la edición **USA**: `preparar_nativo.py --iso <ISO USA>` crea `app_usa`,
  la app de nfsmw-android con sus 56.802 direcciones PAL traducidas, y
  `gradlew assembleRelease -Pnfsmw.motor=nativo -Pnfsmw.edicion=usa` la compila. Mismo
  método que la build USA de nfsmw-nx, con su `crear_arbol.py` y sus parejas corregidas,
  pero con nuestro `emparejar.py` (sin numpy), que resuelve solo 10 de esas 11 parejas.
  Las funciones que toca su código nativo son idénticas en las dos ediciones; los shaders
  de resplandor y cielo cambian de huella (`ediciones.json`, `"nativo"`), y la biblioteca de
  shaders sale igual que la oficial de nfsmw-nx para la USA.
- Motor nativo para la edición **japonesa** (`-Pnfsmw.edicion=jpn`, idioma 2 y país 53). En
  ella `.data` crece 0x5A0 bytes y los datos se mueven dentro: `emparejar.py` los traduce
  por el código que calcula cada dirección (`lis` + parte baja, como su
  `datos_por_referencias.py`), y `comprobar_parejas` exige que cada dirección que calculan
  las funciones que usa la app sea la traducción de la PAL (846, todas bien, en la USA y en
  la japonesa). Cambian tres huellas de shader (resplandor, cielo y composición); la
  biblioteca sale igual que la oficial de nfsmw-nx.
- Vuelve la vibración con el motor nativo (Controles táctiles → Vibración, encendida por
  defecto). Su SDK la trae apagada (`input_vibracion = false`: el juego pide vibrar y a los
  mandos solo les llega cero), por eso un mando de Xbox Series X por USB no vibraba; la app
  pasa `--input_vibracion` con el ajuste. Los mandos físicos vibran por SDL
  (`SDL_RumbleGamepad`), si Android expone sus motores. Por USB, el kernel del RedMagic no
  se los da (su xpad no trae force feedback: el mando sale sin `VIBRATOR`), así que con la
  vibración puesta se enciende el driver HIDAPI de Xbox de SDL
  (`SDL_HINT_JOYSTICK_HIDAPI_XBOX`), que abre los mandos de Xbox 360 y One/Series por USB
  y les manda la vibración él mismo; al conectarlos Android pide permiso de USB. Probado
  con un mando de Xbox Series X por USB-C. Por Bluetooth siguen por Android, que ahí sí
  tiene sus motores. Con el mando táctil vibra el
  móvil: su driver de SDL no le hacía caso al mando táctil y `parche_nativo.py` le hace
  llamar a la app (`NfsmwAndroidVibrar` → `GameActivity.vibrarMando`), que lo traduce a 16
  niveles de fuerza con el vibrador del móvil. Se calla al minimizar, en el editor del
  mando y mientras haya un mando físico (USB o Bluetooth, conectado o usado en la
  partida): entonces vibra solo el mando. **Probar vibración** (Controles táctiles) hace
  vibrar cada mando conectado con su vibrador de Android y dice cuántos motores le da.
- En pausa (el menú de pausa, un mensaje...) no se gira inclinando el móvil: movía las
  opciones del menú. Se quedan los controles de conducir, que tienen la cruceta; también
  al pausar una carrera de aceleración, cuyo menú de pausa no se podía recorrer. La app lee el contador de pausas del juego
  (`FEManager::mPauseRequest`, el de `RequestPauseSimulation`).
- Girar inclinando el móvil (Controles táctiles → Girar inclinando el móvil): en la
  disposición de conducir, el eje X del stick izquierdo sale del sensor de gravedad
  (acelerómetro y giroscopio; sin él, el acelerómetro filtrado), como un volante, y el
  stick no se ve. La sensibilidad, en un deslizador de 1 a 20: gira del todo de 45 a 6
  grados de inclinación, cada paso un ~10 % menos (con 10, 17 grados), con una zona muerta
  del 3 % para el pulso. Tiene en cuenta hacia qué lado está apaisado el móvil. En aceleración siguen las flechas
  de carril.
- Controles de conducir en el resto de carreras, en la conducción libre y en las
  persecuciones: los de aceleración (pedales, palanca de cambios, cámara y retrovisor, sin
  L3 ni R3), pero con el stick izquierdo para girar y la cruceta, que ahí hace más que abrir
  la clasificación. Su disposición de fábrica: el stick en el reflejo de los pedales y la
  cruceta en el de los botones en rombo. El editor tiene ya tres (NORMALES, CONDUCIENDO y
  ACELERACIÓN).
- Disposición propia para las carreras de aceleración, con la suya de fábrica, simétrica:
  abajo los pedales a la derecha y las flechas de carril a la izquierda, en espejo y sobre
  la misma línea, con la palanca junto a las flechas; los botones en rombo a la derecha;
  arriba a la izquierda la clasificación, la cámara y el retrovisor; y BACK y START junto a
  OCULTAR/TÁCTIL. El editor del mando elige qué se edita (CONTROLES: NORMALES /
  ACELERACIÓN), y desde la partida abre la de la parte del juego en que se esté. Lo que se
  mueve se guarda aparte (`a_<control>` en las preferencias); RESTAURAR vuelve a la de
  fábrica de la que se edita.
- Editar el mando táctil desde la partida: un engranaje junto a OCULTAR/TÁCTIL abre el
  mismo editor que la pantalla de inicio (mover, tamaño, opacidad, restaurar), sobre el
  juego. Con el motor nativo el juego se queda quieto mientras tanto: la misma pausa que al
  minimizar, que ahora lleva la cuenta de por qué está en pausa (segundo plano o editor) y
  solo se reanuda cuando no queda ningún motivo. Lo que se guarda en la partida lo ve
  también el editor de la pantalla de inicio (`MODE_MULTI_PROCESS`: van en procesos
  distintos).
- Controles táctiles según la parte del juego (motor nativo). En las carreras de
  aceleración RT y LT son pedales (acelerador y freno), el stick derecho una palanca de
  cambios, RB una cámara y LB un retrovisor, el stick izquierdo, que ahí solo cambia de
  carril, dos botones cuadrados con flecha que lo llevan entero a cada lado, la cruceta,
  que ahí solo abre la clasificación, un botón con su icono que pulsa arriba, y L3 y R3
  desaparecen; en los menús y en el
  resto, los de siempre. La app lee del juego su estado (`GameFlowManager`) y el tipo de la
  carrera en curso (`GRaceStatus`): de la base de datos de carreras o, en las carreras
  rápidas, que son una copia sin esa entrada, de su atributo `racetype`, buscado en sus
  colecciones de atributos como lo hace el juego. Direcciones de la descompilación del
  juego, comprobadas en el código del 360 y traducidas a cada edición. Probado en el móvil
  con carreras rápidas de aceleración (USA).
- El APK se declara como juego (`android:appCategory="game"` e `isGame`): lo ven así el
  modo de juego y el panel de juegos de Android 12+ y los lanzadores de juegos de los
  fabricantes. Con `res/xml/game_mode_config.xml` el modo de juego no baja la resolución
  ni limita los fps por su cuenta: para eso están los ajustes de la app.
- Motor nativo: **Límite de fps → Sin límite** (experimental), y el límite pasa a elegirse
  en un desplegable. Su motor limita los fps con el ritmo del vblank que cuenta el juego;
  sin límite, ese vblank va a 240 Hz, el máximo del SDK, y el juego va tan rápido como dé
  el móvil (`parche_nativo.py`).
- Escalado a la pantalla con AMD FSR 1.0 o CAS (Gráficos → Escalado a la pantalla), en los
  dos motores: el presentador del SDK ya los traía, con sus shaders precompilados, pero
  solo los activaba descargando el FidelityFX SDK de escritorio.
- Ediciones del juego en Android (ver [docs/ediciones.md](docs/ediciones.md)): además de la
  PAL España se puede compilar para la **USA**. `tools/android/generar_codigo.py` reconoce
  la edición del `default.xex` (por su SHA-256 o por el PDB que lleva dentro) y traduce las
  direcciones de `overrides.toml`, `huecos.toml` y los ganchos de C++, que están escritas
  para la española, con la tabla de `tools/ediciones/<edición>/direcciones.tsv`. El idioma
  y el país que se pasan al juego salen de la edición (antes iban fijos a 5 y 31). Método
  de [nfsmw-nx](https://github.com/StevensND/nfsmw-nx), reescrito sin dependencias:
  `tools/ediciones/imagen.py` saca la imagen del XEX (AES y LZX en Python puro),
  `emparejar.py` empareja las dos ediciones y `ediciones.py` hace la tabla y la aplica.
  Comprobado hasta el codegen y la compilación del APK: mismas funciones, avisos y huecos
  que PAL, y las 11 funciones enganchadas idénticas. **Sin probar en el juego**: la ISO de
  USA con la que se hizo estaba incompleta.
- `emparejar.py` toma por datos las tablas de saltos aunque no cambien de bloque de 64 KB
  (antes, una entrada `0x82xxxxxx` se leía como un `lwz` con otro inmediato), y con
  `por_contenido=False` no traduce por coincidencia de contenido, que en la USA cayó una
  vez dentro de otra función.
- La pantalla de inicio dice para qué edición se compiló el APK y avisa si la ISO elegida
  es de otra edición o está incompleta (le falta el final: una descarga o una extracción
  cortada), en vez de dejar una pantalla en negro.
- Port a Android para Snapdragon, en desarrollo (ver [docs/android.md](docs/android.md)):
  `tools/android/preparar_sdk.py` deja un SDK v0.10.0 aparte con el parche Android y
  libadrenotools; `tools/android/generar_codigo.py` genera el código con ese SDK; y
  `android/` es la app Gradle, con una sonda de Vulkan que se puede usar sin el juego.
  Arranca desde la ISO en un Snapdragon 8 Elite y llega al menú y a las carreras.
- `parche_turnip.py`: carga un driver Vulkan propio (Turnip) con libadrenotools en Android.
  El turbo de GPU se aplica siempre, también con `false`: `KGSL_PROP_PWRCTRL` es de todo el
  móvil y persiste, y en un 8 Elite el turbo rinde menos (menú a 14,6 fps frente a 42).
- Rendimiento en Android, menú principal 3D de 15 a ~50 fps (ver
  [docs/android.md](docs/android.md#rendimiento-lo-que-se-encontró-perfilando)):
  `parche_pipeline.py`, `parche_fallos.py`, `parche_esperas.py`, `parche_cola_presentar.py`
  (presentar en una segunda cola de Vulkan), `parche_espera_anillo.py` con
  `android/.../ganchos.cpp` (el D3D del juego ya no gira esperando espacio en el anillo) y
  `parche_subidas.py` (las subidas a la memoria compartida no cortan el pase de render).
- Ajuste "Pintar la escena de una vez" (cvar `nfsmw_una_franja`, gancho sobre el `BeginTiling`
  de D3D): el juego parte la pantalla en tres franjas porque con MSAA 4x no cabe en la EDRAM
  de la Xbox 360, y el procesador de comandos repite toda la lista de dibujos por cada
  franja. Medido: la predicación solo se salta el 0,5 % de los dibujos, así que el triple es
  real. Con el gancho, en el menú 3D se pasa de 830 a 329 dibujos por fotograma y de 31 a 17
  resolves, con la imagen idéntica.
- `parche_xma_paquetes.py` y `parche_xma_edge.py`: cuatro arreglos del descodificador XMA
  (uno propio y tres portados de xenia-edge), incluido el que dejaba medio fotograma sin
  entregar y colgaba el hilo de audio del juego.
- `parche_anillo_bloques.py` (de xenia-edge): el puntero de lectura del anillo se publica
  según se vacía y no solo al acabar la ráfaga, que es lo que el juego consulta para saber
  si tiene sitio para escribir comandos.
- `parche_msaa.py` y el ajuste "Sin suavizado de bordes" en la app: el juego pide MSAA 4x y
  cada pase de render movía las cuatro muestras entre GPU y memoria. Quitándolo, el menú y
  el garaje pasan de 46 a **60 fps** (el tope) en un Snapdragon 8 Elite. En carrera no
  cambia: allí el cuello es la CPU.
- `parche_fences.py`: consultar un fence costaba ~8 ms con Turnip (una sincronización de
  caché dentro del driver). Ahora se usa `vkGetFenceStatus`, solo al abrir fotograma, y un
  hilo vigía espera los fences aparte. Turnip pasa de 20,3 a 30,7 fps.
- `parche_vblank.py`: el juego va a doble búfer con vsync, así que cada fotograma espera al
  vblank; el procesador de comandos lo sondeaba con `Sleep(1ms)` y el hilo del vblank
  miraba el reloj cada milisegundo. Ahora el vblank llega a su hora y despierta al CP por
  futex. Y con vsync adaptativo: si el fotograma ya llegó tarde, el vblank lo sigue en vez
  de esperar a la rejilla, así que los fps ya no caen de 60 a 30 de golpe (un fotograma de
  17,9 ms da 55,8 fps en vez de 30).
- `parche_registros.py`, `parche_vertices.py` y `parche_constantes.py`: tres intentos de
  bajar el coste de CPU por dibujo (no invalidar cachés si el valor no cambia, pedir los
  buffers de vértices de una vez, copiar las constantes en bloques seguidos). **Medidos:
  ninguno cambia nada** —7,8–8,1 ms de dibujos por fotograma con y sin ellos—, así que no
  se aplican por defecto. Se dejan escritos con la medida, que descarta esas tres vías.
- `parche_area.py`: cada pase de render se abre solo sobre la zona que pinta (cvar
  `vulkan_recortar_area`). Con el driver de Qualcomm no cambia nada; queda con interruptor.
- El contador en pantalla muestra también el tiempo por fotograma y el peor del último medio
  segundo, que es donde se ven los tirones.
- `parche_cvars.py`: los cvars del plugin de GPU conservan lo pasado por línea de comandos
  cuando el plugin se recarga. Sin él, en Android no se aplicaba ninguno.
- `parche_teclado.py`: el teclado (`--mnk_mode`) genera las pulsaciones que leen los menús.
- `parche_audio.py`: salida estéreo en Android.
- `parche_fps.py` y `parche_tiempos.py`: contador de fps y reparto del tiempo del
  procesador de comandos en el log (`--medir_paquetes=true` para el detalle por paquete).
- App Android: contador de fps en pantalla, resoluciones de pantalla más bajas, ajustes de
  rendimiento y un banco de pruebas lanzable desde `adb` (`nfsmw.banco`, `nfsmw.driver`).
- `parche_iso.py`: `--game_data_root` acepta una ISO, montada con el `DiscImageDevice` del
  SDK. En la v0.10.0 solo se montaban carpetas.
- Los parches aceptan la variable `NFSMW_SDK` para apuntar a otro árbol del SDK.
- `parche_una_pasada.py` y el ajuste "Una sola pasada de la escena" (cvar `nfsmw_una_pasada`,
  apagado por defecto, necesita `gpu_sin_msaa`): pintar la escena una vez en vez de tres.
  A diferencia de `nfsmw_una_franja`, que parcheaba el `BeginTiling` del juego, éste actúa
  en el procesador de comandos: pone `PA_SC_WINDOW_OFFSET` a cero, abre el recorte de
  ventana a la unión de las tres franjas (aprendida del fotograma anterior) y, en las
  vueltas 2 y 3, **tira los paquetes de dibujo y solo ésos**. Medido en carrera: 669
  669–802 dibujos por fotograma tirados y ~35 fps frente a ~30 sin el ajuste. Pero **la
  imagen se corrompe**: la mitad derecha sale en negro con una mancha blanca, igual que
  `nfsmw_una_franja`. Es intermitente —la primera captura salió limpia— y apunta a la unión
  del recorte, que se aprende del fotograma anterior. Queda apagado y sin resolver.
- [docs/pipeline-nativo.md](docs/pipeline-nativo.md): diseño de un camino de pintado de
  Vulkan hecho a medida del juego, para dejar de emular la GPU de la Xbox 360. Se engancha
  en el estado (`IssueDraw`), no en las funciones de D3D del juego.
- `tools/diagnostico/censo_pipeline.py`: la fase 1 de ese diseño. Cvars `nfsmw_censo` y
  `nfsmw_censo_minimo`, que vuelcan de un fotograma los pases, los objetivos, los resolves
  y las parejas de shaders. Elige la escena por densidad de dibujos en vez de navegando por
  los menús, que con adb no es fiable. Medido en el móvil: un fotograma de carrera son
  **nueve pases y 4.570 dibujos, para solo 31 parejas de shaders y 9 estados de mezcla**.
  Los resolves enseñan el troceado en tres tiras (`1280x256 + 1280x256 + 1280x208`), un
  mapa cúbico de reflejos de seis caras de `256x256` y dos sombras de `1600x1600`. Y
  buscándole el periodo a la secuencia de dibujos queda **demostrado** lo que hasta ahora
  se deducía: el pase de la escena son **2.748 dibujos en tres bloques idénticos** de 916,
  o sea 1.832 repeticiones puras, el 40 % del fotograma y ~13,6 ms de CPU.
- Ajuste "Fijar hilos a núcleos" en Android (cvar `thread_affinity`, encendido de fábrica):
  `android/app/src/main/cpp/afinidad.cpp` recorre los hilos del proceso cada 2 s y fija
  por nombre el procesador de comandos y el hilo principal a los núcleos prime, y el resto
  fuera de ellos. Con `auto` los prime se sacan de `cpu_capacity`; también admite reglas
  a mano. Medido en dos rondas en orden inverso, con 2.700–4.200 dibujos por fotograma:
  **de 24,5 a 28,6 fps (+17 %)** y de 8,8 a 7,1 µs por dibujo. La ganancia es del hilo
  principal, que sin fijar solo pasaba el 25–32 % del tiempo en un prime; el procesador de
  comandos ya estaba ahí. Ver [docs/android.md](docs/android.md#afinidad-de-hilos-a-núcleos).
- App de Android en inglés, español y portugués. Por defecto sigue el idioma del móvil, y en
  la sección Idioma se puede forzar uno. Van traducidos también los avisos al importar un
  driver y el botón HIDE/TOUCH del mando táctil.
- Suavizado de bordes a elegir en la app de Android: apagado, MSAA o FXAA, cada uno con su
  configuración.
  - MSAA: 2x o 4x. `parche_msaa.py` añade el cvar `gpu_msaa_muestras`, que recorta las muestras
    que pide el juego en vez de quitarlas todas.
  - FXAA: calidad normal o alta. Ya estaba en el SDK sin usar (`swap_post_effect=fxaa` o
    `fxaa_extreme`): un pase de cómputo sobre la imagen final.

  Sustituye a la casilla "Sin suavizado de bordes", y lo elegido en ella se conserva. Ver
  [docs/android.md](docs/android.md#suavizado-de-bordes-msaa-o-fxaa).

### Cambiado

- **Girar con flechas, a tope de golpe como el teclado de PC** (0.5.6): antes el giro empezaba al
  50 % y llegaba al 100 % en 300 ms. En la versión de PC la tecla llega al coche como 1.0 sin
  rampa, y lo gradual lo pone la física (`SuspensionRacer::DoHumanSteering`: 180°/s como mucho,
  menos a velocidad, y el ángulo máximo baja de 40° a unos 3°), que es la misma que en el 360:
  las tablas de giro están iguales en `speed.exe` y en el XEX. Un salto de 0 a 1 además le da el
  giro de ruedas más rápido posible. Android e iOS.
- **Android 8.0 como mínimo** (0.5.5, `minSdk 26`, antes 29): para llegar a los móviles con GLES 3.2
  que se quedaron en Android 8 o 9 (Galaxy S7/S8/Note 8, Snapdragon 820/835, Pixel 1), que es
  para lo que está el backend de OpenGL ES. Los motivos de cierre (Android 11) y el recorte de la
  cámara del editor táctil (Android 9) comprueban la versión; ninguna biblioteca nativa usa nada
  más nuevo que Android 8.0. Sin probar en un móvil con Android 8 o 9.
- **Las sombras se dibujan siempre en cada fotograma** (0.5.5): se quita la opción «cada 2
  fotogramas», que dejaba el mapa un fotograma por detrás del coche y hacía temblar las sombras.
  Vale también para quien la tuviera guardada, para el preajuste «Ahorro de batería» y para el
  perfil automático de las GPU débiles.
- «API gráfica» es la primera opción de Gráficos (0.5.5), antes de los preajustes.
- El APK del motor nativo ya no exige Vulkan en el manifiesto (0.5.3): se puede instalar en un
  móvil sin Vulkan (el de Xenos lo sigue exigiendo).
- Motor nativo (0.5.3): lo que los dibujos y las copias de Vulkan hacen sin tocar la API
  (desenmosaicado, mips, orden de bytes, rectángulos de copia, escritura de texels al guest) va
  ahora en cabeceras comunes con el backend de OpenGL ES
  (`nfsmw_nativo_texturas_comun.h`, `nfsmw_nativo_copias_comun.h`), con los mismos cuerpos.
- El launcher va por secciones con iconos (0.5.2): Multijugador, Controles y Otras opciones.
- Los controles táctiles de fábrica en carrera (0.5.2): pedales al doble de tamaño; los
  botones con iconos (B el nitro, A el freno de mano, X el Speedbreaker) y sin Y, porque mirar
  atrás ya es LB; flechas, stick derecho y botones en el mismo sitio en las de aceleración y en
  las demás. Inclinando el móvil, la cruceta baja a la esquina.
- Menús con el dedo (0.5.2): en las cinemáticas también se quita el mando (ver arriba).
- El campo del servidor LAN se llama ahora "IP del servidor" en vez de "IP del PC" (0.5.1):
  puede ser un PC o un móvil anfitrión.
- Menús con el dedo (0.5.0): las indicaciones de abajo crecen hacia el espacio libre. En las
  pantallas del online van en filas a unos 4 mm («Ⓐ Cambiar coche» encima de «Ⓑ Atrás») y el
  dedo pulsaba la de al lado. Ahora cada fila llega hasta el centro de la vecina (la primera
  sube un poco más, sin pisar ninguna opción, y la última baja hasta el borde), la primera de
  cada fila llega hasta el borde izquierdo y, donde dos se pisan, gana la fila más cercana al
  dedo. En el punto de encuentro del online, cada fila pasa de unos 3 mm a 6-7 mm de alto en
  un Galaxy A22 (`CrecerBarra`, `nativo_android.cpp`).
- El servidor de EA de la LAN sale del repositorio a su propio proyecto (0.5.0),
  [nfsmw-server-lan](https://github.com/victorgbd/nfsmw-server-lan); en `tools/red/`
  queda una nota.
- El motor nativo, dentro del repositorio: [`nativo/`](nativo) es el código de
  codepdbh/nfsmw-android en `6df1501` (GPL-3.0, con su licencia), en un commit tal cual y
  otro con nuestros cambios a 25 de sus ficheros, en vez de una copia al lado
  (`../nfsmw-android`) que `preparar_nativo.py` clonaba y parcheaba. Gradle, CMake y los
  scripts lo buscan ahí; los parches dicen que ya estaban. Lo que sale del juego y las
  librerías de terceros de su SDK siguen sin versionarse. La primera compilación de cada
  edición, entera otra vez (cambian las rutas).
- "Fijar hilos a núcleos" con un solo núcleo prime (0.4.2; Snapdragon 8 Gen 2 y 3, 865, 7 Gen x;
  la AYN Thor y la Thor Lite, entre otros): el hilo principal y el de la GPU iban los dos a
  ese núcleo y se lo quitaban uno a otro. Ahora el principal se queda el prime y el de la
  GPU baja al escalón siguiente (en el 865, cpu7 y cpu4–6). Y si el kernel no da
  `cpu_capacity`, o da lo mismo en todos los núcleos, se mira la frecuencia máxima de cada
  uno. Con dos prime o más (el 8 Elite) no cambia nada. Comprobado con una topología falsa
  de cada caso; falta medirlo en un móvil con un solo prime.
- Controles táctiles conduciendo (0.4.1; el resto de carreras, la conducción libre y las
  persecuciones): el stick derecho vuelve a ser un stick, como antes de esa disposición. La
  palanca de cambios queda solo en las carreras de aceleración. Nueva disposición de fábrica
  para los sticks, la que dejó el usuario en el móvil: el izquierdo en su sitio pero a 1,5
  veces su tamaño, y el derecho a su derecha y algo más abajo. Lo movido con el editor se
  conserva.
- Driver de audio AAudio de Android (`android/app/src/main/cpp/audio/`), más ligero:
  - El anillo de muestras va **sin cerrojo** (un productor, un consumidor, con atómicos).
    Antes el callback de tiempo real de AAudio cogía el mismo mutex que el hilo del juego,
    y si el juego lo tenía cogido el callback esperaba, que es la causa clásica de cortes.
  - Índices con máscara en vez de módulo (antes, una división por cada muestra), y el
    callback copia con `memcpy` en uno o dos tramos en vez de muestra a muestra.
  - Fuera el cálculo del pico por fotograma y la línea de logcat que lo usaba.
- Editor del mando táctil, en la pantalla de inicio ("Editar controles táctiles",
  `EditorTactilActivity`): apaisado y a pantalla completa como la partida, con el marco donde irá
  la imagen del juego.
  - Se arrastra cada control y se cambia su tamaño, del 50 al 200 %; sin ninguno elegido, el de
    todos.
  - La opacidad es la de todos, del 20 al 100 %.
  - RESTAURAR vuelve a la disposición de fábrica, CANCELAR (o atrás) sale sin guardar y LISTO
    guarda.

  Se guarda por control como fracción de la pantalla, así que vale para cualquier resolución, y
  lo que no se toca sigue la disposición de fábrica.
- La escena en una pasada, ahora **desde el juego** (`android/app/src/main/cpp/render_targets.cpp`,
  cvar `nfsmw_render_sin_mosaico`), portado de
  [StevensND/nfsmw-nx](https://github.com/StevensND/nfsmw-nx) (GPL-3.0, derivado de este proyecto).
  En vez de tirar las franjas en el procesador de comandos, se iguala la tabla de modos de
  antialiasing del juego (`sub_82458850`) y se le deja en su modo 2: una tira de 1280x736 sin
  MSAA. El juego pinta una vez con sus propios resolves, sin desplazamiento de ventana ni
  predicación que corregir. También se quita el MSAA de cualquier conjunto que se registre
  (`sub_8245D320`), y se corrige la referencia de muestras de las consultas de oclusión del
  destello del sol (`sub_82225610`). La opción "Escena en una pasada" usa esto, y
  `nfsmw_una_pasada` ya no se pasa. Su modo de 1080p no se trae: no cabe en la EDRAM emulada.
  Medido en la carrera de demostración, sin MSAA en los dos casos: 28-31 fps con las tiras
  frente a 34-45 en una pasada, con 3.400-4.800 dibujos por fotograma frente a 2.300-3.100, y la
  imagen completa. **Encendido de fábrica.**
- `parche_cache_pipelines.py`: el precreado de pipelines al arrancar, que **nunca había
  funcionado en Android**, y una cache de pipelines del driver en disco.
  - El SDK guarda los shaders (`.xsh`) y los pipelines usados (`.xpso`), pero los abre con
    `"a+b"` y lee la cabecera sin rebobinar. En Android (bionic) `"a+"` empieza a leer por el
    final, así que el fichero se daba por malo y se vaciaba en cada arranque. Ahora se rebobina.
  - Una `VkPipelineCache` guardada en disco, un fichero por driver. Antes cada pipeline se
    creaba con `VK_NULL_HANDLE` y el driver lo compilaba entero.

  Medido: al arrancar se precrean 125 pipelines en 0,07 s, frente a 131 en 1,71 s sin la cache
  del driver (~13 ms por pipeline). Esos pipelines ya no se compilan en plena carrera.
- `nfsmw_una_pasada` (en la app, "Pipeline nativo: escena en una pasada"), **sin probar
  todavía**: arreglo candidato de la corrupción de imagen. Dos causas, sacadas leyendo
  `GetResolveInfo`:
  - el resolve escribía cada franja 256 o 512 filas por debajo de su textura, porque saca el
    destino de las mismas coordenadas que el origen;
  - el parche tiraba los resolves de las franjas 2 y 3, que también van como paquete de dibujo.

  Ahora el destino lleva el desplazamiento que pidió el juego, los resolves no se tiran, y el
  recorte se decide al dibujar, con la superficie de verdad. El parche toca también
  `util/draw.cpp`. Ver [docs/pipeline-nativo.md](docs/pipeline-nativo.md).

  Probado en el móvil, seguía saliendo a ratos la mitad derecha en negro. La causa, encontrada en
  el log: el parche ponía a cero el desplazamiento de ventana **entero**. El juego usa su x
  (−640) para resolver la mitad derecha de la imagen desde una superficie de 640 de ancho. Sin
  ella, ese resolve caía fuera y el SDK lo rechazaba 5.124 veces por partida (`outside the
  surface pitch 640`), y los dibujos de esa mitad se tiraban como si fueran repeticiones. Ahora
  solo se toca la y, que es lo que mueven las franjas. Probado: 0 rechazos y la imagen completa
  en carrera, tirando ~480 dibujos por fotograma. También, por si acaso, en la primera vuelta
  pasan los dibujos de cualquier franja (`SET_BIN_MASK`).
- El APK de release va sin registro: `--log_level=off` y `--log_file=/dev/null` (sin
  `log_file`, el SDK se inventaría una ruta en el directorio actual, que en Android no se puede
  escribir). No se escribe fichero ni llega nada a logcat, y el interruptor "Registro detallado"
  no aparece. El contador de fps sigue, porque no sale del registro. Solo quedan en logcat los
  errores nativos de arranque y de AAudio, que únicamente salen si algo falla.
- Gamertag de Xbox 360 editable desde la pantalla de inicio (tarjeta "Perfil de Xbox 360").
  `parche_gamertag.py` añade el cvar `user_gamertag`: el SDK lo tenía fijo a "User" en
  `UserProfile`, que es de donde leen `XamUserGetName`, `XamUserGetGamerTag` y
  `XamUserGetSigninInfo`. La app solo lo pasa si vale para Xbox 360: hasta 15 letras, números y
  espacios, empezando por letra. Solo cambia el nombre; las partidas guardadas van por el XUID,
  que es el mismo. El log lo confirma con `[perfil] gamertag '...'`.
- Mando táctil: el botón OCULTAR/TÁCTIL se va a los 4 s de aparecer o de usarse. Tocar donde está
  lo vuelve a sacar sin pulsarlo, y el toque siguiente ya lo pulsa.
- Mando táctil con un mando físico: al conectarlo o usarlo, los controles táctiles y el botón
  TÁCTIL desaparecen. Tocar la pantalla saca el botón durante 4 s, para poder volver a
  encenderlos, y usar el mando lo esconde otra vez. Al desconectarlo vuelve lo que hubiera
  sin mando. Solo cuentan gamepads y joysticks de verdad, no lo que solo trae cruceta.
- Mando táctil: el dibujado ya no crea objetos (dos `RectF` en la cruceta y un
  `FontMetrics` por cada una de las ~17 etiquetas, en cada redibujado), y la densidad de
  pantalla se lee una vez. Al nativo solo se le mandan los botones y ejes que cambian:
  al arrastrar un stick son dos ejes en vez de 21 llamadas a SDL por evento de toque,
  cada una con el cerrojo de joysticks que comparte con el hilo del juego.
- El rótulo de fps ya no despierta al hilo principal en segundo plano, y no relanza la
  maquetación cuando el texto no cambia.
- La pantalla de inicio de Android está reorganizada:
  - El botón de jugar va arriba, junto a la ISO, y dice por qué está desactivado cuando lo
    está.
  - Los ajustes van por secciones (Pantalla, Gráficos, Rendimiento, Idioma), con interruptores
    y una línea de explicación debajo de cada uno.
  - Lo que solo sirve para medir va en Avanzado, plegado.
  - Suavizado y posprocesado se muestran en positivo: encendido es como el juego original.
  - El acento es el #6F7432 del mando táctil.
  - El turbo de GPU solo se puede encender con un driver propio (Turnip) elegido. Con el del
    sistema sale gris, y la app manda `android_gpu_turbo=false` aunque estuviera marcado: el
    SDK lo aplica en cada arranque y es un ajuste de todo el móvil que persiste.
  - Sin la barra de título, que repetía el nombre. El margen suma las barras del sistema:
    con `targetSdk` 35, Android 15 pinta la app por debajo, y lo último de la lista quedaba
    tapado por la barra de navegación.
- Resolución de pantalla en Android: 480p (720×480), 720p (1280×720) o la nativa. Sin
  estirar, el juego ocupa el 16:9 más grande que cabe, centrado con barras. Con "Estirar",
  ocupa la pantalla entera. La proporción la pone el tamaño de la vista y el presentador llena
  el búfer (`present_letterbox=false`), porque el presentador supone píxeles cuadrados y con
  720×480 habría puesto las barras mal.
- `nfsmw_una_pasada`: con el ajuste apagado ya no hace nada al acabar cada fotograma. Antes
  copiaba un `std::map` y escribía `"[una pasada] 0 dibujos..."` en el log cada 60
  fotogramas, para siempre.

### Arreglado

- **El juego se cerraba al poco de la primera cinemática de la carrera** (0.5.5, PR #4 de
  Kayo-b; SIGSEGV en
  `sub_8237FE58`, el hilo del juego, a los ~15 s de empezar la del inicio del modo carrera; en
  una AYN Thor siempre, con 30 o 60 fps). `[rexcrt]` cambia el memcpy del juego
  (`sub_826BE1B0`) por `std::memcpy`, y el juego lo usa con rangos que se solapan: así quita un
  controlador de animación de su lista de activos, copiando la cola un hueco hacia atrás. El del
  juego copia hacia delante; el de bionic en arm64 no, y duplicaba elementos (en el móvil, otra
  lista en 1203 de 2016 casos; en el PC, ninguno): un controlador parado seguía en la lista sin
  pista, y al actualizarlo se leía la dirección 4. `tools/parche_memcpy.py` hace que el memcpy
  nativo dé lo mismo que el del juego también con solape, en los dos SDK (en el del motor nativo
  ya va aplicado). Ahora el memcpy nativo usa `std::memcpy` si no hay solape, `std::memmove`
  si el destino va antes que el origen, y el algoritmo del juego si va después y se pisan; da
  lo mismo que un modelo del del juego en 819.200 combinaciones de desplazamiento y longitud,
  en el PC y en el móvil. Comprobado en la Thor (Snapdragon 8 Gen 2, Android 13, motor
  nativo, PAL España): el cierre salía 4 de 4 veces; con el arreglo pasa la cinemática y
  siete minutos de carrera. Va en el `librexruntime.so` común, así que vale para todas las
  ediciones del APK. El motor de Xenos lleva el parche pero no se ha compilado ni probado con él.
- **Vulkan en Mali: retrovisor negro, franja negra en el horizonte de la pista del bosque y vídeo
  del menú duplicado** (0.5.5). No eran del driver de ARM, sino fallos de sincronización nuestros
  que el de Adreno perdona: la capa de validación, en el Samsung A22 (Mali-G57), vio copias,
  borrados, blits y lecturas de imágenes seguidos sin barrera entre ellos (por ejemplo, el destino
  de render se borraba antes de que su resolve lo hubiera copiado, y el cubo del retrovisor
  recibía sus caras antes de su borrado inicial), también entre búferes enviados uno tras otro.
  Ahora hay una barrera entre dos transferencias seguidas (`BarreraTransferencias`,
  `nfsmw_nativo_barreras_transferencia`, encendida; `false` para comparar). Con eso, cero avisos
  de sincronización de la capa de validación y los tres fallos resueltos en el A22.
- El presentador de Vulkan reutilizaba su semáforo de presentación antes de que la presentación
  anterior lo soltara (0.5.5, `VUID-vkQueueSubmit-pSignalSemaphores-00067`): ahora hay uno por
  imagen de la cadena de presentación.
- El gamertag vacío se veía montado sobre su etiqueta (0.5.1): sin foco, la etiqueta
  "Gamertag" y el ejemplo "User" se pintaban en el mismo sitio. Ahora la etiqueta va siempre
  arriba y el ejemplo, en gris, dentro. Lo mismo en el campo del servidor.
- `preparar_nativo.py` dejaba sin efecto los ganchos nuevos en las otras ediciones (0.5.0): si el
  generador de código no tenía nada que reescribir, las llamadas que una preparación anterior
  había hecho directas seguían saltándose el gancho, porque `llamadas_directas.py` solo las
  hace directas y nunca al revés. Ahora se deshacen primero. Salió con los ganchos de red;
  comprobado en las cinco ediciones que ninguna función enganchada se llama directa.
- Menús con el dedo (0.5.0): «RB Tarjeta de jugador» en la sala de juego y el punto de encuentro no
  hacía nada (su icono no es el RB conocido): se reconoce por la etiqueta del texto. Y un
  icono sin reconocer en el mismo sitio que una indicación conocida ya no se queda con el
  toque.
- Una ISO que no es de Xbox 360 (0.5.0; la versión de PC o PS2, un .zip sin extraer, otro juego) se
  avisa en la pantalla de inicio en vez de dar una pantalla negra, y el SDK del motor nativo
  acepta las imágenes XGD3 que la app ya daba por buenas.
- Pantalla dividida (motor nativo): las sombras del jugador 2, la mitad de abajo, salían
  congeladas. El juego dibuja un mapa de sombras por jugador (vistas 13 y 14, de las vistas 1 y
  2), y `nfsmw_sombras_mapas = 1` le hacía dibujar solo el primero. Ahora ese recorte solo vale
  con un jugador, mientras la vista 2 está apagada (`nfsmw_recorte_sombras.cpp`). Comprobado en
  el móvil con la PAL España. Y `preparar_nativo.py --juego` con una ruta relativa ya no falla al
  generar la biblioteca de shaders.
- Las estelas de luz de los pilotos con el nitro salían la mitad de largas que en la Xbox 360
  a 60 fps: el juego las dibuja entre las 3 últimas posiciones del coche, una por fotograma, y
  la consola va a 30. `parche_nativo.py` (sección 12) le da las posiciones de hace 1/30 s y
  2/30 s, así que miden lo mismo a cualquier límite de fps (comprobado en el móvil).
- El juego se cerraba al empezar una carrera en móviles lentos (Samsung A22: 3 de 3
  carreras) con "Call to invalid or unregistered function at guest address 0x00000000".
  Los dos hilos del juego se pasan órdenes por una lista (0x82909650): uno las añade
  (`sub_823C8378`) y el otro las va ejecutando a la vez (`sub_823C83F8`), sin barreras. En
  ARM el que ejecuta podía ver el contador nuevo antes que la orden y llamar a una función
  0. Ahora la orden se escribe igual pero con una barrera de liberación antes de publicar el
  final y el contador, y el ejecutor solo hace las ya publicadas (`parche_nativo.py`,
  sección 9). Con el arreglo, dos carreras seguidas sin cierre.
- Controles táctiles según la parte del juego con el motor nativo nuevo: la app preguntaba
  antes de que el ejecutable y la tabla de tipos de carrera estuvieran en memoria, leía
  ceros y daba la detección por imposible para siempre. Mientras no estén, vuelve a mirar.
- AAudio: con `audio_maxqframes=64` el juego podía tener 64 bloques en vuelo, pero el
  anillo solo cabía 32. Iba siempre lleno, tirando el audio más viejo en cada bloque.
  Ahora cabe `kMaximumQueuedFrames` (64) entero, así que no desborda con ningún valor.
- AAudio: al desconectarse la salida (cascos, Bluetooth) el driver cerraba y reabría el
  flujo **desde su propio callback de error**, cosa que AAudio prohíbe y que puede colgar.
  Ahora se hace en un hilo aparte.
- Mando táctil: los dos sticks iban invertidos en vertical. Mandaban arriba como positivo, y
  SDL usa positivo hacia abajo; el SDK le da la vuelta al pasarlo al mando de la Xbox, igual
  que con uno físico, así que al juego le llegaba al revés. Además, sus preferencias se llaman
  ahora `nfsmw_touch_controller`, no `skate3_touch_controller`, que venía de otro port; el
  fichero viejo se borra.
- Mando táctil: al ocultarlo se mandaba `-1` como "todo suelto", pero el nativo lo leía
  como una máscara con todos los bits a uno y **pulsaba los 15 botones a la vez**.
- `parche_una_pasada.py` insertaba su código en medio de los bloques de `parche_msaa.py` y
  `parche_vblank.py`: el binario funcionaba, pero su `--estado` decía "a medias" y su
  `--revertir` ya no los encontraba. Ahora ancla fuera de esas zonas.
- `preparar_sdk.py` no registraba `parche_una_pasada` ni `parche_camino_edram`, que estaban
  aplicados a mano. Un SDK preparado desde cero no tenía el cvar que la app le pasa.
- Android, con una resolución de pantalla reducida: SDL mete la superficie con
  `WRAP_CONTENT`, y con `setFixedSize` un `SurfaceView` así mide lo que el búfer. La vista del
  juego encogía a una esquina. Ahora lleva siempre un tamaño exacto.

### Quitado

- La colocación de hilos por núcleo de `android_main.cpp` (`thread_affinity`). **Nunca tuvo
  efecto**: el SDK no lee ese cvar en ningún sitio, y en POSIX su
  `EnableAffinityConfiguration()` está vacía. El log decía "Afinidad de hilos configurada"
  sin que se fijara ninguno. Lo sustituye `afinidad.cpp` (ver Añadido), que sí los fija.
- El ajuste "Pintar la escena de una vez" (`nfsmw_una_franja`) y su gancho sobre
  `BeginTiling`: lo sustituye `nfsmw_una_pasada`, y en carrera ni siquiera bajaba los
  dibujos.
- `parche_audio.py`: forzaba a estéreo el driver de audio de SDL, que en Android ya no se
  usa desde que el audio va por AAudio.
- `nativeMsPeor` y la resolución del rótulo de fps, que ya no se mostraban.
- En la app de Android:
  - El ajuste "Llenar recortando arriba y abajo" (`present_allow_overscan_cutoff`). Solo
    queda Estirar.
  - Las resoluciones de 1080 y 540 líneas: quedan 480p, 720p y la nativa.
  - `CustomGpuDriver.java`, que nada usaba.
- Herramientas medidas y descartadas: `parche_prioridades.py`, `parche_xma_sin_hilo.py`,
  `parche_registros.py`, `parche_vertices.py` y `parche_constantes.py`. Lo que se midió
  queda en docs/android.md.

## [0.0.2] - 2026-09-17

### Añadido

- El lanzador acepta un `.iso` directamente: lo extrae solo la primera vez a
  `game_root_cache\` dentro de la carpeta portable y reutiliza esa copia después. Antes
  solo servía apuntar a una carpeta ya extraída (`--game_data_root` exige un directorio,
  el SDK no sabe montar `.iso`).
- Ventana del lanzador redimensionable y con scroll: la banda de portada se estrecha en
  pantallas pequeñas en vez de forzar scroll horizontal, y los ajustes se centran en
  pantallas anchas en vez de quedarse pegados a un lado con un hueco enorme.
- Ajustes del lanzador en dos columnas en vez de una lista larga.
- Tema oscuro para el lanzador.

### Cambiado

- El lanzador arranca por defecto a 1080p + escala x2 en vez de 720p + x1 en una
  instalación nueva (sin `lanzador.json` todavía) — coincide con lo que `nfsmw.toml` ya
  trae configurado de fábrica, en vez de arrancar más bajo que eso sin que nadie lo pida.
- La portada del lanzador cubre el panel entero ("cover", no "fit"): antes dejaba un
  tramo negro vacío debajo en proporciones de ventana altas.
- El juego se lanza con prioridad de proceso más alta.

### Arreglado

- Ventana del lanzador marcada DPI-aware: en monitores con escala de Windows (125%,
  150%...) salía borrosa por el bitmap-stretch de Windows; ahora nítida.
- "Banner duplicado" al agrandar la ventana del lanzador: faltaba
  `ControlStyles.ResizeRedraw` en el panel de la portada, así que al crecer el control
  solo se invalidaba la franja nueva expuesta y quedaba el recorte antiguo debajo.
- `nfsmw.toml` de la carpeta portable había perdido la sección de resolución
  (`video_mode_width`/`video_mode_height`/`resolution_scale`) al restaurar una copia de
  seguridad anterior; repuesta para que coincida con `app/nfsmw.toml` del repositorio.

## [0.0.1] - 2026-09-10

Primera versión ordenada del proyecto. Todo lo de abajo se hizo antes de que existiera
este repositorio; queda registrado aquí porque es el estado del que parte.

### Añadido

- Recompilación estática completa de NFS Most Wanted (2005, Xbox 360, `454107D9`) que
  arranca, pasa el prólogo y llega a mundo abierto.
- `parche_desatasco.py`: arregla el cuelgue del descodificador XMA que mataba el audio al
  salir del garaje y congelaba el juego al volver al menú.
- `parche_presentador.py`: vsync real y limitador de fps. Ninguno de los dos existía en
  el SDK.
- `parche_backend.py`: selector de API gráfica (D3D12 / Vulkan) desde el menú de F4, con
  respaldo automático si la elegida no está compilada.
- `parche_velocidad.py`: velocidad del juego ajustable en porcentaje, 0–200%.
- `parche_restaurar.py`: mejoras del menú de F4 — aviso de reinicio pendiente con botón
  para reiniciar, botón de restaurar la configuración de arranque, deslizadores con
  límites para los ajustes decimales, y la API gráfica en uso a la vista.
- `parche_gpu_fallback.py`: respaldo a WARP si no se puede crear el dispositivo D3D12.
- `parche_privilegios.py`: ajuste `grant_user_privileges` para pasar la puerta de
  privilegios de Xbox Live. Apagado por defecto.
- Lanzador nativo en C#/WinForms con la portada al lado, compilado con el `csc.exe` que
  ya trae Windows. Comparte los ajustes con el lanzador antiguo de PowerShell.
- Escalado de resolución interna hasta x4 desde el lanzador.

### Cambiado

- En la carpeta portable, `NFS_Most_Wanted.exe` pasa a ser **el lanzador** y el juego se
  llama `nfsmw.exe`, para que el icono del juego abra la ventana de opciones.
- El camino RTV de la EDRAM es el que viene elegido: casi duplica los fps en gráficas
  integradas frente a ROV.
- Los ajustes del lanzador se llaman "Tamaño de la ventana" y "Resolución interna", que
  es lo que hacen. Antes eran "Resolución de salida" y "Escala de renderizado" y se
  confundían.

### Arreglado

- El menú de F4 ya no abre siempre con un aviso falso de "hace falta reiniciar".
- Los parches ya no se duplican al ejecutarlos dos veces.
- `comprobar_dist.ps1` reconoce `mscoree.dll` como DLL del sistema, y ya no da por rota
  una carpeta que lleva el lanzador de .NET dentro.

### Sin resolver

- Vulkan renderiza en negro en Intel.
- Franja horizontal con el camino RTV en algunas integradas.
- Multijugador: faltan 114 de 158 funciones de red del SDK, incluidas las del System
  Link, y los manejadores de sesión son stubs. Ver
  [docs/diario/red-y-privilegios.md](docs/diario/red-y-privilegios.md).
