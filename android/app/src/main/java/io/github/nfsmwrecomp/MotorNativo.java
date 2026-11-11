package io.github.nfsmwrecomp;

import android.content.Context;
import android.content.SharedPreferences;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;

/**
 * Lo que cambia en la app cuando el APK se compila con el motor nativo
 * (-Pnfsmw.motor=nativo): el renderizador de nfsmw-android en vez de la GPU de
 * Xbox 360 imitada del SDK. Ver docs/motor-nativo.md.
 *
 * Es otro programa nativo, con otros ajustes y otras carpetas:
 *
 *   - Sus datos van en almacenamiento interno (files/nativo), no en
 *     Android/data: ahi estan nfsmw.toml, la biblioteca de shaders, la cache de
 *     pipelines y las partidas, y el almacenamiento compartido es mas lento.
 *   - nfsmw.toml y nfsmw_shaders.nfsp viajan en el APK (assets/nativo) y se
 *     copian ahi antes de cargar las librerias. La biblioteca de shaders sale
 *     de TU copia del juego al compilar (tools/android/preparar_nativo.py):
 *     como libmain.so, es una razon mas para no compartir el APK.
 *   - Los ajustes de la pantalla de inicio se traducen a sus cvars.
 */
final class MotorNativo {

    /** El APK se compilo con el motor nativo. */
    static final boolean ACTIVO = "nativo".equals(BuildConfig.MOTOR);

    private static final String TAG = "nfsmw";
    private static final String PREFS = "motor_nativo";
    private static final String AJUSTES = "nfsmw.toml";
    private static final String SHADERS = "nfsmw_shaders.nfsp";

    private MotorNativo() {
    }

    /** nfsmw.toml, la biblioteca de shaders, las caches y las partidas. */
    static File carpetaUsuario(Context ctx) {
        return new File(ctx.getFilesDir(), "nativo/usuario");
    }

    static File carpetaCache(Context ctx) {
        return new File(ctx.getFilesDir(), "nativo/cache");
    }

    /**
     * Antes de cargar las librerias nativas, en el proceso del juego: las
     * carpetas, la variable que le dice al motor cual es "la carpeta del
     * ejecutable" (en Android seria /system/bin), sus ajustes y sus shaders.
     */
    static void preparar(Context ctx, Ediciones.Edicion edicion) {
        File usuario = carpetaUsuario(ctx);
        boolean nueva = !usuario.isDirectory();
        //noinspection ResultOfMethodCallIgnored
        usuario.mkdirs();
        //noinspection ResultOfMethodCallIgnored
        carpetaCache(ctx).mkdirs();
        if (nueva) {
            traerPartidas(ctx, usuario);
        }
        try {
            Os.setenv("REX_APP_FOLDER", usuario.getAbsolutePath(), true);
        } catch (ErrnoException e) {
            Log.w(TAG, "REX_APP_FOLDER", e);
        }
        instalarAjustes(ctx, usuario);
        instalarShaders(ctx, usuario, edicion);
    }

    /**
     * La primera vez: lo que hubiera guardado el motor de Xenos (perfil y
     * partidas, en Android/data/.../files/datos) se copia a la carpeta del
     * nativo. Los originales no se tocan.
     */
    private static void traerPartidas(Context ctx, File usuario) {
        File antes = Ajustes.carpetaDatos(ctx);
        try {
            copiarArbol(antes, usuario, 0);
        } catch (IOException e) {
            Log.w(TAG, "No se pudieron traer las partidas de " + antes, e);
        }
    }

    private static void copiarArbol(File origen, File destino, int profundidad) throws IOException {
        File[] hijos = origen.listFiles();
        if (hijos == null || profundidad > 16) {
            return;
        }
        for (File h : hijos) {
            // Las caches del otro motor (shaders y pipelines de Xenos) no sirven aqui.
            if (profundidad == 0 && h.getName().startsWith("cache")) {
                continue;
            }
            File d = new File(destino, h.getName());
            if (h.isDirectory()) {
                //noinspection ResultOfMethodCallIgnored
                d.mkdirs();
                copiarArbol(h, d, profundidad + 1);
            } else {
                try (InputStream in = new FileInputStream(h); OutputStream out = new FileOutputStream(d)) {
                    volcar(in, out);
                }
            }
        }
    }

    /**
     * nfsmw.toml: los ajustes del motor que la pantalla de inicio no enseña.
     * Se instala el del APK, y se renueva cuando el APK trae otro, salvo que
     * el del movil se haya cambiado a mano o desde el menu del juego.
     */
    private static void instalarAjustes(Context ctx, File usuario) {
        File destino = new File(usuario, AJUSTES);
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        try {
            byte[] delApk = leer(ctx.getAssets().open("nativo/" + AJUSTES));
            String huellaApk = sha256(delApk);
            if (destino.isFile()) {
                String huellaActual = sha256(leer(new FileInputStream(destino)));
                if (huellaActual.equals(huellaApk)
                        || !huellaActual.equals(p.getString("ajustes_sha256", ""))) {
                    return;
                }
            }
            try (OutputStream out = new FileOutputStream(destino)) {
                out.write(delApk);
            }
            p.edit().putString("ajustes_sha256", huellaApk).apply();
        } catch (IOException e) {
            Log.w(TAG, "No se pudo instalar " + AJUSTES, e);
        }
    }

    /**
     * La biblioteca de shaders de esa edicion (cada una tiene la suya): se
     * copia si falta, si el APK trae otra o si se juega con otra edicion.
     */
    private static void instalarShaders(Context ctx, File usuario, Ediciones.Edicion edicion) {
        File destino = new File(usuario, SHADERS);
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        // Cambia con cada instalacion del APK: basta para saber si hay que mirar.
        long apk = new File(ctx.getApplicationInfo().sourceDir).lastModified();
        if (destino.isFile() && p.getLong("shaders_apk", -1) == apk
                && edicion.id.equals(p.getString("shaders_edicion", edicion.id))) {
            return;
        }
        if (!copiarAsset(ctx, edicion.shaders, usuario, SHADERS)) {
            return;
        }
        // La del backend de OpenGL ES va al lado, si el APK la trae
        // (nativo/<edicion>/nfsmw_shaders_gles.nfsp); si no, se quita una vieja.
        String gles = edicion.shaders.replace(".nfsp", "_gles.nfsp");
        String nombreGles = SHADERS.replace(".nfsp", "_gles.nfsp");
        if (!copiarAsset(ctx, gles, usuario, nombreGles)) {
            //noinspection ResultOfMethodCallIgnored
            new File(usuario, nombreGles).delete();
        }
        // commit y no apply: el proceso del juego muere al salir, y lo
        // pendiente de apply podria no llegar a escribirse.
        p.edit().putLong("shaders_apk", apk).putString("shaders_edicion", edicion.id).commit();
    }

    /**
     * La edicion que se va a jugar trae biblioteca de OpenGL ES (instalada por
     * preparar). Las ediciones preparadas antes del backend de GLES no la
     * traen, y su libmain tampoco conoce nfsmw_nativo_api.
     */
    static boolean glesDisponible(Context ctx) {
        return new File(carpetaUsuario(ctx), SHADERS.replace(".nfsp", "_gles.nfsp")).isFile();
    }

    /** Copia un asset a la carpeta con ese nombre, por un temporal. */
    private static boolean copiarAsset(Context ctx, String asset, File carpeta, String nombre) {
        File temporal = new File(carpeta, nombre + ".tmp");
        try (InputStream in = ctx.getAssets().open(asset);
             OutputStream out = new FileOutputStream(temporal)) {
            volcar(in, out);
        } catch (IOException e) {
            Log.w(TAG, "No se pudo instalar " + nombre, e);
            //noinspection ResultOfMethodCallIgnored
            temporal.delete();
            return false;
        }
        return temporal.renameTo(new File(carpeta, nombre));
    }

    /**
     * Los argumentos del juego (o de la sonda) con el motor nativo. La ISO la
     * añade GameActivity. nombreDriver como en Ajustes.argumentos.
     */
    // Las opciones de la LAN que pone argumentos() con un servidor configurado.
    private static final String[] OPCIONES_RED = {"--grant_user_privileges", "--red_lan",
            "--red_live", "--red_dns", "--red_registro", "--nfsmw_red_sonda"};

    /** Para jugar una vez sin online (SetupActivity: el gamertag lo tiene otro movil). */
    static void quitarRed(List<String> args) {
        args.removeIf(a -> {
            for (String o : OPCIONES_RED) {
                if (a.startsWith(o + "=")) {
                    return true;
                }
            }
            return false;
        });
    }

    static List<String> argumentos(Context ctx, Ajustes ajustes, boolean sonda, String nombreDriver) {
        List<String> a = new ArrayList<>();
        a.add("--user_data_root=" + carpetaUsuario(ctx).getAbsolutePath());
        a.add("--cache_root=" + carpetaCache(ctx).getAbsolutePath());
        // La proporcion la pone el tamano de la vista (GameActivity), no el
        // presentador: que llene el bufer entero, sin barras ni margen.
        a.add("--present_letterbox=false");
        a.add("--present_safe_area_x=100");
        a.add("--present_safe_area_y=100");
        // Generacion de fotogramas de Lossless Scaling, con el Lossless.dll del usuario.
        java.io.File dll = Ajustes.dllLsfg(ctx);
        if (ajustes.lsfg() >= 2 && dll.isFile()) {
            a.add("--nfsmw_lsfg=true");
            a.add("--nfsmw_lsfg_dll=" + dll.getAbsolutePath());
            a.add("--nfsmw_lsfg_multiplicador=" + ajustes.lsfg());
        }
        // GSR 2 lo hace el renderizador nativo y entrega la imagen ya al tamano de
        // la pantalla: el presentador solo la copia.
        if ("gsr2".equals(ajustes.escalado())) {
            a.add("--present_effect=bilinear");
            a.add("--nfsmw_gsr2=true");
        } else {
            a.add("--present_effect=" + ajustes.escalado());
        }

        // Graficos y sonido
        // Solo si no es la de siempre: las libmain de las otras ediciones (app_<edicion>) no
        // conocen esta cvar hasta que se regeneran desde la app PAL.
        if (Ajustes.API_GLES.equals(ajustes.apiGrafica())) {
            a.add("--nfsmw_nativo_api=" + Ajustes.API_GLES);
        }
        a.add("--nfsmw_resolucion_interna=" + ajustes.resolucionInterna());
        a.add("--nfsmw_limite_fps=" + (ajustes.limiteFps() == Ajustes.FPS_SIN_LIMITE
                ? "sin_limite" : String.valueOf(ajustes.limiteFps())));
        // MSAA no hay: la escena se pinta en una pasada. Solo FXAA.
        a.add("--nfsmw_antialiasing="
                + (Ajustes.AA_FXAA.equals(ajustes.antialiasing()) ? "fxaa" : "apagado"));
        a.add("--nfsmw_nativo_anisotropico=" + anisotropico(ajustes.anisotropico()));
        a.add("--nfsmw_tratamiento_visual=" + ajustes.filtroColor());
        a.add("--nfsmw_nativo_sin_desenfoque=" + !ajustes.desenfoque());
        a.add("--nfsmw_posproceso=" + ajustes.filtroImagen());
        String sombras = ajustes.calidadSombras();
        a.add("--nfsmw_nativo_pcf_barato=" + Ajustes.SOMBRAS_RAPIDA.equals(sombras));
        a.add("--nfsmw_sombras_sin_vegetacion=" + !Ajustes.SOMBRAS_XBOX.equals(sombras));
        a.add("--nfsmw_nativo_sombras_escala=" + ajustes.sombrasEscala());
        // Siempre cada fotograma: cada 2 deja el mapa un fotograma por detras del
        // coche y las sombras tiemblan (ya no hay opcion en el lanzador).
        a.add("--nfsmw_sombras_cada=1");
        a.add("--nfsmw_sombras_corte=" + ajustes.sombrasCorte());
        a.add("--nfsmw_cubemap_caras_max=" + ajustes.reflejosCoche());
        a.add("--nfsmw_reflejo_carretera=" + ajustes.reflejoAsfalto());
        a.add("--nfsmw_resplandor_cielo=" + ajustes.resplandorCielo());
        // El volumen del juego, siempre el original: por encima de 100 su
        // limitador lo comprime y los momentos fuertes se oyen aplastados.
        a.add("--audio_ganancia_pct=100");
        a.add("--nfsmw_audio_aaudio=" + ajustes.audioAAudio());
        // Su SDK viene con la vibracion apagada (a los mandos solo les llega cero):
        // con el ajuste, la del juego, en los mandos fisicos y, por la app, en el
        // movil con el mando tactil.
        a.add("--input_vibracion=" + ajustes.vibracion());
        if (ajustes.compatibilidad()) {
            argumentosCompatibilidad(a);
        } else {
            a.add("--nfsmw_renderizador=nativo");
        }

        // Los hilos del anillo y del juego en los nucleos prime (afinidad.cpp).
        if (ajustes.fijarHilos()) {
            a.add("--thread_affinity=auto");
        }
        // Los de la edicion de la ISO, que con varias en el APK puede no ser la principal.
        Ediciones.Edicion edicion = ajustes.edicion();
        a.add("--user_language=" + edicion.idioma);
        a.add("--user_country=" + edicion.pais);
        String gamertag = ajustes.gamertag();
        if (Ajustes.gamertagValido(gamertag) && !gamertag.isEmpty()) {
            a.add("--user_gamertag=" + gamertag);
        }
        // Xbox Live del juego contra el servidor de EA de la LAN: los permisos de
        // Live, la red de verdad, la sesion de Live y todos los nombres (los
        // servidores de EA) a esa IP. Solo en este motor: son opciones de su SDK.
        // Un servidor puesto por nombre sale aqui tal cual; SetupActivity.jugar
        // cambia el red_dns por su IP (el SDK solo entiende IPs).
        // Con este movil de anfitrion, su propia IP de la LAN (no 127.0.0.1: es la
        // que el servidor pone en el registro de la partida para los demas).
        String servidor = ajustes.servidorEfectivo();
        if (Ajustes.direccionValida(servidor)) {
            a.add("--grant_user_privileges=true");
            a.add("--red_lan=true");
            a.add("--red_live=true");
            a.add("--red_dns=" + servidor);
            // Diagnostico del multijugador (solo en el APK con registro): las llamadas
            // de red y la sonda de NetConn/ConnApi al log. Las direcciones de la sonda
            // se traducen a cada edicion con las del resto de la app (preparar_nativo.py).
            if (BuildConfig.REGISTRO) {
                a.add("--red_registro=true");
                a.add("--nfsmw_red_sonda=true");
            }
        }
        if (!BuildConfig.DEBUG && !BuildConfig.REGISTRO) {
            a.add("--log_file=/dev/null");
            a.add("--log_level=off");
        } else {
            a.add("--log_file=" + new File(Ajustes.carpetaLogs(ctx), "nfsmw.log").getAbsolutePath());
            a.add("--log_level=" + (ajustes.registroDetallado() ? "debug" : "info"));
            a.add("--log_flush_interval=2");
        }
        // El driver de GPU (Turnip) y la sonda, como con el motor de Xenos.
        ajustes.argumentosDriver(ctx, sonda, nombreDriver, a);
        return a;
    }

    /**
     * Su modo de compatibilidad (GameOptions.java de nfsmw-android): su
     * emulacion de la GPU, con los ajustes que pide un driver antiguo y sin las
     * funciones del juego reescritas en nativo, que dan por hecho el
     * renderizador nativo.
     */
    private static void argumentosCompatibilidad(List<String> a) {
        a.add("--nfsmw_renderizador=xenos");
        a.add("--vulkan_native_shader_features=false");
        a.add("--render_target_path_vulkan=fbo");
        a.add("--vulkan_require_geometry_shader=false");
        a.add("--vulkan_require_fill_mode_non_solid=false");
        a.add("--async_shader_compilation=false");
        for (String cvar : new String[] {"nfsmw_d3d_registros_nativo", "nfsmw_d3d_marcador",
                "nfsmw_d3d_marcador_registro", "nfsmw_d3d_efectos_nativo", "nfsmw_render_sin_mosaico",
                "nfsmw_material_nativo", "nfsmw_visible_nativo", "nfsmw_matrices_nativo",
                "nfsmw_eview_nativo", "nfsmw_escenario_nativo", "nfsmw_efecto_pasada_nativo",
                "nfsmw_pegamento_nativo"}) {
            a.add("--" + cvar + "=false");
        }
    }

    /** anisotropic_override del SDK (0, 2 = 2x, 3 = 4x, 5 = 16x) a nfsmw_nativo_anisotropico (0, 2, 4, 16). */
    private static int anisotropico(int sdk) {
        switch (sdk) {
            case 2: return 2;
            case 3: return 4;
            case 4: return 8;
            case 5: return 16;
            default: return 0;
        }
    }

    private static byte[] leer(InputStream in) throws IOException {
        try (InputStream entrada = in; ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            volcar(entrada, out);
            return out.toByteArray();
        }
    }

    private static void volcar(InputStream in, OutputStream out) throws IOException {
        byte[] trozo = new byte[1 << 16];
        for (int n; (n = in.read(trozo)) > 0; ) {
            out.write(trozo, 0, n);
        }
    }

    private static String sha256(byte[] datos) {
        try {
            StringBuilder s = new StringBuilder();
            for (byte b : MessageDigest.getInstance("SHA-256").digest(datos)) {
                s.append(String.format("%02x", b));
            }
            return s.toString();
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        }
    }
}
