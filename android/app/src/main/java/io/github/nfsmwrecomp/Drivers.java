package io.github.nfsmwrecomp;

import android.content.Context;
import android.net.Uri;

import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Drivers Vulkan propios (Turnip) importados por el usuario.
 *
 * Formato: el .zip de los paquetes para adrenotools (los de Mesa Turnip que
 * circulan para emuladores), con un meta.json que dice que .so cargar:
 *
 *     { "name": "Turnip v25.x", "libraryName": "libvulkan_freedreno.so", ... }
 *
 * Se descomprime en files/gpu_drivers/NOMBRE/ -almacenamiento INTERNO: adrenotools
 * no carga nada desde /sdcard, y dlopen tampoco deberia-.
 */
final class Drivers {

    static final class Driver {
        final String nombre;
        final File carpeta;
        final String libreria;

        Driver(String nombre, File carpeta, String libreria) {
            this.nombre = nombre;
            this.carpeta = carpeta;
            this.libreria = libreria;
        }

        /**
         * Turnip (Mesa para Adreno): su libreria es libvulkan_freedreno.so. El
         * turbo (KGSL) solo tiene sentido con el.
         */
        boolean esTurnip() {
            return "libvulkan_freedreno.so".equals(libreria)
                    || nombre.toLowerCase(java.util.Locale.ROOT).contains("turnip");
        }
    }

    private Drivers() {}

    static File raiz(Context ctx) {
        File f = new File(ctx.getFilesDir(), "gpu_drivers");
        //noinspection ResultOfMethodCallIgnored
        f.mkdirs();
        return f;
    }

    static List<Driver> lista(Context ctx) {
        List<Driver> drivers = new ArrayList<>();
        File[] carpetas = raiz(ctx).listFiles(File::isDirectory);
        if (carpetas == null) {
            return drivers;
        }
        for (File c : carpetas) {
            File lib = new File(c, ".libreria");
            if (!lib.isFile()) {
                continue;
            }
            try (InputStream in = new java.io.FileInputStream(lib)) {
                String nombreLib = leerTexto(in).trim();
                if (new File(c, nombreLib).isFile()) {
                    drivers.add(new Driver(c.getName(), c, nombreLib));
                }
            } catch (IOException ignored) {
                // carpeta a medias: no se lista
            }
        }
        return drivers;
    }

    static Driver buscar(Context ctx, String nombre) {
        if (nombre == null) {
            return null;
        }
        for (Driver d : lista(ctx)) {
            if (d.nombre.equals(nombre)) {
                return d;
            }
        }
        return null;
    }

    /**
     * Importa un .zip de driver. Devuelve el driver o lanza con un mensaje
     * legible, en el idioma de la app: sale tal cual en un aviso.
     */
    static Driver importar(Context ctx, Uri zip) throws IOException {
        File tmp = new File(raiz(ctx), ".importando");
        borrar(tmp);
        if (!tmp.mkdirs()) {
            throw new IOException(ctx.getString(R.string.error_driver_carpeta, tmp));
        }

        String nombreMeta = null;
        String libreriaMeta = null;
        String primeraSo = null;
        try (InputStream in = ctx.getContentResolver().openInputStream(zip);
             ZipInputStream z = new ZipInputStream(in)) {
            ZipEntry e;
            byte[] buf = new byte[1 << 16];
            while ((e = z.getNextEntry()) != null) {
                if (e.isDirectory()) {
                    continue;
                }
                // Todo plano, por nombre: nada de rutas del zip, asi un
                // "../../algo" no puede escribir fuera de la carpeta.
                String nombre = new File(e.getName()).getName();
                if (nombre.isEmpty() || nombre.startsWith(".")) {
                    continue;
                }
                if (nombre.equals("meta.json")) {
                    JSONObject meta = new JSONObject(leerTexto(z));
                    nombreMeta = meta.optString("name", null);
                    libreriaMeta = meta.optString("libraryName", null);
                    continue;
                }
                if (nombre.endsWith(".so") && primeraSo == null) {
                    primeraSo = nombre;
                }
                try (FileOutputStream out = new FileOutputStream(new File(tmp, nombre))) {
                    int n;
                    while ((n = z.read(buf)) > 0) {
                        out.write(buf, 0, n);
                    }
                }
            }
        } catch (org.json.JSONException ex) {
            borrar(tmp);
            throw new IOException(ctx.getString(R.string.error_driver_meta));
        } catch (IOException ex) {
            borrar(tmp);
            throw ex;
        }

        String libreria = libreriaMeta != null ? libreriaMeta : primeraSo;
        if (libreria == null || !new File(tmp, libreria).isFile()) {
            borrar(tmp);
            throw new IOException(ctx.getString(R.string.error_driver_sin_so));
        }

        String nombre = limpiar(nombreMeta != null ? nombreMeta : libreria.replace(".so", ""));
        File destino = new File(raiz(ctx), nombre);
        borrar(destino);
        if (!tmp.renameTo(destino)) {
            borrar(tmp);
            throw new IOException(ctx.getString(R.string.error_driver_mover, destino));
        }
        try (FileOutputStream out = new FileOutputStream(new File(destino, ".libreria"))) {
            out.write(libreria.getBytes(StandardCharsets.UTF_8));
        }
        // Solo lectura: el cargador de Android no quiere librerias que la app
        // pueda reescribir mientras las usa.
        File[] ficheros = destino.listFiles();
        if (ficheros != null) {
            for (File f : ficheros) {
                //noinspection ResultOfMethodCallIgnored
                f.setWritable(false, false);
            }
        }
        return new Driver(nombre, destino, libreria);
    }

    static void eliminar(Driver d) {
        borrar(d.carpeta);
    }

    private static String limpiar(String s) {
        String r = s.replaceAll("[^A-Za-z0-9._-]+", "_");
        return r.isEmpty() ? "driver" : r;
    }

    private static String leerTexto(InputStream in) throws IOException {
        ByteArrayOutputStream b = new ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf)) > 0) {
            b.write(buf, 0, n);
        }
        return b.toString("UTF-8");
    }

    private static void borrar(File f) {
        if (f.isDirectory()) {
            File[] hijos = f.listFiles();
            if (hijos != null) {
                for (File h : hijos) {
                    borrar(h);
                }
            }
        }
        //noinspection ResultOfMethodCallIgnored
        f.setWritable(true, true);
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
