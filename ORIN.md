# Nvidia Orin Nano Super
Version 0.1-dev<br>
8/26/2025

## Objective
Perform a feasibility study for running chat-oriented LLMs on highly resource-constrained hardware.

I will also use this exercise as a way to test setting up a self-contained LLM server, as I expect much of the setup and configurations will be useful elsewhere.

## Hypothesis
On a Raspberry Pi 5, it is feasible to run Large Language Models (LLMs) with sufficient speed and utility for practical local usage, with performance varying based on model size, model selection, and tuning settings.

I believe the smallest ~1B parameter models are likely to be speedy but less helpful, and ~3-4B parameter models slower but more helpful (potentially optimal for this hardware class?), and with enough time and patience the larger (~7B parameter) models might complete tasks extremely slowly. We may be able to get speedups with tuning settings (CPU tuning? GPU offload?), and carefully selecting models (quant levels particularly). All of this will be explored further here.

## Hardware
The following build with a budget of approximately $300
* [Jetson Orin Nano Super](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/) ($249.00 from Amazon)
* [Crucial P310 500GB NVMe M.2 SSD](https://www.raspberrypi.com/products/ssd-kit/) ($45.99 from Amazon)

## Software
* [Nvidia JetPack]([https://ubuntu.com/download/raspberry-pi](https://developer.nvidia.com/embedded/jetpack-sdk-621)) 6.2.1
  * Jetson Linux 36.4.4, using Linux Kernel 5.15 and an Ubuntu 22.04-based root file system
 
## Setup
> **Under Development**
> 
> This is an incoherent collection of things, I intend to make it like a runbook
> 
* Imaged the nVME using an external USB-NVMe enclosure (Sabrent EC-SNVE) using Rufus.

The boot fails. You need to edit /boot/extlinux/extlinux.conf as follows:

Original (line 10):
```shell
      APPEND ${cbootargs} root=/dev/mmcblk0p1 rw rootwait rootfstype=ext4 mminit_loglevel=4 console=ttyTCU0,115200 firmware_class.path=/etc/firmware fbcon=map:0 video=efifb:off console=tty0 
```

Correct (new line 10, for an NVMe drive in the 2280 slot):
```shell
      APPEND ${cbootargs} root=/dev/nvme0n1p1 rw rootwait rootfstype=ext4 mminit_loglevel=4 console=ttyTCU0,115200 firmware_class.path=/etc/firmware fbcon=map:0 video=efifb:off console=tty0 
```

Once you fix the extlinux.conf file, it will boot and finish setup as normal.
