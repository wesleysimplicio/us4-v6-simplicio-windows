# Vision — US4 V6 Windows Edition

## Problema
Em Windows x86-64, rodar LLMs locais (DeepSeek/Kimi/MiniMax/GLM/Llama/Qwen/Gemma/BitNet/Ternary) significa lidar com 4 ecossistemas diferentes: NVIDIA (CUDA), AMD/Intel (DirectML), cross-vendor (Vulkan) e CPU (AVX/AMX). Ferramentas atuais (llama.cpp, KoboldCpp, Ollama Windows) sao single-backend ou exigem tunelamento manual por GPU. NPUs novos (Snapdragon X, Intel Core Ultra, AMD Ryzen AI) sao ignorados por quase todos.

## Quem usa
- Engenheiros ML em desktop NVIDIA / AMD / Intel + laptops com NPU.
- Pesquisadores em workstations Windows (single GPU + multi GPU).
- Devs construindo apps Windows-native com inferencia embutida.
- Sysadmins gerenciando fleet corporativo Windows (Intune/SCCM deploy).

## Diferencial
- **1 runtime, 9 adapters, 6 backends** — CUDA + DirectML + Vulkan + oneDNN + AVX/AMX + Windows ML/NPU.
- **Backend selector automatico** — NVIDIA->CUDA, AMD/Intel->DirectML, fallback Vulkan, CPU sempre disponivel, NPU opt-in.
- **Hot-cold KV tiering VRAM/RAM/SSD** — mmap Windows nativo, async flush.
- **SP-MoE prefetch** — reduz latencia per-token >=20%.
- **Speculative decoding P-EAGLE/EAGLE-3** — >=1.8x speedup.
- **CUDA Graphs reuse** — mantem grafo capturado entre steps speculative.
- **NPU offload dense layers** — libera GPU para hot path em laptops com Snapdragon X / Core Ultra / Ryzen AI.
- **Single signed binary x64 + MSIX installer** — sem Python, sem WSL.

## Metricas (post-v1.0)
- Latencia per-token vs llama.cpp em mesma quantizacao: >=1.5x mais rapido em CUDA; >=1.2x em DirectML.
- Hit-rate hot KV: >=80% em prompts longos (8k+).
- Cobertura testes: >=80% touched; correctness diff <= 1e-3 dense, <= 1e-2 BitNet.
- 8 hardware profiles suportados (desktop NV high/mid/low, AMD high/mid, Intel iGPU, laptop NPU, CPU_ONLY).
- 9 adapters funcionando em pelo menos 1 backend cada.

## Nao-objetivos
- Treinamento / fine-tuning (so inferencia).
- Cloud / distributed inference (single-machine focus).
- Suporte Apple Silicon (ver Apple Edition).
- Linux / macOS.
- GUI desktop app.

## Tese longo prazo
Mesma tese da Apple Edition aplicada a Windows: runtime universal multi-adapter + multi-backend + auto-tune cria commodity de inferencia local-first. US4 V6 Windows e o pilar x86 + NPU.
