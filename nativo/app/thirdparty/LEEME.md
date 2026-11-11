# Third-party code of the native engine

Used only by the Lossless Scaling frame generation (`src/nfsmw_lsfg.cpp`, Android, cvar `nfsmw_lsfg`). Each folder
keeps its license; the APK carries them in `res/raw/licencia_lsfg.txt`.

| Folder | What | Source | License |
|---|---|---|---|
| `lsfg-vk/` | framegen (LSFG 3.1 and 3.1p) and the shader extraction from Lossless.dll | github.com/PancakeTAS/lsfg-vk, tag v1.0.0 | MIT |
| `dxbc/` | DXVK's DXBC to SPIR-V compiler, standalone | github.com/Username404-59/dxbc, commit 78ab59a (the one lsfg-vk v1.0.0 uses; its original repo is gone) | zlib |
| `pe-parse/` | PE parser (only `pe-parser-library`) | github.com/trailofbits/pe-parse, commit 31ac596 | MIT |
| `volk/` | Vulkan function loader (`volk.c`, `volk.h`) | github.com/zeux/volk, commit be3dbd4 | MIT |

Changes to lsfg-vk are marked `NFSMW`: framegen runs on the renderer's own Vulkan device and queue (it used to create
its own and share images through file descriptors), its submissions go through a hook (the queue's lock), the
renderer draws straight into its inputs, and `public/lsfg_nfsmw.hpp` + `nfsmw/api.cpp` replace `lsfg_3_1.hpp` and
both `lsfg.cpp`. `extract/` takes the DLL path instead of searching Steam. lsfg-vk versions after v1.0.0 moved to
GPL-3 and, from August 2026, to a more restrictive license: do not update from them without checking it.

The shaders are not here: Lossless Scaling is a paid program and each user copies their own Lossless.dll.
