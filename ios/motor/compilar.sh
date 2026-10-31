#!/usr/bin/env bash
# Builds the native engine for iOS in WSL, from the Windows checkout.
#
#   wsl -e bash /mnt/c/.../ios/motor/compilar.sh [--dependencias] [--limpio] [--instalar]
#
# Leaves build-ios/NFSMW.ipa; --instalar signs and installs it on the iPhone with xtool (it must be
# attached to WSL: usbipd attach --wsl --busid <id>).
#
# The sources are copied to ~/nfsmw first: building on /mnt/c is several times slower. The SDK's
# third-party clones and the generated game code are not in git, so they come from the main checkout
# (NFSMW_PRINCIPAL), once or with --dependencias.
set -euo pipefail

aqui=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$aqui/../.." && pwd)
principal=${NFSMW_PRINCIPAL:-$(dirname "$repo")/NFSMW-Recompiled}
copia=${NFSMW_COPIA:-$HOME/nfsmw}
build=${NFSMW_BUILD:-$copia/build-ios}
moltenvk=${NFSMW_MOLTENVK:-$HOME/deps/moltenvk}
version_moltenvk=v1.4.1

dependencias=0
limpio=0
instalar=0
for arg in "$@"; do
    case "$arg" in
        --dependencias) dependencias=1 ;;
        --limpio) limpio=1 ;;
        --instalar) instalar=1 ;;
        *) echo "Opcion desconocida: $arg" >&2; exit 2 ;;
    esac
done

# Without the game, Android, Mesa, the other editions and the build folders.
excluir=(--exclude=.git --exclude=/nativo/assets/ --exclude=/nativo/out/ --exclude=/nativo/sdk/out/
         --exclude=/nativo/app/out/ --exclude=/nativo/mesa/ --exclude='/nativo/app_*/'
         --exclude=/nativo/pgo/ --exclude=/nativo/extras/ --exclude=build/)

mkdir -p "$copia"
if [ "$dependencias" = 1 ] || [ ! -d "$copia/nativo/sdk/thirdparty/sdl3" ] || [ ! -d "$copia/nativo/app/generated/default" ]; then
    echo "== Dependencias y codigo generado desde $principal"
    [ -f "$principal/nativo/app/generated/default/sources.cmake" ] || {
        echo "No hay codigo generado en $principal/nativo/app/generated" >&2; exit 1; }
    rsync -a "${excluir[@]}" "$principal/nativo/sdk/thirdparty/" "$copia/nativo/sdk/thirdparty/"
    rsync -a "$principal/nativo/app/generated/" "$copia/nativo/app/generated/"
fi
echo "== Fuentes desde $repo"
rsync -a "${excluir[@]}" "$repo/nativo" "$repo/ios" "$copia/"
# El codigo nativo del mando tactil de la app de Android (nativo_android.cpp), que iOS comparte.
mkdir -p "$copia/android/app/src/main/cpp"
rsync -a "$repo/android/app/src/main/cpp/nativo" "$copia/android/app/src/main/cpp/"

if [ ! -f "$moltenvk/MoltenVK/MoltenVK/static/MoltenVK.xcframework/ios-arm64/libMoltenVK.a" ]; then
    echo "== MoltenVK $version_moltenvk (release de Khronos)"
    mkdir -p "$moltenvk"
    curl -fL -o "$moltenvk/MoltenVK-ios.tar" \
        "https://github.com/KhronosGroup/MoltenVK/releases/download/$version_moltenvk/MoltenVK-ios.tar"
    tar -xf "$moltenvk/MoltenVK-ios.tar" -C "$moltenvk"
fi

[ "$limpio" = 1 ] && rm -rf "$build"
if [ ! -f "$build/build.ninja" ]; then
    echo "== Configurando en $build"
    cmake -G Ninja -S "$copia/ios/motor" -B "$build" \
        -DCMAKE_TOOLCHAIN_FILE="$copia/ios/motor/ios.toolchain.cmake" \
        -DCMAKE_BUILD_TYPE=Release \
        -DNFSMW_MOLTENVK="$moltenvk/MoltenVK/MoltenVK"
fi

echo "== Compilando"
cmake --build "$build" --target nfsmw -- -j "${NFSMW_JOBS:-6}"

echo "== Empaquetando $build/NFSMW.ipa"
rm -rf "$build/Payload" "$build/NFSMW.ipa"
mkdir -p "$build/Payload"
cp -R "$build/nfsmw-app/nfsmw.app" "$build/Payload/"
app="$build/Payload/nfsmw.app"
# Lo que lleva el APK en sus assets: los textos de la app en sus 10 idiomas (los de Android y los
# propios de iOS), la edicion del juego del codigo generado (el lanzador la ensena, la compara con la
# ISO y le pasa su idioma y su pais al juego) y la biblioteca de shaders de esa edicion, que sale de
# TU copia del juego: como el codigo generado, es una razon mas para no compartir la app.
python3 "$aqui/textos.py" "$repo/android/app/src/main/res" "$aqui/textos" "$app/textos.json"
# El icono, el de Android en los tamanos de iOS (los nombra CFBundleIconFiles en Info.plist).
python3 "$aqui/iconos.py" "$repo/android/app/src/main/res/mipmap-xxxhdpi/ic_launcher.png" "$app"
# La fuente del boton Salir del menu principal (res/font de Android, OFL) y su licencia.
cp "$repo/android/app/src/main/res/font/michroma.ttf" "$app/michroma.ttf"
cp "$repo/android/app/src/main/res/raw/licencia_michroma.txt" "$app/licencia_michroma.txt"
# Los ajustes del motor que lleva el APK (assets/nativo/nfsmw.toml): el lanzador los copia a Documents.
cp "$repo/nativo/app/nfsmw_movil.toml" "$app/nfsmw.toml"
for f in edicion.json nfsmw_shaders.nfsp; do
    if [ -f "$principal/nativo/out/$f" ]; then
        cp "$principal/nativo/out/$f" "$app/"
    else
        echo "Aviso: no hay $principal/nativo/out/$f; la app va sin el" >&2
    fi
done
(cd "$build" && python3 -m zipfile -c NFSMW.ipa Payload/)
ls -lh "$build/NFSMW.ipa"

if [ "$instalar" = 1 ]; then
    echo "== Instalando con xtool"
    xtool install "$build/NFSMW.ipa"
fi
