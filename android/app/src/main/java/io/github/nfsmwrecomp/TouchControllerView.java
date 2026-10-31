package io.github.nfsmwrecomp;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.input.InputManager;
import android.hardware.usb.UsbConstants;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbInterface;
import android.hardware.usb.UsbManager;
import android.os.SystemClock;
import android.os.Build;
import android.util.SparseArray;
import android.view.InputDevice;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * A lightweight multitouch Xbox-style controller layered over the SDL surface.
 *
 * El boton OCULTAR/TACTIL se va a los 4 s de aparecer o de usarse, para no
 * estorbar. Tocar donde esta lo vuelve a sacar (ese toque no lo pulsa).
 *
 * Con un mando fisico (conectado o en uso) se oculta entero, tambien el boton.
 * Entonces cualquier toque lo vuelve a sacar, para poder encender los
 * controles tactiles; usar el mando lo esconde otra vez.
 *
 * EDITOR (EditorTactilActivity, desde la pantalla de inicio, o el engranaje
 * junto a OCULTAR/TACTIL en la partida): la misma vista en modo editor deja
 * mover cada control con el dedo, cambiar su tamano y la opacidad de todos. En
 * la partida, el juego se queda quieto mientras tanto (TouchControllerBridge.
 * pausarJuego) y al terminar se vuelve a jugar.
 *
 * GIRAR CON FLECHAS (Ajustes.modoGiro): en la disposicion de conducir, las
 * flechas de carril de las de aceleracion en lugar del stick izquierdo, que no
 * se ve; cada una lo lleva entero a su lado. Tienen su sitio en esa
 * disposicion, aparte del de aceleracion, y se mueven con el editor.
 *
 * GIRAR INCLINANDO EL MOVIL (Ajustes.girarInclinando): en la disposicion de
 * conducir, el eje X del stick izquierdo sale de lo que se inclina el movil
 * hacia los lados, como un volante (el sensor de gravedad), y el stick no se
 * ve. En aceleracion siguen las flechas de carril, y en los menus nada cambia.
 *
 * TRES DISPOSICIONES: la normal (menus y cargas), la de conducir (carreras que
 * no son de aceleracion, conduccion libre y persecuciones: pedales, camara y
 * retrovisor como en aceleracion, pero con el stick izquierdo para girar, la
 * cruceta y el stick derecho de siempre, no la palanca) y la de las carreras
 * de aceleracion. Cada una con su
 * disposicion de fabrica y lo que se guarde con el editor ("<id>_x", "c_<id>_x"
 * y "a_<id>_x"...). El editor elige cual se edita; desde la partida, la de la
 * parte del juego en que se este. RESTAURAR vuelve a la de fabrica de la que
 * se edita. Se guarda en nfsmw_touch_controller por control, como
 * fraccion de la pantalla ("<id>_x", "<id>_y") y escala ("<id>_s"), asi que
 * vale para cualquier resolucion. Lo que no se toca no se guarda y sigue la
 * disposicion de fabrica. La partida lo lee al arrancar, en su proceso.
 *
 * SEGUN LA PARTE DEL JUEGO: cada CONTEXTO_MS se le pregunta al juego donde
 * esta (TouchControllerBridge.contexto, solo con el motor nativo). En las
 * carreras de aceleracion los mismos controles, en el mismo sitio, cambian de
 * forma: RT es el acelerador y LT el freno (pedales), el stick derecho una
 * palanca de cambios (arriba sube marcha, abajo la baja), RB lleva una camara
 * y LB un retrovisor, y L3 y R3 no estan, porque ahi no hacen nada. El stick
 * izquierdo solo cambia de carril: en su lugar hay dos botones con flecha que
 * lo llevan entero a un lado o al otro. Y la cruceta solo abre la
 * clasificacion (arriba): en su lugar, un boton con el icono de la
 * clasificacion que pulsa arriba. Al volver
 * a los menus o a conducir, los de siempre.
 *
 * LOS MENUS CON EL DEDO (Ajustes.menusTactiles, solo con el motor nativo): en
 * los menus y en la pausa el mando no se ve y la pantalla entera es del dedo.
 * Deslizar pulsa la cruceta, un paso cada PASO_DESLIZAR. Un toque pregunta al
 * juego que hay debajo (TouchControllerBridge.menuTocar, que lo lee de su motor
 * de menus): en una indicacion de abajo ("(B) Atras", "(A) Aceptar") pulsa ese
 * boton; en una flecha de la opcion resaltada, la cruceta hacia ese lado; en otra
 * opcion, la cruceta hacia ella, un paso cada vez (menuRumbo), hasta elegirla.
 * Aceptar es tocar la indicacion de A o la opcion ya elegida (se pidio asi: un toque
 * cualquiera no puede ir aceptando cosas); donde no hay indicacion de A (videos,
 * "Presionar START"), un toque acepta. En un dialogo (Si / No), igual: el primer
 * toque elige la respuesta y el segundo la acepta.
 * En una lista de opciones (menuEsLista) deslizar en horizontal cambia el valor
 * de la resaltada en el sentido del dedo. Dos dedos, B; pellizcar, el zoom (al
 * visualizar un coche: separar los dedos pisa RT, "Acercar zoom", y juntarlos,
 * LT). Las indicaciones de LT y RT se pisan mientras el dedo esta encima. Donde la
 * barra de abajo dice "Rotar" (menuRotar, al visualizar un coche), arrastrar mueve
 * el stick derecho, que es el que gira la camara (en los dos ejes), segun la
 * velocidad del dedo: el coche gira con el dedo, sin pasos. El juego recibe las
 * pulsaciones como de un mando (pulsar), asi que suena y se mueve como siempre.
 * El boton OCULTAR/TACTIL dice ahi MANDO: saca el mando en los menus (y DEDO lo
 * vuelve a quitar). Cada toque deja en files/logs/menus.txt lo que se leyo del
 * menu, para afinarlo. En las cinematicas del juego (CONTEXTO_CINEMATICA: la
 * presentacion de cada carrera, las escenas de la historia) tampoco hay mando, y
 * un toque pulsa A, que las salta.
 */
public final class TouchControllerView extends View {
    private static final int DPAD_UP = 1 << 11;
    private static final int DPAD_DOWN = 1 << 12;
    private static final int DPAD_LEFT = 1 << 13;
    private static final int DPAD_RIGHT = 1 << 14;
    private static final int START = 1 << 6;
    private static final int BACK = 1 << 4;
    private static final int L3 = 1 << 7;
    private static final int R3 = 1 << 8;
    private static final int LB = 1 << 9;
    private static final int RB = 1 << 10;
    private static final int A = 1 << 0;
    private static final int B = 1 << 1;
    private static final int X = 1 << 2;
    private static final int Y = 1 << 3;
    // En una pulsacion de los menus (pulsar), los gatillos: no son botones, van como LT y RT
    // pisados del todo. Son los indices 16 y 17 que da TouchControllerBridge.menuTocar.
    private static final int PULSO_LT = 1 << 16;
    private static final int PULSO_RT = 1 << 17;
    private static final String PREFS = "nfsmw_touch_controller";
    // El nombre con el que venia de otro port. Se borra al arrancar.
    private static final String PREFS_VIEJAS = "skate3_touch_controller";

    // Color de acento #6F7432: botones pulsados y borde del boton HIDE/TOUCH.
    // Cada uso conserva su transparencia.
    private static final int PULSADO = Color.argb(190, 0x6F, 0x74, 0x32);
    private static final int PULSADO_CRUCETA = Color.argb(180, 0x6F, 0x74, 0x32);
    private static final int BORDE_TOGGLE = Color.argb(220, 0x6F, 0x74, 0x32);
    private static final int ACENTO = Color.rgb(0x6F, 0x74, 0x32);
    private static final int RELLENO = Color.argb(105, 12, 12, 15);
    private static final int BORDE = Color.argb(205, 255, 255, 255);
    private static final int FONDO_BOTON = Color.argb(175, 15, 15, 18);
    private static final String PREF_VISIBLE = "visible";
    private static final String PREF_OPACIDAD = "opacidad";
    private static final String PREF_MANDO_EN_MENUS = "mando_en_menus";
    // Lo que dura el boton OCULTAR/TACTIL en pantalla desde que aparece o se usa.
    private static final long TOGGLE_MS = 4000;
    // Editor: pasos y limites del tamano y la opacidad.
    private static final float PASO = .1f;
    private static final float ESCALA_MIN = .5f;
    private static final float ESCALA_MAX = 2f;
    private static final float OPACIDAD_MIN = .2f;

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint label = new Paint(Paint.ANTI_ALIAS_FLAG);
    // El recuadro del control elegido en el editor.
    private final Paint marco = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF recuadroElegido = new RectF();
    // Reutilizado en cada etiqueta: getFontMetrics() sin argumento crea un
    // objeto nuevo en cada llamada, y hay ~17 etiquetas por dibujado.
    private final Paint.FontMetrics metricas = new Paint.FontMetrics();
    // La densidad no cambia mientras vive la vista: se lee una vez.
    private final float density;
    private final List<Control> controls = new ArrayList<>();
    private final SparseArray<Control> pointers = new SparseArray<>();
    private final SharedPreferences preferences;
    private boolean controlsVisible;
    // Hay un mando fisico: conectado, o se ha usado en esta partida.
    private boolean mandoFisico;
    // El boton OCULTAR/TACTIL: a la vista hasta TOGGLE_MS despues de usarlo.
    private boolean toggleVisible = true;
    private final Runnable ocultarToggle = () -> {
        toggleVisible = false;
        invalidate();
    };
    private final InputManager inputManager;
    private final InputManager.InputDeviceListener oyenteMandos =
            new InputManager.InputDeviceListener() {
                @Override
                public void onInputDeviceAdded(int deviceId) {
                    if (esMando(deviceId)) entrarModoMando();
                }

                @Override
                public void onInputDeviceRemoved(int deviceId) {
                    if (mandoFisico && !hayMandoFisico(null)) salirModoMando();
                }

                @Override
                public void onInputDeviceChanged(int deviceId) {
                    revisarMandos(null);
                }
            };
    // Los mandos de Xbox por USB: en la partida se los queda SDL (su driver HIDAPI, que
    // les da la vibracion) y Android deja de verlos como InputDevice. Se siguen por USB.
    private final UsbManager usbManager;
    private final BroadcastReceiver oyenteUsb = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            UsbDevice quitado = null;
            if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(intent.getAction())) {
                quitado = Build.VERSION.SDK_INT >= 33
                        ? intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice.class)
                        : intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
            }
            revisarMandos(quitado);
        }
    };
    private final RectF toggle = new RectF();
    private float lastWidth;
    private float lastHeight;
    // Textos en el idioma de la app.
    private final String textoOcultar;
    private final String textoMostrar;
    private final String textoMando;
    private final String textoGestos;
    private final String textoTamano;
    private final String textoOpacidad;
    private final String textoRestaurar;
    private final String textoCancelar;
    private final String textoListo;
    private final String textoAyuda;
    // El nombre de cada disposicion en el editor, por su indice.
    private final String[] textoDisp = new String[3];

    // --- Editor: EditorTactilActivity, o el engranaje en la partida.
    private boolean editando;
    // En EditorTactilActivity, LISTO y CANCELAR la cierran. Nulo en la partida.
    private final Runnable alTerminar;
    // La vista de la partida (no la de EditorTactilActivity).
    private final boolean enPartida;
    // El engranaje, junto a OCULTAR/TACTIL: abre el editor en la partida.
    private final RectF engranaje = new RectF();
    private final Paint dientes = new Paint(Paint.ANTI_ALIAS_FLAG);
    // Como estaban los controles al abrir el editor en la partida.
    private boolean visiblesAntesDeEditar;
    private Control elegido;
    // El dedo que arrastra, y donde se agarro el control respecto a su centro.
    private int dedoEditor = -1;
    private float agarreX;
    private float agarreY;
    // Opacidad de todos los controles (no del boton ni de la barra del editor).
    private float opacidad;
    private final RectF tamMenos = new RectF();
    private final RectF tamMas = new RectF();
    private final RectF opaMenos = new RectF();
    private final RectF opaMas = new RectF();
    private final RectF restaurar = new RectF();
    private final RectF cancelar = new RectF();
    private final RectF listo = new RectF();
    // Que disposicion se edita: NORMAL o ACELERACION.
    private final RectF disposicion = new RectF();
    private static final int NORMAL = 0;
    private static final int ACELERACION = 1;
    private static final int CONDUCIENDO = 2;
    private static final int DISPOSICIONES = 3;
    // El prefijo de las claves de cada una en las preferencias.
    private static final String[] PREFIJOS = {"", "a_", "c_"};
    private static final String[] NOMBRES_DISP = {"normal", "aceleracion", "conduciendo"};
    // En que disposiciones se ve un control.
    private static final int EN_NORMAL = 1 << NORMAL;
    private static final int EN_ACELERACION = 1 << ACELERACION;
    private static final int EN_CONDUCIENDO = 1 << CONDUCIENDO;
    private static final int EN_TODAS = EN_NORMAL | EN_ACELERACION | EN_CONDUCIENDO;
    private int dispEditada = NORMAL;
    private final RectF fondoAyuda = new RectF();
    private float textoTamX;
    private float textoOpaX;
    private float tamanoAyuda;

    private Stick leftStick;
    private Stick rightStick;
    // Las flechas de carril de las carreras de aceleracion.
    private Flecha carrilIzq;
    private Flecha carrilDer;
    private Trigger leftTrigger;
    private Trigger rightTrigger;
    // Los botones A y B, para que un gesto los pulse y se vean pulsados.
    private ButtonControl botonA;
    private ButtonControl botonB;

    // --- Bloqueo del acelerador (crucero) y gestos (nitro y freno de mano).
    // El pedal se bloquea con el candado de su esquina, al tocarlo: el pedal
    // sigue pisado sin dedo, y se suelta al volver a tocar el candado, al
    // frenar (mientras se frena) o al dejar de conducir (menus, pausa, editor,
    // controles ocultos, app en segundo plano).
    // Lo que hay que arrastrar, en vertical y en una zona vacia, para que
    // cuente como gesto.
    private static final float GESTO_RECORRIDO_DP = 48f;
    private boolean aceleradorBloqueado;
    // Arrastrar hacia arriba deja la nitro puesta; hacia abajo, el freno de
    // mano. Los dos duran mientras el dedo siga en la pantalla.
    private boolean gestoNitro;
    private boolean gestoFrenoMano;
    private int dedoGesto = -1;
    private float gestoX;
    private float gestoY;
    private boolean gestoDecidido;
    // Un dedo a menos de esto (dp) del borde de un control no hace nada: ni
    // gira ni empieza un gesto. Es el margen que hay alrededor de cada boton,
    // para que rozarlo al lado no mueva el coche.
    private static final float BUFFER_CONTROLES_DP = 8f;

    // --- Segun la parte del juego.
    private static final long CONTEXTO_MS = 250;
    private static final int SIN_ICONO = 0;
    private static final int ICONO_CAMARA = 1;
    private static final int ICONO_RETROVISOR = 2;
    private static final int ICONO_POSICIONES = 3;
    // En las carreras, los botones: B el nitro, A el freno de mano y X el
    // Speedbreaker (un reloj). Y no esta: mirar atras ya lo hace LB (retrovisor).
    private static final int ICONO_NITRO = 4;
    private static final int ICONO_FRENO_MANO = 5;
    private static final int ICONO_RELOJ = 6;
    private static final String[] PUESTOS = {"1", "2", "3"};
    // En una carrera de aceleracion: los controles cambian de forma.
    // La disposicion de la parte del juego en que se esta.
    private int dispJuego = NORMAL;
    // El juego en pausa: no se gira inclinando el movil.
    private boolean enPausa;

    // --- Girar con las flechas de carril al conducir (Ajustes.modoGiro).
    private final boolean flechasAlConducir;
    // Al pisar, GIRO_FLECHA_INICIAL; mantenida, a tope en GIRO_FLECHA_MS; al
    // soltar, recto. Ahora a tope de golpe, como el teclado del juego en PC: alli
    // la tecla llega al coche como 1.0 y lo gradual lo pone la fisica, que es la
    // misma que en el 360 (SuspensionRacer::DoHumanSteering, mismas tablas en
    // speed.exe y en el XEX). Antes, 50 % y 300 ms: a tope y a velocidad el coche
    // derrapa de morro, pero eso tambien pasa en PC.
    private static final float GIRO_FLECHA_INICIAL = 1f;
    private static final long GIRO_FLECHA_MS = 250;
    private float giroFlechas;
    private long pasoFlechas;
    private final Runnable rampaFlechas = new Runnable() {
        @Override
        public void run() {
            if (avanzarGiroFlechas()) postOnAnimation(this);
        }
    };

    // La nitro pisa el acelerador mientras dura, se lance con el boton B o con
    // el gesto de subir: al soltarla el pedal vuelve a estar como estaba, y
    // mientras se frena no se pisa (los dos gatillos a la vez ni frenan ni dan
    // marcha atras).
    private boolean nitroPisaAcelerador() {
        return (gestoNitro || (botonB != null && botonB.inUse))
                && formasDeConducir() && !enPausa && !enMenu
                && !(leftTrigger != null && leftTrigger.inUse);
    }

    // --- Girar inclinando el movil.
    private final boolean inclinar;
    // El seno del angulo con el que se gira del todo (Ajustes.anguloGiro, segun
    // la sensibilidad).
    private final float senoGiroCompleto;
    // Por debajo, recto: el pulso de quien sujeta el movil no gira el coche.
    private static final float ZONA_MUERTA_GIRO = .03f;
    private final SensorManager sensores;
    private final Sensor gravedad;
    // Sin sensor de gravedad, el acelerometro con un filtro de paso bajo.
    private final boolean conAcelerometro;
    private final float[] filtrada = new float[2];
    private boolean escuchando;
    // El giro de ahora, de -1 (izquierda) a 1.
    private float giroInclinado;
    private final SensorEventListener oyenteInclinacion = new SensorEventListener() {
        @Override
        public void onSensorChanged(SensorEvent e) {
            float gx = e.values[0];
            float gy = e.values[1];
            if (conAcelerometro) {
                filtrada[0] += (gx - filtrada[0]) * .2f;
                filtrada[1] += (gy - filtrada[1]) * .2f;
                gx = filtrada[0];
                gy = filtrada[1];
            }
            // A la pantalla: el eje X del movil es el de su orientacion natural
            // (vertical); en horizontal, la X de lo que se ve es su Y.
            Display pantalla = getDisplay();
            int rotacion = pantalla == null ? Surface.ROTATION_90 : pantalla.getRotation();
            float sx;
            switch (rotacion) {
                case Surface.ROTATION_90: sx = gy; break;
                case Surface.ROTATION_180: sx = -gx; break;
                case Surface.ROTATION_270: sx = -gy; break;
                default: sx = gx; break;
            }
            // Bajar el lado izquierdo, como al girar un volante a la izquierda,
            // deja la X de la pantalla en negativo: girar a la izquierda. El signo
            // esta comprobado en el movil (el contrario giraba al reves).
            float giro = limitar(sx / (SensorManager.GRAVITY_EARTH * senoGiroCompleto), -1f, 1f);
            giro = Math.abs(giro) < ZONA_MUERTA_GIRO ? 0f
                    : Math.signum(giro) * (Math.abs(giro) - ZONA_MUERTA_GIRO) / (1f - ZONA_MUERTA_GIRO);
            if (Math.abs(giro - giroInclinado) > .005f) {
                giroInclinado = giro;
                if (inclinacionActiva()) sendState();
            }
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {}
    };

    // --- Los menus con el dedo.
    private final boolean menusTactiles;
    private final boolean deslizarInvertido;
    // En un menu del juego o en la pausa (lo dice el contexto; con el motor de
    // Xenos nunca, y los menus van con el mando). Tambien en una cinematica.
    private boolean enMenu;
    // Una cinematica del juego (la presentacion de la carrera...): un toque la salta.
    private boolean enCinematica;
    // En los menus, el mando en vez del dedo (el boton MANDO/DEDO). Se guarda.
    private boolean mandoEnMenus;
    // Donde se ve el juego en la vista: el 16:9 del centro, o toda si se estira.
    private boolean estirar;
    private final RectF imagenJuego = new RectF();
    // El dedo que desliza o toca: donde y cuando empezo, en que eje va (EJE_*),
    // lo ya contado en pasos (en ese eje, desde donde empezo) y cuantos van.
    private static final int SIN_EJE = 0;
    private static final int EJE_X = 1;
    private static final int EJE_Y = 2;
    private int dedoMenu = -1;
    private float menuX0;
    private float menuY0;
    private long menuT0;
    private int ejeMenu;
    private float contadoMenu;
    private int pasosMenu;
    private boolean dosDedos;
    // Deslizando en horizontal sobre una lista de opciones: el valor, en el sentido del dedo.
    private boolean deslizarDirecto;
    // Lo que se pisa mientras dura un dedo (no una pulsacion de PULSO_MS): un gatillo de una
    // indicacion con el dedo encima, o el del pellizco. Va con lo de pulsar en sendState.
    private int mantenido;
    private int gatilloDedo;
    // El pellizco: la distancia entre los dos dedos con la que se cuenta, si ya se ha movido y
    // el gatillo que pisa ahora. Se suelta cuando los dedos se quedan quietos ZOOM_QUIETO_MS.
    private float distanciaPellizco;
    private boolean pellizco;
    private int gatilloPellizco;
    private static final float PELLIZCO_DP = 14f;
    private static final float PELLIZCO_PASO_DP = 4f;
    private static final long ZOOM_QUIETO_MS = 140;
    private final Runnable soltarPellizco = () -> soltarGatillo(true);
    // Arrastrar para girar (menuRotar): el stick derecho, segun la velocidad del dedo (a
    // VELOCIDAD_STICK_DP dp por milisegundo, entero; lo poco, al menos STICK_MINIMO, que la zona
    // muerta del juego no se lo coma), suavizado, y donde y cuando estaba el dedo la ultima vez.
    // Si el dedo se queda quieto ARRASTRE_QUIETO_MS, o se levanta, vuelve al centro poco a poco
    // (FRENADA_*): el coche sigue girando un momento, como empujado, y cada deslizamiento gira mas.
    // ROTAR_CON_EL_DEDO: el coche sigue al dedo (la camara va al reves que el); si girase al
    // reves de lo esperado, cambiarlo.
    private static final boolean ROTAR_CON_EL_DEDO = true;
    private static final float VELOCIDAD_STICK_DP = .3f;
    private static final float STICK_MINIMO = .35f;
    private static final long ARRASTRE_QUIETO_MS = 60;
    private static final long FRENADA_MS = 25;
    private static final float FRENADA = .82f;
    private boolean arrastrar;
    private float stickArrastreX;
    private float stickArrastreY;
    private float arrastreX;
    private float arrastreY;
    private long arrastreT;
    private final Runnable soltarArrastre = new Runnable() {
        @Override
        public void run() {
            stickArrastreX *= FRENADA;
            stickArrastreY *= FRENADA;
            if (Math.abs(stickArrastreX) < .05f && Math.abs(stickArrastreY) < .05f) {
                soltarStick();
                return;
            }
            sendState();
            postDelayed(this, FRENADA_MS);
        }
    };
    // Lo que hay que mover el dedo para que empiece a deslizar, para el primer
    // paso, para cada uno de los siguientes (en dp; o PASO_DESLIZAR_MARCO del
    // ancho del juego, si es mas), y lo que dura como mucho un toque.
    private static final float MOVER_DP = 12f;
    private static final float PRIMER_PASO_DP = 28f;
    private static final float PASO_DESLIZAR_DP = 56f;
    private static final float PASO_DESLIZAR_MARCO = .07f;
    private static final long TOQUE_MS = 450;
    private static final long DOS_DEDOS_MS = 600;
    // Las pulsaciones para el juego, una detras de otra: {mascara, TRAS_*}. Cada
    // una dura PULSO_MS y deja ENTRE_PULSOS_MS sin nada, para que el juego (que
    // mira el mando una vez por fotograma) las vea todas, y por separado.
    private static final long PULSO_MS = 90;
    private static final long ENTRE_PULSOS_MS = 90;
    private static final int MAX_PULSOS = 6;
    private static final int TRAS_NADA = 0;
    // Al acabar, preguntar al juego el siguiente paso hacia la opcion tocada.
    private static final int TRAS_RUMBO = 1;
    private static final int MAX_PASOS_RUMBO = 10;
    private final java.util.ArrayDeque<int[]> pulsos = new java.util.ArrayDeque<>();
    private int pulso;
    private int trasPulso;
    private boolean pulsando;
    private int pasosRumbo;
    private final Runnable siguientePulso = this::siguientePulso;
    private final Runnable soltarPulso = () -> {
        pulso = 0;
        sendState();
        postDelayed(siguientePulso, ENTRE_PULSOS_MS);
    };
    // --- Salir del juego desde el menu principal (nativo_android.cpp, seccion 11): una
    // indicacion mas, "Salir", con el estilo de las del juego. Se ve siempre; se toca solo sin
    // el mando tactil a la vista (con el, su BACK hace lo mismo, como el del mando fisico).
    // Toca (o BACK) una vez y pide confirmar; otra antes de SALIR_CONFIRMAR_MS, sale.
    private final float[] menuPrincipal = new float[4];
    private boolean salirVisible;
    private final RectF salirCaja = new RectF();
    private long salirConfirmarHasta;
    private int backVistos = -1;
    // Si el BACK del mando tactil estaba pisado en el ultimo sendState.
    private boolean backTactil;
    private boolean salirAnotado;
    // Lo que mide una unidad del menu del juego en cada eje, en pixeles: el menu va
    // estirado a lo ancho, y el boton con el.
    private float salirUx, salirUy;
    private final Paint salirTexto = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint salirPintura = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path salirFlecha = new Path();
    private final String textoSalir;
    private final String textoSalirSeguro;
    private static final long SALIR_CONFIRMAR_MS = 3000;
    // Medidas de la indicacion "(A) Aceptar" del juego, en unidades del menu (una captura del
    // RedMagic, 2026-10-29): la barra rayada, el circulo del icono y su trazo, el hueco hasta el
    // texto, la altura de las mayusculas, lo que ocupa "Aceptar" y lo que sobra de barra.
    private static final float SALIR_ALTO = 23.7f;
    private static final float SALIR_MARGEN = 4.4f;
    private static final float SALIR_RADIO = 10f;
    private static final float SALIR_TRAZO = 1.6f;
    private static final float SALIR_HUECO = 7.3f;
    private static final float SALIR_MAYUSCULA = 9.8f;
    private static final float SALIR_ANCHO_ACEPTAR = 58.4f;
    private static final float SALIR_COLA = 18f;
    // Las rayas de la barra: cada RAYA_PASO unidades, RAYA_ANCHO de ancho, inclinadas a la
    // derecha como las del juego.
    private static final float SALIR_RAYA_PASO = 14f;
    private static final float SALIR_RAYA_ANCHO = 7f;
    private static final int SALIR_CREMA = Color.rgb(0xF0, 0xF0, 0xD3);
    private static final int SALIR_FONDO = Color.argb(140, 0, 0, 0);
    private static final int SALIR_RAYA = Color.argb(30, 255, 255, 255);
    private final Runnable finConfirmarSalir = () -> {
        salirConfirmarHasta = 0;
        colocarSalir();
        invalidate();
    };

    // Diagnostico de los toques: files/logs/menus.txt.
    private boolean menusAnotados;
    private static final long MAX_DIAGNOSTICO_MENUS = 1 << 20;

    private final Runnable mirarContexto = new Runnable() {
        @Override
        public void run() {
            int contexto = TouchControllerBridge.tryContexto();
            // En pausa (su menu, un mensaje...), la de conducir, que tiene la
            // cruceta para recorrer el menu (tambien si se pauso una carrera de
            // aceleracion, cuya disposicion no la tiene), pero sin girar
            // inclinando el movil: moveria las opciones.
            boolean pausa = contexto == TouchControllerBridge.CONTEXTO_PAUSA;
            // En una cinematica, la disposicion de antes: es un momento, y al acabar
            // la carrera empieza con la que tocaba.
            boolean cinematica = contexto == TouchControllerBridge.CONTEXTO_CINEMATICA;
            int ahora = cinematica ? dispJuego
                    : contexto == TouchControllerBridge.CONTEXTO_ACELERACION ? ACELERACION
                    : contexto == TouchControllerBridge.CONTEXTO_CONDUCIENDO || pausa ? CONDUCIENDO
                    : NORMAL;
            if (pausa != enPausa) {
                enPausa = pausa;
                // El stick, al centro en cuanto se pausa.
                sendState();
            }
            // Los menus y la pausa, con el dedo. Al entrar y al salir, todo suelto
            // (un pedal que se pisaba, un deslizamiento a medias), y OCULTAR/TACTIL
            // cambia de texto y de ancho. Las cinematicas, igual: sin mando, y un
            // toque la salta.
            boolean menu = contexto == TouchControllerBridge.CONTEXTO_MENUS || pausa || cinematica;
            if (menu != enMenu || cinematica != enCinematica) {
                enMenu = menu;
                enCinematica = cinematica;
                cancelarMenu();
                rehacer();
            }
            if (ahora != dispJuego) {
                dispJuego = ahora;
                // Todo suelto: un dedo que estaba en L3, que ahora no esta, o
                // en el stick, que ahora es la palanca, no se queda pulsado.
                // En los menus (fuera del mundo) tampoco queda el pedal
                // bloqueado: ahi no se ve, y el juego ya no es el mismo.
                if (ahora == NORMAL) soltarCruceroYGestos();
                clearInput();
                // Mientras se edita, la disposicion la elige el editor.
                if (!editando) usarDisposicion(dispJuego);
            }
            mirarSalir(contexto == TouchControllerBridge.CONTEXTO_MENUS);
            anotarDiagnostico();
            postDelayed(this, CONTEXTO_MS);
        }
    };
    // Diagnostico del contexto: files/logs/contexto.txt, una linea por cambio.
    private String ultimoDiagnostico;

    private void anotarDiagnostico() {
        String linea = TouchControllerBridge.tryContextoDiagnostico();
        if (linea == null || linea.equals(ultimoDiagnostico)) return;
        boolean primera = ultimoDiagnostico == null;
        ultimoDiagnostico = linea;
        java.io.File carpeta = getContext().getExternalFilesDir("logs");
        if (carpeta == null) return;
        // La primera de la partida empieza el fichero de nuevo.
        try (java.io.FileWriter f = new java.io.FileWriter(
                new java.io.File(carpeta, "contexto.txt"), !primera)) {
            f.write(new java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.ROOT)
                    .format(new java.util.Date()) + " ctx=" + NOMBRES_DISP[dispJuego] + (enPausa ? "+pausa" : "")
                    + " " + linea + "\n");
        } catch (java.io.IOException ignorada) {
            // Solo es diagnostico.
        }
    }
    // Reutilizados al dibujar los pedales y los iconos.
    private final RectF pisado = new RectF();
    private final RectF trazo = new RectF();

    /** El mando de la partida. */
    public TouchControllerView(Context context) {
        this(context, null);
    }

    /**
     * Con alTerminar, el editor: todos los controles a la vista y editandose,
     * sin mirar los mandos fisicos y sin hablar con el juego (en la pantalla
     * de inicio no hay librerias nativas).
     */
    public TouchControllerView(Context context, Runnable alTerminar) {
        super(context);
        this.alTerminar = alTerminar;
        editando = alTerminar != null;
        enPartida = alTerminar == null;
        dientes.setStyle(Paint.Style.STROKE);
        Ajustes ajustes = new Ajustes(context);
        inclinar = ajustes.girarInclinando();
        flechasAlConducir = ajustes.girarConFlechas();
        senoGiroCompleto = (float) Math.sin(Math.toRadians(Ajustes.anguloGiro(ajustes.sensibilidadGiro())));
        sensores = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        Sensor sensor = sensores == null ? null : sensores.getDefaultSensor(Sensor.TYPE_GRAVITY);
        conAcelerometro = sensor == null && sensores != null;
        gravedad = sensor != null ? sensor
                : sensores == null ? null : sensores.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        density = getResources().getDisplayMetrics().density;
        setWillNotDraw(false);
        setFocusable(false);
        fill.setStyle(Paint.Style.FILL);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(dp(2));
        marco.setStyle(Paint.Style.STROKE);
        marco.setStrokeWidth(dp(3));
        marco.setColor(ACENTO);
        label.setColor(Color.WHITE);
        label.setTextAlign(Paint.Align.CENTER);
        label.setTypeface(android.graphics.Typeface.DEFAULT_BOLD);
        textoOcultar = context.getString(R.string.tactil_ocultar);
        textoMostrar = context.getString(R.string.tactil_mostrar);
        textoMando = context.getString(R.string.tactil_mando);
        textoSalir = context.getString(R.string.salir_juego);
        textoSalirSeguro = context.getString(R.string.salir_juego_seguro);
        // Michroma (res/font, licencia OFL en res/raw): ancha y cuadrada como la del juego.
        android.graphics.Typeface fuente = android.graphics.Typeface.DEFAULT_BOLD;
        try {
            fuente = getResources().getFont(R.font.michroma);
        } catch (RuntimeException sinFuente) {
            // La del sistema.
        }
        salirTexto.setTypeface(fuente);
        salirTexto.setColor(SALIR_CREMA);
        salirTexto.setStyle(Paint.Style.FILL_AND_STROKE);
        salirTexto.setStrokeJoin(Paint.Join.ROUND);
        textoGestos = context.getString(R.string.tactil_gestos);
        menusTactiles = ajustes.menusTactiles();
        deslizarInvertido = ajustes.deslizarInvertido();
        textoTamano = context.getString(R.string.editor_tamano);
        textoOpacidad = context.getString(R.string.editor_opacidad);
        textoRestaurar = context.getString(R.string.editor_restaurar);
        textoCancelar = context.getString(R.string.editor_cancelar);
        textoListo = context.getString(R.string.editor_listo);
        textoAyuda = context.getString(R.string.editor_ayuda);
        textoDisp[NORMAL] = context.getString(R.string.editor_disposicion_normal);
        textoDisp[ACELERACION] = context.getString(R.string.editor_disposicion_aceleracion);
        textoDisp[CONDUCIENDO] = context.getString(R.string.editor_disposicion_conduciendo);
        context.deleteSharedPreferences(PREFS_VIEJAS);
        // La partida (proceso :juego) y la pantalla de inicio editan el mismo
        // fichero. MODE_MULTI_PROCESS lo vuelve a leer si el otro lo cambio:
        // sin el, el editor de la pantalla de inicio enseniaria lo de antes de
        // editar en la partida, y al guardar lo pisaria.
        @SuppressWarnings("deprecation")
        int modo = Context.MODE_PRIVATE | Context.MODE_MULTI_PROCESS;
        preferences = context.getSharedPreferences(PREFS, modo);
        opacidad = preferences.getFloat(PREF_OPACIDAD, 1f);
        mandoEnMenus = preferences.getBoolean(PREF_MANDO_EN_MENUS, false);
        inputManager = (InputManager) context.getSystemService(Context.INPUT_SERVICE);
        usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        if (editando) {
            controlsVisible = true;
            toggleVisible = false;
            return;
        }
        mandoFisico = hayMandoFisico(null);
        if (enPartida) GameActivity.callarVibracion(GameActivity.CALLADA_MANDO, mandoFisico);
        // Con mando, oculto del todo; sin el, como lo dejara el usuario.
        controlsVisible = !mandoFisico && preferences.getBoolean(PREF_VISIBLE, true);
        toggleVisible = !mandoFisico;
    }

    /**
     * Lo llama GameActivity con cada evento de mando fisico: esconde los
     * controles tactiles y el boton. Barato si ya estaba todo oculto, que es
     * lo normal (los ejes mandan eventos muy a menudo).
     */
    public void mandoUsado() {
        if (editando) return;
        if (mandoFisico && !controlsVisible && !toggleVisible) return;
        entrarModoMando();
    }

    private void entrarModoMando() {
        if (editando) return;
        mandoFisico = true;
        // Con mando fisico vibra el (por SDL), no el movil.
        GameActivity.callarVibracion(GameActivity.CALLADA_MANDO, true);
        removeCallbacks(ocultarToggle);
        toggleVisible = false;
        if (controlsVisible) {
            controlsVisible = false;
            rehacer();
        } else {
            invalidate();
        }
    }

    private void salirModoMando() {
        if (editando) return;
        mandoFisico = false;
        GameActivity.callarVibracion(GameActivity.CALLADA_MANDO, false);
        controlsVisible = preferences.getBoolean(PREF_VISIBLE, true);
        rehacer();
        mostrarToggle();
    }

    /** Saca el boton OCULTAR/TACTIL, y que se vaya a los TOGGLE_MS. */
    private void mostrarToggle() {
        toggleVisible = true;
        removeCallbacks(ocultarToggle);
        postDelayed(ocultarToggle, TOGGLE_MS);
        invalidate();
    }

    /** Un evento de estas fuentes viene de un mando fisico. */
    static boolean esFuenteDeMando(int sources) {
        return (sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
                || (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    public void clearInput() {
        cancelarMenu();
        for (Control control : controls) control.release();
        pointers.clear();
        soltarCruceroYGestos();
        sendState();
        invalidate();
    }

    /**
     * Suelta el crucero del acelerador y los gestos: al cambiar de parte del
     * juego (menus, cargas), al cancelar el toque o al abrir el editor no se
     * quedan puestos. La pausa NO los suelta: al volver a jugar sigue el
     * pedal bloqueado, sin tener que volver a pisarlo.
     */
    private void soltarCruceroYGestos() {
        aceleradorBloqueado = false;
        gestoNitro = false;
        gestoFrenoMano = false;
        dedoGesto = -1;
        gestoDecidido = false;
    }

    public void disconnect() {
        cancelarMenu();
        for (Control control : controls) control.release();
        pointers.clear();
        soltarCruceroYGestos();
        TouchControllerBridge.trySetState(-1, 0, 0, 0, 0, 0, 0);
    }

    /** GameActivity: si la imagen del juego llena la vista o es el 16:9 del centro. */
    public void estirar(boolean estirar) {
        this.estirar = estirar;
        calcularMarco(lastWidth, lastHeight);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        // null: los avisos llegan por el hilo principal, el de la vista.
        if (enPartida) {
            inputManager.registerInputDeviceListener(oyenteMandos, null);
            IntentFilter usb = new IntentFilter(UsbManager.ACTION_USB_DEVICE_ATTACHED);
            usb.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
            // Avisos del sistema: se registran como lo hace SDL.
            if (Build.VERSION.SDK_INT >= 33) {
                getContext().registerReceiver(oyenteUsb, usb, Context.RECEIVER_EXPORTED);
            } else {
                getContext().registerReceiver(oyenteUsb, usb);
            }
        }
        // Al empezar la partida se ve, y a los TOGGLE_MS se va.
        if (enPartida && toggleVisible) mostrarToggle();
        sendState();
    }

    /** Solo se pregunta al juego, y se escucha el sensor, mientras la partida se ve. */
    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);
        removeCallbacks(mirarContexto);
        if (enPartida && visibility == VISIBLE) post(mirarContexto);
        // De fondo (o en segundo plano) no se queda nada pisado.
        else soltarCruceroYGestos();
        escucharInclinacion(enPartida && inclinar && visibility == VISIBLE);
    }

    private void escucharInclinacion(boolean escuchar) {
        if (gravedad == null || escuchar == escuchando) return;
        if (escuchar) {
            sensores.registerListener(oyenteInclinacion, gravedad, SensorManager.SENSOR_DELAY_GAME);
        } else {
            sensores.unregisterListener(oyenteInclinacion);
            giroInclinado = 0;
        }
        escuchando = escuchar;
    }

    /** Conduciendo, sin pausa, con los controles a la vista y sin editar. */
    private boolean inclinacionActiva() {
        return inclinar && escuchando && !editando && controlsVisible && dispJuego == CONDUCIENDO
                && !enPausa;
    }

    @Override
    protected void onDetachedFromWindow() {
        if (enPartida) {
            inputManager.unregisterInputDeviceListener(oyenteMandos);
            getContext().unregisterReceiver(oyenteUsb);
        }
        removeCallbacks(ocultarToggle);
        removeCallbacks(mirarContexto);
        removeCallbacks(rampaFlechas);
        escucharInclinacion(false);
        disconnect();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        buildControls(width, height);
    }

    /**
     * La disposicion de fabrica, y encima lo guardado con el editor. Las
     * posiciones de fabrica van en fracciones de la pantalla y los tamanos en
     * 'unit', 1/16 del ancho o 1/9 del alto, lo que sea menor.
     */
    private void buildControls(float width, float height) {
        lastWidth = width;
        lastHeight = height;
        calcularMarco(width, height);
        controls.clear();
        float unit = Math.min(width / 16f, height / 9f);
        float stickRadius = unit * 0.78f;
        float buttonRadius = unit * 0.38f;

        leftStick = new Stick("ls", "L", width * .16f, height * .73f, stickRadius);
        rightStick = new Stick("rs", "R", width * .67f, height * .77f, stickRadius);
        controls.add(leftStick);
        controls.add(rightStick);
        // En las carreras de aceleracion el stick izquierdo solo cambia de
        // carril: dos botones con flecha en su lugar. Conduciendo, segun
        // Ajustes.modoGiro: el stick, esas mismas flechas, o nada (inclinando).
        leftStick.visibleEn = inclinar || flechasAlConducir ? EN_NORMAL : EN_NORMAL | EN_CONDUCIENDO;
        float ladoFlecha = unit * 1.15f;
        carrilIzq = new Flecha("carril_izq", width * .16f, height * .73f, ladoFlecha, -1);
        carrilDer = new Flecha("carril_der", width * .16f, height * .73f, ladoFlecha, 1);
        if (flechasAlConducir) {
            carrilIzq.visibleEn |= EN_CONDUCIENDO;
            carrilDer.visibleEn |= EN_CONDUCIENDO;
        }
        controls.add(carrilIzq);
        controls.add(carrilDer);

        Dpad dpad = new Dpad("dpad", width * .32f, height * .72f, unit * .98f);
        controls.add(dpad);
        // En las carreras de aceleracion la cruceta solo abre la clasificacion
        // (arriba): un boton con su icono en su lugar.
        dpad.visibleEn = EN_NORMAL | EN_CONDUCIENDO;
        float ladoPosiciones = unit * .8f;
        RectButton posiciones = new RectButton("posiciones", "1 2 3", width * .32f, height * .72f,
                                               ladoPosiciones, ladoPosiciones, DPAD_UP);
        posiciones.icono = ICONO_POSICIONES;
        posiciones.visibleEn = EN_ACELERACION;
        controls.add(posiciones);

        float faceX = width * .85f;
        float faceY = height * .68f;
        ButtonControl a = new ButtonControl("a", "A", faceX, faceY + unit * .68f, buttonRadius, A);
        ButtonControl b = new ButtonControl("b", "B", faceX + unit * .68f, faceY, buttonRadius, B);
        // Los gestos los pulsan: hay que poder marcarlos.
        botonA = a;
        botonB = b;
        ButtonControl x = new ButtonControl("x", "X", faceX - unit * .68f, faceY, buttonRadius, X);
        ButtonControl y = new ButtonControl("y", "Y", faceX, faceY - unit * .68f, buttonRadius, Y);
        controls.add(a);
        controls.add(b);
        controls.add(x);
        controls.add(y);
        // En las carreras: iconos en vez de letras, y sin Y (mirar atras ya es LB).
        b.icono = ICONO_NITRO;
        a.icono = ICONO_FRENO_MANO;
        x.icono = ICONO_RELOJ;
        y.visibleEn = EN_NORMAL;

        float shoulderW = unit * 1.25f;
        float shoulderH = unit * .55f;
        leftTrigger = new Trigger("lt", "LT", width * .07f, height * .10f, shoulderW, shoulderH, true);
        rightTrigger = new Trigger("rt", "RT", width * .93f, height * .10f, shoulderW, shoulderH, false);
        rightStick.palanca = true;
        controls.add(leftTrigger);
        RectButton lb = new RectButton("lb", "LB", width * .20f, height * .10f,
                                       shoulderW, shoulderH, LB);
        RectButton rb = new RectButton("rb", "RB", width * .80f, height * .10f,
                                       shoulderW, shoulderH, RB);
        lb.icono = ICONO_RETROVISOR;
        rb.icono = ICONO_CAMARA;
        controls.add(lb);
        controls.add(rb);
        controls.add(rightTrigger);

        RectButton back = new RectButton("back", "BACK", width * .44f, height * .79f,
                                         unit * .82f, unit * .45f, BACK);
        RectButton start = new RectButton("start", "START", width * .55f, height * .79f,
                                          unit * .92f, unit * .45f, START);
        controls.add(back);
        controls.add(start);
        ButtonControl l3 = new ButtonControl("l3", "L3", width * .07f, height * .48f,
                                             buttonRadius * .82f, L3);
        ButtonControl r3 = new ButtonControl("r3", "R3", width * .73f, height * .48f,
                                             buttonRadius * .82f, R3);
        l3.visibleEn = EN_NORMAL;
        r3.visibleEn = EN_NORMAL;
        controls.add(l3);
        controls.add(r3);

        // --- La de fabrica de las carreras de aceleracion. Sale de la que se
        // hizo a mano en el movil (2688x1216):
        //   - abajo, sobre la misma linea (pie), los pedales a la derecha, y a
        //     la izquierda las flechas de carril, la palanca y los botones, como
        //     conduciendo (mas abajo, "Igual en las dos");
        //   - arriba a la izquierda, en rejilla: la clasificacion y la camara en
        //     una fila, y el retrovisor debajo de la camara;
        //   - arriba, BACK y START a los lados de OCULTAR/TACTIL y el engranaje,
        //     a la misma distancia.
        // En fracciones de la pantalla y tamanos en 'unit', como la normal.
        float pie = height * .885f;
        // 'pedal' es la medida con la que se hizo la disposicion: de ella salen
        // el reflejo de los pedales (el stick izquierdo, las flechas) y lo que el
        // usuario coloco a partir de ahi. Los pedales se ven a 'tamPedal' (al
        // 200 %, 2026-10-18): crecen hacia arriba y hacia la izquierda desde el
        // pie y el borde del acelerador, sin mover lo demas.
        float pedal = 1.5f;
        float tamPedal = 2f;
        float acelX = width * .885f;
        float frenoX = acelX - (shoulderW * pedal * (.6f + .74f) * .5f + unit * .35f);
        float frenoGrandeX = acelX - (shoulderW * tamPedal * (.6f + .74f) * .5f + unit * .35f);
        rightTrigger.porDefecto(ACELERACION, acelX, pie - shoulderW * tamPedal * 1.15f * .5f, tamPedal);
        leftTrigger.porDefecto(ACELERACION, frenoGrandeX, pie - shoulderW * tamPedal * .9f * .5f, tamPedal);
        float rombo = unit * .82f;
        float romboX = width * .858f;
        float romboY = height * .44f;
        // Y no esta en las carreras: donde no estorbe si se vuelve a ver.
        y.porDefecto(ACELERACION, romboX, romboY - rombo, 1f);
        float filaRejilla = height * .15f;
        float camaraX = width * .13f;
        rb.porDefecto(ACELERACION, camaraX, filaRejilla, 1f);
        lb.porDefecto(ACELERACION, camaraX, filaRejilla + unit, 1f);
        posiciones.porDefecto(ACELERACION, camaraX - shoulderW * .5f - unit * .3f - ladoPosiciones * .5f,
                                 filaRejilla, 1f);
        // Ocultos en la de aceleracion: donde no estorben si se vuelven a ver.
        leftStick.porDefecto(ACELERACION, width * .155f, height * .57f, 1f);
        dpad.porDefecto(ACELERACION, width * .155f, height * .82f, 1f);
        // A la altura de OCULTAR/TACTIL (colocarBotones), a la misma distancia de
        // ese boton y del engranaje.
        float filaArriba = Math.max(dp(18), height * .035f) + unit * .23f;
        back.porDefecto(ACELERACION, width * .41f, filaArriba, 1f);
        start.porDefecto(ACELERACION, width * .622f, filaArriba, 1f);

        // --- La de fabrica conduciendo (las demas carreras, la conduccion libre
        // y las persecuciones): la de aceleracion, pero con el stick izquierdo
        // para girar, sobre la misma linea, en el reflejo de los dos pedales y
        // a 1,5 veces su tamano; el derecho, un stick y no la palanca; y la
        // cruceta, que aqui hace mas que abrir la clasificacion (abajo recoloca
        // el coche), en el reflejo de donde estaban los botones en rombo. El
        // stick izquierdo, como lo dejo el usuario en el movil (2688x1216).
        for (Control c : new Control[] {rightTrigger, leftTrigger, y, rb, lb, back, start}) {
            c.porDefecto(CONDUCIENDO, c.fx[ACELERACION], c.fy[ACELERACION], c.fs[ACELERACION]);
        }
        float pedalesIzq = frenoX - shoulderW * pedal * .74f * .5f;
        float pedalesDer = acelX + shoulderW * pedal * .6f * .5f;
        float stickIzqX = width - (pedalesIzq + pedalesDer) * .5f;
        leftStick.porDefecto(CONDUCIENDO, stickIzqX, pie - stickRadius, 1.5f);
        dpad.porDefecto(CONDUCIENDO, width - romboX, romboY, 1f);

        // --- Igual en las dos (aceleracion como conduciendo, 2026-10-18):
        //   - las flechas, donde iria el stick izquierdo, a sus lados y a 1,5
        //     veces (en aceleracion cambian de carril; conduciendo, si se elige,
        //     giran);
        //   - el stick derecho (en aceleracion, la palanca) y, sin Y, los botones
        //     X, A y B en diagonal sobre el: como los dejo el usuario en el movil
        //     (2688x1216), en 'unit' desde el stick izquierdo y la linea del pie.
        float escalaFlecha = 1.5f;
        float sobrePieFlechas = pie - ladoFlecha * escalaFlecha * .5f;
        for (int d : new int[] {ACELERACION, CONDUCIENDO}) {
            carrilIzq.porDefecto(d, stickIzqX - unit * 1.1f, sobrePieFlechas, escalaFlecha);
            carrilDer.porDefecto(d, stickIzqX + unit * 1.1f, sobrePieFlechas, escalaFlecha);
            rightStick.porDefecto(d, stickIzqX + unit * 3.58f, pie - unit * .31f, 1f);
            x.porDefecto(d, stickIzqX + unit * 1.88f, pie - unit * 3.51f, 1f);
            a.porDefecto(d, stickIzqX + unit * 3.28f, pie - unit * 3.13f, 1f);
            b.porDefecto(d, stickIzqX + unit * 4.12f, pie - unit * 2.08f, 1f);
        }
        // Conduciendo e inclinando el movil no hay stick izquierdo ni flechas: la
        // cruceta baja a la esquina, apoyada en el pie, y el stick derecho y los
        // botones se corren a la izquierda. Como los dejo el usuario en el movil
        // (2688x1216, 2026-10-18).
        if (inclinar) {
            dpad.porDefecto(CONDUCIENDO, width - romboX, pie - unit, 1f);
            rightStick.porDefecto(CONDUCIENDO, stickIzqX + unit * 2.88f, pie - unit * .31f, 1f);
            x.porDefecto(CONDUCIENDO, stickIzqX + unit * 1.06f, pie - unit * 3.52f, 1f);
            a.porDefecto(CONDUCIENDO, stickIzqX + unit * 2.34f, pie - unit * 2.81f, 1f);
            b.porDefecto(CONDUCIENDO, stickIzqX + unit * 3.38f, pie - unit * 1.95f, 1f);
        }

        for (Control c : controls) cargar(c);
        usarDisposicion(disposicion());
        colocarBotones(width, height, unit);
        sendState();
        invalidate();
    }

    /** Lo guardado con el editor para este control, en las dos disposiciones. */
    private void cargar(Control c) {
        for (int d = 0; d < DISPOSICIONES; ++d) {
            String k = clave(c, d);
            if (!preferences.contains(k + "_x")) continue;
            c.px[d] = preferences.getFloat(k + "_x", 0f) * lastWidth;
            c.py[d] = preferences.getFloat(k + "_y", 0f) * lastHeight;
            c.ps[d] = preferences.getFloat(k + "_s", 1f);
            c.propia[d] = true;
        }
    }

    /**
     * Girando inclinando el movil, la de conducir se guarda aparte ("ci_"): sin
     * stick ni flechas, la cruceta va donde estarian, y al volver al stick o a
     * las flechas se pisarian.
     */
    private String clave(Control c, int disposicion) {
        String prefijo = inclinar && disposicion == CONDUCIENDO ? "ci_" : PREFIJOS[disposicion];
        return prefijo + c.id;
    }

    /** Cada control, donde va en esa disposicion. */
    private void usarDisposicion(int d) {
        for (Control c : controls) c.usar(d);
        invalidate();
    }

    /**
     * Donde se ve el juego (GameActivity.ajustarSuperficie): la vista entera si se
     * estira, y si no, el 16:9 mas grande que cabe, centrado.
     */
    private void calcularMarco(float width, float height) {
        float w = width;
        float h = height;
        if (!estirar) {
            if (width * 9 > height * 16) {
                w = height * 16 / 9f;
            } else {
                h = width * 9 / 16f;
            }
        }
        imagenJuego.set((width - w) * .5f, (height - h) * .5f, (width + w) * .5f, (height + h) * .5f);
    }

    /**
     * El texto de OCULTAR/TACTIL. En los menus, con el dedo, no hay nada que
     * ocultar: el boton cambia del dedo al mando (MANDO) y vuelta (DEDO).
     */
    private String textoToggle() {
        if (!controlsVisible) return textoMostrar;
        if (menusConDedo()) return mandoEnMenus ? textoGestos : textoMando;
        return textoOcultar;
    }

    /** Los menus se llevan con el dedo en esta partida (aunque ahora vaya el mando). */
    private boolean menusConDedo() {
        return enPartida && menusTactiles && enMenu && !editando;
    }

    /** En un menu, con el dedo: sin mando a la vista y la pantalla entera para tocar. */
    private boolean conDedo() {
        return menusConDedo() && controlsVisible && !mandoEnMenus;
    }

    /** OCULTAR/TACTIL arriba, y la barra del editor en el centro. */
    private void colocarBotones(float width, float height, float unit) {
        label.setTextSize(dp(11));
        // Lo que pida el texto, que en otros idiomas es mas largo que HIDE.
        float toggleWidth = Math.max(controlsVisible ? unit * 1.05f : unit * 1.35f,
                label.measureText(textoToggle()) + dp(24));
        float toggleTop = Math.max(dp(18), height * .035f);
        toggle.set(width * .5f - toggleWidth * .5f, toggleTop,
                   width * .5f + toggleWidth * .5f, toggleTop + unit * .46f);
        engranaje.set(toggle.right + dp(8), toggle.top,
                      toggle.right + dp(8) + toggle.height(), toggle.bottom);

        // Dos filas en el centro, donde la disposicion de fabrica no tiene
        // nada: [-] TAMANO [+]  [-] OPACIDAD [+], y debajo las acciones.
        label.setTextSize(dp(12));
        float lado = unit * .6f;
        float hueco = dp(10);
        float anchoTam = label.measureText(textoTamano + " 200 %") + dp(16);
        float anchoOpa = label.measureText(textoOpacidad + " 100 %") + dp(16);
        float fila1 = lado * 4 + anchoTam + anchoOpa + hueco * 7;
        float x = width * .5f - fila1 * .5f;
        // Entre LB/RB (arriba, hasta ~13 %) y L3/R3 (a media altura, desde ~44 %).
        float y = height * .18f;
        tamMenos.set(x, y, x + lado, y + lado);
        x += lado + hueco;
        textoTamX = x + anchoTam * .5f;
        x += anchoTam + hueco;
        tamMas.set(x, y, x + lado, y + lado);
        x += lado + hueco * 3;
        opaMenos.set(x, y, x + lado, y + lado);
        x += lado + hueco;
        textoOpaX = x + anchoOpa * .5f;
        x += anchoOpa + hueco;
        opaMas.set(x, y, x + lado, y + lado);

        float anchoDis = 0;
        for (String t : textoDisp) anchoDis = Math.max(anchoDis, label.measureText(t));
        anchoDis += dp(28);
        float anchoRes = label.measureText(textoRestaurar) + dp(28);
        float anchoCan = label.measureText(textoCancelar) + dp(28);
        float anchoLis = label.measureText(textoListo) + dp(28);
        float fila2 = anchoDis + anchoRes + anchoCan + anchoLis + hueco * 3;
        x = width * .5f - fila2 * .5f;
        y += lado + hueco;
        disposicion.set(x, y, x + anchoDis, y + lado);
        x += anchoDis + hueco;
        restaurar.set(x, y, x + anchoRes, y + lado);
        x += anchoRes + hueco;
        cancelar.set(x, y, x + anchoCan, y + lado);
        x += anchoCan + hueco;
        listo.set(x, y, x + anchoLis, y + lado);

        // La ayuda, en una linea: mas pequena si no cabe.
        tamanoAyuda = dp(12);
        label.setTextSize(tamanoAyuda);
        float anchoAyuda = label.measureText(textoAyuda);
        if (anchoAyuda > width * .9f) {
            tamanoAyuda *= width * .9f / anchoAyuda;
            anchoAyuda = width * .9f;
        }
        y += lado + hueco;
        fondoAyuda.set(width * .5f - anchoAyuda * .5f - dp(10), y,
                       width * .5f + anchoAyuda * .5f + dp(10), y + tamanoAyuda * 2f);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (editando) {
            for (Control control : controls) {
                if (!control.oculto()) control.draw(canvas);
            }
            if (elegido != null) {
                elegido.ocupa(recuadroElegido);
                recuadroElegido.inset(-dp(6), -dp(6));
                canvas.drawRect(recuadroElegido, marco);
            }
            drawEditor(canvas);
            return;
        }
        if (toggleVisible) {
            drawToggle(canvas);
            drawEngranaje(canvas);
        }
        if (salirALaVista()) drawSalir(canvas);
        if (!controlsVisible) return;
        // Con el dedo en los menus no se dibuja nada: se toca el juego.
        if (conDedo()) return;
        for (Control control : controls) {
            if (!control.oculto()) control.draw(canvas);
        }
    }

    private void drawToggle(Canvas canvas) {
        drawBoton(canvas, toggle, textoToggle(), dp(11));
    }

    /** Un engranaje: un aro con ocho dientes. */
    private void drawEngranaje(Canvas canvas) {
        drawBoton(canvas, engranaje, "", dp(11));
        float x = engranaje.centerX();
        float y = engranaje.centerY();
        float r = engranaje.height() * .3f;
        dientes.setColor(Color.WHITE);
        dientes.setStrokeWidth(r * .45f);
        for (int i = 0; i < 8; ++i) {
            double a = Math.PI / 4 * i;
            float c = (float) Math.cos(a);
            float s = (float) Math.sin(a);
            canvas.drawLine(x + c * r * .7f, y + s * r * .7f, x + c * r * 1.15f, y + s * r * 1.15f, dientes);
        }
        dientes.setStrokeWidth(r * .35f);
        canvas.drawCircle(x, y, r * .72f, dientes);
    }

    private void drawEditor(Canvas canvas) {
        float t = dp(12);
        drawBoton(canvas, tamMenos, "−", dp(18));
        drawBoton(canvas, tamMas, "+", dp(18));
        drawBoton(canvas, opaMenos, "−", dp(18));
        drawBoton(canvas, opaMas, "+", dp(18));
        // Sin control elegido, TAMANO va para todos: no hay un valor que ensenar.
        String tam = elegido == null ? textoTamano
                : textoTamano + " " + Math.round(elegido.escala * 100) + " %";
        drawTexto(canvas, tam, textoTamX, tamMenos.centerY(), t);
        drawTexto(canvas, textoOpacidad + " " + Math.round(opacidad * 100) + " %",
                  textoOpaX, opaMenos.centerY(), t);
        drawBoton(canvas, disposicion,
                  textoDisp[dispEditada], t);
        drawBoton(canvas, restaurar, textoRestaurar, t);
        drawBoton(canvas, cancelar, textoCancelar, t);
        drawBoton(canvas, listo, textoListo, t);
        fill.setColor(FONDO_BOTON);
        canvas.drawRoundRect(fondoAyuda, dp(8), dp(8), fill);
        drawLabel(canvas, textoAyuda, fondoAyuda.centerX(), fondoAyuda.centerY(), tamanoAyuda, 1f);
    }

    /** Un boton de la interfaz (no del mando): fondo oscuro y borde de acento. */
    private void drawBoton(Canvas canvas, RectF r, String texto, float tamano) {
        fill.setColor(FONDO_BOTON);
        stroke.setColor(BORDE_TOGGLE);
        canvas.drawRoundRect(r, dp(9), dp(9), fill);
        canvas.drawRoundRect(r, dp(9), dp(9), stroke);
        drawLabel(canvas, texto, r.centerX(), r.centerY(), tamano, 1f);
    }

    /** Un texto de la barra del editor, sobre una pastilla oscura para leerlo. */
    private void drawTexto(Canvas canvas, String texto, float x, float y, float tamano) {
        label.setTextSize(tamano);
        float mitad = label.measureText(texto) * .5f + dp(8);
        fill.setColor(FONDO_BOTON);
        canvas.drawRoundRect(x - mitad, y - tamano, x + mitad, y + tamano, dp(8), dp(8), fill);
        drawLabel(canvas, texto, x, y, tamano, 1f);
    }

    /**
     * Si el dedo esta a menos de BUFFER_CONTROLES_DP del borde de un control
     * (o de OCULTAR/TACTIL y el engranaje), la zona reservada. Se mira con
     * ocho puntos alrededor del dedo, para no tener que repetir la forma de
     * cada control.
     */
    private boolean rozaControl(float x, float y) {
        float m = dp(BUFFER_CONTROLES_DP);
        float[] desplazamientos = {m, 0, -m, 0, 0, m, 0, -m,
                                   m * .7f, m * .7f, -m * .7f, m * .7f,
                                   m * .7f, -m * .7f, -m * .7f, -m * .7f};
        for (int i = 0; i < desplazamientos.length; i += 2) {
            float px = x + desplazamientos[i];
            float py = y + desplazamientos[i + 1];
            if (toggleVisible && (toggle.contains(px, py) || engranaje.contains(px, py))) return true;
            for (Control c : controls) {
                if (!c.oculto() && c.contains(px, py)) return true;
            }
        }
        return false;
    }

    public boolean onTouchEvent(MotionEvent event) {
        if (editando) return tocarEditor(event);

        int action = event.getActionMasked();
        int index = event.getActionIndex();
        int pointerId = event.getPointerId(index);
        float x = event.getX(index);
        float y = event.getY(index);
        boolean bajando = action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN;
        if (!bajando && conDedo()) return tocarMenu(event);

        if (bajando) {
            if (toggleVisible && engranaje.contains(x, y)) {
                abrirEditor();
                return true;
            }
            boolean enToggle = toggle.contains(x, y) || engranaje.contains(x, y);
            if (toggleVisible && toggle.contains(x, y)) {
                if (controlsVisible && menusConDedo()) {
                    cambiarMandoEnMenus();
                } else {
                    setControlsVisible(!controlsVisible);
                }
                return true;
            }
            if (salirTocable() && tocaSalir(x, y)) {
                pulsarSalir();
                return true;
            }
            // Oculto: tocar donde esta lo saca, sin pulsarlo. Con mando fisico
            // y sin controles no hay nada mas que tocar, asi que vale cualquier
            // sitio (y mientras se siga tocando, no se va).
            if (enToggle || (mandoFisico && !controlsVisible)) {
                mostrarToggle();
                return true;
            }
            if (!controlsVisible) return false;
            if (conDedo()) return tocarMenu(event);
            // El candado del acelerador: se toca aparte y no pisa el pedal.
            if (rightTrigger != null && rightTrigger.tocaCandado(x, y)) {
                aceleradorBloqueado = !aceleradorBloqueado;
                sendState();
                invalidate();
                return true;
            }
            for (int i = controls.size() - 1; i >= 0; --i) {
                Control control = controls.get(i);
                if (!control.inUse && !control.oculto() && control.contains(x, y)) {
                    pointers.put(pointerId, control);
                    control.press(x, y);
                    sendState();
                    invalidate();
                    return true;
                }
            }
            // El margen de los controles: un roce al lado de un boton no gira
            // ni empieza un gesto.
            if (rozaControl(x, y)) return true;
            // La zona del volante: la mitad izquierda de la pantalla. Como en
            // los juegos de movil, el stick se pone donde caiga el dedo y sigue
            // el giro mientras siga puesto (lo demas ya lo ha cogido cada
            // control, asi que aqui solo queda el hueco).
            if (formasDeConducir() && leftStick != null && !leftStick.inUse
                    && !leftStick.oculto() && x < lastWidth * .5f) {
                pointers.put(pointerId, leftStick);
                leftStick.agarrarFlotante(x, y);
                sendState();
                invalidate();
                return true;
            }
            // Nada debajo: puede ser un gesto (nitro arriba, freno de mano
            // abajo). Se decide en cuanto el dedo se mueve lo suficiente. La
            // mitad derecha, para no pisar la zona del volante.
            if (formasDeConducir() && x > lastWidth * .5f) {
                dedoGesto = pointerId;
                gestoX = x;
                gestoY = y;
                gestoDecidido = false;
                return true;
            }
            return false;
        }

        if (action == MotionEvent.ACTION_MOVE) {
            // El gesto (nitro arriba, freno de mano abajo): se decide con el
            // primer movimiento, y dura mientras el dedo siga puesto.
            if (dedoGesto != -1) {
                int iGesto = event.findPointerIndex(dedoGesto);
                if (iGesto >= 0) {
                    if (!gestoDecidido) {
                        float dxGesto = event.getX(iGesto) - gestoX;
                        float dyGesto = event.getY(iGesto) - gestoY;
                        float recorrido = dp(GESTO_RECORRIDO_DP);
                        if (Math.abs(dyGesto) >= recorrido
                                && Math.abs(dyGesto) > Math.abs(dxGesto)) {
                            gestoDecidido = true;
                            if (dyGesto < 0) gestoNitro = true;
                            else gestoFrenoMano = true;
                            sendState();
                            invalidate();
                        } else if (Math.abs(dxGesto) >= recorrido * 2f) {
                            // Un arrastre de lado no es un gesto.
                            dedoGesto = -1;
                        }
                    }
                } else {
                    dedoGesto = -1;
                }
            }
            boolean handled = false;
            for (int i = 0; i < event.getPointerCount(); ++i) {
                Control control = pointers.get(event.getPointerId(i));
                if (control != null) {
                    control.move(event.getX(i), event.getY(i));
                    handled = true;
                }
            }
            if (handled) {
                sendState();
                invalidate();
            }
            return handled;
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            // El dedo del gesto: al levantarlo se suelta la nitro o el freno.
            if (pointerId == dedoGesto) {
                dedoGesto = -1;
                gestoDecidido = false;
                gestoNitro = false;
                gestoFrenoMano = false;
                sendState();
                invalidate();
                return true;
            }
            Control control = pointers.get(pointerId);
            if (control == null) return false;
            control.release();
            pointers.remove(pointerId);
            sendState();
            invalidate();
            return true;
        }

        if (action == MotionEvent.ACTION_CANCEL) {
            clearInput();
            return true;
        }
        return false;
    }

    // ------------------------------------------------------------------------
    //  Los menus con el dedo
    // ------------------------------------------------------------------------

    /**
     * Un dedo: si se mueve, desliza (un paso de cruceta cada tanto, en el eje en
     * que empezo); si no, al levantarlo es un toque. Un segundo dedo antes de
     * moverse lo convierte en el toque de dos dedos, B.
     */
    private boolean tocarMenu(MotionEvent e) {
        if (enCinematica) return tocarCinematica(e);
        int i = e.getActionIndex();
        switch (e.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                pararRumbo();
                gatilloDedo = gatilloBajo(e.getX(i), e.getY(i));
                if (gatilloDedo != 0) {
                    mantenido |= gatilloDedo;
                    sendState();
                }
                dedoMenu = e.getPointerId(i);
                menuX0 = e.getX(i);
                menuY0 = e.getY(i);
                menuT0 = e.getEventTime();
                ejeMenu = SIN_EJE;
                contadoMenu = 0;
                pasosMenu = 0;
                dosDedos = false;
                arrastrar = false;
                return true;
            case MotionEvent.ACTION_POINTER_DOWN:
                if (dedoMenu != -1 && ejeMenu == SIN_EJE && gatilloDedo == 0 && e.getPointerCount() >= 2) {
                    dosDedos = true;
                    pellizco = false;
                    distanciaPellizco = distancia(e);
                }
                return true;
            case MotionEvent.ACTION_MOVE: {
                if (dosDedos) {
                    pellizcar(e);
                    return true;
                }
                int j = dedoMenu == -1 ? -1 : e.findPointerIndex(dedoMenu);
                if (j < 0 || gatilloDedo != 0) return true;
                float dx = e.getX(j) - menuX0;
                float dy = e.getY(j) - menuY0;
                if (ejeMenu == SIN_EJE) {
                    if (Math.hypot(dx, dy) < dp(MOVER_DP)) return true;
                    ejeMenu = Math.abs(dx) >= Math.abs(dy) ? EJE_X : EJE_Y;
                    deslizarDirecto = ejeMenu == EJE_X && TouchControllerBridge.tryMenuEsLista();
                    arrastrar = TouchControllerBridge.tryMenuRotar();
                    arrastreX = e.getX(j);
                    arrastreY = e.getY(j);
                    arrastreT = e.getEventTime();
                }
                if (arrastrar) {
                    arrastrarStick(e.getX(j), e.getY(j), e.getEventTime());
                    return true;
                }
                float d = (ejeMenu == EJE_X ? dx : dy) - contadoMenu;
                float paso = pasosMenu == 0 ? dp(PRIMER_PASO_DP) : pasoDeslizar();
                while (Math.abs(d) >= paso) {
                    float signo = Math.signum(d);
                    darPaso(signo > 0);
                    contadoMenu += signo * paso;
                    d -= signo * paso;
                    paso = pasoDeslizar();
                }
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP: {
                if (dosDedos && e.getActionMasked() == MotionEvent.ACTION_POINTER_UP) {
                    // Al levantar uno de los dos dedos, se acabo el pellizco.
                    soltarGatillo(true);
                }
                if (e.getPointerId(i) != dedoMenu) return true;
                dedoMenu = -1;
                long dura = e.getEventTime() - menuT0;
                if (arrastrar) {
                    // Sigue girando un momento y frena (soltarArrastre).
                    arrastrar = false;
                    removeCallbacks(soltarArrastre);
                    postDelayed(soltarArrastre, FRENADA_MS);
                } else if (gatilloDedo != 0) {
                    soltarGatillo(false);
                } else if (dosDedos) {
                    soltarGatillo(true);
                    if (!pellizco && dura <= DOS_DEDOS_MS) pulsar(B, TRAS_NADA);
                } else if (ejeMenu == SIN_EJE) {
                    if (dura <= TOQUE_MS) tocarEn(menuX0, menuY0);
                } else if (pasosMenu == 0) {
                    // Un golpe corto que no llego al primer paso: tambien cuenta.
                    if (ejeMenu == EJE_X) deslizarDirecto = TouchControllerBridge.tryMenuEsLista();
                    float d = ejeMenu == EJE_X ? e.getX(i) - menuX0 : e.getY(i) - menuY0;
                    if (dura <= TOQUE_MS) darPaso(d > 0);
                }
                return true;
            }
            case MotionEvent.ACTION_CANCEL:
                dedoMenu = -1;
                soltarGatillo(false);
                soltarGatillo(true);
                arrastrar = false;
                soltarStick();
                return true;
            default:
                return true;
        }
    }

    /**
     * En una cinematica: al levantar el dedo, A, que es lo que la salta en el 360
     * (HUDACTION_SKIPNIS en el mapa "hud" del mando). Sin deslizar ni dos dedos: no hay
     * nada mas que hacer. Una sola A a la vez, para no dejar otras pendientes para lo
     * que venga despues.
     */
    private boolean tocarCinematica(MotionEvent e) {
        int accion = e.getActionMasked();
        if ((accion == MotionEvent.ACTION_UP || accion == MotionEvent.ACTION_POINTER_UP)
                && !pulsando && pulsos.isEmpty()) {
            pulsar(A, TRAS_NADA);
        }
        return true;
    }

    /**
     * El gatillo de la indicacion de LT o RT que hay debajo de (x, y), para pisarlo mientras
     * dure el dedo (el zoom al visualizar un coche va mientras se pisa). 0 si no es eso.
     */
    private int gatilloBajo(float x, float y) {
        if (!imagenJuego.contains(x, y)) return 0;
        int r = TouchControllerBridge.tryMenuTocar((x - imagenJuego.left) / imagenJuego.width(),
                (y - imagenJuego.top) / imagenJuego.height());
        if (r < 0 || (r & TouchControllerBridge.MENU_TIPO) != TouchControllerBridge.MENU_INDICACION) return 0;
        int mascara = 1 << (r & 0xFF);
        return mascara == PULSO_LT || mascara == PULSO_RT ? mascara : 0;
    }

    /**
     * Arrastrando para girar: el stick derecho, segun lo rapido que va el dedo en cada eje (la
     * camara gira mas deprisa cuanto mas se inclina), suavizado para que no tiemble.
     */
    private void arrastrarStick(float x, float y, long t) {
        long dt = t - arrastreT;
        if (dt <= 0) return;
        float lleno = dp(VELOCIDAD_STICK_DP);
        float vx = curvaStick((x - arrastreX) / dt / lleno);
        float vy = curvaStick((y - arrastreY) / dt / lleno);
        arrastreX = x;
        arrastreY = y;
        arrastreT = t;
        // El coche con el dedo: la camara, al reves (los dos ejes del stick, como en SDL, con la
        // Y positiva hacia abajo).
        float signo = ROTAR_CON_EL_DEDO ? -1f : 1f;
        stickArrastreX = stickArrastreX * .6f + signo * vx * .4f;
        stickArrastreY = stickArrastreY * .6f + signo * vy * .4f;
        sendState();
        removeCallbacks(soltarArrastre);
        postDelayed(soltarArrastre, ARRASTRE_QUIETO_MS);
    }

    /** De la velocidad del dedo (1 = entera) al stick: lo poco, por encima de la zona muerta. */
    private static float curvaStick(float v) {
        float a = Math.abs(v);
        if (a < .02f) return 0f;
        return Math.signum(v) * Math.min(1f, STICK_MINIMO + (1f - STICK_MINIMO) * a);
    }

    private void soltarStick() {
        removeCallbacks(soltarArrastre);
        if (stickArrastreX != 0 || stickArrastreY != 0) {
            stickArrastreX = 0;
            stickArrastreY = 0;
            sendState();
        }
    }

    private static float distancia(MotionEvent e) {
        return (float) Math.hypot(e.getX(0) - e.getX(1), e.getY(0) - e.getY(1));
    }

    /**
     * Dos dedos que se separan pisan RT ("Acercar zoom"), y que se juntan, LT: mientras se
     * muevan. El primer paso pide PELLIZCO_DP, para que un toque con dos dedos (B) no cuente.
     */
    private void pellizcar(MotionEvent e) {
        if (e.getPointerCount() < 2) return;
        float d = distancia(e);
        float cambio = d - distanciaPellizco;
        if (Math.abs(cambio) < dp(pellizco ? PELLIZCO_PASO_DP : PELLIZCO_DP)) return;
        pellizco = true;
        distanciaPellizco = d;
        int gatillo = cambio > 0 ? PULSO_RT : PULSO_LT;
        if (gatillo != gatilloPellizco) {
            mantenido = (mantenido & ~gatilloPellizco) | gatillo;
            gatilloPellizco = gatillo;
            sendState();
        }
        removeCallbacks(soltarPellizco);
        postDelayed(soltarPellizco, ZOOM_QUIETO_MS);
    }

    /** Suelta el gatillo del pellizco o el de la indicacion bajo el dedo. */
    private void soltarGatillo(boolean delPellizco) {
        int gatillo = delPellizco ? gatilloPellizco : gatilloDedo;
        if (delPellizco) {
            removeCallbacks(soltarPellizco);
            gatilloPellizco = 0;
        } else {
            gatilloDedo = 0;
        }
        if (gatillo != 0) {
            mantenido &= ~gatillo;
            sendState();
        }
    }

    /** Lo que hay que deslizar para cada paso despues del primero. */
    private float pasoDeslizar() {
        return Math.max(dp(PASO_DESLIZAR_DP), imagenJuego.width() * PASO_DESLIZAR_MARCO);
    }

    /**
     * Un paso de cruceta en el eje del deslizamiento: positivo, el dedo a la derecha o
     * abajo. Como al pasar paginas, la opcion sigue al dedo: deslizar a la izquierda va a
     * la de la derecha (probado en el movil: al reves se sentia invertido). Con
     * Ajustes.deslizarInvertido, deslizar a la derecha va a la derecha. En una lista de
     * opciones, en horizontal se cambia el valor de la resaltada: siempre en el sentido del
     * dedo (a la derecha, mas volumen).
     */
    private void darPaso(boolean positivo) {
        boolean directo = deslizarInvertido || (ejeMenu == EJE_X && deslizarDirecto);
        if (!directo) positivo = !positivo;
        pasosMenu++;
        pulsar(ejeMenu == EJE_X ? (positivo ? DPAD_RIGHT : DPAD_LEFT)
                                : (positivo ? DPAD_DOWN : DPAD_UP), TRAS_NADA);
    }

    /** Un toque: lo que diga el juego que hay debajo (TouchControllerBridge.menuTocar). */
    private void tocarEn(float x, float y) {
        // En las bandas negras, a los lados del 16:9, no hay juego.
        if (!imagenJuego.contains(x, y)) return;
        float u = (x - imagenJuego.left) / imagenJuego.width();
        float v = (y - imagenJuego.top) / imagenJuego.height();
        int r = TouchControllerBridge.tryMenuTocar(u, v);
        int tipo = r < 0 ? r : r & TouchControllerBridge.MENU_TIPO;
        if (tipo == TouchControllerBridge.MENU_INDICACION) {
            // Una indicacion de abajo (A incluida) o una flecha.
            pulsar(1 << (r & 0xFF), TRAS_NADA);
        } else if (tipo == TouchControllerBridge.MENU_IR) {
            pasosRumbo = 1;
            pulsar(1 << (r & 0xFF), TRAS_RUMBO);
        } else if (tipo == TouchControllerBridge.MENU_ACEPTAR || r == TouchControllerBridge.MENU_DESCONOCIDO) {
            // Sin indicacion de A en pantalla (o sin poder leer el menu): el toque acepta.
            pulsar(A, TRAS_NADA);
        }
        // Lo demas (la opcion ya elegida, nada debajo, una opcion a la que no se sabe
        // llegar), nada: aceptar es tocar la indicacion de A.
        anotarMenus(u, v);
    }

    /** Despues de cada paso hacia la opcion tocada: el siguiente, hasta que este elegida. */
    private void seguirRumbo() {
        int r = TouchControllerBridge.tryMenuRumbo();
        if (r == TouchControllerBridge.MENU_ACEPTAR) {
            pulsar(A, TRAS_NADA);
        } else if (r >= 0 && (r & TouchControllerBridge.MENU_TIPO) == TouchControllerBridge.MENU_IR
                && pasosRumbo < MAX_PASOS_RUMBO) {
            pasosRumbo++;
            pulsar(1 << (r & 0xFF), TRAS_RUMBO);
        }
    }

    /** Pulsa `mascara` (botones del mando) cuando acaben las de antes. */
    private void pulsar(int mascara, int tras) {
        if (pulsos.size() >= MAX_PULSOS) return;
        pulsos.add(new int[] {mascara, tras});
        if (!pulsando) siguientePulso();
    }

    private void siguientePulso() {
        // Lo que pedia la anterior, ya soltada y con su pausa: puede pedir otra.
        int tras = trasPulso;
        trasPulso = TRAS_NADA;
        if (tras == TRAS_RUMBO) seguirRumbo();
        int[] p = pulsos.poll();
        if (p == null) {
            pulsando = false;
            return;
        }
        pulsando = true;
        pulso = p[0];
        trasPulso = p[1];
        sendState();
        postDelayed(soltarPulso, PULSO_MS);
    }

    /**
     * Un dedo nuevo para el camino hacia la opcion tocada antes. Lo que ya se
     * esta pulsando acaba: cortado a medias, el juego podria no verlo.
     */
    private void pararRumbo() {
        trasPulso = TRAS_NADA;
        pulsos.removeIf(p -> p[1] == TRAS_RUMBO);
    }

    /** Sin pulsaciones pendientes ni deslizamiento a medias. */
    private void cancelarMenu() {
        removeCallbacks(soltarPulso);
        removeCallbacks(siguientePulso);
        pulsos.clear();
        pulsando = false;
        trasPulso = TRAS_NADA;
        dedoMenu = -1;
        removeCallbacks(soltarPellizco);
        removeCallbacks(soltarArrastre);
        gatilloDedo = 0;
        gatilloPellizco = 0;
        stickArrastreX = 0;
        stickArrastreY = 0;
        arrastrar = false;
        if (pulso != 0 || mantenido != 0) {
            pulso = 0;
            mantenido = 0;
            sendState();
        }
    }

    /** MANDO/DEDO: en los menus, el mando o el dedo. Se queda para las siguientes partidas. */
    private void cambiarMandoEnMenus() {
        mandoEnMenus = !mandoEnMenus;
        preferences.edit().putBoolean(PREF_MANDO_EN_MENUS, mandoEnMenus).apply();
        cancelarMenu();
        rehacer();
        mostrarToggle();
    }

    /**
     * Diagnostico: files/logs/menus.txt, con cada toque lo que se leyo del menu
     * (nativo_android.cpp, menuDiagnostico). La primera vez de la partida empieza
     * el fichero de nuevo, y no pasa de MAX_DIAGNOSTICO_MENUS.
     */
    private void anotarMenus(float u, float v) {
        String leido = TouchControllerBridge.tryMenuDiagnostico();
        if (leido == null) return;
        java.io.File carpeta = getContext().getExternalFilesDir("logs");
        if (carpeta == null) return;
        java.io.File f = new java.io.File(carpeta, "menus.txt");
        boolean seguir = menusAnotados && f.length() < MAX_DIAGNOSTICO_MENUS;
        menusAnotados = true;
        try (java.io.FileWriter w = new java.io.FileWriter(f, seguir)) {
            w.write(new java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.ROOT)
                    .format(new java.util.Date())
                    + String.format(java.util.Locale.ROOT, " u=%.3f v=%.3f ctx=%s%s marco=%.0fx%.0f ",
                            u, v, NOMBRES_DISP[dispJuego], enPausa ? "+pausa" : "", imagenJuego.width(), imagenJuego.height())
                    + leido + "\n");
        } catch (java.io.IOException ignorada) {
            // Solo es diagnostico.
        }
    }

    // ------------------------------------------------------------------------
    //  Salir del juego desde el menu principal
    // ------------------------------------------------------------------------

    /**
     * Cada CONTEXTO_MS: si se esta en el menu principal (y donde va el boton), y si se ha
     * pulsado BACK en el mando fisico desde la ultima vez.
     */
    private void mirarSalir(boolean enMenus) {
        boolean visible = !editando && enMenus && TouchControllerBridge.tryMenuPrincipal(menuPrincipal);
        int back = TouchControllerBridge.tryBackPulsados();
        // Un BACK de antes de llegar al menu principal no cuenta.
        boolean pulsoBack = visible && salirVisible && backVistos >= 0 && back > backVistos;
        backVistos = back;
        boolean cambio = visible != salirVisible;
        if (cambio) {
            salirVisible = visible;
            salirConfirmarHasta = 0;
            removeCallbacks(finConfirmarSalir);
        }
        if (visible) {
            float izquierda = salirCaja.left, arriba = salirCaja.top;
            colocarSalir();
            cambio |= izquierda != salirCaja.left || arriba != salirCaja.top;
            if (!salirAnotado) {
                salirAnotado = true;
                anotarSalir();
            }
        }
        if (cambio) invalidate();
        if (pulsoBack) pulsarSalir();
    }

    /** Se dibuja en el menu principal, con el mando tactil a la vista o sin el. */
    private boolean salirALaVista() {
        return salirVisible && !editando;
    }

    /** Con el mando tactil a la vista solo se ve: para salir esta su BACK. */
    private boolean salirTocable() {
        return salirALaVista() && (!controlsVisible || conDedo());
    }

    private boolean tocaSalir(float x, float y) {
        float m = dp(8);
        return x >= salirCaja.left - m && x <= salirCaja.right + m
                && y >= salirCaja.top - m && y <= salirCaja.bottom + m;
    }

    /** La primera vez pide confirmar; la segunda, antes de SALIR_CONFIRMAR_MS, sale. */
    private void pulsarSalir() {
        long ahora = SystemClock.uptimeMillis();
        if (salirConfirmarHasta > ahora) {
            salir();
            return;
        }
        salirConfirmarHasta = ahora + SALIR_CONFIRMAR_MS;
        removeCallbacks(finConfirmarSalir);
        postDelayed(finConfirmarSalir, SALIR_CONFIRMAR_MS);
        colocarSalir();
        invalidate();
    }

    /**
     * Fuera del juego, a la pantalla de inicio: la de la partida se cierra (y con ella su
     * proceso, GameActivity.onDestroy), y debajo esta SetupActivity.
     */
    private void salir() {
        salirVisible = false;
        removeCallbacks(finConfirmarSalir);
        clearInput();
        invalidate();
        Context c = getContext();
        if (c instanceof android.app.Activity) ((android.app.Activity) c).finish();
    }

    private String textoSalirActual() {
        return salirConfirmarHasta > 0 ? textoSalirSeguro : textoSalir;
    }

    /**
     * Las medidas del boton en pixeles, desde lo que da el juego: su borde izquierdo y su
     * centro sobre la imagen del juego, y lo que mide una unidad del menu en cada eje. El
     * texto, a la altura de las mayusculas del juego y estirado como "Aceptar".
     */
    private void colocarSalir() {
        if (imagenJuego.isEmpty()) return;
        salirUx = menuPrincipal[2] * imagenJuego.width();
        salirUy = menuPrincipal[3] * imagenJuego.height();
        float x0 = imagenJuego.left + menuPrincipal[0] * imagenJuego.width();
        float cy = imagenJuego.top + menuPrincipal[1] * imagenJuego.height();
        salirTexto.setTextScaleX(1f);
        salirTexto.setStrokeWidth(0f);
        salirTexto.setTextSize(100f);
        android.graphics.Rect h = new android.graphics.Rect();
        salirTexto.getTextBounds("H", 0, 1, h);
        if (h.height() <= 0) return;
        salirTexto.setTextSize(100f * SALIR_MAYUSCULA * salirUy / h.height());
        float aceptar = salirTexto.measureText("Aceptar");
        if (aceptar > 0) salirTexto.setTextScaleX(SALIR_ANCHO_ACEPTAR * salirUx / aceptar);
        // Mas grueso: la del juego es negrita.
        salirTexto.setStrokeWidth(salirUy * .9f);
        float ancho = (SALIR_MARGEN + 2 * SALIR_RADIO + SALIR_HUECO + SALIR_COLA) * salirUx
                + salirTexto.measureText(textoSalirActual());
        float alto = SALIR_ALTO * salirUy;
        salirCaja.set(x0, cy - alto * .5f, x0 + ancho, cy + alto * .5f);
    }

    /** Como las indicaciones del juego: barra oscura con rayas, circulo con el boton y texto. */
    private void drawSalir(Canvas canvas) {
        RectF c = salirCaja;
        if (c.isEmpty()) return;
        salirPintura.setStyle(Paint.Style.FILL);
        salirPintura.setColor(SALIR_FONDO);
        canvas.drawRect(c, salirPintura);
        canvas.save();
        canvas.clipRect(c);
        salirPintura.setColor(SALIR_RAYA);
        float paso = SALIR_RAYA_PASO * salirUx;
        float raya = SALIR_RAYA_ANCHO * salirUx;
        float inclinacion = c.height();
        for (float x = c.left - inclinacion; x < c.right; x += paso) {
            salirFlecha.reset();
            salirFlecha.moveTo(x, c.bottom);
            salirFlecha.lineTo(x + raya, c.bottom);
            salirFlecha.lineTo(x + raya + inclinacion, c.top);
            salirFlecha.lineTo(x + inclinacion, c.top);
            salirFlecha.close();
            canvas.drawPath(salirFlecha, salirPintura);
        }
        canvas.restore();
        // El icono: el boton BACK del mando, un circulo con una flecha a la izquierda.
        float cx = c.left + (SALIR_MARGEN + SALIR_RADIO) * salirUx;
        float cy = c.centerY();
        float rx = SALIR_RADIO * salirUx;
        float ry = SALIR_RADIO * salirUy;
        salirPintura.setColor(SALIR_CREMA);
        salirPintura.setStyle(Paint.Style.STROKE);
        salirPintura.setStrokeWidth(SALIR_TRAZO * salirUy);
        float t = SALIR_TRAZO * salirUy * .5f;
        canvas.drawOval(cx - rx + t, cy - ry + t, cx + rx - t, cy + ry - t, salirPintura);
        salirPintura.setStyle(Paint.Style.FILL);
        float fx = rx * .45f;
        float fy = ry * .45f;
        salirFlecha.reset();
        salirFlecha.moveTo(cx - fx, cy);
        salirFlecha.lineTo(cx + fx * .75f, cy - fy);
        salirFlecha.lineTo(cx + fx * .75f, cy + fy);
        salirFlecha.close();
        canvas.drawPath(salirFlecha, salirPintura);
        // El texto, con las mayusculas centradas en la barra.
        float base = cy + SALIR_MAYUSCULA * salirUy * .5f;
        canvas.drawText(textoSalirActual(), cx + rx + SALIR_HUECO * salirUx, base, salirTexto);
    }

    /** Diagnostico: donde se puso el boton, en files/logs/menus.txt (una vez por partida). */
    private void anotarSalir() {
        java.io.File carpeta = getContext().getExternalFilesDir("logs");
        if (carpeta == null) return;
        java.io.File f = new java.io.File(carpeta, "menus.txt");
        boolean seguir = menusAnotados && f.length() < MAX_DIAGNOSTICO_MENUS;
        menusAnotados = true;
        try (java.io.FileWriter w = new java.io.FileWriter(f, seguir)) {
            w.write(new java.text.SimpleDateFormat("HH:mm:ss.SSS", java.util.Locale.ROOT)
                    .format(new java.util.Date())
                    + String.format(java.util.Locale.ROOT,
                            " menu principal: salir u=%.4f v=%.4f unidad=%.5f/%.5f marco=%.0fx%.0f caja=%s\n",
                            menuPrincipal[0], menuPrincipal[1], menuPrincipal[2], menuPrincipal[3],
                            imagenJuego.width(), imagenJuego.height(), salirCaja.toShortString()));
        } catch (java.io.IOException ignorada) {
            // Solo es diagnostico.
        }
    }

    // ------------------------------------------------------------------------
    //  Editor
    // ------------------------------------------------------------------------

    /** Un dedo cada vez: agarra un control y lo lleva, o pulsa la barra. */
    private boolean tocarEditor(MotionEvent event) {
        int action = event.getActionMasked();
        int index = event.getActionIndex();
        float x = event.getX(index);
        float y = event.getY(index);
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
                if (dedoEditor != -1) return true;
                if (tamMenos.contains(x, y)) {
                    cambiarEscala(-PASO);
                } else if (tamMas.contains(x, y)) {
                    cambiarEscala(PASO);
                } else if (opaMenos.contains(x, y)) {
                    cambiarOpacidad(-PASO);
                } else if (opaMas.contains(x, y)) {
                    cambiarOpacidad(PASO);
                } else if (disposicion.contains(x, y)) {
                    cambiarDisposicionEditada();
                } else if (restaurar.contains(x, y)) {
                    restaurarDisposicion();
                } else if (cancelar.contains(x, y)) {
                    cancelarEdicion();
                } else if (listo.contains(x, y)) {
                    guardarEdicion();
                } else {
                    // El de encima primero, como al jugar. Tocar donde no hay
                    // nada deja sin elegir, y TAMANO vuelve a ir para todos.
                    elegido = null;
                    for (int i = controls.size() - 1; i >= 0; --i) {
                        if (!controls.get(i).oculto() && controls.get(i).contains(x, y)) {
                            elegido = controls.get(i);
                            break;
                        }
                    }
                    if (elegido != null) {
                        dedoEditor = event.getPointerId(index);
                        agarreX = x - elegido.cx;
                        agarreY = y - elegido.cy;
                    }
                    invalidate();
                }
                return true;
            case MotionEvent.ACTION_MOVE: {
                int i = dedoEditor == -1 ? -1 : event.findPointerIndex(dedoEditor);
                if (i < 0 || elegido == null) return true;
                // El centro no sale de la pantalla: siempre se puede volver a coger.
                elegido.situar(limitar(event.getX(i) - agarreX, 0, lastWidth),
                               limitar(event.getY(i) - agarreY, 0, lastHeight));
                elegido.personalizado = true;
                invalidate();
                return true;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                if (event.getPointerId(index) == dedoEditor) dedoEditor = -1;
                return true;
            case MotionEvent.ACTION_CANCEL:
                dedoEditor = -1;
                return true;
            default:
                return true;
        }
    }

    private void cambiarEscala(float paso) {
        if (elegido != null) {
            escalar(elegido, paso);
        } else {
            for (Control c : controls) {
                if (!c.oculto()) escalar(c, paso);
            }
        }
        invalidate();
    }

    private void escalar(Control c, float paso) {
        c.escala = limitar(Math.round((c.escala + paso) * 10f) / 10f, ESCALA_MIN, ESCALA_MAX);
        c.personalizado = true;
        c.colocar();
    }

    private void cambiarOpacidad(float paso) {
        opacidad = limitar(Math.round((opacidad + paso) * 10f) / 10f, OPACIDAD_MIN, 1f);
        invalidate();
    }

    /** La que se edita, como de fabrica. No se guarda hasta LISTO. */
    private void restaurarDisposicion() {
        for (Control c : controls) {
            c.propia[dispEditada] = false;
            c.usar(dispEditada);
        }
        if (dispEditada == NORMAL) opacidad = 1f;
        elegido = null;
        invalidate();
    }

    /**
     * NORMAL -> CONDUCIENDO -> ACELERACION -> NORMAL: lo editado en una se
     * queda (sin guardar) al pasar a la siguiente.
     */
    private void cambiarDisposicionEditada() {
        for (Control c : controls) c.guardarEn(dispEditada);
        dispEditada = dispEditada == NORMAL ? CONDUCIENDO : dispEditada == CONDUCIENDO ? ACELERACION : NORMAL;
        elegido = null;
        dedoEditor = -1;
        usarDisposicion(dispEditada);
    }

    /** Sin guardar nada. */
    private void cancelarEdicion() {
        terminarEdicion();
    }

    /**
     * En la partida: el editor sobre el juego, que se queda quieto. Todos los
     * controles a la vista, en su forma de siempre (tambien en una carrera de
     * aceleracion), y nada de lo que se toque llega al juego.
     */
    private void abrirEditor() {
        clearInput();
        TouchControllerBridge.tryPausarJuego(true);
        GameActivity.callarVibracion(GameActivity.CALLADA_EDITOR, true);
        visiblesAntesDeEditar = controlsVisible;
        removeCallbacks(ocultarToggle);
        toggleVisible = false;
        controlsVisible = true;
        editando = true;
        dispEditada = dispJuego;
        elegido = null;
        dedoEditor = -1;
        rehacer();
    }

    /**
     * LISTO y CANCELAR. En EditorTactilActivity la cierran; en la partida se
     * vuelve a jugar con lo guardado: se rehacen los controles desde las
     * preferencias, asi que lo que se cancelo no queda.
     */
    private void terminarEdicion() {
        if (!enPartida) {
            alTerminar.run();
            return;
        }
        editando = false;
        elegido = null;
        dedoEditor = -1;
        opacidad = preferences.getFloat(PREF_OPACIDAD, 1f);
        controlsVisible = visiblesAntesDeEditar;
        rehacer();
        mostrarToggle();
        TouchControllerBridge.tryPausarJuego(false);
        GameActivity.callarVibracion(GameActivity.CALLADA_EDITOR, false);
    }

    /**
     * Con commit() y no apply(): la partida va en otro proceso y lee el
     * fichero al arrancar; tiene que estar ya en disco.
     */
    private void guardarEdicion() {
        for (Control c : controls) c.guardarEn(dispEditada);
        SharedPreferences.Editor e = preferences.edit();
        for (Control c : controls) {
            for (int d = 0; d < DISPOSICIONES; ++d) {
                String k = clave(c, d);
                if (c.propia[d]) {
                    e.putFloat(k + "_x", c.px[d] / lastWidth);
                    e.putFloat(k + "_y", c.py[d] / lastHeight);
                    e.putFloat(k + "_s", c.ps[d]);
                } else {
                    e.remove(k + "_x").remove(k + "_y").remove(k + "_s");
                }
            }
        }
        e.putFloat(PREF_OPACIDAD, opacidad).commit();
        terminarEdicion();
    }

    private static float limitar(float v, float min, float max) {
        return Math.max(min, Math.min(max, v));
    }

    // ------------------------------------------------------------------------

    private void setControlsVisible(boolean visible) {
        controlsVisible = visible;
        // Con los controles fuera no queda nada pisado.
        if (!visible) soltarCruceroYGestos();
        // Con mando no se guarda: la preferencia es la de jugar sin el.
        if (!mandoFisico) preferences.edit().putBoolean(PREF_VISIBLE, visible).apply();
        rehacer();
        // Recien usado: se queda los TOGGLE_MS, por si se quiere deshacer.
        mostrarToggle();
    }

    /** Suelta todo y rehace los controles (el boton cambia de ancho). */
    private void rehacer() {
        for (Control control : controls) control.release();
        pointers.clear();
        if (lastWidth > 0 && lastHeight > 0) buildControls(lastWidth, lastHeight);
        sendState();
        invalidate();
    }

    /** Se piso o se solto una flecha: conduciendo con flechas, el giro empieza de nuevo. */
    private void flechaCambiada() {
        if (!flechasAlConducir) return;
        removeCallbacks(rampaFlechas);
        pasoFlechas = SystemClock.uptimeMillis();
        if (avanzarGiroFlechas()) postOnAnimation(rampaFlechas);
    }

    /**
     * Un paso del giro con flechas (ver GIRO_FLECHA_MS) y se manda al juego.
     * true mientras siga subiendo.
     */
    private boolean avanzarGiroFlechas() {
        int hacia = disposicion() != CONDUCIENDO ? 0
                : (carrilDer.inUse ? 1 : 0) - (carrilIzq.inUse ? 1 : 0);
        long ahora = SystemClock.uptimeMillis();
        // Acotado: si un fotograma tarda, que no salte de golpe al tope.
        long dt = Math.min(ahora - pasoFlechas, 50);
        pasoFlechas = ahora;
        float antes = giroFlechas;
        if (hacia == 0) {
            giroFlechas = 0;
        } else if (Math.signum(giroFlechas) != hacia) {
            // Acaba de pisarse, o se paso al otro lado.
            giroFlechas = hacia * GIRO_FLECHA_INICIAL;
        } else {
            giroFlechas = hacia * Math.min(1f, Math.abs(giroFlechas)
                    + dt * (1f - GIRO_FLECHA_INICIAL) / GIRO_FLECHA_MS);
        }
        if (giroFlechas != antes) sendState();
        return hacia != 0 && Math.abs(giroFlechas) < 1f;
    }

    private void sendState() {
        // En el editor no hay juego al que mandarle nada.
        if (editando) return;
        if (!controlsVisible) {
            backTactil = false;
            TouchControllerBridge.trySetState(-1, 0, 0, 0, 0, 0, 0);
            return;
        }
        // Los gestos pulsan A (freno de mano, hacia abajo) y B (nitro, hacia
        // arriba) y los dejan con el color de pulsado, que se ve en pantalla.
        if (botonA != null) botonA.forzado = gestoFrenoMano;
        if (botonB != null) botonB.forzado = gestoNitro;
        int pisado = pulso | mantenido;
        int buttons = pisado & ~(PULSO_LT | PULSO_RT);
        for (Control control : controls) buttons |= control.buttons();
        // BACK en el menu principal: el boton Salir (y el juego lo recibe igual, como el fisico).
        boolean back = (buttons & BACK) != 0;
        boolean pulsoBack = back && !backTactil;
        backTactil = back;
        if (pulsoBack && salirALaVista()) post(this::pulsarSalir);
        float lx = leftStick == null ? 0 : leftStick.xValue;
        float ly = leftStick == null ? 0 : leftStick.yValue;
        if (carrilIzq != null && disposicion() == CONDUCIENDO && flechasAlConducir) {
            // El stick no esta: las flechas, poco a poco (avanzarGiroFlechas).
            lx = giroFlechas;
            ly = 0;
        } else if (disposicion() == ACELERACION && carrilIzq != null) {
            // El stick no esta: las flechas lo llevan entero a un lado (las dos, al centro).
            lx = (carrilDer.inUse ? 1 : 0) - (carrilIzq.inUse ? 1 : 0);
            ly = 0;
        } else if (inclinacionActiva()) {
            // Ni el stick: lo que se inclina el movil.
            lx = giroInclinado;
            ly = 0;
        }
        // El acelerador: el pedal, el crucero (el pedal bloqueado con el candado)
        // o la nitro, que lo pisa mientras dura. Ni el crucero ni la nitro cuentan
        // al frenar (los dos gatillos a la vez no frenan ni dan marcha atras), en
        // pausa ni fuera de la conduccion (menus, cinematica).
        boolean crucero = aceleradorBloqueado && formasDeConducir() && !enPausa && !enMenu
                && !(leftTrigger != null && leftTrigger.inUse);
        boolean nitro = nitroPisaAcelerador();
        TouchControllerBridge.trySetState(
            buttons,
            lx,
            ly,
            stickArrastreX != 0 || stickArrastreY != 0 ? stickArrastreX : rightStick == null ? 0 : rightStick.xValue,
            stickArrastreX != 0 || stickArrastreY != 0 ? stickArrastreY : rightStick == null ? 0 : rightStick.yValue,
            (leftTrigger != null && leftTrigger.inUse) || (pisado & PULSO_LT) != 0 ? 1 : 0,
            (rightTrigger != null && (rightTrigger.inUse || crucero || nitro))
                    || (pisado & PULSO_RT) != 0 ? 1 : 0);
    }

    /**
     * Mas estricto que SDLControllerManager.isDeviceSDLJoystick, que cuenta
     * tambien lo que solo tiene cruceta (mandos de tele, algunos teclados):
     * aqui solo gamepads y joysticks de verdad, y nada virtual.
     */
    private static boolean esMando(int deviceId) {
        InputDevice d = InputDevice.getDevice(deviceId);
        return d != null && !d.isVirtual() && esFuenteDeMando(d.getSources());
    }

    /** Hay un mando fisico; quitado: un USB que se acaba de desconectar. */
    private boolean hayMandoFisico(UsbDevice quitado) {
        for (int deviceId : InputDevice.getDeviceIds()) {
            if (esMando(deviceId)) return true;
        }
        if (usbManager != null) {
            for (UsbDevice d : usbManager.getDeviceList().values()) {
                if (!d.equals(quitado) && esXboxUsb(d)) return true;
            }
        }
        return false;
    }

    private void revisarMandos(UsbDevice quitado) {
        boolean hay = hayMandoFisico(quitado);
        if (hay && !mandoFisico) entrarModoMando();
        else if (!hay && mandoFisico) salirModoMando();
    }

    /**
     * Un mando de Xbox 360 o One/Series por USB: la interfaz con la que lo reconoce
     * el driver HIDAPI de SDL (HIDDeviceManager), de clase propia del fabricante.
     */
    static boolean esXboxUsb(UsbDevice d) {
        for (int i = 0; i < d.getInterfaceCount(); i++) {
            UsbInterface u = d.getInterface(i);
            if (u.getInterfaceClass() != UsbConstants.USB_CLASS_VENDOR_SPEC) continue;
            int subclase = u.getInterfaceSubclass();
            int protocolo = u.getInterfaceProtocol();
            if ((subclase == 71 && protocolo == 208)
                    || (subclase == 93 && (protocolo == 1 || protocolo == 129))) {
                return true;
            }
        }
        return false;
    }

    /**
     * Un control: su centro (cx, cy) y su escala, que puede cambiar el editor.
     * Cada subclase guarda su forma ya calculada y la rehace en colocar(), asi
     * que dibujar no crea objetos.
     */
    private abstract class Control {
        final String id;
        // La posicion de fabrica, para RESTAURAR.
        final float defX;
        final float defY;
        float cx;
        float cy;
        float escala = 1f;
        // Movido o cambiado de tamano en el editor: se guarda.
        boolean personalizado;
        boolean inUse;
        // Lo pulsa un gesto (nitro o freno de mano), no un dedo en el boton.
        boolean forzado;
        // Las dos disposiciones, NORMAL y ACELERACION: lo guardado con el editor
        // (centro, escala) y si lo hay; sin eso, la de fabrica.
        final float[] px = new float[3];
        final float[] py = new float[3];
        final float[] ps = {1f, 1f, 1f};
        final boolean[] propia = new boolean[3];
        // La de fabrica en cada disposicion; sin poner, la normal.
        final float[] fx = new float[3];
        final float[] fy = new float[3];
        final float[] fs = {1f, 1f, 1f};
        Control(String id, float x, float y) {
            this.id = id;
            defX = cx = x;
            defY = cy = y;
            java.util.Arrays.fill(fx, x);
            java.util.Arrays.fill(fy, y);
        }
        void porDefecto(int d, float x, float y, float s) {
            fx[d] = x;
            fy[d] = y;
            fs[d] = s;
        }
        /** Donde va en la disposicion d. */
        void usar(int d) {
            if (propia[d]) {
                cx = px[d];
                cy = py[d];
                escala = ps[d];
            } else {
                cx = fx[d];
                cy = fy[d];
                escala = fs[d];
            }
            personalizado = propia[d];
            colocar();
        }
        /** Lo que se ha hecho en el editor, a la disposicion d. */
        void guardarEn(int d) {
            px[d] = cx;
            py[d] = cy;
            ps[d] = escala;
            propia[d] = personalizado;
        }
        /** Recalcula la forma tras moverlo o cambiarle la escala. */
        abstract void colocar();
        void situar(float x, float y) { cx = x; cy = y; colocar(); }
        /**
         * Lo que ocupa, para el recuadro del editor. No un cuadrado: el del
         * acelerador, alto y estrecho, tapaba medio freno.
         */
        abstract void ocupa(RectF r);
        abstract boolean contains(float x, float y);
        void press(float x, float y) { inUse = true; move(x, y); }
        void move(float x, float y) {}
        void release() { inUse = false; }
        int buttons() { return 0; }
        // En que disposiciones se ve (EN_NORMAL...): L3 y R3 solo en la normal,
        // las flechas de carril solo en la de aceleracion...
        int visibleEn = EN_TODAS;
        /** No se ve ni se puede tocar en esta parte del juego. */
        boolean oculto() {
            return (visibleEn & (1 << disposicion())) == 0;
        }
        abstract void draw(Canvas canvas);
    }

    /** La disposicion en uso: la de la parte del juego, o la que se edita. */
    private int disposicion() {
        return editando ? dispEditada : dispJuego;
    }

    /** Pedales e iconos: conduciendo y en las carreras de aceleracion. */
    private boolean formasDeConducir() {
        return disposicion() != NORMAL;
    }

    private final class ButtonControl extends Control {
        final String text;
        final float baseRadius;
        final int mask;
        float radius;
        ButtonControl(String id, String text, float x, float y, float radius, int mask) {
            super(id, x, y);
            this.text = text; this.baseRadius = radius; this.mask = mask;
            colocar();
        }
        void colocar() { radius = baseRadius * escala; }
        void ocupa(RectF r) { r.set(cx - radius, cy - radius, cx + radius, cy + radius); }
        boolean contains(float px, float py) {
            float dx = px - cx, dy = py - cy;
            return dx * dx + dy * dy <= radius * radius * 1.35f;
        }
        int buttons() { return (inUse || forzado) ? mask : 0; }
        // En las carreras (conduciendo y aceleracion), un dibujo de lo que hace en vez de la letra.
        int icono = SIN_ICONO;
        void draw(Canvas canvas) {
            drawCircle(canvas, cx, cy, radius, inUse || forzado);
            if (icono == ICONO_NITRO && formasDeConducir()) {
                dibujarNitro(canvas, cx, cy, radius);
            } else if (icono == ICONO_FRENO_MANO && formasDeConducir()) {
                dibujarFrenoMano(canvas, cx, cy, radius);
            } else if (icono == ICONO_RELOJ && formasDeConducir()) {
                dibujarReloj(canvas, cx, cy, radius);
            } else {
                drawLabel(canvas, text, cx, cy, radius * .72f, opacidad);
            }
        }
    }

    private class RectButton extends Control {
        final String text;
        final float baseWidth;
        final float baseHeight;
        final int mask;
        final RectF bounds = new RectF();
        // En las carreras de aceleracion, un dibujo en vez del texto.
        int icono = SIN_ICONO;
        RectButton(String id, String text, float x, float y, float width, float height, int mask) {
            super(id, x, y);
            this.text = text; this.baseWidth = width; this.baseHeight = height; this.mask = mask;
            colocar();
        }
        void colocar() {
            float w = baseWidth * escala * .5f, h = baseHeight * escala * .5f;
            bounds.set(cx - w, cy - h, cx + w, cy + h);
        }
        void ocupa(RectF r) { r.set(bounds); }
        boolean contains(float x, float y) { return bounds.contains(x, y); }
        int buttons() { return inUse ? mask : 0; }
        void draw(Canvas canvas) {
            float r = Math.min(dp(10), bounds.height() * .5f);
            fill.setColor(conOpacidad(inUse ? PULSADO : RELLENO));
            stroke.setColor(conOpacidad(BORDE));
            canvas.drawRoundRect(bounds, r, r, fill);
            canvas.drawRoundRect(bounds, r, r, stroke);
            if (icono == ICONO_CAMARA && formasDeConducir()) {
                dibujarCamara(canvas, bounds);
            } else if (icono == ICONO_RETROVISOR && formasDeConducir()) {
                dibujarRetrovisor(canvas, bounds);
            } else if (icono == ICONO_POSICIONES && formasDeConducir()) {
                dibujarPosiciones(canvas, bounds);
            } else {
                drawLabel(canvas, text, bounds.centerX(), bounds.centerY(), dp(11) * escala, opacidad);
            }
        }
    }

    /**
     * LT y RT. En las carreras de aceleracion, pedales: de pie, mas altos que
     * anchos, en el mismo sitio, y tocan en todo lo que ocupan.
     */
    private final class Trigger extends RectButton {
        final boolean freno;
        final RectF pedal = new RectF();
        // El candado del acelerador (bloqueo del pedal), en su esquina.
        final RectF zonaCandado = new RectF();
        Trigger(String id, String text, float x, float y, float width, float height, boolean freno) {
            super(id, text, x, y, width, height, 0);
            this.freno = freno;
            colocar();
        }
        @Override
        void colocar() {
            super.colocar();
            // El constructor de RectButton llama a colocar() antes de que exista el pedal.
            if (pedal == null) return;
            // El freno, mas ancho y mas bajo; el acelerador, alto y estrecho.
            float ancho = baseWidth * escala * (freno ? .74f : .6f);
            float alto = baseWidth * escala * (freno ? .9f : 1.15f);
            // Que no se salga de la pantalla: de fabrica estan arriba del todo.
            float x = limitar(cx, ancho * .5f + dp(4), lastWidth - ancho * .5f - dp(4));
            float y = limitar(cy, alto * .5f + dp(4), lastHeight - alto * .5f - dp(4));
            pedal.set(x - ancho * .5f, y - alto * .5f, x + ancho * .5f, y + alto * .5f);
            // El candado: la esquina de arriba a la izquierda, y de su tamano.
            float lado = limitar(pedal.width() * .42f, dp(20), pedal.width());
            zonaCandado.set(pedal.left, pedal.top, pedal.left + lado, pedal.top + lado);
        }
        @Override
        boolean contains(float x, float y) {
            return formasDeConducir() ? pedal.contains(x, y) : super.contains(x, y);
        }
        /** El dedo ha caido en el candado del acelerador. */
        boolean tocaCandado(float x, float y) {
            return !freno && !editando && formasDeConducir() && zonaCandado.contains(x, y);
        }
        @Override
        void ocupa(RectF r) {
            if (formasDeConducir()) {
                r.set(pedal);
            } else {
                super.ocupa(r);
            }
        }
        @Override
        void draw(Canvas canvas) {
            if (formasDeConducir()) {
                dibujarPedal(canvas, pedal, freno,
                             inUse || (!freno && (aceleradorBloqueado || nitroPisaAcelerador())));
                if (!freno && !editando) dibujarCandado(canvas, zonaCandado, aceleradorBloqueado);
            } else {
                super.draw(canvas);
            }
        }
    }

    private final class Stick extends Control {
        final String text;
        final float baseRadius;
        float radius;
        float xValue;
        float yValue;
        // El derecho: en las carreras de aceleracion, la palanca de cambios.
        // Conduciendo es el stick de siempre, como antes de esa disposicion.
        boolean palanca;
        final RectF ranura = new RectF();
        // El izquierdo, flotante (como en los juegos de movil): el centro se
        // pone donde caiga el dedo, y si el dedo se aleja mas del radio el
        // centro se va con el. Asi, girando, el volante nunca se queda clavado
        // en un lado: mientras el dedo siga puesto, se sigue girando.
        float origenX;
        float origenY;
        boolean flotante;
        Stick(String id, String text, float x, float y, float radius) {
            super(id, x, y);
            this.text = text; this.baseRadius = radius;
            colocar();
        }
        void colocar() { radius = baseRadius * escala; }
        boolean esPalanca() { return palanca && disposicion() == ACELERACION; }
        void ocupa(RectF r) {
            float ancho = esPalanca() ? radius * .8f : radius;
            float alto = esPalanca() ? radius * 1.3f : radius;
            r.set(cx - ancho, cy - alto, cx + ancho, cy + alto);
        }
        /** El dedo ha caido en la zona del volante: el stick se pone ahi. */
        void agarrarFlotante(float x, float y) {
            origenX = x;
            origenY = y;
            flotante = true;
            inUse = true;
        }
        float centroX() { return flotante ? origenX : cx; }
        float centroY() { return flotante ? origenY : cy; }
        boolean contains(float x, float y) {
            if (esPalanca()) {
                return Math.abs(x - cx) <= radius * .8f && Math.abs(y - cy) <= radius * 1.3f;
            }
            float dx = x - cx, dy = y - cy;
            return dx * dx + dy * dy <= radius * radius * 1.55f;
        }
        void move(float x, float y) {
            if (esPalanca()) {
                // Arriba sube marcha y abajo la baja: el juego cambia cuando el
                // stick cruza su umbral, asi que va entero o en el centro. Tocar
                // ya en un extremo tambien cambia.
                float dy = (y - cy) / radius;
                xValue = 0;
                yValue = dy < -.3f ? -1 : dy > .3f ? 1 : 0;
                return;
            }
            // Flotante: si el dedo se pasa del radio, el centro se acerca hasta
            // dejarlo justo en el borde. El volante sigue teniendo recorrido
            // hacia el otro lado sin levantar el dedo.
            if (flotante) {
                float dxOrigen = x - origenX;
                float dyOrigen = y - origenY;
                float distancia = (float) Math.sqrt(dxOrigen * dxOrigen + dyOrigen * dyOrigen);
                if (distancia > radius) {
                    float sobra = (distancia - radius) / distancia;
                    origenX += dxOrigen * sobra;
                    origenY += dyOrigen * sobra;
                }
            }
            float dx = (x - centroX()) / radius;
            float dy = (y - centroY()) / radius;
            float length = (float) Math.sqrt(dx * dx + dy * dy);
            if (length > 1) { dx /= length; dy /= length; }
            float deadzone = .08f;
            // Y como SDL: positivo hacia ABAJO. El SDK le da la vuelta al pasarlo
            // al mando de la Xbox (thumb_ly = ~valor en sdl_input_driver.cpp),
            // igual que con un mando fisico. Con -dy los dos sticks salian
            // invertidos en vertical.
            xValue = Math.abs(dx) < deadzone ? 0 : dx;
            yValue = Math.abs(dy) < deadzone ? 0 : dy;
        }
        void release() { super.release(); xValue = 0; yValue = 0; flotante = false; }
        void draw(Canvas canvas) {
            if (esPalanca()) {
                dibujarPalanca(canvas);
                return;
            }
            float baseX = centroX();
            float baseY = centroY();
            drawCircle(canvas, baseX, baseY, radius, false);
            float knobX = baseX + xValue * radius * .58f;
            float knobY = baseY + yValue * radius * .58f;
            drawCircle(canvas, knobX, knobY, radius * .43f, inUse);
            drawLabel(canvas, text, knobX, knobY, radius * .34f, opacidad);
        }
        /** Una ranura vertical con el pomo: + arriba, - abajo. */
        void dibujarPalanca(Canvas canvas) {
            float ancho = radius * .36f;
            ranura.set(cx - ancho * .5f, cy - radius, cx + ancho * .5f, cy + radius);
            fill.setColor(conOpacidad(RELLENO));
            stroke.setColor(conOpacidad(BORDE));
            canvas.drawRoundRect(ranura, ancho * .5f, ancho * .5f, fill);
            canvas.drawRoundRect(ranura, ancho * .5f, ancho * .5f, stroke);
            drawLabel(canvas, "+", cx + radius * .55f, cy - radius * .7f, radius * .42f, opacidad);
            drawLabel(canvas, "\u2212", cx + radius * .55f, cy + radius * .7f, radius * .42f, opacidad);
            drawCircle(canvas, cx, cy + yValue * radius * .72f, radius * .36f, inUse);
        }
    }

    /**
     * Cambiar de carril en las carreras de aceleracion: un boton cuadrado con
     * una flecha. Pulsado, el stick izquierdo va entero a ese lado
     * (sendState); no es un boton del mando.
     */
    private final class Flecha extends Control {
        final float baseLado;
        final int lado;  // -1 izquierda, 1 derecha
        final RectF caja = new RectF();
        final Path flecha = new Path();
        Flecha(String id, float x, float y, float baseLado, int lado) {
            super(id, x, y);
            this.baseLado = baseLado;
            this.lado = lado;
            visibleEn = EN_ACELERACION;
            colocar();
        }
        void colocar() {
            float m = baseLado * escala * .5f;
            caja.set(cx - m, cy - m, cx + m, cy + m);
            // Un triangulo con la punta hacia su lado.
            float t = m * .42f;
            flecha.reset();
            flecha.moveTo(cx + lado * t, cy);
            flecha.lineTo(cx - lado * t * .75f, cy - t);
            flecha.lineTo(cx - lado * t * .75f, cy + t);
            flecha.close();
        }
        void ocupa(RectF r) { r.set(caja); }
        boolean contains(float x, float y) { return caja.contains(x, y); }
        @Override
        void press(float x, float y) {
            super.press(x, y);
            flechaCambiada();
        }
        @Override
        void release() {
            super.release();
            flechaCambiada();
        }
        void draw(Canvas canvas) {
            float r = Math.min(dp(10), caja.height() * .2f);
            fill.setColor(conOpacidad(inUse ? PULSADO : RELLENO));
            stroke.setColor(conOpacidad(BORDE));
            canvas.drawRoundRect(caja, r, r, fill);
            canvas.drawRoundRect(caja, r, r, stroke);
            fill.setColor(conOpacidad(BORDE));
            canvas.drawPath(flecha, fill);
        }
    }

    private final class Dpad extends Control {
        final float baseRadius;
        float radius;
        // La forma no cambia al jugar, solo el color: se calcula en colocar()
        // en vez de crear dos RectF nuevos en cada dibujado.
        final RectF vertical = new RectF();
        final RectF horizontal = new RectF();
        int mask;
        Dpad(String id, float x, float y, float radius) {
            super(id, x, y);
            this.baseRadius = radius;
            colocar();
        }
        void colocar() {
            radius = baseRadius * escala;
            float arm = radius * .36f;
            vertical.set(cx - arm, cy - radius, cx + arm, cy + radius);
            horizontal.set(cx - radius, cy - arm, cx + radius, cy + arm);
        }
        void ocupa(RectF r) { r.set(cx - radius, cy - radius, cx + radius, cy + radius); }
        boolean contains(float x, float y) {
            return Math.abs(x - cx) <= radius && Math.abs(y - cy) <= radius;
        }
        void move(float x, float y) {
            float dx = x - cx, dy = y - cy;
            float threshold = radius * .20f;
            mask = 0;
            if (dx < -threshold) mask |= DPAD_LEFT;
            if (dx > threshold) mask |= DPAD_RIGHT;
            if (dy < -threshold) mask |= DPAD_UP;
            if (dy > threshold) mask |= DPAD_DOWN;
        }
        void release() { super.release(); mask = 0; }
        int buttons() { return inUse ? mask : 0; }
        void draw(Canvas canvas) {
            fill.setColor(conOpacidad(inUse ? PULSADO_CRUCETA : RELLENO));
            stroke.setColor(conOpacidad(BORDE));
            float r = dp(6);
            canvas.drawRoundRect(vertical, r, r, fill);
            canvas.drawRoundRect(horizontal, r, r, fill);
            canvas.drawRoundRect(vertical, r, r, stroke);
            canvas.drawRoundRect(horizontal, r, r, stroke);
        }
    }

    /** Un pedal con estrias de goma. Pisado, la placa se hunde un poco. */
    private void dibujarPedal(Canvas canvas, RectF r, boolean freno, boolean pisadoAhora) {
        pisado.set(r);
        if (pisadoAhora) pisado.inset(r.width() * .05f, r.height() * .05f);
        float radio = pisado.width() * (freno ? .18f : .28f);
        fill.setColor(conOpacidad(pisadoAhora ? PULSADO : RELLENO));
        stroke.setColor(conOpacidad(BORDE));
        canvas.drawRoundRect(pisado, radio, radio, fill);
        canvas.drawRoundRect(pisado, radio, radio, stroke);
        int estrias = freno ? 4 : 6;
        float margen = pisado.width() * .2f;
        float paso = pisado.height() / (estrias + 1);
        for (int i = 1; i <= estrias; ++i) {
            float y = pisado.top + paso * i;
            canvas.drawLine(pisado.left + margen, y, pisado.right - margen, y, stroke);
        }
    }

    /**
     * El candado del acelerador, en su esquina: cerrado, el pedal esta
     * bloqueado y va pisado sin dedo; abierto, no. Se toca para cambiar.
     */
    private void dibujarCandado(Canvas canvas, RectF caja, boolean cerrado) {
        float r = caja.width() * .3f;
        fill.setColor(conOpacidad(cerrado ? PULSADO : FONDO_BOTON));
        stroke.setColor(conOpacidad(BORDE));
        canvas.drawRoundRect(caja, r, r, fill);
        canvas.drawRoundRect(caja, r, r, stroke);
        // El cuerpo del candado.
        float ancho = caja.width() * .46f;
        float alto = caja.height() * .32f;
        float x = caja.centerX();
        float y = caja.centerY() + caja.height() * .14f;
        trazo.set(x - ancho * .5f, y - alto * .5f, x + ancho * .5f, y + alto * .5f);
        canvas.drawRoundRect(trazo, alto * .3f, alto * .3f, stroke);
        // El arco: cerrado, encima del cuerpo; abierto, corrido a un lado.
        float radio = ancho * .32f;
        float desvio = cerrado ? 0f : ancho * .40f;
        trazo.set(x - radio + desvio, y - alto * .5f - radio * 1.7f,
                  x + radio + desvio, y - alto * .5f + radio * .3f);
        canvas.drawArc(trazo, 180, cerrado ? 180 : 130, false, stroke);
    }

    /** Una camara de fotos: el visor, el cuerpo y el objetivo. */
    private void dibujarCamara(Canvas canvas, RectF caja) {
        float alto = caja.height() * .5f;
        float ancho = alto * 1.45f;
        float x = caja.centerX();
        float y = caja.centerY() + alto * .06f;
        stroke.setColor(conOpacidad(BORDE));
        trazo.set(x - ancho * .16f, y - alto * .62f, x + ancho * .16f, y - alto * .4f);
        canvas.drawRoundRect(trazo, dp(2), dp(2), stroke);
        trazo.set(x - ancho * .5f, y - alto * .4f, x + ancho * .5f, y + alto * .5f);
        canvas.drawRoundRect(trazo, alto * .15f, alto * .15f, stroke);
        canvas.drawCircle(x, y + alto * .05f, alto * .26f, stroke);
    }

    /** La clasificacion, como su icono en el juego: tres puestos con su linea. */
    private void dibujarPosiciones(Canvas canvas, RectF caja) {
        float alto = caja.height() * .56f;
        float paso = alto / 3f;
        float x = caja.centerX() - alto * .5f;
        stroke.setColor(conOpacidad(BORDE));
        for (int i = 0; i < 3; ++i) {
            float y = caja.centerY() - alto * .5f + paso * (i + .5f);
            drawLabel(canvas, PUESTOS[i], x, y, paso * .85f, opacidad);
            canvas.drawLine(x + alto * .28f, y, x + alto * 1.05f, y, stroke);
        }
    }

    /** Un retrovisor interior: el brazo del techo, el espejo y un reflejo. */
    private void dibujarRetrovisor(Canvas canvas, RectF caja) {
        float alto = caja.height() * .42f;
        float ancho = Math.min(caja.width() * .7f, alto * 2.6f);
        float x = caja.centerX();
        float y = caja.centerY() + alto * .2f;
        stroke.setColor(conOpacidad(BORDE));
        canvas.drawLine(x, y - alto * .5f, x, y - alto * 1.05f, stroke);
        trazo.set(x - ancho * .5f, y - alto * .5f, x + ancho * .5f, y + alto * .5f);
        canvas.drawRoundRect(trazo, alto * .5f, alto * .5f, stroke);
        canvas.drawLine(x - ancho * .25f, y + alto * .2f, x - ancho * .05f, y - alto * .2f, stroke);
        canvas.drawLine(x - ancho * .05f, y + alto * .2f, x + ancho * .15f, y - alto * .2f, stroke);
    }

    /** Una botella de nitro: el cuerpo, el cuello y la valvula de arriba. */
    private void dibujarNitro(Canvas canvas, float x, float y, float r) {
        stroke.setColor(conOpacidad(BORDE));
        float ancho = r * .46f;
        float arriba = y - r * .2f;
        trazo.set(x - ancho * .5f, arriba, x + ancho * .5f, y + r * .52f);
        canvas.drawRoundRect(trazo, ancho * .3f, ancho * .3f, stroke);
        // El hombro de la botella hasta el cuello.
        trazo.set(x - r * .1f, y - r * .4f, x + r * .1f, arriba);
        canvas.drawRect(trazo, stroke);
        canvas.drawLine(x - r * .2f, y - r * .48f, x + r * .2f, y - r * .48f, stroke);
        // Una franja, como la etiqueta.
        canvas.drawLine(x - ancho * .5f, y + r * .12f, x + ancho * .5f, y + r * .12f, stroke);
    }

    /** El freno de mano como en el salpicadero: una P en un circulo, entre parentesis. */
    private void dibujarFrenoMano(Canvas canvas, float x, float y, float r) {
        stroke.setColor(conOpacidad(BORDE));
        float aro = r * .34f;
        canvas.drawCircle(x, y, aro, stroke);
        drawLabel(canvas, "P", x, y, aro * 1.25f, opacidad);
        float fuera = r * .52f;
        trazo.set(x - fuera, y - fuera, x + fuera, y + fuera);
        canvas.drawArc(trazo, 140, 80, false, stroke);
        canvas.drawArc(trazo, -40, 80, false, stroke);
    }

    /** Un reloj (el Speedbreaker): la esfera, dos agujas y el boton de arriba. */
    private void dibujarReloj(Canvas canvas, float x, float y, float r) {
        stroke.setColor(conOpacidad(BORDE));
        float esfera = r * .44f;
        float cy = y + r * .05f;
        canvas.drawCircle(x, cy, esfera, stroke);
        canvas.drawLine(x, cy, x, cy - esfera * .65f, stroke);
        canvas.drawLine(x, cy, x + esfera * .5f, cy, stroke);
        canvas.drawLine(x, cy - esfera, x, cy - esfera - r * .12f, stroke);
        canvas.drawLine(x - r * .1f, cy - esfera - r * .12f, x + r * .1f, cy - esfera - r * .12f, stroke);
    }

    private void drawCircle(Canvas canvas, float x, float y, float radius, boolean pressed) {
        fill.setColor(conOpacidad(pressed ? PULSADO : RELLENO));
        stroke.setColor(conOpacidad(BORDE));
        canvas.drawCircle(x, y, radius, fill);
        canvas.drawCircle(x, y, radius, stroke);
    }

    /** El color con su transparencia multiplicada por la opacidad del editor. */
    private int conOpacidad(int color) {
        return (color & 0x00FFFFFF) | (Math.round(Color.alpha(color) * opacidad) << 24);
    }

    private void drawLabel(Canvas canvas, String text, float x, float y, float size, float alfa) {
        label.setTextSize(size);
        label.setAlpha(Math.round(255 * alfa));
        label.getFontMetrics(metricas);
        canvas.drawText(text, x, y - (metricas.ascent + metricas.descent) / 2, label);
    }

    private float dp(float value) {
        return value * density;
    }
}
