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
sudo apt update
sudo apt upgrade
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

# Configure (Release build, use OpenBLAS)
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=87 \
  -DGGML_CUDA_F16=ON -DGGML_CUDA_FORCE_MMQ=ON \
  -DGGML_NATIVE=ON -DGGML_LTO=ON -DGGML_CCACHE=OFF \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc
# Compile (Jetson has 6 cores; -j6 is sensible)
cmake --build build -j6
```
