# Nvidia Orin Nano Super
Version 0.2-dev<br>
4/8/2026

## Objective
Perform a feasibility study for running chat-oriented LLMs on highly resource-constrained hardware.

I will also use this exercise as a way to test setting up a self-contained LLM server, as I expect much of the setup and configurations will be useful elsewhere.

## Hypothesis
I expect that the Orin Nano will be a much more capable device than the Raspi series when it comes to running and using LLMs. This is just some ramblings for now.

## Hardware
The following build with a budget of approximately $300
* [Nvidia Jetson Orin Nano Super](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/) (8GB) ($249 from Amazon)
  * Ampere GPU w/ 1024 CUDA cores, 32 tensor cores; 6-core ARM Cortex-A78AE v8.2 64-bit CPU; 8GB LPDDR5
* [Crucial P310 500GB NVMe M.2 SSD](https://www.crucial.com/ssd/p310/ct500p310ssd8) ($46 from Amazon)

As of 8/2025, the Orin Nano Super comes with firmware 36.4.3-gcid-38968081. It has "MAXN_SUPER" mode available out of the box and is compatible with JetPack 6.2 without needing to update the firmware.

Upon first boot to desktop with JetPack 6.2.1, it will update you to firmware 36.4.4 and need a couple of reboots.

## Software
* [Nvidia JetPack](https://developer.nvidia.com/embedded/jetpack-sdk-622) 6.2.2*
  * SD Image is Jetson Linux 36.4.4, we will apt update to get to 36.5. nVidia is too lazy to make an updated image and doesn't provide one.
 
## Setup
> **Under Development**
> 
> This is an incoherent collection of things, I intend to make it like a runbook
> 
* Downloaded JetPack 6.2.1 and extracted "sd-blob.img".
* Imaged the nVME using an external USB-NVMe enclosure (Sabrent EC-SNVE) using Rufus. Selected "sd-blob.img" and clicked START.

The boot will fail 5-6 seconds in. You need to edit /boot/extlinux/extlinux.conf as follows:

Original (line 10):
```shell
      APPEND ${cbootargs} root=/dev/mmcblk0p1 rw rootwait rootfstype=ext4 mminit_loglevel=4 console=ttyTCU0,115200 firmware_class.path=/etc/firmware fbcon=map:0 video=efifb:off console=tty0 
```

Correct (new line 10, for an NVMe drive in the 2280 slot):
```shell
      APPEND ${cbootargs} root=/dev/nvme0n1p1 rw rootwait rootfstype=ext4 mminit_loglevel=4 console=ttyTCU0,115200 firmware_class.path=/etc/firmware fbcon=map:0 video=efifb:off console=tty0 
```

Once you fix the extlinux.conf file, it will boot and finish setup as normal, no need for any SD card.

### Snap update issue workaround (specific to the Jetson Nano)
```shell
snap download snapd --revision=24724
sudo snap ack snapd_24724.assert
sudo snap install snapd_24724.snap
sudo snap refresh --hold snapd
```
### Update packages
*APT upgrade to JetPack 6.2.2/Jetson Linux 36.5 [per nVidia docs](https://docs.nvidia.com/jetson/archives/r36.5/DeveloperGuide/SD/SoftwarePackagesAndTheUpdateMechanism.html#updating-to-a-new-minor-release)*

From  CLI:
```shell
sudo nano /etc/apt/sources.list.d/nvidia-l4t-apt-source.list
```
Then change each of the three "r36.4" entries to "r36.5"

Or, from GUI:  
Use "Software & Updates" and click the "Other Software" tab and change each of the three "r36.4" entries to distribution "r36.5":

Once we have made that change, do an update and upgrade:
```shell
sudo apt update && sudo apt upgrade
```
### Grab some basics
```shell
# Tools and such, lets get everything we might need
sudo apt install -y git build-essential cmake pkg-config \
                    libvulkan-dev glslang-tools spirv-tools vulkan-tools libssl-dev\
                    libopenblas-dev python3-venv python3-pip curl \
                    libcurl4-openssl-dev
```
### Build llama.cpp
```shell
cd ~
git clone https://github.com/ggml-org/llama.cpp
cd llama.cpp

# Configure (Release build, use CUDA, should take forever to compile with these flags)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=87 -DGGML_CUDA_FA_ALL_QUANTS=ON \
  -DGGML_CUDA_F16=ON -DGGML_NATIVE=ON -DGGML_LTO=ON -DGGML_CCACHE=OFF \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
# Compile (Jetson has 6 cores; -j6 is sensible)
cmake --build build --config Release -j6
```
## Results (llama-bench)
NVIDIA Jetson Orin Nano Super 8GB (1024 CUDA cores / NV Power Mode: MAXN_SUPER / llama.cpp b9333 built using above flags)

### Tier 1 <= 2GB
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) |
|-------|-------|------|--------|-------------|-------------|---------------|
| TinyLlama 1.1 | Q4_K_M | 636MB | 1.10B | 1,584 | 56.99 | 0.3s |
| OLMo 2 1B | Q4_K_M | 889MB | 1.48B | 1,750 | 51.71 | 0.3s |
| Llama 3.2 1B | Q4_K_M | 763MB | 1.24B | 1,747 | 50.24 | 0.3s |
| Gemma 3 1B | Q4_K_M | 762MB | 1.00B | 2,050 | 44.95 | 0.3s |
| Gemma 3 1B Heretic | Q4_K_M | 762MB | 1.00B | 2,069 | 44.92 | 0.2s |
| SmolLM2 1.7B | Q4_K_M | 1005MB | 1.71B | 1,087 | 37.20 | 0.5s |
| SmolLM3 3B | Q4_K_M | 1.78GB | 3.08B | 766 | 22.63 | 0.7s |
| Llama 3.2 3B | Q4_K_M | 1.87GB | 3.21B | 762 | 21.23 | 0.7s |

### Tier 2 <=4GB
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) |
|-------|-------|------|--------|-------------|-------------|---------------|
| Gemma 4 E2B Heretic (MoE) | Q4_K_M | 3.18GB | 4.65B | 810 | 25.78 | 0.6s |
| Gemma 4 E2B (MoE, think) | Q4_K_M | 3.21GB | 4.65B | 803 | 25.15 | 0.6s |
| Ministral 3B (vision) | Q4_K_M | 1.99GB | 3.43B | 698 | 19.74 | 0.7s |
| Phi-4 Mini | Q4_K_M | 2.31GB | 3.84B | 657 | 18.82 | 0.8s |
| Phi-4 Mini Reasoning (think) | Q4_K_M | 2.31GB | 3.84B | 658 | 18.81 | 0.8s |
| Llama 3.1 8B | UD-Q3_K_XL | 3.90GB | 8.03B | 330 | 9.58 | 1.6s |

### Tier 3 <=8GB
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) |
|-------|-------|------|--------|-------------|-------------|---------------|
| Gemma 4 E4B Heretic (MoE, think) | Q4_K_M | 4.95GB | 7.52B | 466 | 14.26 | 1.1s |
| Gemma 4 E4B (MoE, think) | Q4_K_M | 5.02GB | 7.52B | 384 | 13.99 | 1.3s |
| Gemma 4 E4B (MoE, think) | Q8_0 | 7.46GB | 7.52B | 455 | 12.11 | 1.1s |
| Hermes 3 8B | Q4_K_M | 4.58GB | 8.03B | 346 | 11.49 | 1.5s |
| Llama 3.1 8B | Q4_K_M | 4.58GB | 8.03B | 345 | 11.48 | 1.5s |
| Llama 8B Heretic | Q4_K_M | 4.58GB | 8.03B | 347 | 11.48 | 1.5s |
| Ministral 8B (vision) | Q4_K_M | 4.83GB | 8.49B | 325 | 10.55 | 1.6s |
| Ministral 8B (vision) | UD-Q3_K_XL | 4.12GB | 8.49B | 312 | 8.80 | 1.6s |
| Phi-4 RP (think) | UD-Q2_K_XL | 5.40GB | 14.66B | 142 | 5.51 | 3.6s |
| Ministral 14B (vision) | UD-Q3_K_XL | 6.45GB | 13.51B | 194 | OOM | 2.6s |
| Llama 3.1 8B | Q8_0 | 7.95GB | 8.03B | OOM | OOM | - |
| Ministral 14B (vision) | Q4_K_M | 7.67GB | 13.51B | OOM | OOM | - |
