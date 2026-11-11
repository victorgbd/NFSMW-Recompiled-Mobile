package io.github.nfsmwrecomp;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.net.Inet4Address;
import java.net.InetAddress;
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
    /** El movil declara Vulkan 1.1 por hardware (lo que pedia el manifiesto). */
    private final boolean conVulkan;

    Ajustes(Context ctx) {
        p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        conVulkan = ctx.getPackageManager().hasSystemFeature(
                android.content.pm.PackageManager.FEATURE_VULKAN_HARDWARE_VERSION, 0x401000);
    }

    /** Mando tactil: el movil vibra cuando el juego hace vibrar el mando. */
    boolean vibracion() { return p.getBoolean("vibracion", true); }
    void vibracion(boolean v) { p.edit().putBoolean("vibracion", v).apply(); }

    /**
     * Mando tactil, en la disposicion de conducir (carreras que no son de
     * aceleracion, conduccion libre, persecuciones): con que se gira. El stick
     * izquierdo; dos flechas, como las de carril de las de aceleracion, que lo
     * llevan entero a un lado; o inclinando el movil, como un volante, con la
     * sensibilidad de SENSIBILIDAD_MIN a SENSIBILIDAD_MAX. Inclinar sigue en
     * su clave de antes (girar_inclinando), para no perderlo al actualizar.
     */
    static final String GIRO_STICK = "stick";
    static final String GIRO_FLECHAS = "flechas";
    static final String GIRO_INCLINAR = "inclinar";
    String modoGiro() {
        if (girarInclinando()) return GIRO_INCLINAR;
        return GIRO_FLECHAS.equals(p.getString("giro_carrera", GIRO_STICK)) ? GIRO_FLECHAS : GIRO_STICK;
    }
    void modoGiro(String v) {
        p.edit().putBoolean("girar_inclinando", GIRO_INCLINAR.equals(v)).putString("giro_carrera", v).apply();
    }
    boolean girarInclinando() { return p.getBoolean("girar_inclinando", false); }
    boolean girarConFlechas() { return GIRO_FLECHAS.equals(modoGiro()); }
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

    /**
     * Mando tactil: en los menus y en la pausa, sin mando, con el dedo
     * (TouchControllerView, "LOS MENUS CON EL DEDO"). Solo con el motor nativo,
     * que sabe en que parte del juego se esta y que hay en el menu. Deslizar va
     * como al pasar paginas (a la izquierda, la opcion de la derecha); invertido,
     * deslizar a la derecha va a la de la derecha.
     */
    boolean menusTactiles() { return p.getBoolean("menus_tactiles", true); }
    void menusTactiles(boolean v) { p.edit().putBoolean("menus_tactiles", v).apply(); }
    boolean deslizarInvertido() { return false; }

    /**
     * La ISO elegida: una URI content:// del selector de Android o, si se
     * encontro con el acceso a todos los archivos (BuscadorIsos), su ruta. El
     * juego recibe las dos tal cual en --game_data_root.
     */
    String iso() { return p.getString("iso", null); }
    void iso(String uri) { p.edit().putString("iso", uri).apply(); }

    /** La ISO como URI, para abrirla con el ContentResolver (una ruta pasa a file://). */
    static Uri uriIso(String iso) {
        return iso.startsWith("/") ? Uri.fromFile(new File(iso)) : Uri.parse(iso);
    }

    String nombreIso() { return p.getString("iso_nombre", null); }
    void nombreIso(String n) { p.edit().putString("iso_nombre", n).apply(); }

    /**
     * Lo que IsoXex ha leido de la ISO elegida. xexIso: el SHA-256 de su
     * default.xex, que dice de que edicion del juego es (y cual de las del APK
     * cargar, ver Ediciones); null si aun no se ha mirado y "" si no se pudo leer. isoFaltan:
     * los bytes que le faltan al fichero, 0 si esta entero.
     */
    String xexIso() { return p.getString("iso_xex", null); }
    long isoFaltan() { return p.getLong("iso_faltan", 0); }
    void isoMirada(String sha, long faltan) {
        p.edit().putString("iso_xex", sha).putLong("iso_faltan", faltan).apply();
    }
    void isoSinMirar() { p.edit().remove("iso_xex").remove("iso_faltan").apply(); }

    /** La edicion con la que se juega la ISO elegida (ver Ediciones). */
    Ediciones.Edicion edicion() { return Ediciones.paraJugar(xexIso()); }

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
     * Direccion del servidor de EA de la LAN elegido en la lista de Multijugador
     * (una IPv4 o un nombre de equipo, ver direccionValida), un PC u otro movil de
     * anfitrion (proyecto nfsmw-server-lan, ver tools/red/README.md). Vacia, el
     * juego va como siempre; con una, Xbox Live del juego va a ese servidor: ver
     * MotorNativo.argumentos y docs/diario/red-y-privilegios.md.
     */
    String servidorLan() { return p.getString("servidor_lan", ""); }
    void servidorLan(String v) { p.edit().putString("servidor_lan", v).apply(); }

    /** Un servidor LAN guardado: su direccion y, si se le puso, un nombre ("Casa", "PC de Juan"). */
    static final class Servidor {
        final String nombre;
        final String direccion;

        Servidor(String nombre, String direccion) {
            this.nombre = nombre == null ? "" : nombre.trim();
            this.direccion = direccion;
        }
    }

    /**
     * Servidores LAN guardados por el usuario (la lista de Multijugador), en el
     * orden en que se guardaron. En JSON: [{"nombre": "...", "direccion": "..."}]
     * ("ip" en las primeras pruebas). Los de antes (solo IPs, separadas por
     * comas) se pasan la primera vez. La elegida (servidorLan) siempre esta en la
     * lista: la de una version sin lista no se pierde.
     */
    List<Servidor> servidores() {
        List<Servidor> l = new ArrayList<>();
        String json = p.getString("servidores", null);
        if (json == null) {
            for (String ip : p.getString("servidores_guardados", "").split(",")) {
                if (ipValida(ip)) {
                    l.add(new Servidor("", ip));
                }
            }
        } else {
            try {
                JSONArray a = new JSONArray(json);
                for (int i = 0; i < a.length(); i++) {
                    JSONObject o = a.getJSONObject(i);
                    String d = o.optString("direccion", o.optString("ip"));
                    if (direccionValida(d) && indiceServidor(l, d) < 0) {
                        l.add(new Servidor(o.optString("nombre"), d));
                    }
                }
            } catch (JSONException ignored) {
            }
        }
        String elegida = servidorLan();
        if (direccionValida(elegida) && indiceServidor(l, elegida) < 0) {
            l.add(0, new Servidor("", elegida));
        }
        return l;
    }

    void servidores(List<Servidor> l) {
        JSONArray a = new JSONArray();
        try {
            for (Servidor s : l) {
                a.put(new JSONObject().put("nombre", s.nombre).put("direccion", s.direccion));
            }
        } catch (JSONException ignored) {
        }
        p.edit().putString("servidores", a.toString()).remove("servidores_guardados").apply();
    }

    /** Donde esta esa direccion en la lista (sin mirar mayusculas), o -1. */
    static int indiceServidor(List<Servidor> l, String direccion) {
        for (int i = 0; i < l.size(); i++) {
            if (l.get(i).direccion.equalsIgnoreCase(direccion)) {
                return i;
            }
        }
        return -1;
    }

    /**
     * Este movil es el anfitrion: corre el servidor de EA (ServidorLanService) y
     * el juego va contra el, por la IP de este movil en la Wi-Fi o en su punto de
     * acceso. Manda sobre servidorLan(), que se queda guardada para cuando se quite.
     */
    boolean anfitrionLan() { return p.getBoolean("anfitrion_lan", false); }
    void anfitrionLan(boolean v) { p.edit().putBoolean("anfitrion_lan", v).apply(); }

    /**
     * El servidor contra el que va el juego: este movil si es el anfitrion, o la
     * direccion elegida (IP o nombre: resolverIpv4). Vacio = ninguno.
     */
    String servidorEfectivo() {
        if (anfitrionLan()) {
            String ip = ServidorEa.ipLocal();
            return ip != null ? ip : "";
        }
        return servidorLan();
    }

    static boolean ipValida(String ip) {
        String[] partes = ip.split("\\.", -1);
        if (partes.length != 4) {
            return false;
        }
        for (String parte : partes) {
            if (parte.isEmpty() || parte.length() > 3 || !parte.matches("[0-9]+")
                    || Integer.parseInt(parte) > 255) {
                return false;
            }
        }
        return true;
    }

    /**
     * Una IPv4 o un nombre de equipo ("mi-pc", "servidor.lan"): letras, numeros
     * y guiones por tramo, separados por puntos. Lo que solo tiene numeros y
     * puntos tiene que ser una IP entera: "10.0.0" no es un nombre.
     */
    static boolean direccionValida(String d) {
        if (d == null || d.isEmpty() || d.length() > 253) {
            return false;
        }
        if (d.matches("[0-9.]+")) {
            return ipValida(d);
        }
        return d.matches("(?i)[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?(\\.[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?)*");
    }

    /**
     * La IPv4 de una direccion: ella misma si ya es una IP, o la que da la red
     * para ese nombre; null si no se encuentra. El juego solo entiende IPs (su
     * red_dns contesta con ella a todos sus nombres). Bloquea con un nombre:
     * llamarlo fuera del hilo principal.
     */
    static String resolverIpv4(String direccion) {
        if (ipValida(direccion)) {
            return direccion;
        }
        try {
            for (InetAddress a : InetAddress.getAllByName(direccion)) {
                if (a instanceof Inet4Address) {
                    return a.getHostAddress();
                }
            }
        } catch (IOException | SecurityException ignored) {
        }
        return null;
    }

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
     * Pantalla ancha (prototipo, solo el motor nativo): el juego dibuja a la
     * proporcion de la pantalla (1600x720 en un 20:9) y la camara ve mas por
     * los lados, sin deformar. GameActivity pasa la proporcion
     * (nfsmw_proporcion_pantalla) y llena la pantalla. El HUD y los menus,
     * de momento, se estiran.
     */
    boolean panoramico() { return p.getBoolean("panoramico", false); }
    void panoramico(boolean v) { p.edit().putBoolean("panoramico", v).apply(); }

    /** Si la partida va panoramica: la opcion, con el motor nativo y su renderizador. */
    boolean panoramicoEfectivo() { return MotorNativo.ACTIVO && !compatibilidad() && panoramico(); }

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
     * 3,53 GHz. Con un solo prime (8 Gen 2 y 3, 865, 7 Gen x) el hilo
     * principal se lo queda y el de la GPU baja al escalon siguiente.
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
     * nfsmw_nativo_api: vulkan, o gles (OpenGL ES, en obras: de momento el
     * juego corre y se ve un color de prueba; docs/motor-nativo-gles.md).
     */
    static final String API_VULKAN = "vulkan";
    static final String API_GLES = "gles";
    // Por defecto Vulkan, salvo en los moviles sin Vulkan 1.1: ahi OpenGL ES.
    String apiGrafica() { return p.getString("api_grafica", conVulkan ? API_VULKAN : API_GLES); }
    boolean conVulkan() { return conVulkan; }
    void apiGrafica(String v) { p.edit().putString("api_grafica", v).apply(); }

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
     * con deteccion de bordes y nitidez), "cas" (AMD CAS: solo nitidez) o "gsr"
     * (Snapdragon GSR 1: escalado en una sola pasada) o "gsr2" (Snapdragon GSR 2:
     * escalado temporal del renderizador nativo en Vulkan, --nfsmw_gsr2). Los
     * dos de Snapdragon solo en el motor nativo: el de compatibilidad usa FSR.
     */
    String escalado() { return p.getString("escalado", "bilinear"); }

    /**
     * Generacion de fotogramas de Lossless Scaling (motor nativo, Vulkan): 0 =
     * apagada, 2/3/4 = fotogramas mostrados por cada uno del juego. Solo si el
     * usuario ha copiado su Lossless.dll (dllLsfg): no se incluye, es de pago.
     */
    int lsfg() { return p.getInt("lsfg", 0); }
    void lsfg(int v) { p.edit().putInt("lsfg", v).apply(); }

    /** Donde se guarda la copia del Lossless.dll que elige el usuario. */
    static java.io.File dllLsfg(Context ctx) {
        return new java.io.File(new java.io.File(ctx.getFilesDir(), "lsfg"), "Lossless.dll");
    }
    void escalado(String v) { p.edit().putString("escalado", v).apply(); }

    /** anisotropic_override del SDK: 0 sin filtro, 2 = 2x, 3 = 4x, 5 = 16x. */
    int anisotropico() { return p.getInt("anisotropico", 3); }
    void anisotropico(int v) { p.edit().putInt("anisotropico", v).apply(); }

    /**
     * La primera vez, en una GPU debil (Gpu.debil), unos ajustes que la hagan
     * jugable: 1024x576, 30 fps (a ritmo fijo se siente mas fluido que
     * saltando entre 20 y 30), sin suavizado, sombras de cerca, un solo reflejo del coche y sin el del asfalto, y el bufer de la
     * pantalla a 720p. Solo toca lo que el usuario no haya elegido nunca, y
     * lo hace una vez: despues manda lo que ponga en la pantalla de inicio.
     */
    void aplicarPerfilGpuDebil(Context ctx) {
        if (p.getBoolean("perfil_gpu_debil", false) || Gpu.nombre(ctx).isEmpty()) {
            return;
        }
        SharedPreferences.Editor e = p.edit();
        if (Gpu.debil(ctx)) {
            if (!p.contains("resolucion_interna")) e.putString("resolucion_interna", "1024x576");
            if (!p.contains("limite_fps")) e.putInt("limite_fps", 30);
            if (!p.contains("antialiasing")) e.putString("antialiasing", AA_NO);
            if (!p.contains("resolucion")) e.putString("resolucion", RES_720P);
            if (!p.contains("anisotropico")) e.putInt("anisotropico", 2);
            if (!p.contains("sombras_corte")) e.putInt("sombras_corte", 200);
            if (!p.contains("reflejos_coche")) e.putInt("reflejos_coche", 1);
            if (!p.contains("reflejo_asfalto")) e.putBoolean("reflejo_asfalto", false);
        }
        e.putBoolean("perfil_gpu_debil", true).apply();
    }

    /**
     * Preajuste "Ahorro de bateria" (motor nativo): lo que mas trabajo quita sin
     * romper la imagen. 30 fps a ritmo fijo, 1024x576 escalado con FSR, sin
     * suavizado ni anisotropico, sombras rapidas y de cerca,
     * y un solo reflejo del coche. El reflejo del asfalto se queda: el juego ya
     * solo lo dibuja cuando se ve agua.
     */
    void aplicarAhorro() {
        guardarMisAjustes();
        p.edit()
                .putInt("limite_fps", 30)
                .putString("resolucion_interna", "1024x576")
                .putString("escalado", "fsr")
                .putString("antialiasing", AA_NO)
                .putInt("anisotropico", 0)
                .putString("calidad_sombras", SOMBRAS_RAPIDA)
                .putInt("sombras_escala", 100)
                .putInt("sombras_corte", 200)
                .putInt("reflejos_coche", 1)
                .commit();
        p.edit().putString("huella_preajuste", fotoGraficos().toString()).apply();
    }

    /**
     * Los ajustes graficos de fabrica: los de una instalacion nueva en este
     * movil, con el perfil de GPU debil si le toca (aplicarPerfilGpuDebil).
     */
    void aplicarFabrica(Context ctx) {
        guardarMisAjustes();
        p.edit()
                .remove("limite_fps").remove("resolucion_interna").remove("escalado")
                .remove("antialiasing").remove("sin_msaa").remove("anisotropico")
                .remove("calidad_sombras").remove("sombras_escala").remove("sombras_cada")
                .remove("sombras_corte").remove("reflejos_coche").remove("reflejo_asfalto")
                .remove("perfil_gpu_debil")
                .commit();
        aplicarPerfilGpuDebil(ctx);
        p.edit().putString("huella_preajuste", fotoGraficos().toString()).apply();
    }

    /** Los ajustes graficos que cambian los preajustes (y "sombras_cada", que ya no se usa). */
    private static final String[] CLAVES_GRAFICOS = {
            "limite_fps", "resolucion_interna", "escalado", "antialiasing", "sin_msaa", "anisotropico",
            "calidad_sombras", "sombras_escala", "sombras_cada", "sombras_corte", "reflejos_coche",
            "reflejo_asfalto", "perfil_gpu_debil"};

    /** Los valores guardados de CLAVES_GRAFICOS, con su tipo (las que no estan, no salen). */
    private JSONObject fotoGraficos() {
        JSONObject foto = new JSONObject();
        java.util.Map<String, ?> todo = p.getAll();
        try {
            for (String clave : CLAVES_GRAFICOS) {
                Object v = todo.get(clave);
                if (v instanceof Boolean) foto.put(clave, new JSONArray().put("b").put(v));
                else if (v instanceof Integer) foto.put(clave, new JSONArray().put("i").put(v));
                else if (v instanceof String) foto.put(clave, new JSONArray().put("s").put(v));
            }
        } catch (JSONException ignorada) {
            // put solo falla con numeros no finitos
        }
        return foto;
    }

    /**
     * Preajuste "Mis ajustes": antes de aplicar otro preajuste se guardan los
     * ajustes graficos del usuario, para poder volver a ellos. Si no los ha
     * tocado desde el ultimo preajuste (siguen siendo los de ese preajuste),
     * no se guardan: asi "Ahorro" y luego "Fabrica" no pisan los suyos.
     */
    private void guardarMisAjustes() {
        String ahora = fotoGraficos().toString();
        if (!ahora.equals(p.getString("huella_preajuste", null))) {
            p.edit().putString("mis_ajustes", ahora).commit();
        }
    }

    boolean hayMisAjustes() { return p.contains("mis_ajustes"); }

    /** Vuelve a los ajustes graficos guardados por guardarMisAjustes. */
    void aplicarMisAjustes() {
        String texto = p.getString("mis_ajustes", null);
        if (texto == null) {
            return;
        }
        try {
            JSONObject foto = new JSONObject(texto);
            SharedPreferences.Editor e = p.edit();
            for (String clave : CLAVES_GRAFICOS) {
                JSONArray v = foto.optJSONArray(clave);
                if (v == null) {
                    e.remove(clave);
                } else if ("b".equals(v.getString(0))) {
                    e.putBoolean(clave, v.getBoolean(1));
                } else if ("i".equals(v.getString(0))) {
                    e.putInt(clave, v.getInt(1));
                } else {
                    e.putString(clave, v.getString(1));
                }
            }
            e.commit();
        } catch (JSONException ignorada) {
            // una foto rota no cambia nada
        }
    }

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
        // El SDK de Xenos no tiene GSR: lo mas parecido es FSR.
        a.add("--present_effect=" + (escalado().startsWith("gsr") ? "fsr" : escalado()));
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
        // app/generated-android/edicion.json, via build.gradle.kts (Ediciones).
        a.add("--user_language=" + edicion().idioma);
        a.add("--user_country=" + edicion().pais);
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
