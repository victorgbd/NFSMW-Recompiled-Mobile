package io.github.nfsmwrecomp;

final class TouchControllerBridge {
    private static volatile boolean nativeAvailable = true;
    // Solo el motor nativo sabe en que parte del juego se esta (nativo_android.cpp).
    private static volatile boolean contextoDisponible = true;

    /** Lo que devuelve contexto(): en que parte del juego se esta. */
    static final int CONTEXTO_DESCONOCIDO = -1;
    static final int CONTEXTO_MENUS = 0;
    static final int CONTEXTO_CONDUCIENDO = 1;
    static final int CONTEXTO_ACELERACION = 2;
    // En el mundo, con el juego en pausa (su menu, un mensaje...).
    static final int CONTEXTO_PAUSA = 3;
    // En el mundo, con una cinematica del motor en marcha (la presentacion de la carrera...).
    static final int CONTEXTO_CINEMATICA = 4;

    /**
     * Lo que devuelven menuTocar() y menuRumbo() (nativo_android.cpp, seccion 10):
     * el tipo en MENU_TIPO y, en una indicacion o un rumbo, el boton (el indice de
     * SDL_GamepadButton) en los 8 bits de abajo.
     */
    static final int MENU_DESCONOCIDO = -1;
    static final int MENU_NADA = 0;
    static final int MENU_INDICACION = 0x100;
    static final int MENU_RESALTADA = 0x200;
    static final int MENU_IR = 0x300;
    static final int MENU_SIN_RUMBO = 0x400;
    // El camino a la opcion tocada ha llegado: ya esta elegida.
    static final int MENU_LLEGADA = 0x500;
    // Pulsar A: en esa pantalla no hay indicacion de A con la que hacerlo.
    static final int MENU_ACEPTAR = 0x600;
    static final int MENU_TIPO = 0xF00;
    // Solo el motor nativo sabe leer el menu.
    private static volatile boolean menusDisponibles = true;

    private TouchControllerBridge() {}

    static native void setState(int buttons,
                                float leftX, float leftY,
                                float rightX, float rightY,
                                float leftTrigger, float rightTrigger);

    static native int contexto();

    /** Diagnostico: lo que se leyo para decidir el contexto, en una linea. */
    static native String contextoDiagnostico();

    /** El juego quieto (o en marcha otra vez) mientras se edita el mando en la partida. */
    static native void pausarJuego(boolean pausa);

    /** Que hay en (u, v) del menu, de 0 a 1 sobre la imagen del juego (MENU_*). */
    static native int menuTocar(float u, float v);

    /** Tras pulsar la cruceta hacia la opcion tocada: el siguiente paso (MENU_*). */
    static native int menuRumbo();

    /** Diagnostico: lo leido del menu y el ultimo toque, en varias lineas. */
    static native String menuDiagnostico();

    /** Si la pantalla gira algo con la cruceta mantenida ("Rotar" al visualizar un coche). */
    static native boolean menuRotar();

    static boolean tryMenuRotar() {
        if (!nativeAvailable || !menusDisponibles) return false;
        try {
            return menuRotar();
        } catch (UnsatisfiedLinkError error) {
            return false;
        }
    }

    /**
     * Si se esta en el menu principal, sin nada encima. Entonces deja en salida, sobre la
     * imagen del juego (0..1): el borde izquierdo del boton Salir, su centro en vertical y lo
     * que mide una unidad del menu en cada eje (nativo_android.cpp, seccion 11).
     */
    static native boolean menuPrincipal(float[] salida);

    static boolean tryMenuPrincipal(float[] salida) {
        if (!nativeAvailable || !menusDisponibles) return false;
        try {
            return menuPrincipal(salida);
        } catch (UnsatisfiedLinkError error) {
            return false;
        }
    }

    /** Las veces que se ha pulsado BACK en un mando fisico desde que arranco el juego. */
    static native int backPulsados();

    /** -1 si la libreria no lo sabe. */
    static int tryBackPulsados() {
        if (!nativeAvailable || !menusDisponibles) return -1;
        try {
            return backPulsados();
        } catch (UnsatisfiedLinkError error) {
            return -1;
        }
    }

    /** Si lo que hay en pantalla es una lista de opciones (una debajo de otra). */
    static native boolean menuEsLista();

    static boolean tryMenuEsLista() {
        if (!nativeAvailable || !menusDisponibles) return false;
        try {
            return menuEsLista();
        } catch (UnsatisfiedLinkError error) {
            return false;
        }
    }

    /** MENU_DESCONOCIDO si la libreria no lo sabe (el motor de Xenos). */
    static int tryMenuTocar(float u, float v) {
        if (!nativeAvailable || !menusDisponibles) return MENU_DESCONOCIDO;
        try {
            return menuTocar(u, v);
        } catch (UnsatisfiedLinkError error) {
            menusDisponibles = false;
            return MENU_DESCONOCIDO;
        }
    }

    static int tryMenuRumbo() {
        if (!nativeAvailable || !menusDisponibles) return MENU_SIN_RUMBO;
        try {
            return menuRumbo();
        } catch (UnsatisfiedLinkError error) {
            menusDisponibles = false;
            return MENU_SIN_RUMBO;
        }
    }

    /** null si la libreria no lo sabe. */
    static String tryMenuDiagnostico() {
        if (!nativeAvailable || !menusDisponibles) return null;
        try {
            return menuDiagnostico();
        } catch (UnsatisfiedLinkError error) {
            menusDisponibles = false;
            return null;
        }
    }

    /** Con el motor de Xenos no esta: el juego sigue mientras se edita. */
    static void tryPausarJuego(boolean pausa) {
        if (!nativeAvailable) return;
        try {
            pausarJuego(pausa);
        } catch (UnsatisfiedLinkError error) {
            // Nada que pausar.
        }
    }

    static void trySetState(int buttons,
                            float leftX, float leftY,
                            float rightX, float rightY,
                            float leftTrigger, float rightTrigger) {
        if (!nativeAvailable) return;
        try {
            setState(buttons, leftX, leftY, rightX, rightY,
                     leftTrigger, rightTrigger);
        } catch (UnsatisfiedLinkError error) {
            nativeAvailable = false;
        }
    }

    /** null si la libreria no lo sabe. */
    static String tryContextoDiagnostico() {
        if (!nativeAvailable || !contextoDisponible) return null;
        try {
            return contextoDiagnostico();
        } catch (UnsatisfiedLinkError error) {
            return null;
        }
    }

    /** CONTEXTO_DESCONOCIDO si la libreria no lo sabe (el motor de Xenos). */
    static int tryContexto() {
        if (!nativeAvailable || !contextoDisponible) return CONTEXTO_DESCONOCIDO;
        try {
            return contexto();
        } catch (UnsatisfiedLinkError error) {
            contextoDisponible = false;
            return CONTEXTO_DESCONOCIDO;
        }
    }
}
