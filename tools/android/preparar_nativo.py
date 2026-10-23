#!/usr/bin/env python3
"""
Prepara el MOTOR NATIVO: el renderizador nativo de nfsmw-android, que dibuja el
juego con Vulkan sin imitar la GPU de la Xbox 360.

    python tools/android/preparar_nativo.py --iso "D:\\dumps\\NFSMW.iso"
    python tools/android/preparar_nativo.py --juego D:\\juego_extraido
    python tools/android/preparar_nativo.py --rexglue nativo\\sdk\\out\\win-amd64\\rexglue.exe

Despues:

    cd android && gradlew assembleRelease -Pnfsmw.motor=nativo

Con la ISO de otra edicion (la USA), despues de preparar la PAL Espana:

    python tools/android/preparar_nativo.py --iso "D:\\dumps\\NFSMW (USA).iso"
    cd android && gradlew assembleRelease -Pnfsmw.motor=nativo -Pnfsmw.edicion=usa

Ver docs/motor-nativo.md.


QUE DEJA
========

El arbol del motor nativo, nativo/, es otro programa nativo entero: su SDK (un
ReXGlue v0.10.0 modificado), su app y su codigo generado. Su codigo viene en el
repositorio: https://github.com/codepdbh/nfsmw-android en un commit fijo (el
port a Android de nfsmw-nx, de StevensND; GPL-3.0), con los parches de este
proyecto ya aplicados. Lo que sale del juego y las librerias de terceros no se
versionan: los deja este script, y el APK se compila contra todo ello.

  1. Comprueba que esta nativo/.
  2. Rellena sdk/thirdparty: los submodulos del SDK. Si ya esta el SDK de
     Android de este proyecto (..\\rexglue-sdk-android) y son los mismos commits,
     los copia de ahi; si no, los baja con su tools/fetch_thirdparty.py, que
     tarda.
  3. Pone libadrenotools en su thirdparty (de ..\rexglue-sdk-android si esta,
     o clonado en el mismo commit que preparar_sdk.py) y pasa los parches de
     este proyecto, que ya vienen aplicados (dicen que cada bloque ya estaba;
     siguen sirviendo para el SDK del motor de Xenos): tools/parche_iso.py (leer la ISO sin copiarla),
     tools/parche_gamertag.py, tools/parche_turnip.py (drivers propios),
     tools/parche_pausa.py (soltar la superficie al minimizar),
     tools/parche_memcpy.py (el memcpy nativo con solape, como el del juego) y
     tools/android/parche_nativo.py (abrir la URI, la sonda y el audio).
     Tambien clona SPIRV-Cross en su thirdparty, para el backend de OpenGL ES.
  4. Saca del juego lo que hace falta: default.xex y NFS/ (los videos no).
  5. Compila su generador de codigo (rexglue, un programa del PC) y traduce el
     juego: codegen, llamadas directas y copias literales, como su
     tools/codegen.sh.
  6. Genera la biblioteca de shaders, out/nfsmw_shaders.nfsp, con las
     herramientas WASM que trae su app (hace falta Node.js), y a su lado la
     del backend de OpenGL ES, nfsmw_shaders_gles.nfsp
     (nativo/tools/biblioteca_shaders_gles.mjs).
  7. Apunta la edicion del juego en out/edicion.json.


OTRAS EDICIONES
===============

Su app esta escrita para PAL Espana: cada gancho, cada funcion declarada a mano
y cada limite de funcion lleva una direccion de ese ejecutable. Con la ISO de
otra edicion (la USA o la japonesa), en vez de los pasos 5 a 7 sobre app/:

  a. Saca el juego en assets/game_root_<edicion>: la PAL de assets/game_root
     se queda, porque hace falta.
  b. Traduce todas las direcciones de la app PAL (sus fuentes, sus .toml, el
     reparto de funciones del codegen y las funciones que su codigo generado
     llama con gancho) con tools/ediciones/emparejar.py, y las deja en
     out/<edicion>/tabla.tsv, con el formato de su tools/editions. En la
     japonesa .data se mueve: sus direcciones salen del codigo que las calcula.
  c. Comprueba que las funciones que usa el codigo de la app son las mismas en
     las dos ediciones, enteras (emparejar.comparar_funcion), y que cada
     direccion que calculan es la traducida (emparejar.comprobar_parejas).
  d. Genera su biblioteca de shaders en out/<edicion> y saca de ella las
     huellas de los tres shaders que el renderizador reconoce (resplandor, cielo
     y composicion): el contenedor con el mismo microcodigo que el de la PAL,
     como su tools/editions/shaders_especiales.py. Si ediciones.json ya las
     tiene ("nativo"), tienen que coincidir.
  e. Crea app_<edicion> con su tools/editions/crear_arbol.py: la app con las
     direcciones traducidas, sus parejas corregidas a mano
     (tools/editions/<edicion>/parejas_corregidas.json) y esas huellas.
  f. Codegen, llamadas directas y copias literales sobre app_<edicion>, y
     edicion.json en out/<edicion>.

Las PAL alemana, italiana y francesa son la misma compilacion que la espanola
con otro idioma (cinco instrucciones): su tabla sale identica, pero son otro
XEX, y tambien van en su app_<edicion>. Las ediciones que se reconocen estan en
tools/ediciones/ediciones.json (las de nfsmw-nx: PAL Espana, Alemania, Italia,
Francia y Reino Unido, la polaca de fans, USA, Japon, Corea y Asia).

Necesita la PAL Espana ya preparada: su ejecutable y su codigo generado son el
punto de partida de la traduccion.


LO QUE SALE DEL JUEGO
=====================

assets/, app*/generated/ y out/ salen de tu copia del juego. No se versionan ni
se comparten; el APK que los lleva dentro, tampoco.
"""

import argparse
import glob
import hashlib
import json
import os
import pathlib
import platform
import re
import shutil
import struct
import subprocess
import sys

RAIZ = pathlib.Path(__file__).resolve().parents[2]
# De donde viene nativo/: ese repositorio en ese commit, con los parches de este
# proyecto ya aplicados (ver docs/motor-nativo.md).
REPO = "https://github.com/codepdbh/nfsmw-android.git"
COMMIT = "6df1501"   # android: native renderer on Vulkan 1.1 and without shaderInt64
# Resumen de "git ls-tree HEAD thirdparty/" (solo los submodulos, ordenados) del
# ReXGlue v0.10.0 en que se basa ese arbol.
SUBMODULOS = "13082df16a807b8e5d162b79c461b0f454a9bc47679928b281635cbb4941dbd9"
# SPIRV-Cross, que pasa el SPIR-V de la biblioteca a GLSL ES en el movil (backend de OpenGL ES). No es de
# su SDK: va aparte en sdk/thirdparty/spirv-cross.
SPIRV_CROSS_REPO = "https://github.com/KhronosGroup/SPIRV-Cross.git"
SPIRV_CROSS_ETIQUETA = "vulkan-sdk-1.4.309.0"
SPIRV_CROSS_COMMIT = "2c32b6bf86f3c4a5539aa1f0bacbd59fe61759cf"

sys.path.insert(0, str(RAIZ / "tools" / "ediciones"))
import ediciones  # noqa: E402
import emparejar  # noqa: E402
import imagen  # noqa: E402


def fallar(msg):
    sys.exit(f"[ERROR] {msg}")


def correr(*args, cwd=None, env=None, que=None):
    r = subprocess.run([str(a) for a in args], cwd=cwd, env=env)
    if r.returncode != 0:
        fallar(f"{que or args[0]} ha fallado (codigo {r.returncode}).")


def git(*args, cwd):
    return subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)


# ---------------------------------------------------------------------------
# 1. El arbol
# ---------------------------------------------------------------------------

def comprobar_arbol(arbol):
    """Su codigo viene en el repositorio (nativo/): aqui no se clona nada."""
    if not (arbol / "app" / "CMakeLists.txt").is_file() or not (arbol / "sdk" / "CMakeLists.txt").is_file():
        fallar(f"No esta el motor nativo en {arbol}.\n"
               f"        Viene en el repositorio, en nativo/ ({REPO} en {COMMIT}, con los parches de este "
               "proyecto): recupera la carpeta con git (git checkout -- nativo).")
    print(f"[ok] motor nativo en {arbol}")


# ---------------------------------------------------------------------------
# 2. sdk/thirdparty
# ---------------------------------------------------------------------------

def _submodulos(repo):
    r = git("ls-tree", "HEAD", "thirdparty/", cwd=repo)
    return sorted(l for l in r.stdout.splitlines() if " commit " in l) if r.returncode == 0 else None


def terceros(arbol, sdk_local):
    destino = arbol / "sdk" / "thirdparty"
    if (destino / "sdl3" / "CMakeLists.txt").is_file() and (destino / "fmt" / "CMakeLists.txt").is_file():
        print("[ok] sdk/thirdparty ya esta relleno")
        return
    origen = sdk_local / "thirdparty"
    # Su script clona el SDK en su commit base (v0.10.0) y copia lo que falte.
    # Si ese mismo SDK ya esta aqui al lado, con los submodulos en los mismos
    # commits, se copia de ahi: bajarlos enteros son muchos minutos.
    locales = _submodulos(sdk_local) if origen.is_dir() else None
    iguales = locales and hashlib.sha256("\n".join(locales).encode() + b"\n").hexdigest() == SUBMODULOS
    if not iguales:
        print("== Bajando los submodulos del SDK (tarda)")
        correr(sys.executable, "tools/fetch_thirdparty.py", cwd=arbol, que="fetch_thirdparty.py")
        return
    print(f"== sdk/thirdparty desde {origen}")
    copiados = 0
    for carpeta, subcarpetas, ficheros in os.walk(origen):
        relativa = pathlib.Path(carpeta).relative_to(origen)
        # libadrenotools es del parche Android de este proyecto, no del SDK.
        subcarpetas[:] = [d for d in subcarpetas
                          if d != ".git" and not (relativa == pathlib.Path(".") and d == "libadrenotools")]
        for nombre in ficheros:
            if nombre == ".git":
                continue
            a = destino / relativa / nombre
            if a.exists():
                continue          # lo que este port cambio se queda
            a.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(pathlib.Path(carpeta) / nombre, a)
            copiados += 1
    print(f"[ok] {copiados} ficheros copiados")


# ---------------------------------------------------------------------------
# 3. Parches
# ---------------------------------------------------------------------------

def adrenotools(arbol, sdk_local):
    """libadrenotools para los drivers propios (Turnip), como en nuestro SDK."""
    destino = arbol / "sdk" / "thirdparty" / "libadrenotools"
    if (destino / "CMakeLists.txt").is_file():
        print("[ok] libadrenotools ya esta")
        return
    origen = sdk_local / "thirdparty" / "libadrenotools"
    if (origen / "CMakeLists.txt").is_file() and (origen / "lib" / "linkernsbypass" / "CMakeLists.txt").is_file():
        shutil.copytree(origen, destino, ignore=shutil.ignore_patterns(".git"))
        print(f"[ok] libadrenotools copiado de {origen}")
        return
    sys.path.insert(0, str(RAIZ / "tools" / "android"))
    import preparar_sdk
    correr("git", "clone", "-q", preparar_sdk.ADRENOTOOLS_REPO, destino, que="git clone libadrenotools")
    correr("git", "checkout", "-q", preparar_sdk.ADRENOTOOLS_COMMIT, cwd=destino, que="git checkout libadrenotools")
    correr("git", "submodule", "update", "--init", "--recursive", cwd=destino, que="submodulos de libadrenotools")
    print(f"[ok] libadrenotools en {preparar_sdk.ADRENOTOOLS_COMMIT[:8]}")


def spirv_cross(arbol):
    """SPIRV-Cross para el backend de OpenGL ES (nfsmw_gles_shaders.cpp)."""
    destino = arbol / "sdk" / "thirdparty" / "spirv-cross"
    if (destino / "spirv_glsl.cpp").is_file():
        commit = git("rev-parse", "HEAD", cwd=destino).stdout.strip()
        if commit and commit != SPIRV_CROSS_COMMIT:
            fallar(f"{destino} esta en {commit[:8]} y tiene que estar en {SPIRV_CROSS_COMMIT[:8]} "
                   f"({SPIRV_CROSS_ETIQUETA}): borra la carpeta y vuelve a preparar.")
        print("[ok] SPIRV-Cross ya esta")
        return
    correr("git", "clone", "-q", "--depth", "1", "--branch", SPIRV_CROSS_ETIQUETA, SPIRV_CROSS_REPO, destino,
           que="git clone SPIRV-Cross")
    commit = git("rev-parse", "HEAD", cwd=destino).stdout.strip()
    if commit != SPIRV_CROSS_COMMIT:
        fallar(f"La etiqueta {SPIRV_CROSS_ETIQUETA} de SPIRV-Cross apunta a {commit[:8]}, no a "
               f"{SPIRV_CROSS_COMMIT[:8]}.")
    print(f"[ok] SPIRV-Cross en {SPIRV_CROSS_ETIQUETA}")


def parches(arbol):
    print("== Parches al SDK del motor nativo")
    entorno = dict(os.environ, NFSMW_SDK=str(arbol / "sdk"))
    correr(sys.executable, RAIZ / "tools" / "parche_iso.py", env=entorno, que="parche_iso.py")
    correr(sys.executable, RAIZ / "tools" / "parche_gamertag.py", env=entorno, que="parche_gamertag.py")
    correr(sys.executable, RAIZ / "tools" / "parche_turnip.py", env=entorno, que="parche_turnip.py")
    correr(sys.executable, RAIZ / "tools" / "parche_pausa.py", env=entorno, que="parche_pausa.py")
    correr(sys.executable, RAIZ / "tools" / "parche_memcpy.py", env=entorno, que="parche_memcpy.py")
    correr(sys.executable, RAIZ / "tools" / "android" / "parche_nativo.py",
           "--arbol", arbol, que="parche_nativo.py")


# ---------------------------------------------------------------------------
# 4. El juego
# ---------------------------------------------------------------------------

def es_referencia(ficha):
    # Por la edicion y no por su tabla: la PAL alemana usa la tabla de la
    # espanola (es la misma compilacion), pero es otro XEX con su propio codigo.
    return ficha["id"] == ediciones.datos()["referencia"]


def game_root(arbol, ficha):
    """Donde va el juego en el arbol: la PAL en assets/game_root, que es donde la
    busca su manifiesto, y otra edicion al lado."""
    return arbol / "assets" / ("game_root" if es_referencia(ficha) else f"game_root_{ficha['id']}")


def juego(arbol, iso, carpeta):
    """Devuelve (carpeta del juego extraido, ficha de su edicion)."""
    if carpeta:
        # Absoluta: la biblioteca de shaders se genera desde el arbol (cwd=arbol),
        # y una ruta relativa a este repositorio no valdria alli.
        carpeta = pathlib.Path(carpeta).resolve()
        if not (carpeta / "default.xex").is_file() or not (carpeta / "NFS").is_dir():
            fallar(f"{carpeta} no parece el juego extraido: faltan default.xex o NFS/.")
        return carpeta, ediciones.detectar(str(carpeta / "default.xex"))
    if iso:
        sys.path.insert(0, str(RAIZ / "tools"))
        import fase1_extraer as f1
        with open(iso, "rb") as fh:
            base, _ = f1.detectar_base(fh)
            sector, tam = f1.leer_descriptor(fh, base)
            entradas = [(ruta, e) for ruta, e in f1.recorrer(fh, base, sector, tam)
                        if not e["dir"] and (ruta.lower() == "default.xex" or ruta.lower().startswith("nfs/"))]
            xex = next((e for ruta, e in entradas if ruta.lower() == "default.xex"), None)
            if xex is None:
                fallar(f"{iso} no tiene default.xex.")
            # Primero el XEX solo: la edicion decide adonde va el resto, y la
            # PAL ya extraida no se pisa con otra.
            temporal = arbol / "out" / "default.xex.iso"
            temporal.parent.mkdir(parents=True, exist_ok=True)
            f1.extraer(fh, base, xex, str(temporal))
            ficha = ediciones.detectar(str(temporal))
            destino = game_root(arbol, ficha)
            print(f"== Extrayendo default.xex y NFS/ de {iso} en {destino}")
            destino.mkdir(parents=True, exist_ok=True)
            # El XEX siempre, aunque mida lo mismo: dos ediciones europeas
            # pueden tener el mismo tamano.
            os.replace(temporal, destino / "default.xex")
            for ruta, e in entradas:
                if e is xex:
                    continue
                salida = destino / ruta
                if salida.is_file() and salida.stat().st_size == e["tam"]:
                    continue
                f1.extraer(fh, base, e, str(salida))
        return destino, ficha
    destino = arbol / "assets" / "game_root"
    if not (destino / "default.xex").is_file() or not any((destino / "NFS").glob("*.[Bb][Ii][Nn]")):
        fallar(f"No hay juego en {destino}.\n"
               "        Pasa --iso <tu.iso> (se extraen default.xex y NFS/) o --juego <carpeta extraida>.")
    return destino, ediciones.detectar(str(destino / "default.xex"))


# ---------------------------------------------------------------------------
# 5. Generador y codigo
# ---------------------------------------------------------------------------

def compilar_rexglue(arbol):
    for herramienta in ("cmake", "ninja", "clang++"):
        if not shutil.which(herramienta):
            fallar(f"No encuentro '{herramienta}' en el PATH. En Windows, abre un 'Developer "
                   "PowerShell for VS' con el componente de Clang, o pasa --rexglue <ejecutable>.")
    sdk = arbol / "sdk"
    opciones = ["-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_C_COMPILER=clang",
                "-DCMAKE_CXX_COMPILER=clang++"]
    if platform.machine() in ("x86_64", "AMD64"):
        # Lo que ponen los presets del SDK: sin SSSE3 su memory.cpp no compila.
        opciones += ["-DCMAKE_C_FLAGS=-march=x86-64-v2", "-DCMAKE_CXX_FLAGS=-march=x86-64-v2"]
    print("== Generador de codigo (rexglue) del motor nativo")
    correr("cmake", "-S", sdk, "-B", arbol / "out" / "host", *opciones, que="cmake (rexglue)")
    correr("cmake", "--build", arbol / "out" / "host", "--target", "rexglue", que="la compilacion de rexglue")
    exe = "rexglue.exe" if platform.system() == "Windows" else "rexglue"
    candidatos = sorted([*(arbol / "out").rglob(exe), *(sdk / "out").rglob(exe)],
                        key=lambda p: p.stat().st_mtime, reverse=True)
    if not candidatos:
        fallar(f"Compilado, pero no encuentro {exe}.")
    return candidatos[0]


def codegen(arbol, rexglue, app, enganchadas=None, tabla=None):
    """Como su tools/codegen.sh. Con otra edicion, `enganchadas` y `tabla` son
    las que deja crear_arbol.py."""
    print(f"== Codegen ({app.name})")
    with open(app / "codegen.log", "w", encoding="utf-8") as log:
        r = subprocess.run([str(rexglue), "codegen", "nfsmw_manifest.toml"], cwd=app,
                           stdout=log, stderr=subprocess.STDOUT)
    if r.returncode != 0:
        fallar(f"El codegen ha fallado. Mira {app / 'codegen.log'}")
    # Llamadas directas entre funciones traducidas, para que ThinLTO pueda
    # integrarlas, y las copias literales contra las que se comparan sus
    # funciones nativas.
    generado = app / "generated" / "default"
    # Si el codegen no tenia nada que escribir ("up to date"), el codigo sigue con
    # las llamadas directas de la vez anterior, y llamadas_directas.py solo las
    # pone, no las quita: una funcion enganchada despues seguiria llamandose sin
    # su gancho. Se deshacen primero, que es lo mismo que un codegen nuevo.
    correr(sys.executable, "tools/llamadas_directas.py", "--deshacer", "--gen", generado,
           cwd=arbol, que="llamadas_directas.py --deshacer")
    correr(sys.executable, "tools/llamadas_directas.py", "--gen", generado,
           *(["--enganchadas", enganchadas] if enganchadas else []), cwd=arbol, que="llamadas_directas.py")
    correr(sys.executable, "tools/copia_literal.py", generado, app / "src" / "copias_literales",
           *(["--tabla", tabla] if tabla else []), cwd=arbol, que="copia_literal.py")
    n = sum(1 for _ in generado.glob("*.cpp"))
    print(f"[ok] {n} ficheros .cpp en {generado}")


# ---------------------------------------------------------------------------
# 5b. Otra edicion: app_<edicion>, la app con las direcciones traducidas
# ---------------------------------------------------------------------------

# Las de su tools/editions/crear_arbol.py: lo que reescribe al crear el arbol.
DIRECCION = re.compile(r"(?<![0-9A-Fa-f])82[0-9A-Fa-f]{6}(?![0-9A-Fa-f])")
# sub_824F, D7C0 partido en dos para pegarlo con ## (crear_arbol.py, PARTIDA)
PARTIDA = re.compile(r"sub_(82[0-9A-Fa-f]{0,6})\s*,\s*([0-9A-Fa-f]{1,6})(?![0-9A-Fa-f])")
BINARIOS = ("_spirv.h", "_ps.h", "_vs.h")
FUENTES = (".cpp", ".h", ".hpp", ".inl", ".c")
OTROS = ("CMakeLists.txt", "CMakePresets.json", "nfsmw.toml", "orden_funciones.ld", "overrides.toml",
         "huecos.toml", "generated/rexglue.cmake")
# Las mismas en todas las ediciones: la base de la imagen, .reloc y lo que va
# despues de la imagen (su emparejar.py, "constante").
CONSTANTES = (0x82000000, 0x82CD0000)
FIN_IMAGEN = 0x82D20000
SEGURAS = ("exacta", "misma direccion", "constante", "referencias")


def _direcciones(texto):
    out = {int(x, 16) for x in DIRECCION.findall(texto)}
    for alta, baja in PARTIDA.findall(texto):
        if len(alta) + len(baja) == 8:
            out.add(int(alta + baja, 16))
    return out


COMENTARIO = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)


def _direcciones_en_codigo(texto):
    """Las direcciones fuera de los comentarios: las que el codigo usa de verdad."""
    return _direcciones(COMENTARIO.sub("", texto))


def referencia_preparada(arbol):
    """El XEX PAL y su codigo generado: el punto de partida de otra edicion."""
    xex = arbol / "assets" / "game_root" / "default.xex"
    generado = arbol / "app" / "generated" / "default"
    if not xex.is_file() or not (generado / "codegen.partition.json").is_file() \
            or not any(generado.glob("nfsmw_recomp.*.cpp")):
        fallar("Otra edicion se traduce desde la PAL Espana, y no esta preparada.\n"
               "        Antes: python tools/android/preparar_nativo.py --iso <tu ISO PAL Espana>")
    if not es_referencia(ediciones.detectar(str(xex))):
        fallar(f"{xex} no es el de la PAL Espana.")
    if not (arbol / "out" / "nfsmw_shaders.nfsp").is_file():
        fallar("Falta la biblioteca de shaders de la PAL Espana (out/nfsmw_shaders.nfsp): de ella salen las "
               "huellas de los shaders de otra edicion.\n"
               "        Antes: python tools/android/preparar_nativo.py --iso <tu ISO PAL Espana>")
    return xex


def tabla_de_edicion(arbol, xex_ref, xex_otra, salida):
    """
    Traduce todas las direcciones que crear_arbol.py, llamadas_directas.py y
    copia_literal.py necesitan, y las escribe con el formato de su
    tools/editions/emparejar.py. Devuelve las que usa el codigo de app/src
    (sin las de los comentarios, que tambien se traducen).
    """
    app = arbol / "app"
    todas, en_codigo = set(), set()
    for ruta in (app / "src").rglob("*"):
        if ruta.is_file() and ruta.name.endswith(FUENTES) and not ruta.name.endswith(BINARIOS):
            texto = ruta.read_text(encoding="utf-8")
            todas |= _direcciones(texto)
            en_codigo |= _direcciones_en_codigo(texto)
    for rel in OTROS:
        todas |= _direcciones((app / rel).read_text(encoding="utf-8"))
    generado = app / "generated" / "default"
    with open(generado / "codegen.partition.json", encoding="utf-8") as fh:
        todas |= {int(d, 16) for d in json.load(fh)["assignments"]}
    debil = re.compile(r"(?<![\w])sub_([0-9A-F]{8})\(ctx, base\);")
    for ruta in glob.glob(str(generado / "nfsmw_recomp.*.cpp")):
        with open(ruta, encoding="utf-8") as fh:
            todas |= {int(x, 16) for x in debil.findall(fh.read())}
    todas |= _direcciones((arbol / "tools" / "copia_literal.py").read_text(encoding="utf-8"))

    img_ref, _ = imagen.cargar(str(xex_ref), str(ediciones.CACHE))
    img_otra, _ = imagen.cargar(str(xex_otra), str(ediciones.CACHE))
    e = emparejar.Emparejador(img_ref, img_otra)
    filas, cuenta = [], {}
    for d in sorted(todas):
        # Sin la busqueda por contenido: "exacta" para crear_arbol.py, y puede
        # caer en otra funcion. Lo que no se ancla se queda en dudosa.
        seccion, destino, estado = e.traducir(d, por_contenido=False)
        if destino is None and (d in CONSTANTES or d >= FIN_IMAGEN):
            destino, estado = d, "constante (misma en las dos)"
        filas.append("%08X\t%s\t%s\t%s" % (d, seccion, "%08X" % destino if destino else "-", estado))
        clave = "segura" if estado.startswith(SEGURAS) else "dudosa"
        cuenta[clave] = cuenta.get(clave, 0) + 1
    salida.parent.mkdir(parents=True, exist_ok=True)
    salida.write_text("pal\tseccion\totra\testado\n" + "\n".join(filas) + "\n", encoding="utf-8")
    print(f"[ok] {len(todas)} direcciones: {cuenta.get('segura', 0)} seguras, {cuenta.get('dudosa', 0)} dudosas"
          f" (solo valen en el reparto de funciones) -> {salida}")
    return e, en_codigo


def comprobar_funciones(e, en_codigo):
    """Las funciones que el codigo nativo de su app toca, enteras: tienen que ser
    las mismas en las dos ediciones salvo direcciones (emparejar.py)."""
    malas, avisos, parejas = [], [], 0
    for d in sorted(en_codigo):
        if e.traducir(d)[0] != ".text":
            continue
        # Enteras, y cada direccion que calculan (lis + parte baja) con su
        # traduccion: asi se comprueba tambien la de .data donde se mueve.
        problemas = e.comparar_funcion(d)
        n, otros, constantes = e.comprobar_parejas(d)
        parejas += n
        problemas += otros
        avisos += constantes
        if problemas:
            malas.append("%08X: %s" % (d, "; ".join(problemas[:4])))
    if malas:
        fallar("Funciones que la app toca y que en esta edicion son distintas:\n        "
               + "\n        ".join(malas))
    print(f"[ok] las funciones que toca la app son las mismas en las dos ediciones ({parejas} direcciones"
          " calculadas, todas con su traduccion)")
    if avisos:
        # Su codigo lee sus constantes; la app no las copia.
        print(f"     aviso: {len(avisos)} constantes con otro valor en esta edicion, en funciones que la app"
              " envuelve o solo nombra:")
        for aviso in sorted(set(avisos))[:8]:
            print(f"       {aviso}")


# Los tres shaders que el renderizador reconoce por la huella (XXH3) de su
# contenedor original. En otra edicion el mismo microcodigo puede venir en un
# contenedor distinto, y entonces con otra huella (el resplandor y el cielo de la
# USA, y ademas la composicion en la japonesa).
HUELLAS_ESPECIALES = ("kHuellaBrightPass", "kHuellaCielo", "kHuellaComposicion")


def _biblioteca(ruta):
    """{huella: contenedor original} de una biblioteca NFSSPV (su nfsmw_shader_library.cpp)."""
    datos = ruta.read_bytes()
    if datos[:8] != b"NFSSPV\0\0":
        fallar(f"{ruta} no es una biblioteca de shaders.")
    _version, cantidad = struct.unpack_from("<II", datos, 8)
    pos, contenedores = 24, {}
    for _ in range(cantidad):
        original, palabras, huella = struct.unpack_from("<IIQ", datos, pos)
        pos += 16
        contenedores[huella] = datos[pos:pos + original]
        pos += original + palabras * 4
    return contenedores


def huellas_shaders(arbol, ficha, biblioteca):
    """
    Las huellas de los shaders especiales que cambian en esa edicion, {PAL: suya}:
    en su biblioteca, el contenedor con el mismo microcodigo que el de la PAL
    (como su tools/editions/shaders_especiales.py, que lo saca de los
    contenedores del disco). Si ediciones.json ya las tiene ("nativo"), tienen
    que coincidir.
    """
    fuente = (arbol / "app" / "src" / "nfsmw_nativo_dibujos.cpp").read_text(encoding="utf-8")
    pal = _biblioteca(arbol / "out" / "nfsmw_shaders.nfsp")
    suya = _biblioteca(biblioteca)
    cambian = {}
    for nombre in HUELLAS_ESPECIALES:
        m = re.search(rf"constexpr uint64_t {nombre} = 0x([0-9A-Fa-f]{{16}})ull;", fuente)
        if not m:
            fallar(f"No encuentro {nombre} en app/src/nfsmw_nativo_dibujos.cpp.")
        huella = int(m.group(1), 16)
        if huella not in pal:
            fallar(f"{nombre} ({huella:016X}) no esta en la biblioteca de la PAL Espana.")
        b = pal[huella]
        # La palabra 1 del contenedor (big-endian) dice donde empieza el microcodigo.
        inicio = int.from_bytes(b[4:8], "big")
        iguales = sorted(h for h, u in suya.items() if u[4:8] == b[4:8] and u[inicio:] == b[inicio:])
        if len(iguales) != 1:
            fallar(f"{nombre}: en la biblioteca de {ficha['nombre']} hay {len(iguales)} contenedores con su "
                   "microcodigo, y tiene que haber uno.\n"
                   "        Esa edicion trae otro shader: hay que mirarlo a mano (ver "
                   "tools/editions/shaders_especiales.py).")
        if iguales[0] != huella:
            cambian["%016X" % huella] = "%016X" % iguales[0]
    anotadas = (ficha.get("nativo") or {}).get("huellas_shaders")
    if anotadas is not None and anotadas != cambian:
        fallar(f"Las huellas de shaders de {ficha['nombre']} no son las de tools/ediciones/ediciones.json: "
               f"salen {cambian or 'las de la PAL'}, y alli dice {anotadas or 'las de la PAL'}.")
    print("[ok] huellas de los shaders especiales: "
          + (", ".join(f"{v} -> {n}" for v, n in cambian.items()) if cambian else "las mismas que en la PAL"))
    return cambian


def arbol_de_edicion(arbol, ficha, carpeta_juego, huellas):
    """Crea app_<edicion>. Devuelve (app, tabla)."""
    ident = ficha["id"]
    xex_ref = referencia_preparada(arbol)
    # Su manifiesto quiere el XEX dentro de game_root.
    xex = game_root(arbol, ficha) / "default.xex"
    if carpeta_juego.resolve() != xex.parent.resolve():
        xex.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(carpeta_juego / "default.xex", xex)

    print(f"== Direcciones PAL Espana -> {ficha['nombre']}")
    tabla = arbol / "out" / ident / "tabla.tsv"
    e, fuentes = tabla_de_edicion(arbol, xex_ref, xex, tabla)
    comprobar_funciones(e, fuentes)

    print(f"== app_{ident}")
    extra = []
    parejas = arbol / "tools" / "editions" / ident / "parejas_corregidas.json"
    if parejas.is_file():
        extra += ["--parejas", parejas]
    # Las huellas de los shaders que el renderizador reconoce y que en esta
    # edicion cambian (el resplandor y el cielo de la USA).
    extra += [f"{vieja}={nueva}" for vieja, nueva in huellas.items()]
    correr(sys.executable, "tools/editions/crear_arbol.py", ident, tabla,
           f"../assets/{xex.parent.name}/default.xex", *extra, cwd=arbol, que="crear_arbol.py")
    return arbol / f"app_{ident}", tabla


# ---------------------------------------------------------------------------
# 6. Shaders
# ---------------------------------------------------------------------------

def shaders(arbol, carpeta_juego, salida):
    if not shutil.which("node"):
        fallar("No encuentro 'node' en el PATH. La biblioteca de shaders se genera con Node.js.")
    # Sus herramientas son modulos ES sin package.json: se copian con uno.
    instalador = arbol / "out" / "host-tools" / "installer"
    if instalador.exists():
        shutil.rmtree(instalador)
    shutil.copytree(arbol / "android" / "app" / "src" / "main" / "assets" / "shaders", instalador)
    (instalador / "package.json").write_text('{"type":"module"}\n', encoding="utf-8")
    print("== Biblioteca de shaders")
    salida.parent.mkdir(parents=True, exist_ok=True)
    correr("node", "tools/biblioteca_shaders.mjs", instalador, carpeta_juego, salida,
           cwd=arbol, que="biblioteca_shaders.mjs")
    # La del backend de OpenGL ES: los mismos shaders con una textura fija por registro.
    print("== Biblioteca de shaders de OpenGL ES")
    correr("node", "tools/biblioteca_shaders_gles.mjs", instalador, carpeta_juego,
           salida.with_name("nfsmw_shaders_gles.nfsp"), cwd=arbol, que="biblioteca_shaders_gles.mjs")


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description="Prepara el motor nativo (nfsmw-android).")
    ap.add_argument("--arbol", default=str(RAIZ / "nativo"),
                    help="el arbol del motor nativo (por defecto, nativo/ en este repositorio)")
    ap.add_argument("--sdk", default=str(RAIZ.parent / "rexglue-sdk-android"),
                    help="el SDK de Android de este proyecto, de donde copiar los submodulos")
    ap.add_argument("--iso", help="tu ISO del juego: se extraen default.xex y NFS/")
    ap.add_argument("--juego", help="o una carpeta con el juego ya extraido")
    ap.add_argument("--rexglue", help="un rexglue ya compilado DESDE EL SDK DE ESE ARBOL")
    args = ap.parse_args()
    arbol = pathlib.Path(args.arbol).resolve()

    comprobar_arbol(arbol)
    terceros(arbol, pathlib.Path(args.sdk).resolve())
    adrenotools(arbol, pathlib.Path(args.sdk).resolve())
    spirv_cross(arbol)
    parches(arbol)
    carpeta_juego, ficha = juego(arbol, args.iso, args.juego)
    referencia = es_referencia(ficha)
    print(f"== Edicion del juego: {ficha['nombre']} ({ficha['id']})")
    if not referencia:
        # Antes de compilar nada: que se pueda.
        referencia_preparada(arbol)

    if args.rexglue:
        rexglue = pathlib.Path(args.rexglue).resolve()
        if not rexglue.is_file():
            fallar(f"No encuentro {rexglue}")
    else:
        rexglue = compilar_rexglue(arbol)
    print(f"[ok] {rexglue}")

    if referencia:
        xex = arbol / "assets" / "game_root" / "default.xex"
        if carpeta_juego.resolve() != xex.parent.resolve():
            # Su manifiesto busca el XEX en assets/game_root.
            xex.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(carpeta_juego / "default.xex", xex)
        codegen(arbol, rexglue, arbol / "app")
        salida = arbol / "out"
        shaders(arbol, carpeta_juego, salida / "nfsmw_shaders.nfsp")
    else:
        # La biblioteca primero: de ella salen las huellas que crear_arbol.py cambia.
        salida = arbol / "out" / ficha["id"]
        shaders(arbol, carpeta_juego, salida / "nfsmw_shaders.nfsp")
        huellas = huellas_shaders(arbol, ficha, salida / "nfsmw_shaders.nfsp")
        app, tabla = arbol_de_edicion(arbol, ficha, carpeta_juego, huellas)
        codegen(arbol, rexglue, app, enganchadas=app / "enganchadas.txt", tabla=tabla)

    with open(salida / "edicion.json", "w", encoding="utf-8", newline="\n") as fh:
        json.dump({"id": ficha["id"], "nombre": ficha["nombre"], "tabla": ficha["tabla"],
                   "referencia": referencia, "idioma": ficha["idioma"], "pais": ficha["pais"],
                   "sha256": ficha["sha256"]}, fh, ensure_ascii=False, indent=2)
        fh.write("\n")

    print(f"\n[ok] Motor nativo preparado en {arbol} para {ficha['nombre']}")
    print("     Siguiente: cd android && gradlew assembleRelease -Pnfsmw.motor=nativo"
          + ("" if referencia else f" -Pnfsmw.edicion={ficha['id']}"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
