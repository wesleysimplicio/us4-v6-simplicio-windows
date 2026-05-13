# Personas — US4 V6 Windows Edition

## P1 — ML Engineer NVIDIA desktop (primary)
- Roda Llama/DeepSeek/Qwen em RTX 4090 / 5090 / Pro.
- Quer CUDA Graphs + speculative + continuous batching prontos sem hackear.
- Dores: llama.cpp em Windows tem CMake friction; Ollama esconde tuning; vLLM nao roda Windows.
- Ganha com US4: 1 binary, CUDA + speculative + batching out-of-box, JSON output.

## P2 — ML Engineer AMD/Intel (secondary, urgent)
- Roda em Radeon RX 7900 XTX / Intel Arc A770 / iGPU em laptop Intel/AMD.
- Quer DirectML ou Vulkan funcionando sem reinstalar drivers.
- Dores: maioria das tools so cobre NVIDIA bem; AMD/Intel sao cidadaos de segunda classe.
- Ganha com US4: DirectML primario, Vulkan fallback, mesmas features (KV tiering, MoE, speculative).

## P3 — Researcher Workstation
- Workstation com 2x RTX 4090 + 256GB RAM + 4TB NVMe.
- Compara arquiteturas dense/MoE/ternary, precisa correctness diff vs HF.
- Dores: bench cross-arch leva dias; cada tool reporta metrica diferente.
- Ganha com US4: matriz unificada, correctness gates, telemetria exportavel.

## P4 — Laptop user (NPU + low VRAM)
- Snapdragon X Elite / Intel Core Ultra / AMD Ryzen AI, 16-32GB RAM, iGPU/NPU.
- Quer LLM local sem cozinhar laptop; bateria importa.
- Dores: nenhum runtime usa NPU bem; rodar em iGPU drena bateria.
- Ganha com US4: NPU offload dense, modes MICRO/NANO, thermal-aware downgrade.

## P5 — App Developer Windows-native (.NET/C++/Win32/UWP)
- Constroi app Windows com inferencia embutida.
- Precisa de DLL estavel (`us4-v6.dll`), MSIX installer, codigo nativo (P/Invoke OK).
- Dores: distribuir runtime LLM em app store Windows e dor; Python embarcado quebra.
- Ganha com US4: signed DLL, MSIX, sem deps Python, latencia previsivel.

## P6 — Sysadmin / DevOps (corp fleet)
- Gerencia fleet Windows (Intune/SCCM) rodando inferencia local (compliance).
- Precisa deploy automatizado, telemetria centralizada (Prometheus/OTLP), update.
- Dores: WSL + CUDA em fleet e impraticavel; Ollama nao tem MSIX.
- Ganha com US4: MSIX assinado, telemetria estruturada, sem deps WSL.
