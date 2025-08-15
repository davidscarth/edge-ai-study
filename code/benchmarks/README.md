# benchmarks
Here's a collection of Raspberry Pi benchmarks using [vk-autotune](../vk-autotune) to examine the speed of different tile sizes

#### Raspberry Pi 5 (16GB)
```shell
AT_MAX_RM=12 AT_MAX_RN=8 AT_TIMEOUT_MS=1200000 AT_CSV=pi5_ext16k_lsz16x16.csv ./autotune --preset=extended16k --lsz=16x16 --enable-smem=1
# Device: V3D 7.1.10.2 (API 1.3)  driver=104857607
# maxWGInvocations=256, maxSharedMemPerWG=16384 bytes, subgroupSize=16
# shader-compiler=glslc
# Preset=extended16k  lanes=16x16  candidates=53
```
[pi5_ext16k_lsz16x16.csv](pi5_ext16k_lsz16x16.csv)

```shell
AT_MAX_RM=24 AT_MAX_RN=8 AT_TIMEOUT_MS=1200000 AT_CSV=pi5_ext16k_lsz16x8.csv ./autotune --preset=extended16k --lsz=16x8 --enable-smem=1
# Device: V3D 7.1.10.2 (API 1.3)  driver=104857607
# maxWGInvocations=256, maxSharedMemPerWG=16384 bytes, subgroupSize=16
# shader-compiler=glslc
# Preset=extended16k  lanes=16x8  candidates=53
```
[pi5_ext16k_lsz16x8.csv](pi5_ext16k_lsz16x8.csv)

#### Raspberry Pi 4 (8GB)
The Raspberry Pi 4 GPU is extremely slow. Configuring any workgroup size other than 16x16 will lead to frustration.

```shell
AT_MAX_RM=12 AT_MAX_RN=8 AT_TIMEOUT_MS=1200000 AT_CSV=pi4_ext16k_lsz16x16.csv ./autotune --preset=extended16k --lsz=16x16 --enable-smem=1
# Device: V3D 4.2.14.0 (API 1.3)  driver=104857607
# maxWGInvocations=256, maxSharedMemPerWG=16384 bytes, subgroupSize=16
# shader-compiler=glslc
# Preset=extended16k  lanes=16x16  candidates=53
```
[pi4_ext16k_lsz16x16.csv](pi4_ext16k_lsz16x16.csv)

```shell
AT_MAX_RM=24 AT_MAX_RN=8 AT_WARM=1, AT_REP=1, AT_TIMEOUT_MS=7200000 AT_CSV=pi4_ext16k_lsz16x8.csv ./autotune --preset=extended16k --lsz=16x8 --enable-smem=1
# Device: V3D 4.2.14.0 (API 1.3)  driver=104857607
# maxWGInvocations=256, maxSharedMemPerWG=16384 bytes, subgroupSize=16
# shader-compiler=glslc
# Preset=extended16k  lanes=16x8  candidates=53
```
pi4_ext16k_lsz16x8.csv

#### Summary of results
TBD
