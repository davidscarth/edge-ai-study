# vk-autotune v1

Portable Vulkan GEMM autotuner for small GPUs (Pi 4/5 V3D), featuring:
- Presets: `classic`, `classic_legacy`, `extended16k`, `extended16k_capped`
- Default lanes:
  - classic: `16x8,16x4`
  - classic_legacy: `16x8,16x4,16x1`
  - extended16k: `16x8,16x4,16x1`
  - extended16k_capped: `16x8,16x16`
- Runtime lane overrides: `--lsz=16x8,16x16,32x8`
- Dynamic shared memory via spec constants (`SH_ELEMS=TM*TK + TK*TN`), with SMEM budget check
- Per-candidate timeouts, warmups, timestamp timing, CSV export
- Skips software devices (llvmpipe/lavapipe)
- Reads which shader compiler was used (`glslc` or fallback `glslangValidator`)

## Build
```bash
sudo apt install -y build-essential cmake glslang-tools libvulkan-dev vulkan-tools
cmake -S . -B build
cmake --build build -j4
```

## Run (example)
```bash
cd build

AT_CSV=results.csv ./autotune --preset=classic --lsz=16x8,16x16,32x8
```

## Flags
- `--preset=` classic | classic_legacy | extended16k | extended16k_capped
- `--lsz=16x8[,16x16[,32x8[,16x4[,16x1]]]]`
- `--Ms=64,80,96,112`  `--Ns=32,48,64,80`
- `--enable-smem=1|0`  `--enable-nosmem=1|0`
- `--max-rn=N` `--max-rm=N`  (defaults **8**; limits per-thread accumulator grid)
- `--add-tiles=96x64,112x64,...`

Env:
- `AT_M, AT_N, AT_K` (default 1024)
- `AT_WARM, AT_REP` (default 5, 30)
- `AT_TIMEOUT_MS` (per-candidate timeout, default 600000)
- `AT_CSV` (path to CSV output)
- `AT_SMEM_FRAC` (0.5..1.0 safety factor on SMEM, default 1.0)
