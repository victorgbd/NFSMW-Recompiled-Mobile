package io.github.nfsmwrecomp;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * Lo que el usuario elige en la pantalla de inicio, y su traduccion a la linea
 * de comandos del juego.
 *
 * La linea de comandos manda sobre nfsmw.toml (ver docs/arquitectura.md), asi
 * que lo que se pasa aqui siempre gana. Solo se pasan cvars que existen en el
 * SDK v0.10.0: el launcher de Linux usa algunos (storage_root, audio_*) que en
 * esta version no estan.
 */
final class Ajustes {

    static final String PREFS = "ajustes";

    private final SharedPreferences p;

    Ajustes(Context ctx) {
        p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    /**
     * Mando tactil: girar inclinando el movil, como un volante, en vez de con
     * el stick izquierdo (en la disposicion de conducir), y con que
     * sensibilidad: de SENSIBILIDAD_MIN a SENSIBILIDAD_MAX.
     */
    /** Mando tactil: el movil vibra cuando el juego hace vibrar el mando. */
    boolean vibracion() { return p.getBoolean("vibracion", true); }
    void vibracion(boolean v) { p.edit().putBoolean("vibracion", v).apply(); }

    boolean girarInclinando() { return p.getBoolean("girar_inclinando", false); }
    void girarInclinando(boolean v) { p.edit().putBoolean("girar_inclinando", v).apply(); }
    static final int SENSIBILIDAD_MIN = 1;
    static final int SENSIBILIDAD_MAX = 20;
    int sensibilidadGiro() { return p.getInt("sensibilidad_giro", 10); }
    void sensibilidadGiro(int v) { p.edit().putInt("sensibilidad_giro", v).apply(); }

    /**
     * Los grados que hay que inclinar el movil para girar del todo con esa
     * sensibilidad: de 45 (1) a 6 (20), cada paso un ~10 % menos, para que
     * cada uno se note igual en toda la escala. Con 10, unos 17.
     */
    static float anguloGiro(int sensibilidad) {
        int s = Math.max(SENSIBILIDAD_MIN, Math.min(SENSIBILIDAD_MAX, sensibilidad));
        return (float) (45.0 * Math.pow(6.0 / 45.0, (s - SENSIBILIDAD_MIN)
                / (double) (SENSIBILIDAD_MAX - SENSIBILIDAD_MIN)));
    }

    String iso() { return p.getString("iso", null); }
    void iso(String uri) { p.edit().putString("iso", uri).apply(); }

    String nombreIso() { return p.getString("iso_nombre", null); }
    void nombreIso(String n) { p.edit().putString("iso_nombre", n).apply(); }

    /**
     * Lo que IsoXex ha leido de la ISO elegida. xexIso: el SHA-256 de su
     * default.xex, que dice de que edicion del juego es (el APK solo vale para
     * una); null si aun no se ha mirado y "" si no se pudo leer. isoFaltan:
     * los bytes que le faltan al fichero, 0 si esta entero.
     */
    String xexIso() { return p.getString("iso_xex", null); }
    long isoFaltan() { return p.getLong("iso_faltan", 0); }
    void isoMirada(String sha, long faltan) {
        p.edit().putString("iso_xex", sha).putLong("iso_faltan", faltan).apply();
    }
    void isoSinMirar() { p.edit().remove("iso_xex").remove("iso_faltan").apply(); }

    /** Nombre del driver importado, o null para el del sistema. */
    String driver() { return p.getString("driver", null); }
    void driver(String nombre) { p.edit().putString("driver", nombre).apply(); }

    /**
     * Turbo de GPU (android_gpu_turbo): quita el control de energia de la GPU
     * (KGSL_PROP_PWRCTRL). Solo cuenta con un driver propio: con el del
     * sistema se manda siempre false (ver argumentos()).
     */
    boolean turbo() { return p.getBoolean("turbo", false); }
    void turbo(boolean v) { p.edit().putBoolean("turbo", v).apply(); }

    int escala() { return p.getInt("escala", 1); }
    void escala(int v) { p.edit().putInt("escala", v).apply(); }

    /**
     * Gamertag del perfil de Xbox 360 (user_gamertag, tools/parche_gamertag.py).
     * Vacio = el del SDK, "User". Solo cambia el nombre: el XUID es el mismo,
     * asi que las partidas guardadas siguen valiendo.
     */
    String gamertag() { return p.getString("gamertag", ""); }
    void gamertag(String v) { p.edit().putString("gamertag", v).apply(); }

    /**
     * Lo que acepta Xbox 360: hasta 15 letras, numeros y espacios, empezando
     * por letra. Vacio tambien vale (el de fabrica).
     */
    static boolean gamertagValido(String g) {
        return g.isEmpty() || g.matches("[A-Za-z][A-Za-z0-9 ]{0,14}");
    }

    boolean registroDetallado() { return p.getBoolean("registro", false); }
    void registroDetallado(boolean v) { p.edit().putBoolean("registro", v).apply(); }

    // --- Rendimiento y graficos. Los valores por defecto son los del SDK: lo
    // que se toca aqui es para PROBAR que cuesta cada cosa en cada movil.

    /**
     * Tamano del bufer donde se presenta el juego: RES_480P (720x480),
     * RES_720P (1280x720) o RES_NATIVA (el de la vista). Lo aplica
     * GameActivity.ajustarSuperficie(); el juego se pinta igual, esto solo
     * cambia la imagen final.
     */
    String resolucion() {
        if (!p.contains("resolucion")) {
            // Lo que se eligiera antes como alto de pantalla ("alto_pantalla").
            int alto = p.getInt("alto_pantalla", 0);
            return alto == 480 ? RES_480P : alto == 720 ? RES_720P : RES_NATIVA;
        }
        return p.getString("resolucion", RES_NATIVA);
    }
    void resolucion(String v) { p.edit().putString("resolucion", v).apply(); }

    static final String RES_NATIVA = "nativa";
    static final String RES_720P = "720p";
    static final String RES_480P = "480p";

    /** Alto en pixeles de una resolucion fija; 0 para la nativa. */
    static int alto(String resolucion) {
        return RES_480P.equals(resolucion) ? 480 : RES_720P.equals(resolucion) ? 720 : 0;
    }

    /** Ancho en pixeles de una resolucion fija; 0 para la nativa. */
    static int ancho(String resolucion) {
        return RES_480P.equals(resolucion) ? 720 : RES_720P.equals(resolucion) ? 1280 : 0;
    }

    boolean mostrarFps() { return p.getBoolean("mostrar_fps", true); }
    void mostrarFps(boolean v) { p.edit().putBoolean("mostrar_fps", v).apply(); }

    /** Consultas de oclusion de verdad (occlusion_query_enable). */
    boolean oclusion() { return p.getBoolean("oclusion", true); }
    void oclusion(boolean v) { p.edit().putBoolean("oclusion", v).apply(); }

    /** readback_resolve=fast: la exposicion correcta. Sin el, imagen lavada. */
    boolean exposicionFiel() { return p.getBoolean("exposicion", true); }
    void exposicionFiel(boolean v) { p.edit().putBoolean("exposicion", v).apply(); }

    /** readback_memexport: coherencia de memoria de lo que escriben los shaders. */
    boolean lecturaMemexport() { return p.getBoolean("memexport", true); }
    void lecturaMemexport(boolean v) { p.edit().putBoolean("memexport", v).apply(); }

    /** clear_memory_page_state: refrescar paginas escritas por la GPU. */
    boolean refrescarPaginas() { return p.getBoolean("paginas", true); }
    void refrescarPaginas(boolean v) { p.edit().putBoolean("paginas", v).apply(); }

    static final String AA_NO = "no";
    static final String AA_MSAA = "msaa";
    static final String AA_FXAA = "fxaa";

    /**
     * Suavizado de bordes, uno de los dos o ninguno:
     *
     *   AA_MSAA  el del juego (pide 4x): gpu_sin_msaa=false y
     *            gpu_msaa_muestras (tools/parche_msaa.py). Lo mas fino y lo
     *            mas caro: en el menu 3D del movil de pruebas, 4x son 46 fps
     *            frente a 60 sin nada, porque cada pase de render mueve las
     *            muestras entre la GPU y la memoria.
     *   AA_FXAA  un filtro sobre la imagen final (swap_post_effect=fxaa o
     *            fxaa_extreme, del SDK): un pase de computo sobre 1280x720.
     *
     * AA_NO de fabrica, como el antiguo "sin MSAA" encendido.
     */
    String antialiasing() {
        if (!p.contains("antialiasing")) {
            // Antes era la casilla "sin MSAA", encendida de fabrica.
            return p.getBoolean("sin_msaa", true) ? AA_NO : AA_MSAA;
        }
        return p.getString("antialiasing", AA_NO);
    }
    void antialiasing(String v) { p.edit().putString("antialiasing", v).apply(); }

    /** Muestras con AA_MSAA: 2 o 4 (lo que pide el juego). */
    int msaaMuestras() { return p.getInt("msaa_muestras", 4); }
    void msaaMuestras(int v) { p.edit().putInt("msaa_muestras", v).apply(); }

    /**
     * Con AA_FXAA, la calidad alta del SDK (fxaa_extreme): umbrales mas bajos,
     * asi que suaviza mas bordes, y busca el borde mas lejos (12 px frente a 8).
     */
    boolean fxaaAlta() { return p.getBoolean("fxaa_alta", false); }
    void fxaaAlta(boolean v) { p.edit().putBoolean("fxaa_alta", v).apply(); }

    /**
     * nfsmw_sin_posprocesado: desactiva el postprocesado para ahorrar GPU.
     */
    boolean sinPosprocesado() { return p.getBoolean("sin_posprocesado", false); }
    void sinPosprocesado(boolean v) { p.edit().putBoolean("sin_posprocesado", v).apply(); }

    /**
     * Pintar la escena de una vez en vez de en tres franjas. Medido con el
     * censo: de 4.570 dibujos de un fotograma de carrera, 2.748 son el pase de
     * la escena y son tres bloques identicos; sobran 1.832, el 40 %.
     *
     * Lo hace el propio juego (nfsmw_render_sin_mosaico, render_targets.cpp,
     * portado de nfsmw-nx): usa su modo sin antialiasing, de una tira. Antes se
     * hacia a la fuerza en el procesador de comandos (nfsmw_una_pasada,
     * tools/parche_una_pasada.py), que ya no se pasa.
     *
     * No vale con MSAA: ese modo es justo el que no lo tiene. Con FXAA si.
     *
     * ENCENDIDO de fabrica. Medido el 2026-09-28 en la carrera de
     * demostracion, sin MSAA en los dos casos: 28-31 fps con las tiras frente
     * a 34-45 en una pasada, con 3.400-4.800 dibujos por fotograma frente a
     * 2.300-3.100, y la imagen completa en todas las capturas.
     */
    boolean unaPasada() {
        return p.getBoolean("una_pasada", true) && !AA_MSAA.equals(antialiasing());
    }
    void unaPasada(boolean v) { p.edit().putBoolean("una_pasada", v).apply(); }

    /**
     * El juego pinta 16:9 y la pantalla del movil es mas ancha (2688x1216 es
     * 2,21:1). Apagado, la vista del juego es el 16:9 mas grande que cabe,
     * con barras negras; encendido, ocupa la pantalla entera y la imagen se
     * deforma. Lo aplica GameActivity.ajustarSuperficie().
     *
     * No cuesta rendimiento: es la etapa de presentacion, no el pintado.
     */
    boolean estirar() { return p.getBoolean("estirar", false); }
    void estirar(boolean v) { p.edit().putBoolean("estirar", v).apply(); }

    /**
     * render_target_path_vulkan=fsi: emular la EDRAM dentro del shader de
     * pixel (fragment shader interlock) en vez de con objetivos de render del
     * anfitrion. Se salta las transferencias de propiedad entre objetivos.
     *
     * Si la GPU no trae la extension, el SDK vuelve solo a "fbo", asi que
     * encenderlo no rompe nada.
     *
     * OJO con las expectativas: medido el 2026-09-21, la emulacion de EDRAM
     * cuesta ~1,2 ms por fotograma de los ~68, o sea un 2 %. Este camino puede
     * ayudar o no, pero no es donde esta el problema de fluidez.
     */
    boolean edramEnShader() { return p.getBoolean("edram_fsi", false); }
    void edramEnShader(boolean v) { p.edit().putBoolean("edram_fsi", v).apply(); }

    /**
     * thread_affinity=auto (android/app/src/main/cpp/afinidad.cpp): el
     * procesador de comandos ("GPU Commands") y el hilo principal del juego a
     * los nucleos prime, y todo lo demas a los otros, para que nadie les quite
     * el nucleo. En el Snapdragon 8 Elite: cpu6-7 a 4,32 GHz frente a cpu0-5 a
     * 3,53 GHz.
     *
     * ENCENDIDO de fabrica. Medido el 2026-09-23, dos tandas en orden inverso
     * en la carrera de demostracion: coste por dibujo de 8,85 a 7,15 us (-19 %)
     * y de ~24,4 a ~28,6 fps (+17 %). El CP ya corria en los prime sin esto;
     * lo que cambia es el hilo principal, que antes solo estaba ahi el 25-32 %
     * del tiempo, y que nadie mas les quita esos nucleos.
     */
    boolean fijarHilos() { return p.getBoolean("fijar_hilos", true); }
    void fijarHilos(boolean v) { p.edit().putBoolean("fijar_hilos", v).apply(); }

    /**
     * Motor nativo: tope de fps del juego (nfsmw_limite_fps: 30, 60, 90 o 120).
     * Con 90 y 120 el juego cuenta sus vblank a ese ritmo; es experimental.
     * FPS_SIN_LIMITE: el vblank a 240 Hz, el maximo del SDK (parche_nativo.py).
     */
    static final int FPS_SIN_LIMITE = 0;
    int limiteFps() { return p.getInt("limite_fps", 60); }
    void limiteFps(int v) { p.edit().putInt("limite_fps", v).apply(); }

    // --- Motor nativo: sus ajustes de graficos y sonido. Los valores por
    // defecto son los de su nfsmw.toml para Android. Cada uno es un cvar suyo
    // (MotorNativo.argumentos).

    /** nfsmw_resolucion_interna: a la que dibuja el juego. */
    String resolucionInterna() { return p.getString("resolucion_interna", "1280x720"); }
    void resolucionInterna(String v) { p.edit().putString("resolucion_interna", v).apply(); }

    /** nfsmw_tratamiento_visual: el filtro de color del juego (original, suave, apagado). */
    String filtroColor() { return p.getString("filtro_color", "original"); }
    void filtroColor(String v) { p.edit().putString("filtro_color", v).apply(); }

    /** El desenfoque radial al acelerar y con el NOS (al reves que nfsmw_nativo_sin_desenfoque). */
    boolean desenfoque() { return p.getBoolean("desenfoque", false); }
    void desenfoque(boolean v) { p.edit().putBoolean("desenfoque", v).apply(); }

    /** nfsmw_posproceso: un filtro de imagen encima (apagado, cine, vivo...). */
    String filtroImagen() { return p.getString("filtro_imagen", "apagado"); }
    void filtroImagen(String v) { p.edit().putString("filtro_imagen", v).apply(); }

    /**
     * La calidad de las sombras del motor nativo, en dos cvars suyos:
     *   - rapida: un solo muestreo del mapa de sombras (nfsmw_nativo_pcf_barato)
     *     y sin la sombra de la vegetacion (nfsmw_sombras_sin_vegetacion), como
     *     venia;
     *   - suave: el patron 3x3 de la Xbox 360, que difumina el borde. Cuesta GPU
     *     en la escena, y ya no vale el atajo que evita copiar el mapa;
     *   - xbox360: ademas, arboles, arbustos y vallas vuelven a dar sombra. Son
     *     mas de la mitad de los dibujos del pase de sombras: cuesta sobre todo CPU.
     */
    static final String SOMBRAS_RAPIDA = "rapida";
    static final String SOMBRAS_SUAVE = "suave";
    static final String SOMBRAS_XBOX = "xbox360";

    String calidadSombras() { return p.getString("calidad_sombras", SOMBRAS_RAPIDA); }
    void calidadSombras(String v) { p.edit().putString("calidad_sombras", v).apply(); }

    /**
     * nfsmw_nativo_sombras_escala: el lado del mapa de sombras, en % de los 1600
     * de la Xbox 360 (100, 150, 200 o 250). Ahi se dibujan tambien los coches, y
     * con mas su sombra gana definicion. Por encima de 100 es parche nuestro
     * (parche_nativo.py, seccion 11).
     */
    int sombrasEscala() { return p.getInt("sombras_escala", 100); }
    void sombrasEscala(int v) { p.edit().putInt("sombras_escala", v).apply(); }

    /** nfsmw_sombras_cada: los mapas de sombras, 1 de cada N fotogramas. */
    int sombrasCada() { return p.getInt("sombras_cada", 1); }
    void sombrasCada(int v) { p.edit().putInt("sombras_cada", v).apply(); }

    /** nfsmw_sombras_corte: 100 = como el juego; mas = menos distancia, y mas barato. */
    int sombrasCorte() { return p.getInt("sombras_corte", 150); }
    void sombrasCorte(int v) { p.edit().putInt("sombras_corte", v).apply(); }

    /** nfsmw_cubemap_caras_max: caras del reflejo del coche por fotograma (6, 2 o 1). */
    int reflejosCoche() { return p.getInt("reflejos_coche", 6); }
    void reflejosCoche(int v) { p.edit().putInt("reflejos_coche", v).apply(); }

    /** nfsmw_reflejo_carretera: el reflejo del asfalto mojado (y del agua). */
    boolean reflejoAsfalto() { return p.getBoolean("reflejo_asfalto", true); }
    void reflejoAsfalto(boolean v) { p.edit().putBoolean("reflejo_asfalto", v).apply(); }

    /** nfsmw_resplandor_cielo: natural, original o suave. */
    String resplandorCielo() { return p.getString("resplandor_cielo", "natural"); }
    void resplandorCielo(String v) { p.edit().putString("resplandor_cielo", v).apply(); }

    /**
     * Motor nativo: el sonido por nuestro driver AAudio (nfsmw_audio_aaudio),
     * el que arreglo los cortes con el motor de Xenos, o por el de SDL de su SDK.
     */
    boolean audioAAudio() { return p.getBoolean("audio_aaudio", true); }
    void audioAAudio(boolean v) { p.edit().putBoolean("audio_aaudio", v).apply(); }

    /**
     * El modo de compatibilidad de su app: su propia emulacion de la GPU de
     * Xbox 360, para GPUs que no pueden con el renderizador nativo. Lento.
     */
    boolean compatibilidad() { return p.getBoolean("compatibilidad", false); }
    void compatibilidad(boolean v) { p.edit().putBoolean("compatibilidad", v).apply(); }

    /**
     * present_effect del presentador del SDK: como se lleva la imagen del juego
     * a la pantalla. "bilinear" (el de siempre), "fsr" (AMD FSR 1.0: escalado
     * con deteccion de bordes y nitidez) o "cas" (AMD CAS: solo nitidez). Los
     * dos motores.
     */
    String escalado() { return p.getString("escalado", "bilinear"); }
    void escalado(String v) { p.edit().putString("escalado", v).apply(); }

    /** anisotropic_override del SDK: 0 sin filtro, 2 = 2x, 3 = 4x, 5 = 16x. */
    int anisotropico() { return p.getInt("anisotropico", 3); }
    void anisotropico(int v) { p.edit().putInt("anisotropico", v).apply(); }

    static File carpetaDatos(Context ctx) {
        File f = new File(ctx.getExternalFilesDir(null), "datos");
        //noinspection ResultOfMethodCallIgnored
        f.mkdirs();
        return f;
    }

    static File carpetaLogs(Context ctx) {
        File f = new File(ctx.getExternalFilesDir(null), "logs");
        //noinspection ResultOfMethodCallIgnored
        f.mkdirs();
        return f;
    }

    static File informeSonda(Context ctx) {
        return new File(ctx.getExternalFilesDir(null), "sonda_vulkan.txt");
    }

    /**
     * Argumentos comunes a la partida y a la sonda. La ISO no va aqui: la abre
     * GameActivity en su propio proceso.
     */
    List<String> argumentos(Context ctx, boolean sonda) {
        return argumentos(ctx, sonda, driver());
    }

    /**
     * Igual, pero con el driver que se diga en vez del guardado: null o
     * "sistema" es el de Qualcomm. Para el banco de pruebas, que no debe tocar
     * las preferencias (y repetir --android_gpu_driver_* detras no sirve: el
     * parser de cvars pierde argumentos con opciones repetidas).
     */
    List<String> argumentos(Context ctx, boolean sonda, String nombreDriver) {
        // El motor nativo es otro programa, con sus propios ajustes.
        if (MotorNativo.ACTIVO) {
            return MotorNativo.argumentos(ctx, this, sonda, nombreDriver);
        }
        List<String> a = new ArrayList<>();

        // Lo que en escritorio pone nfsmw_app.h si nadie lo pide; aqui se pasa
        // siempre para que no dependa de ese orden.
        a.add("--gpu_plugin=xenos");
        // Sin esto el SDK pide d3d12 (su valor por defecto), el plugin no lo
        // tiene, se descarga y se vuelve a cargar para vulkan; y en esa
        // recarga sus cvars perdian lo que se les pasa aqui abajo (ver
        // tools/parche_cvars.py).
        a.add("--gpu_backend=vulkan");
        // Lo que ya aplica el launcher ARM64 de Linux para Turnip: el camino
        // de render targets por FBO y sin memoria compartida dispersa.
        a.add("--render_target_path_vulkan=" + (edramEnShader() ? "fsi" : "fbo"));
        // La proporcion la pone el tamano de la vista (GameActivity), no el
        // presentador: este cree que el bufer tiene pixeles cuadrados, y con
        // 720x480 en una vista 16:9 pondria barras donde no tocan. Asi llena
        // el bufer entero y el compositor de Android lo escala a la vista.
        a.add("--present_letterbox=false");
        a.add("--present_allow_overscan_cutoff=false");
        a.add("--present_effect=" + escalado());
        a.add("--vulkan_sparse_shared_memory=false");
        a.add("--fullscreen=true");
        a.add("--resolution_scale=" + escala());
        // Rendimiento y graficos. readback_resolve lo pone nfsmw_app.h a 'fast'
        // si nadie lo pide; aqui se pide siempre, asi que manda esto.
        a.add("--occlusion_query_enable=" + oclusion());
        a.add("--readback_resolve=" + (exposicionFiel() ? "fast" : "none"));
        a.add("--readback_memexport=" + lecturaMemexport());
        a.add("--clear_memory_page_state=" + refrescarPaginas());
        a.add("--anisotropic_override=" + anisotropico());
        String aa = antialiasing();
        a.add("--gpu_sin_msaa=" + !AA_MSAA.equals(aa));
        a.add("--gpu_msaa_muestras=" + msaaMuestras());
        a.add("--swap_post_effect="
                + (AA_FXAA.equals(aa) ? (fxaaAlta() ? "fxaa_extreme" : "fxaa") : "none"));
        a.add("--nfsmw_sin_posprocesado=" + sinPosprocesado());
        a.add("--nfsmw_render_sin_mosaico=" + unaPasada());
        // Solo si se pide: el cvar vacio ya es "no tocar".
        if (fijarHilos()) {
            a.add("--thread_affinity=auto");
        }
        // Muestreo a 64 como en XenDroid para maxima estabilidad de audio
        a.add("--audio_maxqframes=64");
        // El idioma y el pais de la consola, los de la edicion del juego con la
        // que se compilo el APK (PAL Espana: 5 y 31; USA: 1 y 103). Salen de
        // app/generated-android/edicion.json, via build.gradle.kts.
        a.add("--user_language=" + BuildConfig.JUEGO_IDIOMA);
        a.add("--user_country=" + BuildConfig.JUEGO_PAIS);
        // Un solo argumento aunque lleve espacios: va en el array, sin shell.
        String gamertag = gamertag();
        if (gamertagValido(gamertag) && !gamertag.isEmpty()) {
            a.add("--user_gamertag=" + gamertag);
        }
        a.add("--user_data_root=" + carpetaDatos(ctx).getAbsolutePath());

        if (!BuildConfig.DEBUG) {
            // Release: sin registro, ni en fichero ni en logcat (el SDK copia
            // cada linea a logcat en Android, y con "off" no llega a ningun
            // sitio). /dev/null y no vacio: sin log_file el SDK se inventa una
            // ruta en el directorio actual, que en Android no se puede escribir.
            // El contador de fps de la pantalla no sale del registro: sigue.
            a.add("--log_file=/dev/null");
            a.add("--log_level=off");
        } else {
            String log = sonda ? "sonda.log" : "nfsmw.log";
            a.add("--log_file=" + new File(carpetaLogs(ctx), log).getAbsolutePath());
            // info, no debug: lo caro es 'debug', que hace que el plugin de GPU
            // escriba una linea POR FOTOGRAMA en almacenamiento compartido (FUSE)
            // y hunde los fps. Con info son unas pocas lineas por partida, y ahi
            // va el contador de fps de parche_fps.py.
            a.add("--log_level=" + (registroDetallado() ? "debug" : "info"));
            // Sin esto el fichero se vuelca a bloques de 4 KB: con solo una
            // linea de fps cada 5 s, lo que se lee con adb va minutos por
            // detras (y lo que haya en el bufer se pierde si el proceso muere).
            a.add("--log_flush_interval=2");
        }

        argumentosDriver(ctx, sonda, nombreDriver, a);
        return a;
    }

    /**
     * El driver de GPU y la sonda: igual con los dos motores (el nativo lleva
     * el mismo tools/parche_turnip.py en su SDK).
     */
    void argumentosDriver(Context ctx, boolean sonda, String nombreDriver, List<String> a) {
        // Turnip (tools/parche_turnip.py).
        a.add("--android_native_lib_dir=" + ctx.getApplicationInfo().nativeLibraryDir);
        a.add("--android_tmp_dir=" + ctx.getCacheDir().getAbsolutePath());
        Drivers.Driver d = "sistema".equals(nombreDriver) ? null : Drivers.buscar(ctx, nombreDriver);
        if (d != null) {
            a.add("--android_gpu_driver_dir=" + d.carpeta.getAbsolutePath() + "/");
            a.add("--android_gpu_driver_name=" + d.libreria);
        }
        // Con el driver del sistema, false aunque la preferencia diga true: el
        // SDK lo aplica en cada arranque y es un ajuste de TODO el movil que
        // persiste, asi que false es lo que devuelve la GPU a su gobernador si
        // una partida anterior con Turnip lo dejo puesto.
        // Y solo con Turnip: otro driver propio (PanVK en un Mali) no tiene KGSL.
        a.add("--android_gpu_turbo=" + (d != null && d.esTurnip() && turbo()));

        if (sonda) {
            a.add("--nfsmw_sonda");
            a.add("--nfsmw_informe_sonda=" + informeSonda(ctx).getAbsolutePath());
        }
    }
}
