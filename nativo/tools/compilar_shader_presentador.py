"""Compiles a presenter effect shader (sdk/src/ui/shaders/fuentes/*.frag) to the SPIR-V header the VulkanPresenter
includes (sdk/src/ui/shaders/vulkan_spirv/<name>.h), in the same format as the ones `xb buildshaders` generated.

Uses the NDK's glslc and spirv-dis (ANDROID_NDK_HOME, or the newest NDK under %LOCALAPPDATA%/Android/Sdk/ndk).
Run from nativo/:  python tools/compilar_shader_presentador.py sdk/src/ui/shaders/fuentes/guest_output_sgsr1_ps.frag
An optional second argument is the folder for the header (the native renderer's own shaders:
  python tools/compilar_shader_presentador.py app/src/shaders/nfsmw_sgsr2_convertir.frag app/src/shaders
  writes app/src/shaders/nfsmw_sgsr2_convertir_ps.h with the array nfsmw_sgsr2_convertir_ps).
After it, run tools/gles_efectos_presentador.cpp again so GlesPresenter gets the shader too.
"""

import os
import pathlib
import struct
import subprocess
import sys
import tempfile


def herramientas():
    ndk = os.environ.get("ANDROID_NDK_HOME")
    if not ndk:
        raiz = pathlib.Path(os.environ.get("LOCALAPPDATA", "")) / "Android" / "Sdk" / "ndk"
        versiones = sorted(raiz.glob("*"), key=lambda p: [int(x) for x in p.name.split(".") if x.isdigit()])
        if not versiones:
            sys.exit("no NDK found: set ANDROID_NDK_HOME")
        ndk = versiones[-1]
    carpeta = next(pathlib.Path(ndk, "shader-tools").glob("*"))
    ext = ".exe" if os.name == "nt" else ""
    return carpeta / ("glslc" + ext), carpeta / ("spirv-dis" + ext)


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    fuente = pathlib.Path(sys.argv[1])
    carpeta = pathlib.Path(sys.argv[2]) if len(sys.argv) == 3 else pathlib.Path("sdk/src/ui/shaders/vulkan_spirv")
    nombre = fuente.stem if len(sys.argv) == 2 else fuente.stem + "_ps"
    glslc, spirv_dis = herramientas()
    with tempfile.TemporaryDirectory() as tmp:
        spv = pathlib.Path(tmp, nombre + ".spv")
        # SPIR-V 1.0 like the other presenter shaders (Vulkan 1.0 devices).
        subprocess.run([str(glslc), "--target-env=vulkan1.0", "-fshader-stage=frag", "-O", str(fuente),
                        "-o", str(spv)], check=True)
        datos = spv.read_bytes()
        texto = subprocess.run([str(spirv_dis), str(spv)], check=True, capture_output=True, text=True).stdout
    palabras = struct.unpack("<%dI" % (len(datos) // 4), datos)
    lineas = ["// Generated with tools/compilar_shader_presentador.py from %s." % fuente.as_posix(),
              "#if 0"]
    lineas += texto.rstrip("\n").split("\n")
    lineas += ["#endif", "", "const uint32_t %s[] = {" % nombre]
    for i in range(0, len(palabras), 8):
        lineas.append("    " + ", ".join("0x%08X" % p for p in palabras[i:i + 8]) + ",")
    lineas.append("};")
    salida = carpeta / (nombre + ".h")
    salida.write_text("\n".join(lineas) + "\n", newline="\n")
    print("%s: %d palabras" % (salida, len(palabras)))


if __name__ == "__main__":
    main()
