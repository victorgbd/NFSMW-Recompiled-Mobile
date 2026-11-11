package io.github.nfsmwrecomp;

import androidx.appcompat.app.AppCompatActivity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.ColorStateList;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.Rect;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.provider.OpenableColumns;
import android.provider.Settings;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.text.format.Formatter;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.materialswitch.MaterialSwitch;
import com.google.android.material.radiobutton.MaterialRadioButton;
import com.google.android.material.slider.LabelFormatter;
import com.google.android.material.slider.Slider;
import com.google.android.material.snackbar.Snackbar;
import com.google.android.material.textfield.MaterialAutoCompleteTextView;
import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Consumer;

/**
 * Pantalla de inicio, en horizontal: menu de secciones (Juego, Graficos,
 * Multijugador, Controles, Idioma, Otras opciones, Avanzado) con Jugar debajo.
 */
public class SetupActivity extends AppCompatActivity {

    private static final int PEDIR_ISO = 1;
    private static final int PEDIR_DRIVER = 2;
    private static final int PEDIR_AVISOS = 3;
    private static final int PEDIR_EXPORTAR = 4;
    private static final int PEDIR_IMPORTAR = 5;
    // El acceso a todos los archivos (Android 11+, en sus ajustes) y el almacenamiento de antes (8-10).
    private static final int PEDIR_TODOS_ARCHIVOS = 6;
    private static final int PEDIR_ALMACENAMIENTO = 7;
    private static final int PEDIR_LSFG = 8;

    private static final int ACENTO = 0xFF6F7432;
    // El acento sobre fondo oscuro, para iconos y textos: el de siempre se pierde.
    private static final int ACENTO_CLARO = 0xFFB9BF6A;
    private static final int GRIS = 0xFFA0A0A0;
    private static final int AVISO = 0xFFFFB74D;
    private static final int FONDO = 0xFF121212;
    private static final int FONDO_MENU = 0xFF181818;
    // Lo elegido: la entrada del menu y el servidor en uso.
    private static final int ELEGIDO = 0xFF2E301E;

    // Las secciones del menu lateral, en su orden.
    private static final int S_JUEGO = 0;
    private static final int S_PANTALLA = 1;
    private static final int S_GRAFICOS = 2;
    private static final int S_MULTIJUGADOR = 3;
    private static final int S_CONTROLES = 4;
    private static final int S_IDIOMA = 5;
    private static final int S_OTRAS = 6;
    private static final int S_AVANZADO = 7;
    private static final int[] SECCIONES = {R.string.seccion_juego, R.string.seccion_pantalla,
            R.string.seccion_graficos, R.string.seccion_multijugador, R.string.seccion_controles,
            R.string.seccion_idioma, R.string.seccion_otras, R.string.seccion_avanzado};
    private static final int[] ICONOS = {R.drawable.ic_juego, R.drawable.ic_pantalla,
            R.drawable.ic_graficos, R.drawable.ic_multijugador, R.drawable.ic_controles,
            R.drawable.ic_idioma, R.drawable.ic_ajustes, R.drawable.ic_avanzado};
    // La seccion abierta sobrevive a girar el movil y a cambiar de idioma.
    private static final String ESTADO_SECCION = "seccion";
    private int seccion = S_JUEGO;
    private final View[] paneles = new View[SECCIONES.length];
    private final LinearLayout[] entradasMenu = new LinearLayout[SECCIONES.length];

    private Ajustes ajustes;
    private TextView textoIso;
    private TextView textoEdicion;
    private TextView avisoJugar;
    private RadioGroup grupoDrivers;
    private MaterialButton botonJugar;
    private TextView textoInforme;
    private View filaTurbo;
    // Exportar e importar las partidas (Partidas): en su hilo; el texto dice como fue.
    private final ExecutorService archivos = Executors.newSingleThreadExecutor();
    private TextView textoPartidas;
    private MaterialButton botonExportar;
    private MaterialButton botonImportar;
    private MaterialButton botonElegirIso;
    // Lo que se hace al volver de pedir el acceso a todos los archivos: con el o sin el.
    private Runnable trasDarPermiso;
    private Runnable trasNegarPermiso;

    /** Exportar o importar las partidas, en el hilo de archivos. */
    private interface TrabajoPartidas {
        Partidas.Resultado hacer(Context app) throws IOException;
    }

    // Si el gamertag esta libre en el servidor de la LAN (ComprobacionGamertag).
    // La red, en su hilo; `comprobaciones` cuenta las consultas para tirar las
    // respuestas que llegan tarde (ya se cambio el nombre o el servidor).
    private final ExecutorService red = Executors.newSingleThreadExecutor();
    private final Handler hiloPrincipal = new Handler(Looper.getMainLooper());
    private final Runnable comprobacionPendiente = this::comprobarGamertag;
    // Cada consulta que sale libre aparta el nombre para este movil unos minutos
    // (10 en el servidor); mientras esta pantalla se ve, se renueva cada minuto.
    private static final long RENOVAR_MS = 60_000;
    private final Runnable renovarReserva = new Runnable() {
        @Override public void run() {
            comprobarGamertag();
            hiloPrincipal.postDelayed(this, RENOVAR_MS);
        }
    };
    private TextInputLayout cajaGamertag;
    // Servidor LAN: la lista de servidores guardados (pintarServidores) y, si este
    // movil es el anfitrion, en su lugar la IP que ponen los demas (actualizarAnfitrion).
    private LinearLayout bloqueServidores;
    private LinearLayout listaServidores;
    private MaterialButton botonAnadir;
    private MaterialButton botonBuscar;
    private View filaBotonesServidores;
    // La busqueda de servidores en la red (BuscadorServidores): lo que contesto
    // la ultima (null = no se ha buscado o ya se eligio uno), si este movil no
    // estaba en ninguna red y si hay una en marcha.
    private LinearLayout listaEncontrados;
    private List<BuscadorServidores.Encontrado> encontrados;
    private boolean busquedaSinRed;
    private boolean buscando;
    // El formulario de anadir o editar un servidor (abrirEditor), metido en la
    // lista; null = cerrado. direccionEditada: la del servidor que se edita, "" = uno nuevo.
    private View editorServidor;
    private String direccionEditada;
    private TextView textoAnfitrion;
    private MaterialSwitch interruptorAnfitrion;
    // El servicio quita el anfitrion por su cuenta (boton "Detener" de la
    // notificacion, o no pudo abrir los puertos): el interruptor tiene que verlo
    // aunque esta pantalla siga delante (bajar la barra no pasa por onResume).
    private final SharedPreferences.OnSharedPreferenceChangeListener cambioAnfitrion = (prefs, clave) -> {
        if ("anfitrion_lan".equals(clave) && interruptorAnfitrion != null
                && interruptorAnfitrion.isChecked() != ajustes.anfitrionLan()) {
            interruptorAnfitrion.setChecked(ajustes.anfitrionLan());
        }
    };
    private boolean gamertagEscritoValido = true;
    private boolean comprobandoParaJugar;
    private int comprobaciones;

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(Idioma.envolver(base));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        ajustes = new Ajustes(this);
        ajustes.aplicarPerfilGpuDebil(this);
        if (savedInstanceState != null) {
            seccion = Math.max(0, Math.min(SECCIONES.length - 1,
                    savedInstanceState.getInt(ESTADO_SECCION, S_JUEGO)));
        }
        if (Build.VERSION.SDK_INT >= 30) {
            // De borde a borde: las barras y el teclado llegan como insets
            // (construir) y el teclado no encoge el menu lateral.
            getWindow().setDecorFitsSystemWindows(false);
            getWindow().setStatusBarColor(Color.TRANSPARENT);
            getWindow().setNavigationBarColor(Color.TRANSPARENT);
        }
        setContentView(construir());
        // Una ISO elegida con una version anterior de la app: aun sin mirar.
        mirarIso();
        bancoDePruebas(getIntent());
    }

    @Override
    protected void onSaveInstanceState(Bundle estado) {
        super.onSaveInstanceState(estado);
        estado.putInt(ESTADO_SECCION, seccion);
    }

    private void bancoDePruebas(Intent in) {
        if (in == null || !in.getBooleanExtra("nfsmw.banco", false) || ajustes.iso() == null) {
            return;
        }
        String driver = in.getStringExtra("nfsmw.driver");
        Ediciones.Edicion edicion = edicionAlJugar();
        List<String> args = ajustes.argumentos(this, false, driver != null ? driver : ajustes.driver());
        String[] extra = in.getStringArrayExtra("nfsmw.args");
        if (extra != null) {
            for (String e : extra) {
                String nombre = nombreOpcion(e);
                args.removeIf(a -> nombreOpcion(a).equals(nombre));
            }
            args.addAll(Arrays.asList(extra));
        }
        int alto = in.getIntExtra("nfsmw.alto", -1);
        String resolucion = alto < 0 ? ajustes.resolucion()
                : alto == 480 ? Ajustes.RES_480P
                : alto == 720 ? Ajustes.RES_720P
                : Ajustes.RES_NATIVA;
        Intent i = new Intent(this, GameActivity.class);
        i.putExtra(GameActivity.EXTRA_ARGUMENTOS, args.toArray(new String[0]));
        i.putExtra(GameActivity.EXTRA_ISO, ajustes.iso());
        i.putExtra(GameActivity.EXTRA_EDICION, edicion.id);
        i.putExtra(GameActivity.EXTRA_RESOLUCION, resolucion);
        i.putExtra(GameActivity.EXTRA_ESTIRAR, ajustes.estirar());
        i.putExtra(GameActivity.EXTRA_PANORAMICO, ajustes.panoramicoEfectivo());
        i.putExtra(GameActivity.EXTRA_MOSTRAR_FPS, true);
        startActivity(i);
    }

    @Override
    protected void onResume() {
        super.onResume();
        refrescar();
        // Si el juego se acaba de cerrar, por que (files/logs/cierre.txt).
        Cierres.guardar(this);
        // El servidor de este movil, si es el anfitrion: al abrir la app tras
        // reiniciar el movil, o si Android lo cerro. Arrancarlo dos veces no hace nada.
        if (MotorNativo.ACTIVO && ajustes.anfitrionLan()) {
            ServidorLanService.arrancar(this);
        }
        actualizarAnfitrion();
        // Al volver (otro jugador ha podido entrar con el nombre mientras tanto).
        programarComprobacion();
        hiloPrincipal.removeCallbacks(renovarReserva);
        hiloPrincipal.postDelayed(renovarReserva, RENOVAR_MS);
    }

    @Override
    protected void onPause() {
        // En el juego ya no hace falta: al entrar en Xbox Live cuenta la conexion.
        hiloPrincipal.removeCallbacks(renovarReserva);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        getSharedPreferences(Ajustes.PREFS, MODE_PRIVATE)
                .unregisterOnSharedPreferenceChangeListener(cambioAnfitrion);
        hiloPrincipal.removeCallbacksAndMessages(null);
        red.shutdownNow();
        // Una copia a medias si que se termina: no se corta un .zip ni se dejan las partidas sin poner.
        archivos.shutdown();
        super.onDestroy();
    }

    private View espacio(int altoDp) {
        View v = new View(this);
        v.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(altoDp)));
        return v;
    }

    private MaterialCardView tarjeta(View... vistas) {
        MaterialCardView tarjeta = new MaterialCardView(this);
        tarjeta.setCardBackgroundColor(0xFF1E1E1E);
        tarjeta.setRadius(dp(16));
        tarjeta.setCardElevation(dp(4));
        tarjeta.setUseCompatPadding(true);
        tarjeta.setStrokeWidth(0);

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(dp(16), dp(16), dp(16), dp(16));
        
        for (View v : vistas) {
            if (v != null) layout.addView(v);
        }
        
        tarjeta.addView(layout);
        return tarjeta;
    }

    private void estilarBotonPrimario(MaterialButton b) {
        b.setCornerRadius(dp(12));
        b.setBackgroundTintList(segunActivado(0xFF3A3A3A, ACENTO));
        b.setTextColor(segunActivado(0xFF808080, 0xFF121212));
    }

    private void estilarBotonSecundario(MaterialButton b) {
        b.setCornerRadius(dp(10));
        b.setBackgroundTintList(ColorStateList.valueOf(0xFF2C2C2C));
        b.setTextColor(Color.WHITE);
    }

    private void estilarBotonPeligro(MaterialButton b) {
        b.setCornerRadius(dp(10));
        b.setBackgroundTintList(ColorStateList.valueOf(0xFF4A1A1A));
        b.setTextColor(0xFFFF6666);
    }

    /**
     * En horizontal: a la izquierda el menu de secciones con Jugar debajo, que se
     * ve desde todas; a la derecha, la seccion elegida.
     */
    @SuppressWarnings("deprecation")
    private View construir() {
        LinearLayout raiz = new LinearLayout(this);
        raiz.setOrientation(LinearLayout.HORIZONTAL);
        raiz.setBackgroundColor(FONDO);

        // --- Menu lateral
        LinearLayout menu = new LinearLayout(this);
        menu.setOrientation(LinearLayout.VERTICAL);
        menu.setBackgroundColor(FONDO_MENU);
        menu.setPadding(dp(12), dp(16), dp(12), dp(12));

        TextView titulo = new TextView(this);
        titulo.setText(R.string.app_name);
        titulo.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        titulo.setTypeface(Typeface.DEFAULT_BOLD);
        titulo.setTextColor(Color.WHITE);
        titulo.setPadding(dp(12), 0, dp(12), dp(12));
        menu.addView(titulo);

        // Las entradas se desplazan si no caben (moviles bajos, letra grande);
        // Jugar se queda siempre abajo.
        LinearLayout lista = new LinearLayout(this);
        lista.setOrientation(LinearLayout.VERTICAL);
        for (int i = 0; i < SECCIONES.length; i++) {
            entradasMenu[i] = entradaMenu(i);
            lista.addView(entradasMenu[i]);
        }
        ScrollView desplazarMenu = new ScrollView(this);
        desplazarMenu.setVerticalScrollBarEnabled(false);
        desplazarMenu.addView(lista);
        menu.addView(desplazarMenu, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        botonJugar = new MaterialButton(this);
        botonJugar.setText(R.string.jugar);
        botonJugar.setTextSize(TypedValue.COMPLEX_UNIT_SP, 18);
        botonJugar.setTypeface(Typeface.DEFAULT_BOLD);
        botonJugar.setMinHeight(dp(56));
        botonJugar.setIconResource(R.drawable.ic_jugar);
        botonJugar.setIconGravity(MaterialButton.ICON_GRAVITY_TEXT_START);
        estilarBotonPrimario(botonJugar);
        botonJugar.setIconTint(segunActivado(0xFF808080, 0xFF121212));
        botonJugar.setOnClickListener(v -> jugar());
        LinearLayout.LayoutParams lpJugar = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lpJugar.topMargin = dp(8);
        menu.addView(botonJugar, lpJugar);

        raiz.addView(menu, new LinearLayout.LayoutParams(dp(220), ViewGroup.LayoutParams.MATCH_PARENT));

        // --- La seccion elegida. Todas se construyen al empezar y solo se ve una:
        // refrescar() y la comprobacion del gamertag tienen siempre sus vistas.
        FrameLayout contenido = new FrameLayout(this);
        paneles[S_JUEGO] = panel(tarjetaJuego(), tarjetaPartidas());
        paneles[S_PANTALLA] = panel(tarjetaPantalla());
        paneles[S_GRAFICOS] = panel(tarjetaGraficos());
        paneles[S_MULTIJUGADOR] = panel(tarjetaPerfil(),
                MotorNativo.ACTIVO ? tarjetaServidorLan() : null);
        paneles[S_CONTROLES] = panel(tarjetaControles());
        paneles[S_IDIOMA] = panel(tarjetaIdioma());
        paneles[S_OTRAS] = panel(soloNativo(tarjetaSonido()), tarjetaRendimiento());
        paneles[S_AVANZADO] = panel(tarjetaAvanzado());
        for (View p : paneles) {
            contenido.addView(p);
        }
        raiz.addView(contenido, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1f));
        mostrarSeccion(seccion);

        // Fuera de las barras del sistema y el recorte de la camara. El teclado
        // solo encoge la seccion: el menu se queda debajo de el tal cual (si se
        // encogiera, en horizontal no quedaria ni una entrada a la vista).
        // Para eso la ventana no se redimensiona con el teclado (onCreate): llega
        // como inset y aqui se aplica solo a la seccion.
        raiz.setOnApplyWindowInsetsListener((v, ins) -> {
            if (Build.VERSION.SDK_INT >= 30) {
                Insets barras = ins.getInsets(WindowInsets.Type.systemBars()
                        | WindowInsets.Type.displayCutout());
                Insets teclado = ins.getInsets(WindowInsets.Type.ime());
                v.setPadding(barras.left, barras.top, barras.right, barras.bottom);
                contenido.setPadding(0, 0, 0, Math.max(0, teclado.bottom - barras.bottom));
                // Con el formulario de un servidor abierto, entero a la vista si cabe
                // (la seccion solo se asegura de ver el campo con el cursor).
                View editor = editorServidor;
                if (teclado.bottom > 0 && editor != null) {
                    editor.post(() -> editor.requestRectangleOnScreen(
                            new Rect(0, 0, editor.getWidth(), editor.getHeight()), true));
                }
            } else {
                // Android 10: la ventana se encoge entera con el teclado.
                v.setPadding(ins.getSystemWindowInsetLeft(), ins.getSystemWindowInsetTop(),
                        ins.getSystemWindowInsetRight(), ins.getSystemWindowInsetBottom());
            }
            return ins;
        });
        return raiz;
    }

    /** Una entrada del menu: icono y nombre de la seccion. */
    private LinearLayout entradaMenu(int i) {
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.HORIZONTAL);
        fila.setGravity(Gravity.CENTER_VERTICAL);
        fila.setMinimumHeight(dp(48));
        fila.setPadding(dp(12), dp(6), dp(12), dp(6));
        ImageView icono = new ImageView(this);
        icono.setImageResource(ICONOS[i]);
        fila.addView(icono, new LinearLayout.LayoutParams(dp(22), dp(22)));
        TextView t = new TextView(this);
        t.setText(SECCIONES[i]);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        t.setMaxLines(2);
        LinearLayout.LayoutParams lpTexto = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lpTexto.leftMargin = dp(14);
        fila.addView(t, lpTexto);
        fila.setOnClickListener(v -> mostrarSeccion(i));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(2);
        fila.setLayoutParams(lp);
        return fila;
    }

    /** Ensena una seccion y marca su entrada del menu. */
    private void mostrarSeccion(int i) {
        // El teclado de un campo de otra seccion no se queda abierto encima.
        View foco = getCurrentFocus();
        if (foco != null && paneles[seccion] != null && seccion != i) {
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(foco.getWindowToken(), 0);
            }
            foco.clearFocus();
        }
        seccion = i;
        for (int j = 0; j < paneles.length; j++) {
            boolean esta = j == i;
            paneles[j].setVisibility(esta ? View.VISIBLE : View.GONE);
            LinearLayout fila = entradasMenu[j];
            fila.setSelected(esta);
            fila.setBackground(fondoFila(esta ? ELEGIDO : Color.TRANSPARENT, 0));
            ((ImageView) fila.getChildAt(0)).setImageTintList(
                    ColorStateList.valueOf(esta ? ACENTO_CLARO : GRIS));
            TextView t = (TextView) fila.getChildAt(1);
            t.setTextColor(esta ? Color.WHITE : 0xFFC8C8C8);
            t.setTypeface(esta ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
        }
    }

    /** Fondo redondeado que se ilumina al tocarlo; borde 0 = sin borde. */
    private Drawable fondoFila(int color, int borde) {
        GradientDrawable forma = new GradientDrawable();
        forma.setCornerRadius(dp(12));
        forma.setColor(color);
        if (borde != 0) {
            forma.setStroke(dp(1), borde);
        }
        GradientDrawable mascara = new GradientDrawable();
        mascara.setCornerRadius(dp(12));
        mascara.setColor(Color.WHITE);
        return new RippleDrawable(ColorStateList.valueOf(0x22FFFFFF), forma, mascara);
    }

    /** Las tarjetas de una seccion, una debajo de otra, con desplazamiento. */
    private ScrollView panel(View... tarjetas) {
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(dp(12), dp(8), dp(12), dp(12));
        for (View t : tarjetas) {
            if (t != null) col.addView(t);
        }
        ScrollView s = new ScrollView(this);
        s.addView(col);
        return s;
    }

    private View tarjetaJuego() {
        textoIso = new TextView(this);
        textoIso.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        textoIso.setTextColor(Color.WHITE);

        // Para que edicion del juego es el APK y, si la ISO es de otra, el aviso.
        textoEdicion = resumen(0);
        textoEdicion.setPadding(0, dp(4), 0, 0);

        MaterialButton elegirIso = new MaterialButton(this);
        elegirIso.setText(R.string.elegir_iso);
        estilarBotonSecundario(elegirIso);
        elegirIso.setOnClickListener(v -> conAccesoTotal(getString(R.string.permiso_archivos_iso),
                this::buscarIsos, this::elegirIsoConSelector));
        botonElegirIso = elegirIso;

        // Por que Jugar, en el menu, no se puede pulsar.
        avisoJugar = resumen(0);
        avisoJugar.setTextColor(AVISO);
        avisoJugar.setPadding(0, dp(8), 0, 0);

        TextView apk = resumen(BuildConfig.CON_JUEGO ? R.string.apk_con_juego : R.string.apk_sonda);
        apk.setPadding(0, 0, 0, dp(12));
        return tarjeta(
            seccion(R.string.seccion_juego),
            apk,
            textoIso,
            textoEdicion,
            espacio(12),
            elegirIso,
            avisoJugar
        );
    }

    // --- Acceso a todos los archivos -------------------------------------------------

    /**
     * Con acceso a todos los archivos (Partidas.accesoTotal), conAcceso. Si no,
     * se explica para que es y se pide, o se usa el selector de Android
     * (conSelector), que en algunos moviles no ensena el almacenamiento interno.
     */
    private void conAccesoTotal(String explicacion, Runnable conAcceso, Runnable conSelector) {
        if (Partidas.accesoTotal(this)) {
            conAcceso.run();
            return;
        }
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.permiso_archivos_titulo)
                .setMessage(explicacion)
                .setPositiveButton(R.string.permiso_archivos_dar,
                        (d, w) -> pedirAccesoTotal(conAcceso, conSelector))
                .setNeutralButton(R.string.permiso_archivos_selector, (d, w) -> conSelector.run())
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    /**
     * Android 11+: la pantalla de ajustes del acceso a todos los archivos de esta
     * app (o la lista general, en las capas que no tienen la de una app). Android
     * 8-10: los permisos de almacenamiento de antes. Al volver, volverDePermiso.
     */
    private void pedirAccesoTotal(Runnable conAcceso, Runnable conSelector) {
        trasDarPermiso = conAcceso;
        trasNegarPermiso = conSelector;
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivityForResult(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())), PEDIR_TODOS_ARCHIVOS);
            } catch (ActivityNotFoundException e) {
                try {
                    startActivityForResult(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION),
                            PEDIR_TODOS_ARCHIVOS);
                } catch (ActivityNotFoundException e2) {
                    volverDePermiso();
                }
            }
        } else {
            requestPermissions(new String[] {android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE}, PEDIR_ALMACENAMIENTO);
        }
    }

    /** De vuelta de pedir el permiso: lo que tocaba con el o, si no se dio, el selector. */
    private void volverDePermiso() {
        Runnable si = trasDarPermiso;
        Runnable no = trasNegarPermiso;
        trasDarPermiso = null;
        trasNegarPermiso = null;
        if (si == null) {
            return;
        }
        if (Partidas.accesoTotal(this)) {
            si.run();
        } else {
            Toast.makeText(this, R.string.permiso_archivos_denegado, Toast.LENGTH_LONG).show();
            no.run();
        }
    }

    @Override
    public void onRequestPermissionsResult(int peticion, String[] permisos, int[] resultados) {
        super.onRequestPermissionsResult(peticion, permisos, resultados);
        if (peticion == PEDIR_ALMACENAMIENTO) {
            volverDePermiso();
        }
    }

    // --- La ISO ---------------------------------------------------------------------------

    private void elegirIsoConSelector() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("*/*");
        startActivityForResult(i, PEDIR_ISO);
    }

    /**
     * Las .iso del almacenamiento interno y de la SD que son de una edicion que
     * lleva el APK (BuscadorIsos), en su hilo.
     */
    private void buscarIsos() {
        botonElegirIso.setEnabled(false);
        botonElegirIso.setText(R.string.iso_buscando);
        final Context app = getApplicationContext();
        archivos.execute(() -> {
            BuscadorIsos.Busqueda b = BuscadorIsos.buscarCompatibles(app);
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                botonElegirIso.setEnabled(true);
                botonElegirIso.setText(R.string.elegir_iso);
                mostrarIsos(b);
            });
        });
    }

    /**
     * Una fila por ISO: nombre, edicion, tamano y carpeta, y si le falta el
     * final. Sin ninguna, que ediciones valen y cuantas se descartaron. El
     * selector de Android sigue a mano.
     */
    private void mostrarIsos(BuscadorIsos.Busqueda b) {
        MaterialAlertDialogBuilder d = new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.iso_encontradas_titulo)
                .setNeutralButton(R.string.iso_con_selector, (x, w) -> elegirIsoConSelector())
                .setNegativeButton(android.R.string.cancel, null);
        List<BuscadorIsos.Encontrada> isos = b.compatibles;
        if (isos.isEmpty()) {
            String texto = getString(R.string.iso_ninguna_encontrada, Ediciones.nombres());
            if (b.descartadas > 0) {
                texto += "\n\n" + getString(R.string.iso_descartadas, b.descartadas);
            }
            d.setMessage(texto);
        } else {
            CharSequence[] filas = new CharSequence[isos.size()];
            for (int i = 0; i < filas.length; i++) {
                BuscadorIsos.Encontrada e = isos.get(i);
                String fila = e.iso.getName() + "\n" + e.edicion.nombre + " · "
                        + Formatter.formatShortFileSize(this, e.iso.length()) + " · "
                        + BuscadorIsos.carpetaVisible(e.iso);
                if (e.info.faltan > 0) {
                    fila += "\n" + getString(R.string.iso_incompleta_corta,
                            Formatter.formatShortFileSize(this, e.info.faltan));
                }
                filas[i] = fila;
            }
            d.setItems(filas, (x, i) -> usarIso(isos.get(i)));
        }
        d.show();
    }

    /**
     * La ISO elegida es su ruta: el juego la abre directamente (Ajustes.iso). Ya
     * esta mirada (su edicion y si esta entera), asi que no se lee otra vez.
     */
    private void usarIso(BuscadorIsos.Encontrada e) {
        ajustes.iso(e.iso.getAbsolutePath());
        ajustes.nombreIso(e.iso.getName());
        ajustes.isoMirada(e.info.xex, e.info.faltan);
        refrescar();
    }

    // --- Las partidas ----------------------------------------------------------------------

    /**
     * Las partidas guardadas: el juego las deja dentro de la app, donde no
     * llegan el explorador ni el PC. Se exportan a un .zip (Partidas): con
     * acceso a todos los archivos, a NFSMW/partidas; si no, donde diga el
     * selector de Android. Se importan siempre con el selector.
     */
    private View tarjetaPartidas() {
        botonExportar = new MaterialButton(this);
        botonExportar.setText(R.string.partidas_exportar);
        estilarBotonSecundario(botonExportar);
        botonExportar.setOnClickListener(v -> {
            if (!Partidas.hay(this)) {
                textoPartidas.setText(R.string.partidas_ninguna);
                textoPartidas.setVisibility(View.VISIBLE);
                return;
            }
            conAccesoTotal(getString(R.string.permiso_archivos_partidas, Partidas.RUTA_VISIBLE),
                    this::exportarACarpeta, this::exportarConSelector);
        });
        botonImportar = new MaterialButton(this);
        botonImportar.setText(R.string.partidas_importar);
        estilarBotonSecundario(botonImportar);
        botonImportar.setOnClickListener(v -> importarConSelector());
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams izquierda =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        izquierda.setMarginEnd(dp(8));
        fila.addView(botonExportar, izquierda);
        fila.addView(botonImportar, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        textoPartidas = resumen(0);
        textoPartidas.setTextColor(Color.WHITE);
        textoPartidas.setPadding(0, dp(8), 0, 0);
        textoPartidas.setVisibility(View.GONE);
        return tarjeta(
            seccion(R.string.seccion_partidas),
            resumen(R.string.partidas_resumen),
            espacio(8),
            fila,
            textoPartidas
        );
    }

    private void exportarACarpeta() {
        File destino = new File(Partidas.carpetaVisible(), Partidas.nombreSugerido());
        copiarPartidas(true, app -> Partidas.exportar(app, destino),
                Partidas.RUTA_VISIBLE + "/" + destino.getName());
    }

    private void exportarConSelector() {
        Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        i.setType("application/zip");
        i.putExtra(Intent.EXTRA_TITLE, Partidas.nombreSugerido());
        startActivityForResult(i, PEDIR_EXPORTAR);
    }

    private void importarConSelector() {
        Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        i.addCategory(Intent.CATEGORY_OPENABLE);
        // Cada explorador le pone al .zip su tipo: la marca de dentro dice si vale.
        i.setType("*/*");
        i.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/zip",
                "application/x-zip-compressed", "application/octet-stream"});
        startActivityForResult(i, PEDIR_IMPORTAR);
    }

    /** Antes de importar: las partidas de ahora se sustituyen. */
    private void confirmarImportar(TrabajoPartidas importar) {
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.partidas_importar_titulo)
                .setMessage(getString(R.string.partidas_importar_aviso, Partidas.rutaCopiaAntesDeImportar(this)))
                .setPositiveButton(R.string.partidas_importar, (d, w) -> copiarPartidas(false, importar, null))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    /**
     * Exporta o importa en su hilo y lo cuenta debajo de los botones. donde: el
     * .zip exportado, como lo ve el jugador (null = lo eligio el con el selector).
     */
    private void copiarPartidas(boolean exportar, TrabajoPartidas trabajo, String donde) {
        botonExportar.setEnabled(false);
        botonImportar.setEnabled(false);
        textoPartidas.setText(exportar ? R.string.partidas_exportando : R.string.partidas_importando);
        textoPartidas.setVisibility(View.VISIBLE);
        final Context app = getApplicationContext();
        archivos.execute(() -> {
            String texto;
            try {
                Partidas.Resultado r = trabajo.hacer(app);
                String tamano = Formatter.formatShortFileSize(app, r.bytes);
                texto = !exportar ? getString(R.string.partidas_importadas, r.archivos, tamano)
                        : donde != null ? getString(R.string.partidas_exportadas_en, r.archivos, tamano, donde)
                        : getString(R.string.partidas_exportadas, r.archivos, tamano);
            } catch (Partidas.NoValido e) {
                texto = getString(R.string.partidas_no_valido);
            } catch (IOException | RuntimeException e) {
                texto = getString(R.string.partidas_error, String.valueOf(e.getMessage()));
            }
            final String fin = texto;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                botonExportar.setEnabled(true);
                botonImportar.setEnabled(true);
                textoPartidas.setText(fin);
            });
        });
    }

    private View tarjetaPantalla() {
        // Estirar y pantalla ancha se excluyen: las dos llenan la pantalla, una
        // deformando y la otra no. Encender una apaga la otra. Si venian las dos
        // encendidas (antes de esto), se queda la pantalla ancha.
        boolean conPanoramico = MotorNativo.ACTIVO;
        if (conPanoramico && ajustes.estirar() && ajustes.panoramico()) {
            ajustes.estirar(false);
        }
        MaterialSwitch[] interruptores = new MaterialSwitch[2];
        View estirar = interruptor(R.string.estirar, R.string.estirar_resumen, ajustes.estirar(), v -> {
            ajustes.estirar(v);
            if (v && conPanoramico && interruptores[1] != null) {
                interruptores[1].setChecked(false);
            }
        });
        View panoramico = interruptor(R.string.panoramico, R.string.panoramico_resumen, ajustes.panoramico(),
                v -> {
                    ajustes.panoramico(v);
                    if (v && interruptores[0] != null) {
                        interruptores[0].setChecked(false);
                    }
                });
        interruptores[0] = (MaterialSwitch) ((ViewGroup) estirar).getChildAt(0);
        interruptores[1] = (MaterialSwitch) ((ViewGroup) panoramico).getChildAt(0);
        return tarjeta(
            seccion(R.string.seccion_pantalla),
            selectorResolucion(),
            estirar,
            soloNativo(panoramico),
            interruptor(R.string.mostrar_fps, R.string.mostrar_fps_resumen, ajustes.mostrarFps(), ajustes::mostrarFps)
        );
    }

    private View tarjetaGraficos() {
        return tarjeta(
            seccion(R.string.seccion_graficos),
            soloNativo(selector(R.string.api_grafica, R.string.api_grafica_resumen,
                    Arrays.asList(getString(R.string.api_vulkan), getString(R.string.api_gles)),
                    Arrays.asList(Ajustes.API_VULKAN, Ajustes.API_GLES), ajustes.apiGrafica(), true,
                    ajustes::apiGrafica)),
            soloNativo(filaPreajustes()),
            soloNativo(selector(R.string.resolucion_interna, R.string.resolucion_interna_resumen,
                    Arrays.asList(getString(R.string.res_interna_576), getString(R.string.res_interna_720),
                            getString(R.string.res_interna_1080)),
                    Arrays.asList("1024x576", "1280x720", "1920x1080"), ajustes.resolucionInterna(), false,
                    ajustes::resolucionInterna)),
            soloNativo(desplegable(R.string.limite_fps, R.string.limite_fps_resumen,
                    Arrays.asList(getString(R.string.fps_valor, 30), getString(R.string.fps_valor, 60),
                            getString(R.string.fps_experimental, 90), getString(R.string.fps_experimental, 120),
                            getString(R.string.fps_sin_limite)),
                    Arrays.asList(30, 60, 90, 120, Ajustes.FPS_SIN_LIMITE), ajustes.limiteFps(),
                    ajustes::limiteFps)),
            selectorSuavizado(),
            selector(R.string.escalado, R.string.escalado_resumen,
                    Arrays.asList(getString(R.string.escalado_bilineal), getString(R.string.escalado_fsr),
                            getString(R.string.escalado_cas), getString(R.string.escalado_gsr),
                            getString(R.string.escalado_gsr2)),
                    Arrays.asList("bilinear", "fsr", "cas", "gsr", "gsr2"), ajustes.escalado(), false, ajustes::escalado),
            soloNativo(selector(R.string.lsfg, R.string.lsfg_resumen,
                    Arrays.asList(getString(R.string.lsfg_no), getString(R.string.lsfg_x, 2),
                            getString(R.string.lsfg_x, 3), getString(R.string.lsfg_x, 4)),
                    Arrays.asList(0, 2, 3, 4), ajustes.lsfg(), true, ajustes::lsfg)),
            soloNativo(filaDllLsfg()),
            soloXenos(interruptor(R.string.posprocesado, R.string.posprocesado_resumen, !ajustes.sinPosprocesado(), v -> ajustes.sinPosprocesado(!v))),
            selector(R.string.anisotropico, R.string.anisotropico_resumen,
                    Arrays.asList(getString(R.string.aniso_no), getString(R.string.aniso_2), getString(R.string.aniso_4), getString(R.string.aniso_16)),
                    Arrays.asList(0, 2, 3, 5), ajustes.anisotropico(), true, ajustes::anisotropico),
            soloXenos(selector(R.string.escala, R.string.escala_resumen,
                    Arrays.asList(getString(R.string.escala_valor, 1), getString(R.string.escala_valor, 2)),
                    Arrays.asList(1, 2), ajustes.escala(), true, ajustes::escala)),
            soloNativo(selector(R.string.calidad_sombras, R.string.calidad_sombras_resumen,
                    Arrays.asList(getString(R.string.sombras_rapida), getString(R.string.sombras_suave),
                            getString(R.string.sombras_xbox360)),
                    Arrays.asList(Ajustes.SOMBRAS_RAPIDA, Ajustes.SOMBRAS_SUAVE, Ajustes.SOMBRAS_XBOX),
                    ajustes.calidadSombras(), true, ajustes::calidadSombras)),
            soloNativo(desplegable(R.string.resolucion_sombras, 0,
                    Arrays.asList(getString(R.string.sombras_nativa), getString(R.string.sombras_alta),
                            getString(R.string.sombras_muy_alta), getString(R.string.sombras_ultra)),
                    Arrays.asList(100, 150, 200, 250), ajustes.sombrasEscala(), ajustes::sombrasEscala)),
            soloNativo(selector(R.string.sombras_distancia, R.string.sombras_distancia_resumen,
                    Arrays.asList(getString(R.string.sombras_lejos), getString(R.string.sombras_media),
                            getString(R.string.sombras_cerca)),
                    Arrays.asList(100, 150, 200), ajustes.sombrasCorte(), true, ajustes::sombrasCorte)),
            soloNativo(selector(R.string.reflejos_coche, R.string.reflejos_coche_resumen,
                    Arrays.asList(getString(R.string.calidad_alta), getString(R.string.calidad_media),
                            getString(R.string.calidad_baja)),
                    Arrays.asList(6, 2, 1), ajustes.reflejosCoche(), true, ajustes::reflejosCoche)),
            soloNativo(interruptor(R.string.reflejo_asfalto, R.string.reflejo_asfalto_resumen,
                    ajustes.reflejoAsfalto(), ajustes::reflejoAsfalto)),
            soloNativo(selector(R.string.resplandor_cielo, R.string.resplandor_cielo_resumen,
                    Arrays.asList(getString(R.string.cielo_natural), getString(R.string.cielo_original),
                            getString(R.string.cielo_suave)),
                    Arrays.asList("natural", "original", "suave"), ajustes.resplandorCielo(), true,
                    ajustes::resplandorCielo)),
            soloNativo(selector(R.string.filtro_color, R.string.filtro_color_resumen,
                    Arrays.asList(getString(R.string.filtro_color_original), getString(R.string.filtro_color_suave),
                            getString(R.string.filtro_color_apagado)),
                    Arrays.asList("original", "suave", "apagado"), ajustes.filtroColor(), true,
                    ajustes::filtroColor)),
            soloNativo(interruptor(R.string.desenfoque, R.string.desenfoque_resumen,
                    ajustes.desenfoque(), ajustes::desenfoque)),
            soloNativo(desplegable(R.string.filtro_imagen, R.string.filtro_imagen_resumen,
                    Arrays.asList(getString(R.string.filtro_imagen_no), getString(R.string.filtro_imagen_cine),
                            getString(R.string.filtro_imagen_vivo), getString(R.string.filtro_imagen_calido),
                            getString(R.string.filtro_imagen_frio), getString(R.string.filtro_imagen_sepia),
                            getString(R.string.filtro_imagen_noir), getString(R.string.filtro_imagen_crt)),
                    Arrays.asList("apagado", "cine", "vivo", "calido", "frio", "sepia", "noir", "crt"),
                    ajustes.filtroImagen(), ajustes::filtroImagen))
        );
    }

    /** Preajustes del motor nativo: cambian de golpe los ajustes graficos de abajo. */
    /**
     * El Lossless.dll de Lossless Scaling para la generacion de fotogramas: lo
     * elige el usuario (su copia comprada) y se copia a files/lsfg. No se
     * incluye ni se descarga.
     */
    private View filaDllLsfg() {
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.setPadding(0, 0, 0, dp(12));
        caja.addView(titulo(R.string.lsfg_dll));
        java.io.File dll = Ajustes.dllLsfg(this);
        TextView estado = resumen(dll.isFile() ? R.string.lsfg_dll_presente : R.string.lsfg_dll_falta);
        caja.addView(estado);
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.HORIZONTAL);
        fila.setPadding(0, dp(6), 0, 0);
        MaterialButton elegir = new MaterialButton(this);
        elegir.setText(R.string.lsfg_dll_elegir);
        estilarBotonSecundario(elegir);
        elegir.setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");
            startActivityForResult(i, PEDIR_LSFG);
        });
        MaterialButton quitar = new MaterialButton(this);
        quitar.setText(R.string.lsfg_dll_quitar);
        estilarBotonPeligro(quitar);
        quitar.setEnabled(dll.isFile());
        quitar.setOnClickListener(v -> {
            if (Ajustes.dllLsfg(this).delete()) {
                refrescar();
            }
        });
        LinearLayout.LayoutParams izquierda =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        izquierda.setMarginEnd(dp(8));
        fila.addView(elegir, izquierda);
        fila.addView(quitar, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        caja.addView(fila);
        return caja;
    }

    /**
     * Copia el Lossless.dll elegido a files/lsfg, en su hilo. Solo comprueba que
     * es un ejecutable de Windows (empieza por MZ); los shaders los busca el
     * juego al arrancar y, si faltan, lo dice en su log y juega sin generacion.
     */
    private void copiarDllLsfg(Uri uri) {
        final Context app = getApplicationContext();
        archivos.execute(() -> {
            java.io.File destino = Ajustes.dllLsfg(app);
            java.io.File temporal = new java.io.File(destino.getParentFile(), "Lossless.dll.tmp");
            int texto;
            try {
                destino.getParentFile().mkdirs();
                try (java.io.InputStream in = app.getContentResolver().openInputStream(uri);
                     java.io.OutputStream out = new java.io.FileOutputStream(temporal)) {
                    if (in == null) {
                        throw new IOException("sin datos");
                    }
                    byte[] bufer = new byte[1 << 16];
                    long total = 0;
                    boolean cabecera = false;
                    for (int n; (n = in.read(bufer)) > 0; ) {
                        if (total == 0 && n >= 2) {
                            cabecera = bufer[0] == 'M' && bufer[1] == 'Z';
                        }
                        out.write(bufer, 0, n);
                        total += n;
                    }
                    if (!cabecera) {
                        throw new IllegalArgumentException();
                    }
                }
                if (!temporal.renameTo(destino)) {
                    throw new IOException("no se pudo guardar");
                }
                texto = R.string.lsfg_dll_copiado;
            } catch (IllegalArgumentException e) {
                temporal.delete();
                texto = R.string.lsfg_dll_no_valido;
            } catch (IOException | SecurityException e) {
                temporal.delete();
                texto = R.string.lsfg_dll_error;
            }
            final int mensaje = texto;
            runOnUiThread(() -> {
                Toast.makeText(this, mensaje, Toast.LENGTH_LONG).show();
                refrescar();
            });
        });
    }

    private View filaPreajustes() {
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.setPadding(0, 0, 0, dp(6));
        caja.addView(titulo(R.string.preajustes));
        caja.addView(resumen(R.string.preajustes_resumen));
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.HORIZONTAL);
        fila.setPadding(0, dp(6), 0, 0);
        MaterialButton ahorro = new MaterialButton(this);
        ahorro.setText(R.string.preajuste_ahorro);
        estilarBotonSecundario(ahorro);
        ahorro.setOnClickListener(v -> confirmarPreajuste(R.string.preajuste_ahorro,
                R.string.preajuste_ahorro_detalle, ajustes::aplicarAhorro));
        MaterialButton fabrica = new MaterialButton(this);
        fabrica.setText(R.string.preajuste_fabrica);
        estilarBotonSecundario(fabrica);
        fabrica.setOnClickListener(v -> confirmarPreajuste(R.string.preajuste_fabrica,
                R.string.preajuste_fabrica_detalle, () -> ajustes.aplicarFabrica(this)));
        LinearLayout.LayoutParams izquierda =
                new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        izquierda.setMarginEnd(dp(8));
        fila.addView(ahorro, izquierda);
        fila.addView(fabrica, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        caja.addView(fila);
        // Los del usuario, guardados al aplicar uno de los otros dos: solo si los hay.
        if (ajustes.hayMisAjustes()) {
            MaterialButton mios = new MaterialButton(this);
            mios.setText(R.string.preajuste_mios);
            estilarBotonSecundario(mios);
            mios.setOnClickListener(v -> confirmarPreajuste(R.string.preajuste_mios,
                    R.string.preajuste_mios_detalle, ajustes::aplicarMisAjustes));
            LinearLayout.LayoutParams ancho = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            ancho.topMargin = dp(4);
            caja.addView(mios, ancho);
        }
        return caja;
    }

    /** Pregunta antes de cambiar varios ajustes a la vez; al aplicar, la pantalla se rehace con ellos. */
    private void confirmarPreajuste(int titulo, int detalle, Runnable aplicar) {
        new MaterialAlertDialogBuilder(this)
                .setTitle(titulo)
                .setMessage(detalle)
                .setPositiveButton(R.string.preajuste_aplicar, (d, w) -> {
                    aplicar.run();
                    recreate();
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private View tarjetaPerfil() {
        return tarjeta(
            seccion(R.string.seccion_perfil),
            resumen(R.string.gamertag_resumen),
            espacio(8),
            campoGamertag()
        );
    }

    /**
     * Multijugador en la LAN (solo el motor nativo tiene red): ser el anfitrion
     * o elegir uno de los servidores guardados.
     */
    private View tarjetaServidorLan() {
        View filaAnfitrion = interruptor(R.string.servidor_anfitrion, R.string.servidor_anfitrion_resumen,
                ajustes.anfitrionLan(), this::cambiarAnfitrion);
        interruptorAnfitrion = (MaterialSwitch) ((LinearLayout) filaAnfitrion).getChildAt(0);
        textoAnfitrion = resumen(0);
        textoAnfitrion.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        textoAnfitrion.setPadding(0, dp(4), 0, 0);

        // Tocar un servidor lo elige; el lapiz lo edita y la papelera lo quita.
        listaServidores = new LinearLayout(this);
        listaServidores.setOrientation(LinearLayout.VERTICAL);
        botonAnadir = new MaterialButton(this);
        botonAnadir.setText(R.string.servidor_anadir);
        botonAnadir.setIconResource(R.drawable.ic_anadir);
        estilarBotonSecundario(botonAnadir);
        botonAnadir.setIconTint(ColorStateList.valueOf(Color.WHITE));
        botonAnadir.setOnClickListener(v -> abrirEditor(null));
        // Buscar: los que contestan salen debajo de los guardados, y tocar uno lo
        // guarda y lo elige.
        botonBuscar = new MaterialButton(this);
        botonBuscar.setText(R.string.servidor_buscar);
        botonBuscar.setIconResource(R.drawable.ic_buscar);
        estilarBotonSecundario(botonBuscar);
        botonBuscar.setIconTint(ColorStateList.valueOf(Color.WHITE));
        botonBuscar.setOnClickListener(v -> buscarServidores());
        listaEncontrados = new LinearLayout(this);
        listaEncontrados.setOrientation(LinearLayout.VERTICAL);
        LinearLayout botones = new LinearLayout(this);
        botones.setOrientation(LinearLayout.HORIZONTAL);
        botones.addView(botonBuscar);
        LinearLayout.LayoutParams lpAnadir = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lpAnadir.leftMargin = dp(8);
        botones.addView(botonAnadir, lpAnadir);
        filaBotonesServidores = botones;
        LinearLayout.LayoutParams lpBotones = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lpBotones.topMargin = dp(10);
        bloqueServidores = new LinearLayout(this);
        bloqueServidores.setOrientation(LinearLayout.VERTICAL);
        bloqueServidores.addView(titulo(R.string.servidores_guardados));
        bloqueServidores.addView(listaServidores);
        bloqueServidores.addView(listaEncontrados);
        bloqueServidores.addView(botones, lpBotones);
        pintarServidores();

        getSharedPreferences(Ajustes.PREFS, MODE_PRIVATE)
                .registerOnSharedPreferenceChangeListener(cambioAnfitrion);
        View t = tarjeta(
            seccion(R.string.servidor_lan),
            resumen(R.string.servidor_lan_resumen),
            filaAnfitrion,
            textoAnfitrion,
            espacio(4),
            bloqueServidores
        );
        actualizarAnfitrion();
        return t;
    }

    /** Controles tactiles: el editor va en su propia pantalla. */
    private View tarjetaControles() {
        MaterialButton editarTactil = new MaterialButton(this);
        editarTactil.setText(R.string.editar_tactil);
        estilarBotonSecundario(editarTactil);
        editarTactil.setOnClickListener(v -> startActivity(new Intent(this, EditorTactilActivity.class)));
        MaterialButton probarVibracion = new MaterialButton(this);
        probarVibracion.setText(R.string.probar_vibracion);
        estilarBotonSecundario(probarVibracion);
        TextView vibracionProbada = resumen(0);
        vibracionProbada.setVisibility(View.GONE);
        probarVibracion.setOnClickListener(v -> probarVibracion(vibracionProbada));
        LinearLayout filaVibracion = new LinearLayout(this);
        filaVibracion.setOrientation(LinearLayout.VERTICAL);
        filaVibracion.addView(interruptor(R.string.vibracion, R.string.vibracion_resumen,
                ajustes.vibracion(), ajustes::vibracion));
        filaVibracion.addView(probarVibracion);
        filaVibracion.addView(vibracionProbada);
        return tarjeta(
            seccion(R.string.seccion_tactil),
            resumen(R.string.editar_tactil_resumen),
            espacio(8),
            editarTactil,
            soloNativo(filaVibracion),
            soloNativo(interruptor(R.string.menus_tactiles, R.string.menus_tactiles_resumen,
                    ajustes.menusTactiles(), ajustes::menusTactiles)),
            selectorGiro()
        );
    }

    /**
     * Con que se gira en las carreras (Ajustes.modoGiro). La sensibilidad solo
     * cuenta inclinando el movil: con lo demas no se ve.
     */
    private View selectorGiro() {
        View sensibilidad = deslizador(R.string.sensibilidad_giro, R.string.sensibilidad_giro_resumen,
                Ajustes.SENSIBILIDAD_MIN, Ajustes.SENSIBILIDAD_MAX, ajustes.sensibilidadGiro(),
                s -> getString(R.string.sensibilidad_valor, s, Math.round(Ajustes.anguloGiro(s))),
                ajustes::sensibilidadGiro);
        sensibilidad.setPadding(dp(32), sensibilidad.getPaddingTop(), 0, sensibilidad.getPaddingBottom());
        Consumer<String> mostrar = modo -> sensibilidad.setVisibility(
                Ajustes.GIRO_INCLINAR.equals(modo) ? View.VISIBLE : View.GONE);
        mostrar.accept(ajustes.modoGiro());
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.addView(selector(R.string.giro_carrera, R.string.giro_carrera_resumen,
                Arrays.asList(getString(R.string.giro_stick), getString(R.string.giro_flechas),
                        getString(R.string.giro_inclinar)),
                Arrays.asList(Ajustes.GIRO_STICK, Ajustes.GIRO_FLECHAS, Ajustes.GIRO_INCLINAR),
                ajustes.modoGiro(), true, modo -> {
                    ajustes.modoGiro(modo);
                    mostrar.accept(modo);
                }));
        caja.addView(sensibilidad);
        return caja;
    }

    /** Todos los idiomas a la vista; al elegir uno, la pantalla se rehace en el. */
    private View tarjetaIdioma() {
        return tarjeta(
            seccion(R.string.seccion_idioma),
            selector(0, 0, nombresIdiomas(), Arrays.asList(Idioma.CODIGOS), Idioma.elegido(this), false,
                    codigo -> {
                        if (!codigo.equals(Idioma.elegido(this))) {
                            Idioma.elegir(this, codigo);
                            recreate();
                        }
                    })
        );
    }

    /** Sonido (motor nativo: por donde sale el audio). */
    private View tarjetaSonido() {
        return tarjeta(
            seccion(R.string.seccion_sonido),
            interruptor(R.string.audio_aaudio, R.string.audio_aaudio_resumen,
                    ajustes.audioAAudio(), ajustes::audioAAudio)
        );
    }

    private View tarjetaRendimiento() {
        grupoDrivers = new RadioGroup(this);
        LinearLayout filaDriver = new LinearLayout(this);
        filaDriver.setOrientation(LinearLayout.HORIZONTAL);
        filaDriver.setPadding(0, dp(8), 0, dp(8));

        MaterialButton importar = new MaterialButton(this);
        importar.setText(R.string.importar_driver);
        estilarBotonSecundario(importar);
        importar.setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("application/zip");
            startActivityForResult(i, PEDIR_DRIVER);
        });

        MaterialButton borrar = new MaterialButton(this);
        borrar.setText(R.string.borrar_driver);
        estilarBotonPeligro(borrar);
        borrar.setOnClickListener(v -> {
            Drivers.Driver d = Drivers.buscar(this, ajustes.driver());
            if (d != null) {
                Drivers.eliminar(d);
                ajustes.driver(null);
                refrescar();
            }
        });

        LinearLayout.LayoutParams lpBotones = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lpBotones.rightMargin = dp(8);
        filaDriver.addView(importar, lpBotones);
        LinearLayout.LayoutParams lpBorrar = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        filaDriver.addView(borrar, lpBorrar);

        filaTurbo = interruptor(R.string.turbo, R.string.turbo_resumen, ajustes.turbo(), ajustes::turbo);

        return tarjeta(
            seccion(R.string.seccion_rendimiento),
            interruptor(R.string.fijar_hilos, R.string.fijar_hilos_resumen, ajustes.fijarHilos(), ajustes::fijarHilos),
            titulo(R.string.driver),
            ayudaDriver(),
            grupoDrivers,
            filaDriver,
            filaTurbo
        );
    }

    /** Avanzado: ya en su propia seccion, sin plegar. */
    private View tarjetaAvanzado() {
        MaterialButton sonda = new MaterialButton(this);
        sonda.setText(R.string.probar_vulkan);
        estilarBotonSecundario(sonda);
        sonda.setOnClickListener(v -> lanzar(true));

        textoInforme = new TextView(this);
        textoInforme.setTypeface(Typeface.MONOSPACE);
        textoInforme.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        textoInforme.setTextColor(0xFFCCCCCC);
        textoInforme.setTextIsSelectable(true);
        textoInforme.setPadding(0, dp(12), 0, 0);

        LinearLayout avanzado = new LinearLayout(this);
        avanzado.setOrientation(LinearLayout.VERTICAL);
        avanzado.addView(resumen(R.string.avanzado_resumen));
        // Los de la GPU de Xbox 360 imitada: con el motor nativo no existen.
        avanzado.addView(soloXenos(interruptor(R.string.una_pasada, R.string.una_pasada_resumen, ajustes.unaPasada(), ajustes::unaPasada)));
        avanzado.addView(soloXenos(interruptor(R.string.edram_fsi, R.string.edram_fsi_resumen, ajustes.edramEnShader(), ajustes::edramEnShader)));
        avanzado.addView(soloXenos(interruptor(R.string.oclusion, R.string.oclusion_resumen, ajustes.oclusion(), ajustes::oclusion)));
        avanzado.addView(soloXenos(interruptor(R.string.exposicion, R.string.exposicion_resumen, ajustes.exposicionFiel(), ajustes::exposicionFiel)));
        avanzado.addView(soloXenos(interruptor(R.string.memexport, R.string.memexport_resumen, ajustes.lecturaMemexport(), ajustes::lecturaMemexport)));
        avanzado.addView(soloXenos(interruptor(R.string.paginas, R.string.paginas_resumen, ajustes.refrescarPaginas(), ajustes::refrescarPaginas)));
        // En release no hay registro: el interruptor no haria nada.
        if (BuildConfig.DEBUG) {
            avanzado.addView(interruptor(R.string.registro_detallado, R.string.registro_detallado_resumen, ajustes.registroDetallado(), ajustes::registroDetallado));
        }
        // Motor nativo: su emulacion de la GPU, para las GPU que no pueden con el renderizador.
        avanzado.addView(soloNativo(interruptor(R.string.compatibilidad, R.string.compatibilidad_resumen,
                ajustes.compatibilidad(), ajustes::compatibilidad)));
        avanzado.addView(espacio(16));
        avanzado.addView(titulo(R.string.probar_vulkan));
        avanzado.addView(resumen(R.string.probar_vulkan_resumen));
        avanzado.addView(espacio(8));
        avanzado.addView(sonda);
        avanzado.addView(textoInforme);

        return tarjeta(seccion(R.string.seccion_avanzado), avanzado);
    }

    /**
     * El gamertag. Se guarda segun se escribe, pero solo si es valido para
     * Xbox 360 (Ajustes.gamertagValido); si no, se avisa y se queda el
     * ultimo bueno. Vacio = el de fabrica, "User".
     */
    private View campoGamertag() {
        TextInputLayout caja = new TextInputLayout(this, null,
                com.google.android.material.R.attr.textInputOutlinedStyle);
        caja.setHint(R.string.gamertag);
        caja.setCounterEnabled(true);
        caja.setCounterMaxLength(15);
        caja.setBoxStrokeColor(ACENTO);
        caja.setHintTextColor(ColorStateList.valueOf(ACENTO));
        TextInputEditText texto = new TextInputEditText(caja.getContext());
        texto.setSingleLine(true);
        texto.setTextColor(Color.WHITE);
        texto.setFilters(new InputFilter[] {new InputFilter.LengthFilter(15)});
        // En horizontal, sin el teclado a pantalla completa: se ve el campo y
        // debajo si el nombre esta libre en el servidor.
        texto.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_ACTION_DONE);
        texto.setText(ajustes.gamertag());
        // El ejemplo va en la caja, no en el campo: con el campo vacio y sin
        // foco, la etiqueta y su pista se pintaban una encima de otra. Asi la
        // etiqueta se queda arriba y el ejemplo, en gris, dentro.
        caja.setExpandedHintEnabled(false);
        caja.setPlaceholderText("User");
        texto.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void afterTextChanged(Editable s) {
                String g = s.toString().trim();
                gamertagEscritoValido = Ajustes.gamertagValido(g);
                if (gamertagEscritoValido) {
                    caja.setError(null);
                    ajustes.gamertag(g);
                    programarComprobacion();
                } else {
                    // Mal escrito: manda el error de formato, no el del servidor.
                    hiloPrincipal.removeCallbacks(comprobacionPendiente);
                    comprobaciones++;
                    caja.setHelperText(null);
                    caja.setError(getString(R.string.gamertag_error));
                }
            }
        });
        caja.addView(texto);
        cajaGamertag = caja;
        return caja;
    }

    /**
     * La lista de servidores LAN: "Sin servidor" y los guardados. El elegido
     * (Ajustes.servidorLan) va marcado.
     */
    private void pintarServidores() {
        if (listaServidores == null) {
            return;
        }
        listaServidores.removeAllViews();
        String elegida = ajustes.servidorLan();
        List<Ajustes.Servidor> guardados = ajustes.servidores();
        listaServidores.addView(filaServidor(null, Ajustes.indiceServidor(guardados, elegida) < 0));
        for (Ajustes.Servidor s : guardados) {
            // El que se edita: el formulario en su sitio.
            listaServidores.addView(editorServidor != null && s.direccion.equals(direccionEditada)
                    ? editorServidor : filaServidor(s, s.direccion.equals(elegida)));
        }
        if (editorServidor != null && editorServidor.getParent() == null) {
            listaServidores.addView(editorServidor);
        }
        filaBotonesServidores.setVisibility(editorServidor == null ? View.VISIBLE : View.GONE);
        // "Ya guardado" depende de la lista.
        pintarEncontrados();
    }

    /** Busca servidores en la red (BuscadorServidores, ~1,5 s) y los pone bajo los guardados. */
    private void buscarServidores() {
        if (buscando) {
            return;
        }
        buscando = true;
        pintarEncontrados();
        new Thread(() -> {
            List<BuscadorServidores.Encontrado> r = BuscadorServidores.buscar();
            hiloPrincipal.post(() -> {
                if (isDestroyed()) {
                    return;
                }
                buscando = false;
                busquedaSinRed = r == null;
                encontrados = r == null ? new ArrayList<>() : r;
                pintarEncontrados();
            });
        }, "buscar-servidores").start();
    }

    /** Lo que encontro la ultima busqueda, o por que no hay nada. */
    private void pintarEncontrados() {
        if (listaEncontrados == null) {
            return;
        }
        botonBuscar.setEnabled(!buscando);
        botonBuscar.setText(buscando ? R.string.servidor_buscando : R.string.servidor_buscar);
        listaEncontrados.removeAllViews();
        if (encontrados == null) {
            listaEncontrados.setVisibility(View.GONE);
            return;
        }
        listaEncontrados.setVisibility(View.VISIBLE);
        listaEncontrados.addView(titulo(R.string.servidores_encontrados));
        if (encontrados.isEmpty()) {
            TextView nada = resumen(busquedaSinRed ? R.string.servidor_anfitrion_sin_red
                    : R.string.servidor_ninguno_encontrado);
            nada.setPadding(0, dp(4), 0, 0);
            listaEncontrados.addView(nada);
            return;
        }
        List<Ajustes.Servidor> guardados = ajustes.servidores();
        for (BuscadorServidores.Encontrado e : encontrados) {
            listaEncontrados.addView(filaEncontrado(e, Ajustes.indiceServidor(guardados, e.ip) >= 0));
        }
    }

    /** Un servidor que contesto a la busqueda; tocarlo lo guarda (si no lo estaba) y lo elige. */
    private View filaEncontrado(BuscadorServidores.Encontrado e, boolean guardado) {
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.HORIZONTAL);
        fila.setGravity(Gravity.CENTER_VERTICAL);
        fila.setMinimumHeight(dp(56));
        fila.setPadding(dp(12), dp(4), dp(4), dp(4));
        fila.setBackground(fondoFila(0xFF262626, 0));

        ImageView icono = new ImageView(this);
        icono.setImageResource(R.drawable.ic_servidor);
        icono.setImageTintList(ColorStateList.valueOf(ACENTO_CLARO));
        icono.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        LinearLayout.LayoutParams lpIcono = new LinearLayout.LayoutParams(dp(24), dp(24));
        lpIcono.rightMargin = dp(12);
        fila.addView(icono, lpIcono);

        String etiqueta = e.nombre.isEmpty() ? e.ip : e.nombre;
        LinearLayout textos = new LinearLayout(this);
        textos.setOrientation(LinearLayout.VERTICAL);
        TextView principal = new TextView(this);
        principal.setText(etiqueta);
        principal.setTextColor(Color.WHITE);
        principal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        textos.addView(principal);
        TextView segundo = resumen(0);
        segundo.setText(getString(guardado ? R.string.servidor_encontrado_guardado
                : R.string.servidor_encontrado_detalle, e.ip, e.jugadores));
        textos.addView(segundo);
        fila.addView(textos, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        if (!guardado) {
            fila.addView(botonIcono(R.drawable.ic_anadir, getString(R.string.servidor_encontrado_anadir, etiqueta),
                    v -> usarEncontrado(e)));
        }
        fila.setOnClickListener(v -> usarEncontrado(e));

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(6);
        fila.setLayoutParams(lp);
        return fila;
    }

    private void usarEncontrado(BuscadorServidores.Encontrado e) {
        cerrarEditor();
        List<Ajustes.Servidor> l = ajustes.servidores();
        boolean nuevo = Ajustes.indiceServidor(l, e.ip) < 0;
        if (nuevo) {
            // El nombre, con el limite del formulario.
            String n = e.nombre.length() > 24 ? e.nombre.substring(0, 24) : e.nombre;
            l.add(new Ajustes.Servidor(n, e.ip));
            ajustes.servidores(l);
        }
        // Ya esta en la lista de arriba, elegido: la busqueda ha cumplido.
        encontrados = null;
        elegirServidor(e.ip);
        pintarServidores();
        if (nuevo) {
            Snackbar.make(listaServidores, getString(R.string.servidor_guardado,
                    e.nombre.isEmpty() ? e.ip : e.nombre), Snackbar.LENGTH_SHORT).show();
        }
    }

    /** Una fila de la lista; s null = "Sin servidor", que no se edita ni se quita. */
    private View filaServidor(Ajustes.Servidor s, boolean elegido) {
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.HORIZONTAL);
        fila.setGravity(Gravity.CENTER_VERTICAL);
        fila.setMinimumHeight(dp(56));
        fila.setPadding(dp(4), dp(4), dp(4), dp(4));
        fila.setBackground(fondoFila(elegido ? ELEGIDO : 0xFF262626, elegido ? ACENTO : 0));

        // La marca la pone la fila entera: el boton solo se ve.
        MaterialRadioButton marca = new MaterialRadioButton(this);
        marca.setChecked(elegido);
        marca.setClickable(false);
        marca.setFocusable(false);
        marca.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        marca.setButtonTintList(new ColorStateList(
                new int[][] {{android.R.attr.state_checked}, {}},
                new int[] {ACENTO_CLARO, 0xFF9E9E9E}));
        fila.addView(marca);

        String etiqueta = s == null ? getString(R.string.servidor_ninguno)
                : s.nombre.isEmpty() ? s.direccion : s.nombre;
        LinearLayout textos = new LinearLayout(this);
        textos.setOrientation(LinearLayout.VERTICAL);
        TextView principal = new TextView(this);
        principal.setText(etiqueta);
        principal.setTextColor(Color.WHITE);
        principal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        if (elegido) principal.setTypeface(Typeface.DEFAULT_BOLD);
        textos.addView(principal);
        String detalle = s == null ? getString(R.string.servidor_ninguno_resumen)
                : s.nombre.isEmpty() ? null : s.direccion;
        if (detalle != null) {
            TextView segundo = resumen(0);
            segundo.setText(detalle);
            textos.addView(segundo);
        }
        fila.addView(textos, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        if (s != null) {
            fila.addView(botonIcono(R.drawable.ic_editar, getString(R.string.servidor_editar, etiqueta),
                    v -> abrirEditor(s)));
            fila.addView(botonIcono(R.drawable.ic_eliminar, getString(R.string.servidor_eliminar, etiqueta),
                    v -> eliminarServidor(s)));
        }
        fila.setOnClickListener(v -> elegirServidor(s == null ? "" : s.direccion));
        fila.setSelected(elegido);

        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(6);
        fila.setLayoutParams(lp);
        return fila;
    }

    private ImageButton botonIcono(int icono, String descripcion, View.OnClickListener alPulsar) {
        ImageButton b = new ImageButton(this);
        b.setImageResource(icono);
        b.setImageTintList(ColorStateList.valueOf(0xFFD0D0D0));
        b.setContentDescription(descripcion);
        TypedValue fondo = new TypedValue();
        getTheme().resolveAttribute(android.R.attr.selectableItemBackgroundBorderless, fondo, true);
        b.setBackgroundResource(fondo.resourceId);
        b.setOnClickListener(alPulsar);
        b.setLayoutParams(new LinearLayout.LayoutParams(dp(48), dp(48)));
        return b;
    }

    /** "" = sin servidor: el juego como siempre. */
    private void elegirServidor(String ip) {
        if (ip.equals(ajustes.servidorLan())) {
            return;
        }
        ajustes.servidorLan(ip);
        pintarServidores();
        // Otro servidor, otros jugadores: el nombre puede estar en uso alli.
        programarComprobacion();
    }

    /**
     * Anadir (editando null) o editar un servidor: nombre opcional e IP. No en un
     * dialogo: en horizontal el teclado lo deja sin sitio para los campos. El
     * formulario va en la propia lista, en el sitio del servidor que se edita
     * (o al final, si es nuevo), con la IP y el nombre en una fila para que
     * quepan encima del teclado. Guardar solo se puede con una IPv4 valida que
     * no este ya en la lista. Uno nuevo queda elegido; al editar el elegido,
     * sigue elegido con su IP nueva.
     */
    private void abrirEditor(Ajustes.Servidor editando) {
        cerrarEditor();
        direccionEditada = editando == null ? "" : editando.direccion;

        TextInputLayout cajaIp = new TextInputLayout(this, null,
                com.google.android.material.R.attr.textInputOutlinedStyle);
        cajaIp.setHint(R.string.servidor_lan_ip);
        cajaIp.setBoxStrokeColor(ACENTO);
        cajaIp.setHintTextColor(ColorStateList.valueOf(ACENTO_CLARO));
        cajaIp.setExpandedHintEnabled(false);
        cajaIp.setPlaceholderText("10.0.0.4");
        TextInputEditText ip = new TextInputEditText(cajaIp.getContext());
        ip.setSingleLine(true);
        ip.setTextColor(Color.WHITE);
        // Una IP o un nombre de equipo: teclado de direcciones (con el punto a
        // mano y sin autocorregir), no solo numeros.
        ip.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        ip.setFilters(new InputFilter[] {new InputFilter.LengthFilter(253)});
        ip.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_ACTION_NEXT);
        cajaIp.addView(ip);

        TextInputLayout cajaNombre = new TextInputLayout(this, null,
                com.google.android.material.R.attr.textInputOutlinedStyle);
        cajaNombre.setHint(R.string.servidor_nombre);
        cajaNombre.setBoxStrokeColor(ACENTO);
        cajaNombre.setHintTextColor(ColorStateList.valueOf(ACENTO_CLARO));
        TextInputEditText nombre = new TextInputEditText(cajaNombre.getContext());
        nombre.setSingleLine(true);
        nombre.setTextColor(Color.WHITE);
        nombre.setFilters(new InputFilter[] {new InputFilter.LengthFilter(24)});
        nombre.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI | EditorInfo.IME_ACTION_DONE);
        cajaNombre.addView(nombre);

        if (editando != null) {
            ip.setText(editando.direccion);
            nombre.setText(editando.nombre);
        }

        LinearLayout campos = new LinearLayout(this);
        campos.setOrientation(LinearLayout.HORIZONTAL);
        campos.addView(cajaIp, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        LinearLayout.LayoutParams lpNombre = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.3f);
        lpNombre.leftMargin = dp(8);
        campos.addView(cajaNombre, lpNombre);

        // Los botones en la linea del titulo, bajos: con el teclado abierto, en
        // horizontal, debajo de los campos quedarian tapados.
        MaterialButton cancelar = new MaterialButton(this);
        cancelar.setText(android.R.string.cancel);
        estilarBotonSecundario(cancelar);
        cancelar.setOnClickListener(v -> cerrarEditor());
        MaterialButton guardar = new MaterialButton(this);
        guardar.setText(R.string.guardar);
        estilarBotonPrimario(guardar);
        TextView titulo = titulo(editando == null ? R.string.servidor_anadir : R.string.servidor_editar_titulo);
        titulo.setPadding(dp(4), 0, 0, 0);
        LinearLayout cabecera = new LinearLayout(this);
        cabecera.setOrientation(LinearLayout.HORIZONTAL);
        cabecera.setGravity(Gravity.CENTER_VERTICAL);
        cabecera.setPadding(0, 0, 0, dp(6));
        cabecera.addView(titulo, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        for (MaterialButton b : new MaterialButton[] {cancelar, guardar}) {
            b.setInsetTop(0);
            b.setInsetBottom(0);
            b.setMinHeight(dp(40));
            b.setMinimumHeight(dp(40));
            LinearLayout.LayoutParams lpBoton = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, dp(40));
            lpBoton.leftMargin = dp(8);
            cabecera.addView(b, lpBoton);
        }

        LinearLayout editor = new LinearLayout(this);
        editor.setOrientation(LinearLayout.VERTICAL);
        editor.setPadding(dp(12), dp(8), dp(12), dp(4));
        GradientDrawable fondo = new GradientDrawable();
        fondo.setCornerRadius(dp(12));
        fondo.setColor(0xFF262626);
        fondo.setStroke(dp(1), ACENTO);
        editor.setBackground(fondo);
        editor.addView(cabecera);
        editor.addView(campos);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(6);
        editor.setLayoutParams(lp);

        // La direccion que se puede guardar, o null (y el porque, bajo el campo).
        java.util.function.Supplier<String> ipValidada = () -> {
            String t = ip.getText().toString().trim();
            boolean valida = Ajustes.direccionValida(t);
            boolean repetida = valida && (editando == null || !t.equalsIgnoreCase(editando.direccion))
                    && Ajustes.indiceServidor(ajustes.servidores(), t) >= 0;
            cajaIp.setError(t.isEmpty() ? null : !valida ? getString(R.string.servidor_lan_error)
                    : repetida ? getString(R.string.servidor_repetido) : null);
            boolean vale = valida && !repetida;
            guardar.setEnabled(vale);
            return vale ? t : null;
        };
        ip.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void onTextChanged(CharSequence s, int a, int b, int c) {}
            @Override public void afterTextChanged(Editable s) { ipValidada.get(); }
        });
        ipValidada.get();
        Runnable aceptar = () -> {
            String nueva = ipValidada.get();
            if (nueva == null) {
                ip.requestFocus();
                return;
            }
            List<Ajustes.Servidor> l = ajustes.servidores();
            Ajustes.Servidor s = new Ajustes.Servidor(nombre.getText().toString(), nueva);
            int donde = editando == null ? -1 : Ajustes.indiceServidor(l, editando.direccion);
            if (donde >= 0) {
                l.set(donde, s);
            } else {
                l.add(s);
            }
            boolean eraElegido = editando != null && editando.direccion.equals(ajustes.servidorLan());
            // Primero la lista: servidores() mete la IP elegida si no esta.
            ajustes.servidores(l);
            if (editando == null || eraElegido) {
                ajustes.servidorLan(nueva);
                programarComprobacion();
            }
            cerrarEditor();
        };
        guardar.setOnClickListener(v -> aceptar.run());
        nombre.setOnEditorActionListener((v, accion, e) -> {
            if (accion == EditorInfo.IME_ACTION_DONE) {
                aceptar.run();
                return true;
            }
            return false;
        });

        editorServidor = editor;
        pintarServidores();
        ip.requestFocus();
        ip.setSelection(ip.length());
        ip.post(() -> {
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput(ip, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    /** Quita el formulario de la lista (sin guardar) y el teclado. */
    private void cerrarEditor() {
        if (editorServidor == null) {
            return;
        }
        View foco = editorServidor.findFocus();
        if (foco != null) {
            InputMethodManager imm = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(foco.getWindowToken(), 0);
            }
            foco.clearFocus();
        }
        editorServidor = null;
        direccionEditada = null;
        pintarServidores();
    }

    /** Quita un servidor guardado, con la opcion de deshacerlo. Si era el elegido, queda "Sin servidor". */
    private void eliminarServidor(Ajustes.Servidor s) {
        List<Ajustes.Servidor> l = ajustes.servidores();
        int donde = Ajustes.indiceServidor(l, s.direccion);
        if (donde < 0) {
            return;
        }
        boolean eraElegido = s.direccion.equals(ajustes.servidorLan());
        l.remove(donde);
        if (eraElegido) {
            ajustes.servidorLan("");
            programarComprobacion();
        }
        ajustes.servidores(l);
        pintarServidores();
        String etiqueta = s.nombre.isEmpty() ? s.direccion : s.nombre;
        Snackbar.make(listaServidores, getString(R.string.servidor_eliminado, etiqueta), Snackbar.LENGTH_LONG)
                .setAction(R.string.deshacer, v -> {
                    List<Ajustes.Servidor> ahora = ajustes.servidores();
                    if (Ajustes.indiceServidor(ahora, s.direccion) >= 0) {
                        return;
                    }
                    ahora.add(Math.min(donde, ahora.size()), s);
                    ajustes.servidores(ahora);
                    if (eraElegido && ajustes.servidorLan().isEmpty()) {
                        ajustes.servidorLan(s.direccion);
                        programarComprobacion();
                    }
                    pintarServidores();
                })
                .show();
    }

    @SuppressWarnings("deprecation")
    private View selectorResolucion() {
        DisplayMetrics dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(dm);
        int largo = Math.max(dm.widthPixels, dm.heightPixels);
        int corto = Math.min(dm.widthPixels, dm.heightPixels);
        List<String> textos = new ArrayList<>();
        List<String> valores = new ArrayList<>();
        if (corto > 480) {
            textos.add(getString(R.string.res_480p));
            valores.add(Ajustes.RES_480P);
        }
        if (corto > 720) {
            textos.add(getString(R.string.res_720p));
            valores.add(Ajustes.RES_720P);
        }
        textos.add(getString(R.string.res_nativa, largo, corto));
        valores.add(Ajustes.RES_NATIVA);
        String actual = valores.contains(ajustes.resolucion())
                ? ajustes.resolucion() : Ajustes.RES_NATIVA;
        return selector(R.string.resolucion, R.string.resolucion_resumen, textos, valores,
                actual, false, ajustes::resolucion);
    }

    /** Un ajuste que solo existe con el motor de Xenos: con el nativo no se muestra. */
    private View soloXenos(View v) {
        if (MotorNativo.ACTIVO) {
            v.setVisibility(View.GONE);
        }
        return v;
    }

    /** Y al reves. */
    private View soloNativo(View v) {
        if (!MotorNativo.ACTIVO) {
            v.setVisibility(View.GONE);
        }
        return v;
    }

    private View selectorSuavizado() {
        View opcionesMsaa = selector(R.string.msaa_muestras, R.string.msaa_muestras_resumen,
                Arrays.asList(getString(R.string.escala_valor, 2), getString(R.string.escala_valor, 4)),
                Arrays.asList(2, 4), ajustes.msaaMuestras(), true, ajustes::msaaMuestras);
        View opcionesFxaa = selector(R.string.fxaa_calidad, R.string.fxaa_calidad_resumen,
                Arrays.asList(getString(R.string.fxaa_normal), getString(R.string.fxaa_alta)),
                Arrays.asList(false, true), ajustes.fxaaAlta(), true, ajustes::fxaaAlta);
        
        opcionesMsaa.setPadding(dp(32), 0, 0, 0);
        opcionesFxaa.setPadding(dp(32), 0, 0, 0);
        Consumer<String> mostrar = aa -> {
            opcionesMsaa.setVisibility(Ajustes.AA_MSAA.equals(aa) ? View.VISIBLE : View.GONE);
            opcionesFxaa.setVisibility(Ajustes.AA_FXAA.equals(aa) ? View.VISIBLE : View.GONE);
        };
        mostrar.accept(ajustes.antialiasing());

        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        if (MotorNativo.ACTIVO) {
            // El motor nativo pinta la escena en una pasada, sin MSAA: solo hay
            // FXAA, y sin niveles.
            caja.addView(selector(R.string.suavizado, R.string.suavizado_resumen,
                    Arrays.asList(getString(R.string.aa_no), getString(R.string.aa_fxaa)),
                    Arrays.asList(Ajustes.AA_NO, Ajustes.AA_FXAA),
                    Ajustes.AA_FXAA.equals(ajustes.antialiasing()) ? Ajustes.AA_FXAA : Ajustes.AA_NO,
                    true, ajustes::antialiasing));
            return caja;
        }
        caja.addView(selector(R.string.suavizado, R.string.suavizado_resumen,
                Arrays.asList(getString(R.string.aa_no), getString(R.string.aa_msaa),
                        getString(R.string.aa_fxaa)),
                Arrays.asList(Ajustes.AA_NO, Ajustes.AA_MSAA, Ajustes.AA_FXAA),
                ajustes.antialiasing(), true, aa -> {
                    ajustes.antialiasing(aa);
                    mostrar.accept(aa);
                }));
        caja.addView(opcionesMsaa);
        caja.addView(opcionesFxaa);
        return caja;
    }

    private LinearLayout seccion(int texto) {
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.setPadding(0, dp(8), 0, dp(16));
        TextView t = new TextView(this);
        t.setText(texto);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setTextColor(Color.WHITE);
        caja.addView(t);
        View raya = new View(this);
        raya.setBackgroundColor(ACENTO);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(2));
        lp.topMargin = dp(8);
        caja.addView(raya, lp);
        return caja;
    }

    /** Turnip es solo para Adreno: con otra GPU, la ayuda lo dice. */
    private TextView ayudaDriver() {
        TextView t = resumen(0);
        if (Gpu.familia(this) == Gpu.Familia.ADRENO) {
            t.setText(R.string.ayuda_driver);
        } else {
            String gpu = Gpu.nombre(this);
            t.setText(getString(R.string.ayuda_driver_otra,
                    gpu.isEmpty() ? getString(R.string.gpu_desconocida) : gpu));
        }
        return t;
    }

    private TextView titulo(int texto) {
        TextView t = new TextView(this);
        if (texto != 0) t.setText(texto);
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        t.setTypeface(Typeface.DEFAULT_BOLD);
        t.setTextColor(Color.WHITE);
        t.setPadding(0, dp(12), 0, 0);
        return t;
    }

    /**
     * Un deslizador de enteros, con su valor escrito debajo del titulo
     * mientras se mueve. Se guarda en cada paso.
     */
    private View deslizador(int titulo, int resumen, int min, int max, int actual,
                            java.util.function.IntFunction<String> texto, Consumer<Integer> guardar) {
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.setPadding(0, dp(6), 0, dp(6));
        caja.addView(titulo(titulo));
        TextView valor = resumen(0);
        valor.setTextColor(Color.WHITE);
        valor.setText(texto.apply(actual));
        caja.addView(valor);
        Slider s = new Slider(this);
        s.setValueFrom(min);
        s.setValueTo(max);
        s.setStepSize(1);
        s.setValue(Math.max(min, Math.min(max, actual)));
        s.setLabelBehavior(LabelFormatter.LABEL_GONE);
        s.setThumbTintList(ColorStateList.valueOf(Color.WHITE));
        s.setTrackActiveTintList(ColorStateList.valueOf(ACENTO));
        s.setTrackInactiveTintList(ColorStateList.valueOf(0xFF303030));
        s.setHaloTintList(ColorStateList.valueOf(0x336F7432));
        s.addOnChangeListener((slider, v, delUsuario) -> {
            int n = Math.round(v);
            valor.setText(texto.apply(n));
            if (delUsuario) guardar.accept(n);
        });
        caja.addView(s, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        if (resumen != 0) caja.addView(resumen(resumen));
        return caja;
    }

    /**
     * Probar vibracion: cada mando fisico conectado vibra con su vibrador de Android, el
     * mismo que usa SDL en la partida, y se dice cuantos motores le da Android. Sin
     * motores ahi, en la partida tampoco puede vibrar. Sin mandos, vibra el movil.
     */
    private void probarVibracion(TextView salida) {
        StringBuilder texto = new StringBuilder();
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice d = InputDevice.getDevice(id);
            if (d == null || d.isVirtual() || !TouchControllerView.esFuenteDeMando(d.getSources())) {
                continue;
            }
            String usb = String.format(Locale.ROOT, "%04X:%04X", d.getVendorId(), d.getProductId());
            int motores = 0;
            if (Build.VERSION.SDK_INT >= 31) {
                VibratorManager vm = d.getVibratorManager();
                for (int v : vm.getVibratorIds()) {
                    if (vibrarUnRato(vm.getVibrator(v))) motores++;
                }
            } else if (vibrarUnRato(d.getVibrator())) {
                motores = 1;
            }
            if (texto.length() > 0) texto.append('\n');
            if (motores > 0) {
                texto.append(getString(R.string.vibracion_mando_motores, d.getName(), usb, motores));
            } else if (esXboxUsb(d.getVendorId(), d.getProductId())) {
                texto.append(getString(R.string.vibracion_mando_xbox_usb, d.getName(), usb));
            } else {
                texto.append(getString(R.string.vibracion_mando_sin_motores, d.getName(), usb));
            }
        }
        if (texto.length() == 0) {
            Vibrator movil;
            if (Build.VERSION.SDK_INT >= 31) {
                VibratorManager vm = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                movil = vm == null ? null : vm.getDefaultVibrator();
            } else {
                movil = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            }
            vibrarUnRato(movil);
            texto.append(getString(R.string.vibracion_sin_mandos));
        }
        salida.setText(texto);
        salida.setVisibility(View.VISIBLE);
    }

    /** Ese mando esta conectado por USB y es de Xbox: en la partida lo abre SDL. */
    private boolean esXboxUsb(int vendor, int producto) {
        UsbManager um = (UsbManager) getSystemService(Context.USB_SERVICE);
        if (um == null) return false;
        for (UsbDevice u : um.getDeviceList().values()) {
            if (u.getVendorId() == vendor && u.getProductId() == producto
                    && TouchControllerView.esXboxUsb(u)) {
                return true;
            }
        }
        return false;
    }

    /** Medio segundo de vibracion; false si ese vibrador no existe. */
    private static boolean vibrarUnRato(Vibrator v) {
        if (v == null || !v.hasVibrator()) return false;
        try {
            v.vibrate(VibrationEffect.createOneShot(500, VibrationEffect.DEFAULT_AMPLITUDE));
        } catch (RuntimeException e) {
            v.vibrate(500);
        }
        return true;
    }

    private TextView resumen(int texto) {
        TextView t = new TextView(this);
        if (texto != 0) {
            t.setText(texto);
        }
        t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        t.setTextColor(GRIS);
        return t;
    }

    private View interruptor(int titulo, int resumen, boolean valor, Consumer<Boolean> guardar) {
        LinearLayout fila = new LinearLayout(this);
        fila.setOrientation(LinearLayout.VERTICAL);
        fila.setPadding(0, dp(10), 0, dp(6));
        MaterialSwitch s = new MaterialSwitch(this);
        s.setText(titulo);
        s.setTextColor(Color.WHITE);
        s.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        
        // Colores personalizados del toggle (Blanco al encender)
        ColorStateList thumbTint = new ColorStateList(
                new int[][] {{android.R.attr.state_checked}, {}},
                new int[] {Color.WHITE, 0xFF9E9E9E});
        ColorStateList trackTint = new ColorStateList(
                new int[][] {{android.R.attr.state_checked}, {}},
                new int[] {0xFF666666, 0xFF303030});
        s.setThumbTintList(thumbTint);
        s.setTrackTintList(trackTint);
        
        s.setChecked(valor);
        s.setOnCheckedChangeListener((b, v) -> guardar.accept(v));
        fila.addView(s, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        TextView r = resumen(resumen);
        r.setPadding(0, 0, dp(64), 0);
        r.setOnClickListener(v -> {
            if (s.isEnabled()) {
                s.toggle();
            }
        });
        fila.addView(r);
        return fila;
    }

    private <T> View selector(int titulo, int resumen, List<String> textos, List<T> valores,
                              T actual, boolean horizontal, Consumer<T> guardar) {
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.setPadding(0, dp(12), 0, dp(12));
        if (titulo != 0) {
            caja.addView(titulo(titulo));
        }
        if (resumen != 0) {
            caja.addView(resumen(resumen));
        }
        // En fila solo si caben: con textos largos los botones se estrechan, el
        // texto se parte en varias lineas y la fila queda alta, con un hueco en
        // blanco debajo. Entonces, en columna.
        int total = 0;
        int mayor = 0;
        for (String t : textos) {
            total += t.length();
            mayor = Math.max(mayor, t.length());
        }
        boolean enFila = horizontal && total <= 30 && mayor <= 14;
        RadioGroup grupo = new RadioGroup(this);
        grupo.setOrientation(enFila ? RadioGroup.HORIZONTAL : RadioGroup.VERTICAL);
        grupo.setPadding(0, dp(8), 0, 0);
        for (int i = 0; i < textos.size(); i++) {
            MaterialRadioButton rb = new MaterialRadioButton(this);
            rb.setId(View.generateViewId());
            rb.setText(textos.get(i));
            rb.setTextColor(Color.WHITE);
            rb.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
            rb.setButtonTintList(new ColorStateList(
                new int[][] {{android.R.attr.state_checked}, {}},
                new int[] {ACENTO, 0xFF9E9E9E}
            ));
            rb.setTag(i);
            grupo.addView(rb);
            if (valores.get(i).equals(actual)) {
                rb.setChecked(true);
            }
        }
        grupo.setOnCheckedChangeListener((g, id) -> {
            MaterialRadioButton rb = g.findViewById(id);
            if (rb != null && rb.isChecked()) {
                guardar.accept(valores.get((Integer) rb.getTag()));
            }
        });
        caja.addView(grupo);
        return caja;
    }

    private List<String> nombresIdiomas() {
        List<String> nombres = new ArrayList<>();
        for (int id : Idioma.NOMBRES) {
            nombres.add(getString(id));
        }
        return nombres;
    }

    /**
     * Como selector(), pero en un menu desplegable: para las listas largas, que
     * en botones ocupan media pantalla.
     */
    private <T> View desplegable(int titulo, int resumen, List<String> textos, List<T> valores,
                                 T actual, Consumer<T> guardar) {
        LinearLayout caja = new LinearLayout(this);
        caja.setOrientation(LinearLayout.VERTICAL);
        caja.setPadding(0, dp(12), 0, dp(12));
        if (titulo != 0) {
            caja.addView(titulo(titulo));
        }
        if (resumen != 0) {
            caja.addView(resumen(resumen));
        }
        TextInputLayout campo = new TextInputLayout(this, null,
                com.google.android.material.R.attr.textInputOutlinedExposedDropdownMenuStyle);
        campo.setBoxStrokeColor(ACENTO);
        MaterialAutoCompleteTextView lista = new MaterialAutoCompleteTextView(campo.getContext());
        // Solo se elige de la lista: ni teclado ni texto libre.
        lista.setInputType(InputType.TYPE_NULL);
        lista.setTextColor(Color.WHITE);
        lista.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, textos));
        int elegido = Math.max(0, valores.indexOf(actual));
        lista.setText(textos.get(elegido), false);
        lista.setOnItemClickListener((padre, vista, posicion, id) -> guardar.accept(valores.get(posicion)));
        campo.addView(lista, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        // Despues de meter el campo: la flecha del menu necesita su AutoCompleteTextView.
        campo.setEndIconMode(TextInputLayout.END_ICON_DROPDOWN_MENU);
        campo.setEndIconTintList(ColorStateList.valueOf(Color.WHITE));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(8);
        caja.addView(campo, lp);
        return caja;
    }

    private static ColorStateList segunActivado(int desactivado, int activado) {
        return new ColorStateList(
                new int[][] {{-android.R.attr.state_enabled}, {}},
                new int[] {desactivado, activado});
    }

    private void refrescar() {
        String nombre = ajustes.nombreIso();
        textoIso.setText(ajustes.iso() == null
                ? getString(R.string.sin_iso, Ediciones.nombres())
                : getString(R.string.iso_elegida, nombre != null ? nombre : ajustes.iso()));
        // El APK lleva el codigo de una edicion del juego (o de varias, ver
        // Ediciones): se dice cual y, si el default.xex de la ISO elegida es
        // de otra o a la ISO le falta el final, se avisa. No se impide jugar:
        // lo que no se ha podido leer no demuestra nada.
        String xex = ajustes.xexIso();
        boolean mirada = ajustes.iso() != null && xex != null;
        boolean incompleta = mirada && ajustes.isoFaltan() > 0;
        boolean otraEdicion = mirada && Ediciones.ajena(xex);
        if (mirada && IsoXex.NO_ES_ISO.equals(xex)) {
            textoEdicion.setText(R.string.iso_no_es_xbox);
        } else if (incompleta) {
            textoEdicion.setText(getString(R.string.iso_incompleta,
                    Formatter.formatShortFileSize(this, ajustes.isoFaltan())));
        } else {
            String texto;
            Ediciones.Edicion deLaIso = Ediciones.deXex(xex);
            if (otraEdicion) {
                texto = getString(Ediciones.varias() ? R.string.iso_edicion_no_incluida
                        : R.string.iso_otra_edicion, Ediciones.nombres());
            } else if (!Ediciones.varias()) {
                texto = getString(R.string.edicion_apk, Ediciones.principal().nombre);
            } else if (mirada && deLaIso != null) {
                texto = getString(R.string.edicion_iso, deLaIso.nombre, Ediciones.nombres());
            } else {
                texto = getString(R.string.ediciones_apk, Ediciones.nombres());
            }
            if (MotorNativo.ACTIVO && !otraEdicion) {
                texto += " " + getString(R.string.motor_nativo);
            }
            textoEdicion.setText(texto);
        }
        textoEdicion.setTextColor(incompleta || otraEdicion ? AVISO : GRIS);
        textoEdicion.setVisibility(BuildConfig.CON_JUEGO ? View.VISIBLE : View.GONE);
        boolean puedeJugar = BuildConfig.CON_JUEGO && ajustes.iso() != null;
        botonJugar.setEnabled(puedeJugar);
        avisoJugar.setText(BuildConfig.CON_JUEGO ? R.string.jugar_sin_iso : R.string.jugar_sin_juego);
        avisoJugar.setVisibility(puedeJugar ? View.GONE : View.VISIBLE);

        grupoDrivers.setOnCheckedChangeListener(null);
        grupoDrivers.removeAllViews();
        MaterialRadioButton sistema = new MaterialRadioButton(this);
        sistema.setId(View.generateViewId());
        String gpu = Gpu.nombre(this);
        sistema.setText(gpu.isEmpty() ? getString(R.string.driver_sistema_sin_gpu)
                : getString(R.string.driver_sistema, gpu));
        sistema.setTextColor(Color.WHITE);
        sistema.setButtonTintList(new ColorStateList(
            new int[][] {{android.R.attr.state_checked}, {}},
            new int[] {ACENTO, 0xFF9E9E9E}
        ));
        sistema.setTag(null);
        grupoDrivers.addView(sistema);
        MaterialRadioButton marcado = sistema;
        List<Drivers.Driver> drivers = Drivers.lista(this);
        for (Drivers.Driver d : drivers) {
            MaterialRadioButton rb = new MaterialRadioButton(this);
            rb.setId(View.generateViewId());
            rb.setText(d.nombre + "  (" + d.libreria + ")");
            rb.setTextColor(Color.WHITE);
            rb.setButtonTintList(new ColorStateList(
                new int[][] {{android.R.attr.state_checked}, {}},
                new int[] {ACENTO, 0xFF9E9E9E}
            ));
            rb.setTag(d.nombre);
            grupoDrivers.addView(rb);
            if (d.nombre.equals(ajustes.driver())) {
                marcado = rb;
            }
        }
        marcado.setChecked(true);
        grupoDrivers.setOnCheckedChangeListener((g, id) -> {
            MaterialRadioButton rb = g.findViewById(id);
            if (rb != null) {
                ajustes.driver((String) rb.getTag());
                actualizarTurbo();
            }
        });
        actualizarTurbo();

        File informe = Ajustes.informeSonda(this);
        if (informe.isFile()) {
            try {
                textoInforme.setText(new String(Files.readAllBytes(informe.toPath()),
                        StandardCharsets.UTF_8));
            } catch (IOException e) {
                textoInforme.setText("");
            }
        } else {
            textoInforme.setText(R.string.sin_informe);
        }
    }

    private void actualizarTurbo() {
        // Solo con Turnip: es un ajuste de KGSL, la GPU de Adreno.
        Drivers.Driver d = Drivers.buscar(this, ajustes.driver());
        boolean propio = d != null && d.esTurnip();
        MaterialSwitch s = (MaterialSwitch) ((LinearLayout) filaTurbo).getChildAt(0);
        s.setOnCheckedChangeListener(null);
        s.setChecked(propio && ajustes.turbo());
        s.setOnCheckedChangeListener((b, v) -> ajustes.turbo(v));
        s.setEnabled(propio);
        ((LinearLayout) filaTurbo).getChildAt(1).setAlpha(propio ? 1f : 0.5f);
    }

    /**
     * Medio segundo despues de dejar de escribir: no una consulta por tecla. Con
     * este movil de anfitrion, algo mas: su servidor acaba de arrancar.
     */
    private void programarComprobacion() {
        hiloPrincipal.removeCallbacks(comprobacionPendiente);
        comprobaciones++;
        hiloPrincipal.postDelayed(comprobacionPendiente,
                MotorNativo.ACTIVO && ajustes.anfitrionLan() ? 1500 : 500);
    }

    /** El interruptor de anfitrion: arranca o para el servidor de este movil. */
    private void cambiarAnfitrion(boolean si) {
        ajustes.anfitrionLan(si);
        if (si) {
            // Sin permiso el servidor va igual; solo no se ve su notificacion.
            if (Build.VERSION.SDK_INT >= 33 && checkSelfPermission(
                    android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[] {android.Manifest.permission.POST_NOTIFICATIONS}, PEDIR_AVISOS);
            }
            ServidorLanService.arrancar(this);
        } else {
            ServidorLanService.parar(this);
        }
        actualizarAnfitrion();
        // Otro servidor, otros jugadores.
        programarComprobacion();
    }

    /**
     * Con este movil de anfitrion: la IP que ponen los demas (o que no hay red), y
     * sin la lista de servidores. La IP se mira cada vez: cambia con la Wi-Fi.
     */
    private void actualizarAnfitrion() {
        if (textoAnfitrion == null) {
            return;
        }
        boolean anfitrion = ajustes.anfitrionLan();
        bloqueServidores.setVisibility(anfitrion ? View.GONE : View.VISIBLE);
        textoAnfitrion.setVisibility(anfitrion ? View.VISIBLE : View.GONE);
        if (anfitrion) {
            String ip = ServidorEa.ipLocal();
            textoAnfitrion.setText(ip != null ? getString(R.string.servidor_anfitrion_ip, ip)
                    : getString(R.string.servidor_anfitrion_sin_red));
            textoAnfitrion.setTextColor(ip != null ? Color.WHITE : AVISO);
        }
    }

    /**
     * Pregunta al servidor de la LAN si el gamertag esta libre y lo dice debajo
     * del campo. Sin servidor (o sin el motor nativo, que es el que tiene red) no
     * hay nada que comprobar.
     */
    private void comprobarGamertag() {
        if (cajaGamertag == null || !gamertagEscritoValido) {
            return;
        }
        // IP o nombre: el socket de la comprobacion resuelve el nombre.
        String ip = ajustes.servidorEfectivo();
        if (!MotorNativo.ACTIVO || !Ajustes.direccionValida(ip)) {
            cajaGamertag.setError(null);
            cajaGamertag.setHelperText(null);
            return;
        }
        String gamertag = ajustes.gamertag();
        final int n = ++comprobaciones;
        red.execute(() -> {
            ComprobacionGamertag.Resultado r = ComprobacionGamertag.comprobar(ip, gamertag);
            hiloPrincipal.post(() -> {
                if (n == comprobaciones) {
                    mostrarComprobacion(ip, gamertag, r);
                }
            });
        });
    }

    private void mostrarComprobacion(String ip, String gamertag, ComprobacionGamertag.Resultado r) {
        if (cajaGamertag == null || !gamertagEscritoValido) {
            return;
        }
        String nombre = ComprobacionGamertag.efectivo(gamertag);
        switch (r) {
            case LIBRE:
                cajaGamertag.setError(null);
                cajaGamertag.setHelperText(getString(R.string.gamertag_libre, nombre, ip));
                break;
            case EN_USO:
                cajaGamertag.setHelperText(null);
                cajaGamertag.setError(getString(R.string.gamertag_en_uso, nombre, ip));
                break;
            default:
                cajaGamertag.setError(null);
                cajaGamertag.setHelperText(getString(R.string.gamertag_sin_respuesta, ip));
                break;
        }
    }

    /**
     * Jugar. Con servidor de la LAN se pregunta otra vez antes de entrar (otro
     * jugador ha podido conectarse con ese nombre desde que se escribio) y, si
     * esta en uso, no se entra. Sin servidor configurado, o si no contesta, se
     * juega como siempre. Un servidor puesto por nombre se pasa aqui a su IP: el
     * juego solo entiende IPs. Si la red no lo encuentra, se pregunta.
     */
    private void jugar() {
        String servidor = ajustes.servidorEfectivo();
        if (!MotorNativo.ACTIVO || !Ajustes.direccionValida(servidor)) {
            lanzar(false);
            return;
        }
        if (comprobandoParaJugar) {
            return;
        }
        comprobandoParaJugar = true;
        botonJugar.setEnabled(false);
        String gamertag = ajustes.gamertag();
        final int n = ++comprobaciones;
        red.execute(() -> {
            String ip = Ajustes.resolverIpv4(servidor);
            ComprobacionGamertag.Resultado r = ip == null ? ComprobacionGamertag.Resultado.SIN_RESPUESTA
                    : ComprobacionGamertag.comprobar(ip, gamertag);
            hiloPrincipal.post(() -> {
                comprobandoParaJugar = false;
                if (isFinishing() || isDestroyed()) {
                    return;
                }
                refrescar();
                if (n == comprobaciones) {
                    mostrarComprobacion(servidor, gamertag, r);
                }
                if (ip == null) {
                    preguntarServidorNoEncontrado(servidor);
                    return;
                }
                if (r == ComprobacionGamertag.Resultado.EN_USO) {
                    preguntarJugarSinOnline(servidor, gamertag);
                    return;
                }
                lanzar(false, false, ip);
            });
        });
    }

    /** El nombre del servidor no da ninguna IP en esta red: o se corrige, o se juega sin online. */
    private void preguntarServidorNoEncontrado(String servidor) {
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.servidor_no_encontrado_titulo)
                .setMessage(getString(R.string.servidor_no_encontrado, servidor))
                .setPositiveButton(R.string.jugar_sin_online, (d, w) -> lanzar(false, true, null))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    /**
     * El gamertag lo tiene otro movil: o se cambia, o se juega esta vez sin el
     * modo online (sin las opciones de la LAN; el servidor guardado no se toca).
     */
    private void preguntarJugarSinOnline(String ip, String gamertag) {
        new MaterialAlertDialogBuilder(this)
                .setTitle(R.string.gamertag_en_uso_titulo)
                .setMessage(getString(R.string.gamertag_en_uso_pregunta,
                        ComprobacionGamertag.efectivo(gamertag), ip))
                .setPositiveButton(R.string.jugar_sin_online, (d, w) -> lanzar(false, true, null))
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void lanzar(boolean sonda) {
        lanzar(sonda, false, null);
    }

    /** ipServidor: la IP a la que jugar() resolvio el servidor, si se puso por nombre. */
    private void lanzar(boolean sonda, boolean sinOnline, String ipServidor) {
        if (sonda) {
            Ajustes.informeSonda(this).delete();
        }
        // La sonda no carga el juego: le vale la libmain principal.
        Ediciones.Edicion edicion = sonda ? Ediciones.principal() : edicionAlJugar();
        Intent i = new Intent(this, GameActivity.class);
        List<String> argumentos = ajustes.argumentos(this, sonda);
        if (sinOnline) {
            MotorNativo.quitarRed(argumentos);
        } else if (ipServidor != null) {
            argumentos.replaceAll(a -> a.startsWith("--red_dns=") ? "--red_dns=" + ipServidor : a);
        }
        i.putExtra(GameActivity.EXTRA_ARGUMENTOS, argumentos.toArray(new String[0]));
        i.putExtra(GameActivity.EXTRA_EDICION, edicion.id);
        if (!sonda) {
            i.putExtra(GameActivity.EXTRA_ISO, ajustes.iso());
            i.putExtra(GameActivity.EXTRA_RESOLUCION, ajustes.resolucion());
            i.putExtra(GameActivity.EXTRA_ESTIRAR, ajustes.estirar());
            i.putExtra(GameActivity.EXTRA_PANORAMICO, ajustes.panoramicoEfectivo());
            i.putExtra(GameActivity.EXTRA_MOSTRAR_FPS, ajustes.mostrarFps());
        }
        startActivity(i);
    }

    @Override
    protected void onActivityResult(int peticion, int resultado, Intent datos) {
        super.onActivityResult(peticion, resultado, datos);
        // Los ajustes de Android no devuelven nada: lo que cuenta es si ahora hay acceso.
        if (peticion == PEDIR_TODOS_ARCHIVOS) {
            volverDePermiso();
            return;
        }
        if (resultado != RESULT_OK || datos == null || datos.getData() == null) {
            return;
        }
        Uri uri = datos.getData();
        if (peticion == PEDIR_EXPORTAR) {
            copiarPartidas(true, app -> Partidas.exportar(app, uri), null);
            return;
        }
        if (peticion == PEDIR_IMPORTAR) {
            confirmarImportar(app -> Partidas.importar(app, uri));
            return;
        }
        if (peticion == PEDIR_LSFG) {
            copiarDllLsfg(uri);
            return;
        }
        if (peticion == PEDIR_ISO) {
            try {
                getContentResolver().takePersistableUriPermission(uri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (SecurityException e) {
                Toast.makeText(this, R.string.iso_sin_permiso, Toast.LENGTH_LONG).show();
                return;
            }
            ajustes.iso(uri.toString());
            ajustes.nombreIso(nombreDe(uri));
            ajustes.isoSinMirar();
            mirarIso();
        } else if (peticion == PEDIR_DRIVER) {
            try {
                Drivers.Driver d = Drivers.importar(this, uri);
                ajustes.driver(d.nombre);
                Toast.makeText(this, getString(R.string.driver_importado, d.nombre),
                        Toast.LENGTH_SHORT).show();
            } catch (IOException e) {
                Toast.makeText(this, getString(R.string.driver_error, e.getMessage()),
                        Toast.LENGTH_LONG).show();
            }
        }
        refrescar();
    }

    /**
     * La edicion con la que jugar la ISO elegida: decide la libmain, los
     * shaders, el idioma y el pais (Ediciones). Con varias en el APK, si al
     * pulsar Jugar aun no se habia mirado la ISO (mirarIso va en otro hilo), se
     * mira ahora: son unos megas.
     */
    private Ediciones.Edicion edicionAlJugar() {
        String iso = ajustes.iso();
        if (Ediciones.varias() && iso != null && ajustes.xexIso() == null) {
            IsoXex.Info info = IsoXex.mirar(getApplicationContext(), Ajustes.uriIso(iso));
            ajustes.isoMirada(info != null ? info.xex : "", info != null ? info.faltan : 0);
        }
        return ajustes.edicion();
    }

    /**
     * Lee de la ISO elegida de que edicion es y si esta entera (IsoXex), si aun
     * no se sabe. Son unos megas de lectura: en otro hilo.
     */
    private void mirarIso() {
        final String iso = ajustes.iso();
        if (iso == null || ajustes.xexIso() != null) {
            return;
        }
        final Context app = getApplicationContext();
        new Thread(() -> {
            IsoXex.Info info = IsoXex.mirar(app, Ajustes.uriIso(iso));
            runOnUiThread(() -> {
                if (iso.equals(ajustes.iso())) {
                    // Vacio = no se pudo leer; asi no se reintenta en cada arranque.
                    ajustes.isoMirada(info != null ? info.xex : "", info != null ? info.faltan : 0);
                    if (!isFinishing() && !isDestroyed()) {
                        refrescar();
                    }
                }
            });
        }, "mirar-iso").start();
    }

    private String nombreDe(Uri uri) {
        try (Cursor c = getContentResolver().query(uri,
                new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                return c.getString(0);
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    private int dp(int v) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v,
                getResources().getDisplayMetrics());
    }

    private static String nombreOpcion(String arg) {
        String n = arg.startsWith("--") ? arg.substring(2) : arg;
        int igual = n.indexOf('=');
        if (igual >= 0) {
            n = n.substring(0, igual);
        } else if (n.startsWith("no-")) {
            n = n.substring(3);
        }
        return n;
    }
}
