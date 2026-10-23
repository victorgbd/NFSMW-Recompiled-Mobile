# Cambios al SDK ReXGlue

Esta carpeta **no** contiene el SDK. Contiene un solo diff con todo lo que este
proyecto le cambia al SDK, para poder leerlo de una vez:
[`nfsmw-parches.diff`](nfsmw-parches.diff).

Lo que se aplica de verdad son los scripts `tools/parche_*.py`, en el orden de
[`tools/android/preparar_sdk.py`](../tools/android/preparar_sdk.py). El diff se
regenera con:

```
python tools/android/diff_sdk.py
```

## De dónde sale el SDK de Android

`tools/android/preparar_sdk.py` lo monta en `..\rexglue-sdk-android`, aparte del de
Windows:

1. [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) en la etiqueta `v0.10.0`
   (BSD 3-Clause).
2. El parche Android de
   [hells-gate-recomp-android](https://github.com/deivid22srk/hells-gate-recomp-android),
   de deivid22srk, fijado a un commit. **No está en este repositorio**: su repo no
   declara licencia, así que se descarga de allí al preparar el SDK.
3. [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD 2-Clause), fijado a
   un commit.
4. Los parches de este proyecto, en este orden.

El diff compara el paso 3 con el paso 4. Así sale **solo** lo de este proyecto: lo de
hells-gate está en los dos lados. En los tres ficheros donde el contexto habría
arrastrado líneas suyas (`src/graphics/command_processor.cpp`,
`src/graphics/vulkan/command_processor.cpp` y `src/ui/CMakeLists.txt`), el diff va sin
contexto. Para aplicarlo a mano sobre un SDK con los pasos 1 a 3:

```
git apply --unidiff-zero nfsmw-parches.diff
```

## Los parches

| Script | Qué hace |
|---|---|
| `parche_diagnostico` | Dos parches sobre hilos y memoria |
| `parche_anillo` | Escucha la conversación entre el juego y el XMA, del lado del kernel |
| `parche_desatasco` | Desatasca la voz XMA cuando el juego se queda girando sobre ella |
| `parche_restaurar` | Menú de ajustes (F4): botón de restaurar y deslizadores |
| `parche_velocidad` | Ajuste de velocidad del juego, en porcentaje, desde F4 |
| `parche_backend` | Selector de API gráfica, D3D12 o Vulkan, desde F4 |
| `parche_privilegios` | Conceder los privilegios de Xbox Live, para el multijugador |
| `parche_iso` | `--game_data_root` puede ser una ISO y, en Android, una URI `content://` |
| `parche_turnip` | Cargar un driver Vulkan propio (Turnip) en Adreno |
| `parche_pausa` | Soltar la superficie cuando Android manda la app a segundo plano |
| `parche_fps` | Los fps en el log, una línea cada cinco segundos |
| `parche_pipeline` | No recompilar el pipeline de salida de la imagen en cada fotograma |
| `parche_fallos` | Fallos de página de la vigilancia de escritura baratos en Linux/Android |
| `parche_esperas` | Que las esperas múltiples de POSIX duerman de verdad |
| `parche_teclado` | Que el teclado sirva en los menús del juego |
| `parche_tiempos` | Reparto del tiempo del procesador de comandos de la GPU |
| `parche_espera_anillo` | Avisar cuando el procesador de comandos publica su puntero de lectura |
| `parche_cola_presentar` | Presentar desde una cola de Vulkan propia |
| `parche_subidas` | Subidas de memoria antes del pase de render, sin cortarlo |
| `parche_cvars` | Que los ajustes de un plugin sobrevivan a que se cargue dos veces |
| `parche_fences` | Que el procesador de comandos no pregunte al driver por los envíos acabados |
| `parche_area` | Que cada pase de render cargue y guarde solo la zona que pinta |
| `parche_vblank` | Despertar al procesador de comandos en cuanto llega el vblank |
| `parche_msaa` | MSAA a la carta: 1, 2 o 4 muestras |
| `parche_xma_paquetes` | Paquetes XMA que cruzan de buffer de entrada (de Xenia Canary) |
| `parche_xma_edge` | Tres arreglos del descodificador XMA (de Xenia Edge) |
| `parche_anillo_bloques` | Publicar el puntero de lectura del anillo mientras se vacía (de Xenia Edge) |
| `parche_una_pasada` | La escena en una pasada desde el procesador de comandos (ya no se usa: la sustituye `android/app/src/main/cpp/render_targets.cpp`) |
| `parche_camino_edram` | Escribir en el log qué camino de EDRAM se usa |
| `parche_gamertag` | Cvar `user_gamertag`, el gamertag del perfil emulado |
| `parche_cache_pipelines` | Cache de pipelines del driver en disco, y el precreado de pipelines arreglado en Android |
| `parche_memcpy` | El memcpy nativo (`[rexcrt]`) copia como el del juego aunque el origen y el destino se solapen |

Quedan fuera los dos que solo tocan Direct3D 12, que en Android no se compila:
`parche_presentador` y `parche_gpu_fallback`.
