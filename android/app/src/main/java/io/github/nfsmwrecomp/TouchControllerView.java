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

    // --- Segun la parte del juego.
    private static final long CONTEXTO_MS = 250;
    private static final int SIN_ICONO = 0;
    private static final int ICONO_CAMARA = 1;
    private static final int ICONO_RETROVISOR = 2;
    private static final int ICONO_POSICIONES = 3;
    private static final String[] PUESTOS = {"1", "2", "3"};
    // En una carrera de aceleracion: los controles cambian de forma.
    // La disposicion de la parte del juego en que se esta.
    private int dispJuego = NORMAL;
    // El juego en pausa: no se gira inclinando el movil.
    private boolean enPausa;

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
    private final Runnable mirarContexto = new Runnable() {
        @Override
        public void run() {
            int contexto = TouchControllerBridge.tryContexto();
            // En pausa (su menu, un mensaje...), la de conducir, que tiene la
            // cruceta para recorrer el menu (tambien si se pauso una carrera de
            // aceleracion, cuya disposicion no la tiene), pero sin girar
            // inclinando el movil: moveria las opciones.
            boolean pausa = contexto == TouchControllerBridge.CONTEXTO_PAUSA;
            int ahora = contexto == TouchControllerBridge.CONTEXTO_ACELERACION ? ACELERACION
                    : contexto == TouchControllerBridge.CONTEXTO_CONDUCIENDO || pausa ? CONDUCIENDO
                    : NORMAL;
            if (pausa != enPausa) {
                enPausa = pausa;
                // El stick, al centro en cuanto se pausa.
                sendState();
            }
            if (ahora != dispJuego) {
                dispJuego = ahora;
                // Todo suelto: un dedo que estaba en L3, que ahora no esta, o
                // en el stick, que ahora es la palanca, no se queda pulsado.
                clearInput();
                // Mientras se edita, la disposicion la elige el editor.
                if (!editando) usarDisposicion(dispJuego);
            }
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
        for (Control control : controls) control.release();
        pointers.clear();
        sendState();
        invalidate();
    }

    public void disconnect() {
        for (Control control : controls) control.release();
        pointers.clear();
        TouchControllerBridge.trySetState(-1, 0, 0, 0, 0, 0, 0);
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
        controls.clear();
        float unit = Math.min(width / 16f, height / 9f);
        float stickRadius = unit * 0.78f;
        float buttonRadius = unit * 0.38f;

        leftStick = new Stick("ls", "L", width * .16f, height * .73f, stickRadius);
        rightStick = new Stick("rs", "R", width * .67f, height * .77f, stickRadius);
        controls.add(leftStick);
        controls.add(rightStick);
        // En las carreras de aceleracion el stick izquierdo solo cambia de
        // carril: dos botones con flecha en su lugar.
        leftStick.visibleEn = inclinar ? EN_NORMAL : EN_NORMAL | EN_CONDUCIENDO;
        float ladoFlecha = unit * 1.15f;
        carrilIzq = new Flecha("carril_izq", width * .16f, height * .73f, ladoFlecha, -1);
        carrilDer = new Flecha("carril_der", width * .16f, height * .73f, ladoFlecha, 1);
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
        ButtonControl x = new ButtonControl("x", "X", faceX - unit * .68f, faceY, buttonRadius, X);
        ButtonControl y = new ButtonControl("y", "Y", faceX, faceY - unit * .68f, buttonRadius, Y);
        controls.add(a);
        controls.add(b);
        controls.add(x);
        controls.add(y);

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
        // hizo a mano en el movil (2688x1216), hecha simetrica:
        //   - abajo, sobre la misma linea (pie), los pedales a la derecha y las
        //     flechas de carril a la izquierda, cada flecha en el reflejo de su
        //     pedal (la izquierda del acelerador, la derecha del freno), y la
        //     palanca junto a las flechas;
        //   - a la derecha, los botones en rombo;
        //   - arriba a la izquierda, en rejilla: la clasificacion y la camara en
        //     una fila, y el retrovisor debajo de la camara;
        //   - arriba, BACK y START a los lados de OCULTAR/TACTIL y el engranaje,
        //     a la misma distancia.
        // En fracciones de la pantalla y tamanos en 'unit', como la normal.
        float pie = height * .885f;
        float pedal = 1.5f;
        float acelX = width * .885f;
        float frenoX = acelX - (shoulderW * pedal * (.6f + .74f) * .5f + unit * .35f);
        rightTrigger.porDefecto(ACELERACION, acelX, pie - shoulderW * pedal * 1.15f * .5f, pedal);
        leftTrigger.porDefecto(ACELERACION, frenoX, pie - shoulderW * pedal * .9f * .5f, pedal);
        float sobrePie = pie - ladoFlecha * .5f;
        carrilIzq.porDefecto(ACELERACION, width - acelX, sobrePie, 1f);
        carrilDer.porDefecto(ACELERACION, width - frenoX, sobrePie, 1f);
        rightStick.porDefecto(ACELERACION, width - frenoX + ladoFlecha * .5f + unit * .95f, pie - stickRadius, 1f);
        float rombo = unit * .82f;
        float romboX = width * .858f;
        float romboY = height * .44f;
        y.porDefecto(ACELERACION, romboX, romboY - rombo, 1f);
        a.porDefecto(ACELERACION, romboX, romboY + rombo, 1f);
        x.porDefecto(ACELERACION, romboX - rombo, romboY, 1f);
        b.porDefecto(ACELERACION, romboX + rombo, romboY, 1f);
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
        // a 1,5 veces su tamano; el derecho, un stick y no la palanca, a su
        // derecha y algo mas abajo; y la cruceta, que aqui hace mas que abrir la
        // clasificacion (abajo recoloca el coche), en el reflejo de los botones
        // en rombo. Los sticks, como los dejo el usuario en el movil (2688x1216).
        for (Control c : new Control[] {rightTrigger, leftTrigger, a, b, x, y, rb, lb,
                                        back, start}) {
            c.porDefecto(CONDUCIENDO, c.fx[ACELERACION], c.fy[ACELERACION], c.fs[ACELERACION]);
        }
        float pedalesIzq = frenoX - shoulderW * pedal * .74f * .5f;
        float pedalesDer = acelX + shoulderW * pedal * .6f * .5f;
        float stickIzqX = width - (pedalesIzq + pedalesDer) * .5f;
        leftStick.porDefecto(CONDUCIENDO, stickIzqX, pie - stickRadius, 1.5f);
        rightStick.porDefecto(CONDUCIENDO, stickIzqX + unit * 2.94f, pie - stickRadius + unit * .33f, 1f);
        dpad.porDefecto(CONDUCIENDO, width - romboX, romboY, 1f);

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

    private static String clave(Control c, int disposicion) {
        return PREFIJOS[disposicion] + c.id;
    }

    /** Cada control, donde va en esa disposicion. */
    private void usarDisposicion(int d) {
        for (Control c : controls) c.usar(d);
        invalidate();
    }

    /** OCULTAR/TACTIL arriba, y la barra del editor en el centro. */
    private void colocarBotones(float width, float height, float unit) {
        label.setTextSize(dp(11));
        // Lo que pida el texto, que en otros idiomas es mas largo que HIDE.
        float toggleWidth = Math.max(controlsVisible ? unit * 1.05f : unit * 1.35f,
                label.measureText(controlsVisible ? textoOcultar : textoMostrar) + dp(24));
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
                float r = elegido.alcance() + dp(6);
                canvas.drawRect(elegido.cx - r, elegido.cy - r, elegido.cx + r, elegido.cy + r, marco);
            }
            drawEditor(canvas);
            return;
        }
        if (toggleVisible) {
            drawToggle(canvas);
            drawEngranaje(canvas);
        }
        if (!controlsVisible) return;
        for (Control control : controls) {
            if (!control.oculto()) control.draw(canvas);
        }
    }

    private void drawToggle(Canvas canvas) {
        drawBoton(canvas, toggle, controlsVisible ? textoOcultar : textoMostrar, dp(11));
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

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (editando) return tocarEditor(event);

        int action = event.getActionMasked();
        int index = event.getActionIndex();
        int pointerId = event.getPointerId(index);
        float x = event.getX(index);
        float y = event.getY(index);

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            if (toggleVisible && engranaje.contains(x, y)) {
                abrirEditor();
                return true;
            }
            boolean enToggle = toggle.contains(x, y) || engranaje.contains(x, y);
            if (toggleVisible && toggle.contains(x, y)) {
                setControlsVisible(!controlsVisible);
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
            return false;
        }

        if (action == MotionEvent.ACTION_MOVE) {
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

    private void sendState() {
        // En el editor no hay juego al que mandarle nada.
        if (editando) return;
        if (!controlsVisible) {
            TouchControllerBridge.trySetState(-1, 0, 0, 0, 0, 0, 0);
            return;
        }
        int buttons = 0;
        for (Control control : controls) buttons |= control.buttons();
        float lx = leftStick == null ? 0 : leftStick.xValue;
        float ly = leftStick == null ? 0 : leftStick.yValue;
        if (disposicion() == ACELERACION && carrilIzq != null) {
            // El stick no esta: las flechas lo llevan entero a un lado (las dos, al centro).
            lx = (carrilDer.inUse ? 1 : 0) - (carrilIzq.inUse ? 1 : 0);
            ly = 0;
        } else if (inclinacionActiva()) {
            // Ni el stick: lo que se inclina el movil.
            lx = giroInclinado;
            ly = 0;
        }
        TouchControllerBridge.trySetState(
            buttons,
            lx,
            ly,
            rightStick == null ? 0 : rightStick.xValue,
            rightStick == null ? 0 : rightStick.yValue,
            leftTrigger != null && leftTrigger.inUse ? 1 : 0,
            rightTrigger != null && rightTrigger.inUse ? 1 : 0);
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
        /** La mitad de lo que ocupa, para el recuadro del editor. */
        abstract float alcance();
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
        float alcance() { return radius; }
        boolean contains(float px, float py) {
            float dx = px - cx, dy = py - cy;
            return dx * dx + dy * dy <= radius * radius * 1.35f;
        }
        int buttons() { return inUse ? mask : 0; }
        void draw(Canvas canvas) {
            drawCircle(canvas, cx, cy, radius, inUse);
            drawLabel(canvas, text, cx, cy, radius * .72f, opacidad);
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
        float alcance() { return Math.max(bounds.width(), bounds.height()) * .5f; }
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
        }
        @Override
        boolean contains(float x, float y) {
            return formasDeConducir() ? pedal.contains(x, y) : super.contains(x, y);
        }
        @Override
        float alcance() {
            return formasDeConducir() ? Math.max(pedal.width(), pedal.height()) * .5f : super.alcance();
        }
        @Override
        void draw(Canvas canvas) {
            if (formasDeConducir()) {
                dibujarPedal(canvas, pedal, freno, inUse);
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
        Stick(String id, String text, float x, float y, float radius) {
            super(id, x, y);
            this.text = text; this.baseRadius = radius;
            colocar();
        }
        void colocar() { radius = baseRadius * escala; }
        boolean esPalanca() { return palanca && disposicion() == ACELERACION; }
        float alcance() { return esPalanca() ? radius * 1.3f : radius; }
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
            float dx = (x - cx) / radius;
            float dy = (y - cy) / radius;
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
        void release() { super.release(); xValue = 0; yValue = 0; }
        void draw(Canvas canvas) {
            if (esPalanca()) {
                dibujarPalanca(canvas);
                return;
            }
            drawCircle(canvas, cx, cy, radius, false);
            float knobX = cx + xValue * radius * .58f;
            float knobY = cy + yValue * radius * .58f;
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
        float alcance() { return caja.width() * .5f; }
        boolean contains(float x, float y) { return caja.contains(x, y); }
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
        float alcance() { return radius; }
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
