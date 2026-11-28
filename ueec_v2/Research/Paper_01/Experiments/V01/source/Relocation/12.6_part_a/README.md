# UEEC Variant II — 12.6 Full Relocation Integration (part A)

Run `make test` and `make sanitize` from 12_relocation. Same cross compilers and QEMU runners as stage 12.5. No generated payloads or fixtures included.

Retains the 12.5 regression suite (1728 positive, 84 rejection cases per architecture). Adds 72 common UEEC fixtures with three trailing relocation sections per node, mixing BLOCK_4096_16, BLOCK_N_32 and FLAT16. BLOCK_N sizes: 16, 24, 128, 4096. Each fixture is tested in four load modes (288 cases per architecture), plus six cross-section overlap rejections. All original 17 relocation entries are retained in each mixed node. A global overlap tracker spans relocation sections; failures prevent execution and release the image.

`build/integration-report.json` contains regression and mixed results. `build/benchmark.json` compares encoded section sizes and median Python decoder time (9 samples of 1000 decodes each, includes equality assertion). This is not a native relocation benchmark. The benchmark is run once after the full matrix. `make sanitize` instruments only the x86-64 C loader.

Scope limits: Couple16 only; relocation sections must trail the four runtime sections; absolute destinations remain INIT_DATA and branch relocation remains inside the same CODE section. Overlap tracking is quadratic; no large stress or fault-based memory protection tests yet. The converter remains the stage 12.5 implementation. Additional requirements belong to a later integration increment, not claimed complete here.
