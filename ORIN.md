# AI on the Nvidia Jetson Orin Nano Super
Version 0.4.1<br>
6/4/2026

## Objective
Perform a feasibility study for running chat-oriented LLMs on highly resource-constrained hardware.

I will also use this exercise as a way to test setting up a self-contained LLM server, as I expect much of the setup and configurations will be useful elsewhere.

## Hypothesis
I expect that the Orin Nano will be a much more capable device than the Raspi series when it comes to running and using LLMs. This is just some ramblings for now.

## Hardware
The following build with a budget of approximately $300
* [Nvidia Jetson Orin Nano Super](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/) (8GB) ($249 from Amazon)
  * Ampere GPU w/ 1024 CUDA cores, 32 tensor cores; 6-core ARM Cortex-A78AE v8.2 64-bit CPU; 8GB LPDDR5
* [Crucial P310 500GB NVMe M.2 SSD](https://www.crucial.com/ssd/p310/ct500p310ssd8) ($46 from Amazon, as of 8/2025)

As of 8/2025, the Orin Nano Super comes with firmware 36.4.3-gcid-38968081. It has "MAXN_SUPER" mode available out of the box and is compatible with JetPack 6.2 without needing to update the firmware.

Upon first boot to desktop with JetPack 6.2.1, it will update you to firmware 36.4.4 and need a couple of reboots.

## Software
* [Nvidia JetPack](https://developer.nvidia.com/embedded/jetpack/downloads) 7.2
 
## Setup
> **Under Development**
> 
> This is an incoherent collection of things, I intend to make it like a runbook
> 
* You MUST install [JetPack 6.2.2](https://developer.nvidia.com/embedded/jetpack-sdk-622) first to get on firmware 36.x or later, or you will not be able to use the 25W or MAXN_SUPER power profiles. You MUST check the firmware version before proceeding to install 7.2. Use an SD card to install and update to 6.2.2 first, as much as a waste of time as it seems.
* Once on firmware 36.x or later (i.e. 36.5.0), pop out the microSD and proceed further.
* Installed fresh, empty nVME drive into Orin Nano.
* Downloaded JetPack 7.2 and used balenaEtcher (alternatively, can use Rufus 4.13 to image a USB flash drive, partition scheme "GPT" and after hitting "Start" select "DD mode").
* JetPack 7.2 will prompt you to update to 39.2.0, do so. There will be a couple of reboots. Then finish installing Ubuntu.

### Update packages
```shell
sudo apt update && sudo apt upgrade
```
### Grab some basics
```shell
# Tools and such, lets get everything we might need
sudo apt install -y git build-essential cmake pkg-config \
                    libvulkan-dev glslang-tools spirv-tools vulkan-tools libssl-dev\
                    libopenblas-dev python3-venv python3-pip curl \
                    libcurl4-openssl-dev nvidia-jetpack
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
### Headless (free up some RAM, login via SSH)
```shell
sudo systemctl set-default multi-user.target && sudo reboot
```
### Run llama.cpp’s OpenAI-compatible server and WebUI
```shell
~/llama.cpp/build/bin/llama-server \
  -m ~/models/gemma-4-e2b/Q4_K_M/google_gemma-4-E2B-it-Q4_K_M.gguf \
  -t 6 -ngl 99 -fa 1 -c 8192 -np 1 \
  --host 0.0.0.0 --port 8081 --jinja \
  --api-key YOUR_RANDOM_KEY # REPLACE "YOUR_RANDOM_KEY" WITH AN ACTUAL RANDOM STRING!!!
```
[Quick and easy random API key strings from random.org](https://www.random.org/strings/?num=1&len=24&digits=on&loweralpha=on&upperalpha=on&format=plain)

## Results (llama-bench)
NVIDIA Jetson Orin Nano Super 8GB (1024 CUDA cores / NV Power Mode: MAXN_SUPER / llama.cpp b9333 built using above flags / Flash Attn: ON)<br>
5/27/2026
```shell
llama-bench -m MODEL -p 512 -n 128 -ngl 99 -fa 1 -t 6 -r 3
```
### Tier 1 <=2GB
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) | Notes |
|-------|-------|------|--------|-------------|-------------|---------------|-------|
| [TinyLlama 1.1B Chat](https://huggingface.co/mradermacher/TinyLlama-1.1B-Chat-v1.0-i1-GGUF) | Q4_K_M | 668MB | 1.10B | 2,223 | 66.69 | 0.2s |  |
| [Gemma 3 1B IT](https://huggingface.co/bartowski/google_gemma-3-1b-it-GGUF) | Q4_K_M | 762MB | 1.00B | 2,300 | 46.18 | 0.2s |  |
| [Llama 3.2 1B IT](https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF) | Q4_K_M | 763MB | 1.24B | 2,293 | 55.42 | 0.2s |  |
| [Gemma 3 1B IT Heretic](https://huggingface.co/mradermacher/gemma-3-1b-it-heretic-extreme-uncensored-abliterated-i1-GGUF) | Q4_K_M | 806MB | 1.00B | 2,318 | 46.23 | 0.2s |  |
| [OLMo 2 1B IT](https://huggingface.co/mradermacher/OLMo-2-0425-1B-Instruct-i1-GGUF) | Q4_K_M | 889MB | 1.48B | 1,995 | 55.20 | 0.3s |  |
| [SmolLM2 1.7B IT](https://huggingface.co/bartowski/SmolLM2-1.7B-Instruct-GGUF) | Q4_K_M | 1005MB | 1.71B | 1,355 | 41.67 | 0.4s |  |
### Tier 2 <=4GB
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) | Notes |
|-------|-------|------|--------|-------------|-------------|---------------|-------|
| [SmolLM3 3B](https://huggingface.co/bartowski/HuggingFaceTB_SmolLM3-3B-GGUF) | Q4_K_M | 1.78GB | 3.08B | 892 | 24.24 | 0.6s |  |
| [Llama 3.2 3B IT](https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF) | Q4_K_M | 1.87GB | 3.21B | 893 | 22.88 | 0.6s |  |
| [Ministral 3 3B IT](https://huggingface.co/bartowski/mistralai_Ministral-3-3B-Instruct-2512-GGUF) (vision) | Q4_K_M | 1.99GB | 3.43B | 841 | 21.66 | 0.6s |  |
| [Phi-4 Mini Instruct](https://huggingface.co/bartowski/microsoft_Phi-4-mini-instruct-GGUF) | Q4_K_M | 2.31GB | 3.84B | 778 | 20.29 | 0.7s |  |
| [Phi-4 Mini Reasoning](https://huggingface.co/bartowski/microsoft_Phi-4-mini-reasoning-GGUF) (think) | Q4_K_M | 2.31GB | 3.84B | 775 | 20.29 | 0.7s |  |
| [Gemma 4 E2B IT Heretic](https://huggingface.co/mradermacher/gemma-4-E2B-it-heretic-ara-GGUF) (MoE, think) | Q4_K_M | 3.18GB | 4.65B | 815 | 27.65 | 0.6s |  |
| [Gemma 4 E2B IT](https://huggingface.co/bartowski/google_gemma-4-E2B-it-GGUF) (MoE, think) | Q4_K_M | 3.21GB | 4.65B | 814 | 26.85 | 0.6s |  |
### Tier 3 <=8GB
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) | Notes |
|-------|-------|------|--------|-------------|-------------|---------------|-------|
| [Llama 3.1 8B IT](https://huggingface.co/unsloth/Llama-3.1-8B-Instruct-GGUF) | UD-Q3_K_XL | 3.90GB | 8.03B | 370 | 10.04 | 1.4s |  |
| [Ministral 3 8B IT](https://huggingface.co/unsloth/Ministral-3-8B-Instruct-2512-GGUF) (vision) | UD-Q3_K_XL | 4.12GB | 8.49B | 345 | 9.25 | 1.5s |  |
| [Hermes 3 8B](https://huggingface.co/bartowski/Hermes-3-Llama-3.1-8B-GGUF) | Q4_K_M | 4.58GB | 8.03B | 382 | 12.21 | 1.3s |  |
| [Llama 3.1 8B IT](https://huggingface.co/bartowski/Meta-Llama-3.1-8B-Instruct-GGUF) | Q4_K_M | 4.58GB | 8.03B | 387 | 12.21 | 1.3s |  |
| [Llama 3.1 8B IT Heretic](https://huggingface.co/bartowski/p-e-w_Llama-3.1-8B-Instruct-heretic-GGUF) | Q4_K_M | 4.58GB | 8.03B | 387 | 12.24 | 1.3s |  |
| [Ministral 3 8B IT](https://huggingface.co/bartowski/mistralai_Ministral-3-8B-Instruct-2512-GGUF) (vision) | Q4_K_M | 4.83GB | 8.49B | 362 | 11.30 | 1.4s |  |
| [Gemma 4 E4B IT Heretic](https://huggingface.co/llmfan46/gemma-4-E4B-it-ultra-uncensored-heretic-GGUF) (MoE, think) | Q4_K_M | 4.95GB | 7.52B | 498 | 14.86 | 1.0s |  |
| [Gemma 4 E4B IT](https://huggingface.co/bartowski/google_gemma-4-E4B-it-GGUF) (MoE, think) | Q4_K_M | 5.02GB | 7.52B | 464 | 14.62 | 1.1s |  |
| [Phi-4 Reasoning Plus](https://huggingface.co/unsloth/Phi-4-reasoning-plus-GGUF) (think) | UD-Q2_K_XL | 5.40GB | 14.66B | 151 | 5.74 | 3.4s |  |
### Tier 4 <=16GB (tested to see what works)
| Model | Quant | Size | Params | pp512 (t/s) | tg128 (t/s) | TTFT (512tok) | Notes |
|-------|-------|------|--------|-------------|-------------|---------------|-------|
| [Ministral 3 14B IT](https://huggingface.co/unsloth/Ministral-3-14B-Instruct-2512-GGUF) (vision) | UD-Q3_K_XL | 6.45GB | 13.51B | OOM | OOM | - |  |
| [Gemma 4 E4B IT](https://huggingface.co/bartowski/google_gemma-4-E4B-it-GGUF) (MoE, think) | Q8_0 | 7.46GB | 7.52B | 482 | 12.54 | 1.1s |  |
| [Ministral 3 14B IT](https://huggingface.co/bartowski/mistralai_Ministral-3-14B-Instruct-2512-GGUF) (vision) | Q4_K_M | 7.67GB | 13.51B | OOM | OOM | - |  |
| [Llama 3.1 8B IT](https://huggingface.co/bartowski/Meta-Llama-3.1-8B-Instruct-GGUF) | Q8_0 | 7.95GB | 8.03B | OOM | OOM | - |  |

*MoE = mixture of experts · think = thinking/reasoning · vision = image input capable*
