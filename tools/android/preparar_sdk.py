#!/usr/bin/env python3
"""
Prepara el arbol del SDK ReXGlue para compilar el port de Android.

    python tools/android/preparar_sdk.py
    python tools/android/preparar_sdk.py --sdk D:\\otra\\ruta\\rexglue-sdk-android
    python tools/android/preparar_sdk.py --estado

Deja un SDK v0.10.0 en ..\\rexglue-sdk-android, APARTE del de Windows, con todo
lo que el port necesita encima. Se puede lanzar las veces que haga falta: cada
paso mira si ya esta hecho antes de tocar nada.


POR QUE UN SDK APARTE
=====================

El parche Android cambia la plantilla del codegen (resources/templates/codegen/
pch_h.inja): barreras de memoria para ARM64 y un offset de memoria fisica que
se decide en tiempo de ejecucion. Con eso aplicado sobre ..\\rexglue-sdk, el
codigo generado para Windows cambiaria tambien. Dos arboles, cero sorpresas.


LOS PASOS, EN ORDEN
===================

1. Clonar rexglue-sdk en la etiqueta v0.10.0, con submodulos.

2. Reparar los symlinks. En Windows, sin permisos de symlink, git deja cada
   enlace como un fichero de texto cuyo unico contenido es la ruta del destino.
   libmspack tiene quince asi, y clang se encuentra "../../libmspack/mspack/
   lzxd.c" donde esperaba C. Ver docs/00-entorno.md. Se sustituye cada enlace a
   fichero por una copia real del destino.

3. El parche Android del SDK. NO ESTA EN ESTE REPOSITORIO: es trabajo de
   deivid22srk para hells-gate-recomp-android, y su repo no declara licencia.
   Mientras no haya permiso para incluirlo, este script lo descarga de SU repo,
   fijado a un commit, y lo aplica en local, igual que el AppImage de ARM64
   descarga extract-xiso. Nada suyo se redistribuye desde aqui.

     rexglue-sdk-v0.10.0-android.patch       22 ficheros: bionic, ASharedMemory,
                                             paginas de 16 KB, ucontext aarch64,
                                             logcat, superficie ANativeWindow,
                                             barreras de memoria del codegen
     rexglue-sdk-v0.10.0-android-perf.patch  cache de /proc/self/maps en los
                                             fallos de write-watch, menos giros
                                             en el procesador de comandos

   Del de rendimiento se excluye src/system/mmio_handler.cpp: son trazas de
   diagnostico y dependen de un parche base de su juego que aqui no se usa.

4. libadrenotools (Billy Laws, BSD-2) en thirdparty/libadrenotools, fijado a un
   commit. Es lo que carga Turnip en los Adreno. Ver tools/parche_turnip.py.

5. Los parches de este proyecto (la lista PARCHES_PROYECTO, mas abajo), con
   NFSMW_SDK apuntando a este arbol.

   Quedan fuera los dos que solo tocan Direct3D 12, que en Android no se
   compila: parche_presentador y parche_gpu_fallback.

Despues de esto, el SDK se compila desde Gradle (android/), no a mano.
"""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import urllib.request

RAIZ = pathlib.Path(__file__).resolve().parents[2]

SDK_REPO = "https://github.com/rexglue/rexglue-sdk.git"
SDK_TAG = "v0.10.0"

REF_COMMIT = "0a79f938ab839c187d25bd7bf48e51e30a6150be"
REF_URL = ("https://raw.githubusercontent.com/deivid22srk/hells-gate-recomp-android/"
           f"{REF_COMMIT}/patches/sdk/")
PARCHES_REF = [
    ("rexglue-sdk-v0.10.0-android.patch", []),
    ("rexglue-sdk-v0.10.0-android-perf.patch", ["src/system/mmio_handler.cpp"]),
]

ADRENOTOOLS_REPO = "https://github.com/bylaws/libadrenotools.git"
ADRENOTOOLS_COMMIT = "8fae8ce254dfc1344527e05301e43f37dea2df80"

# El ORDEN importa: varios parches anclan en el mismo fichero, y cada uno cuenta
# con lo que dejaron los anteriores. Los nuevos van al final.
#
# parche_audio ya no esta: forzaba a estereo el driver de audio de SDL, y en
# Android el audio va por AAudio (android/app/src/main/cpp/audio/), asi que ese
# driver no se instancia.
PARCHES_PROYECTO = [
    "parche_diagnostico", "parche_anillo", "parche_desatasco", "parche_restaurar",
    "parche_velocidad", "parche_backend", "parche_privilegios", "parche_iso",
    "parche_turnip", "parche_pausa", "parche_fps", "parche_pipeline", "parche_fallos",
    "parche_esperas", "parche_teclado", "parche_tiempos",
    "parche_espera_anillo", "parche_cola_presentar", "parche_subidas", "parche_cvars",
    "parche_fences", "parche_area", "parche_vblank",
    "parche_msaa", "parche_xma_paquetes", "parche_xma_edge", "parche_anillo_bloques",
    # parche_una_pasada ya no lo usa la app (la escena en una pasada la hace
    # android/app/src/main/cpp/render_targets.cpp), pero se deja: el cvar
    # nfsmw_una_pasada sigue existiendo para compararlo.
    "parche_una_pasada", "parche_camino_edram",
    # Y --user_gamertag, desde la pantalla de inicio.
    "parche_gamertag",
    # Cache de pipelines del driver en disco (menos tirones y arranque mas rapido).
    "parche_cache_pipelines",
    # El memcpy nativo con rangos que se solapan, como el del juego: con el de
    # bionic el juego se cerraba en la primera cinematica de la carrera.
    "parche_memcpy",
]


def run(cmd, cwd=None, check=True, quiet=False):
    r = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    if check and r.returncode != 0:
        sys.stdout.write(r.stdout)
        sys.stderr.write(r.stderr)
        sys.exit(f"[ERROR] Fallo: {' '.join(str(c) for c in cmd)}")
    if not quiet and r.stdout.strip():
        print(r.stdout.rstrip())
    return r


def paso(n, texto):
    print(f"\n== {n}. {texto}")


# ---------------------------------------------------------------------------

def clonar(sdk):
    if (sdk / "CMakeLists.txt").exists():
        print(f"[ok] Ya clonado en {sdk}")
    else:
        run(["git", "clone", "--branch", SDK_TAG, "--depth", "1", SDK_REPO, str(sdk)])
    # core.symlinks=false a proposito: sin permisos de symlink en Windows el
    # checkout fallaria. Los enlaces se reparan en el paso siguiente.
    run(["git", "-c", "core.symlinks=false", "submodule", "update", "--init",
         "--recursive", "--depth", "1"], cwd=sdk, quiet=True)
    print("[ok] Submodulos al dia")


def symlinks_de(repo, prefijo):
    r = run(["git", "ls-files", "-s"], cwd=repo, quiet=True)
    for linea in r.stdout.splitlines():
        modo, _, _, ruta = linea.split(maxsplit=3)
        if modo == "120000":
            yield prefijo / ruta


def reparar_symlinks(sdk):
    enlaces = list(symlinks_de(sdk, pathlib.Path(".")))
    subs = run(["git", "submodule", "foreach", "--quiet", "--recursive", "echo $displaypath"],
               cwd=sdk, quiet=True).stdout.split()
    for sub in subs:
        enlaces += list(symlinks_de(sdk / sub, pathlib.Path(sub)))

    reparados = 0
    for rel in enlaces:
        f = sdk / rel
        if f.is_symlink() or not f.is_file() or f.stat().st_size > 512:
            continue  # symlink de verdad, directorio, o ya es una copia real
        destino_txt = f.read_text(encoding="utf-8", errors="replace").strip()
        destino = (f.parent / destino_txt).resolve()
        if not destino.is_file():
            continue  # enlace a directorio o roto: no se compila, se deja
        shutil.copyfile(destino, f)
        reparados += 1
    print(f"[ok] {reparados} symlinks sustituidos por copias ({len(enlaces)} enlaces en total)")


def aplicar_parches_ref(sdk):
    carpeta = sdk / ".nfsmw-android"
    carpeta.mkdir(exist_ok=True)
    for nombre, excluir in PARCHES_REF:
        f = carpeta / nombre
        if not f.exists():
            print(f"[..] Descargando {nombre} (commit {REF_COMMIT[:8]})")
            with urllib.request.urlopen(REF_URL + nombre, timeout=60) as resp:
                f.write_bytes(resp.read())
        # Un marcador y no "git apply --reverse --check": los parches del
        # proyecto van despues, escriben en modo texto (CRLF en Windows) y
        # tocan algunos de los mismos ficheros, asi que la comprobacion inversa
        # dejaria de reconocer un parche que si esta puesto.
        marca = carpeta / (nombre + ".aplicado")
        extra = [f"--exclude={e}" for e in excluir]
        base = ["git", "apply", "--whitespace=nowarn", *extra]
        if marca.exists():
            print(f"[ok] {nombre} ya estaba aplicado")
        elif run([*base, "--check", str(f)], cwd=sdk, check=False, quiet=True).returncode == 0:
            run([*base, str(f)], cwd=sdk)
            marca.write_text(REF_COMMIT + "\n", encoding="utf-8")
            print(f"[ok] Aplicado {nombre}")
        else:
            r = run([*base, "--check", str(f)], cwd=sdk, check=False, quiet=True)
            sys.stderr.write(r.stderr)
            sys.exit(f"[ERROR] {nombre} no entra ni esta aplicado. No he tocado nada mas.\n"
                     f"        Si ya aplicaste parches de este proyecto, puede que "
                     f"hayan cambiado los finales de linea: vuelve a clonar el SDK.")


def adrenotools(sdk):
    dst = sdk / "thirdparty" / "libadrenotools"
    if not (dst / ".git").exists():
        run(["git", "clone", ADRENOTOOLS_REPO, str(dst)], quiet=True)
    actual = run(["git", "rev-parse", "HEAD"], cwd=dst, quiet=True).stdout.strip()
    if actual != ADRENOTOOLS_COMMIT:
        run(["git", "fetch", "--quiet", "origin", ADRENOTOOLS_COMMIT], cwd=dst, check=False, quiet=True)
        run(["git", "checkout", "--quiet", ADRENOTOOLS_COMMIT], cwd=dst, quiet=True)
    run(["git", "submodule", "update", "--init", "--recursive"], cwd=dst, quiet=True)
    print(f"[ok] libadrenotools en {ADRENOTOOLS_COMMIT[:8]}")


def parches_proyecto(sdk, estado=False):
    env = dict(os.environ, NFSMW_SDK=str(sdk))
    for p in PARCHES_PROYECTO:
        cmd = [sys.executable, str(RAIZ / "tools" / f"{p}.py")]
        if estado:
            cmd.append("--estado")
        r = subprocess.run(cmd, env=env, text=True, capture_output=True)
        salida = (r.stdout + r.stderr).strip()
        if r.returncode != 0:
            print(salida)
            sys.exit(f"[ERROR] {p} ha fallado. Los anteriores ya estan aplicados; "
                     f"relanzar este script es seguro.")
        if estado:
            print(salida)
        else:
            primera = next((l for l in salida.splitlines() if l.startswith("[")), "")
            print(f"  {p:20s} {primera}")


def main():
    ap = argparse.ArgumentParser(description="Prepara el SDK ReXGlue para Android.")
    ap.add_argument("--sdk", default=str(RAIZ.parent / "rexglue-sdk-android"),
                    help="carpeta del SDK de Android (por defecto ..\\rexglue-sdk-android)")
    ap.add_argument("--estado", action="store_true", help="solo decir que parches hay puestos")
    args = ap.parse_args()
    sdk = pathlib.Path(args.sdk).resolve()

    if args.estado:
        if not (sdk / "CMakeLists.txt").exists():
            sys.exit(f"[ERROR] No hay SDK en {sdk}")
        parches_proyecto(sdk, estado=True)
        return 0

    paso(1, f"SDK {SDK_TAG}")
    clonar(sdk)
    paso(2, "Symlinks")
    reparar_symlinks(sdk)
    paso(3, "Parche Android del SDK (hells-gate-recomp-android)")
    aplicar_parches_ref(sdk)
    paso(4, "libadrenotools")
    adrenotools(sdk)
    paso(5, "Parches de este proyecto")
    parches_proyecto(sdk)

    print(f"\n[ok] SDK listo en {sdk}")
    print("     Siguiente: compilar el APK desde android/ (ver docs/android.md).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
