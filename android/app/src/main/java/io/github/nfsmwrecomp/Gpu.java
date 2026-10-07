package io.github.nfsmwrecomp;

import android.content.Context;
import android.content.SharedPreferences;
import android.opengl.EGL14;
import android.opengl.EGLConfig;
import android.opengl.EGLContext;
import android.opengl.EGLDisplay;
import android.opengl.EGLSurface;
import android.opengl.GLES20;
import android.os.Build;
import android.util.Log;

/**
 * La GPU del movil, para los textos de la pantalla de inicio: su nombre segun
 * OpenGL ES (GL_RENDERER: "Adreno (TM) 830", "Mali-G57 MC2", "PowerVR Rogue
 * GE8320"...). Se pregunta con un contexto EGL de 1x1, sin tocar Vulkan ni
 * cargar el juego, y se guarda mientras el sistema sea el mismo
 * (Build.FINGERPRINT): una actualizacion puede cambiar el driver.
 */
final class Gpu {

    enum Familia { ADRENO, MALI, POWERVR, OTRA }

    private static final String TAG = "nfsmw";
    private static final String PREFS = "gpu";

    private static String nombre;

    private Gpu() {
    }

    /** "Adreno 830", "Mali-G57 MC2"...; "" si no se pudo saber. */
    static synchronized String nombre(Context ctx) {
        if (nombre != null) {
            return nombre;
        }
        SharedPreferences p = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        if (Build.FINGERPRINT.equals(p.getString("sistema", null))) {
            nombre = p.getString("nombre", "");
            return nombre;
        }
        nombre = limpiar(preguntarEgl());
        p.edit().putString("sistema", Build.FINGERPRINT).putString("nombre", nombre).apply();
        return nombre;
    }

    static Familia familia(Context ctx) {
        String n = nombre(ctx).toLowerCase(java.util.Locale.ROOT);
        if (n.contains("adreno")) {
            return Familia.ADRENO;
        }
        if (n.contains("mali") || n.contains("immortalis")) {
            return Familia.MALI;
        }
        if (n.contains("powervr")) {
            return Familia.POWERVR;
        }
        return Familia.OTRA;
    }

    /** "Adreno (TM) 830" -> "Adreno 830". */
    private static String limpiar(String s) {
        if (s == null) {
            return "";
        }
        return s.replace("(TM)", "").replace("(tm)", "").replaceAll("\\s+", " ").trim();
    }

    private static String preguntarEgl() {
        EGLDisplay pantalla = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY);
        if (pantalla == EGL14.EGL_NO_DISPLAY) {
            return null;
        }
        int[] version = new int[2];
        if (!EGL14.eglInitialize(pantalla, version, 0, version, 1)) {
            return null;
        }
        EGLContext contexto = EGL14.EGL_NO_CONTEXT;
        EGLSurface superficie = EGL14.EGL_NO_SURFACE;
        try {
            int[] atributos = {
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                EGL14.EGL_NONE
            };
            EGLConfig[] configuracion = new EGLConfig[1];
            int[] cuantas = new int[1];
            if (!EGL14.eglChooseConfig(pantalla, atributos, 0, configuracion, 0, 1, cuantas, 0)
                    || cuantas[0] == 0) {
                return null;
            }
            contexto = EGL14.eglCreateContext(pantalla, configuracion[0], EGL14.EGL_NO_CONTEXT,
                    new int[] {EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE}, 0);
            superficie = EGL14.eglCreatePbufferSurface(pantalla, configuracion[0],
                    new int[] {EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE}, 0);
            if (contexto == EGL14.EGL_NO_CONTEXT || superficie == EGL14.EGL_NO_SURFACE
                    || !EGL14.eglMakeCurrent(pantalla, superficie, superficie, contexto)) {
                return null;
            }
            return GLES20.glGetString(GLES20.GL_RENDERER);
        } catch (RuntimeException e) {
            Log.w(TAG, "No se pudo saber la GPU", e);
            return null;
        } finally {
            EGL14.eglMakeCurrent(pantalla, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE,
                    EGL14.EGL_NO_CONTEXT);
            if (superficie != EGL14.EGL_NO_SURFACE) {
                EGL14.eglDestroySurface(pantalla, superficie);
            }
            if (contexto != EGL14.EGL_NO_CONTEXT) {
                EGL14.eglDestroyContext(pantalla, contexto);
            }
            // Sin eglTerminate: la pantalla EGL por defecto es la misma que usa
            // el render de la interfaz de Android en este proceso.
        }
    }
}
