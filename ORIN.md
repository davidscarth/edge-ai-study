# Nvidia Orin Nano Super
Version 0.1-dev<br>
8/26/2025

## Objective
Perform a feasibility study for running chat-oriented LLMs on highly resource-constrained hardware.

I will also use this exercise as a way to test setting up a self-contained LLM server, as I expect much of the setup and configurations will be useful elsewhere.

## Hypothesis
I expect that the Orin Nano will be a much more capable device than the Raspi series when it comes to running and using LLMs. This is just some ramblings for now.

## Hardware
The following build with a budget of approximately $300
* [Jetson Orin Nano Super](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/) ($249.00 from Amazon)
* [Crucial P310 500GB NVMe M.2 SSD](https://www.raspberrypi.com/products/ssd-kit/) ($45.99 from Amazon)

As of 8/2025, the 

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

Once you fix the extlinux.conf file, it will boot and finish setup as normal, no need for any SD card.
