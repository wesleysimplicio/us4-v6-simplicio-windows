---
sprint: sprint-11
status: in_progress
start: 2026-10-01
end: 2026-10-14
owner: us4-core
---

# Sprint 11 — Vulkan + Windows ML/NPU (Windows)

## Objetivo
Vulkan compute backend (cross-vendor AMD/Intel + fallback NVIDIA). Windows ML adapter para NPU (Snapdragon X / Intel Core Ultra / AMD Ryzen AI). Dense layer offload pro NPU.

## Tasks
- [x] T11.1 — `runtime/backends/vulkan/VulkanContext` (device, queue, descriptor sets)
- [x] T11.2 — `runtime/backends/vulkan/kernels/{matmul,softmax,rmsnorm}.comp` (GLSL)
- [x] T11.3 — `runtime/backends/windows_ml/WinMlAdapter` (ONNX graph via Windows ML, NPU exec provider)
- [x] T11.4 — `runtime/backends/windows_ml/LayerOffloader` (dense layers -> NPU)
- [x] T11.5 — Mixed dispatch (GPU primary + NPU dense static)
- [x] T11.6 — Power/thermal monitor (Windows ETW + `GetSystemPowerStatus`) downgrade dispatch
- [ ] T11.7 — Bench Qwen/Llama em Vulkan; em NPU offload
- [x] T11.8 — Fallback graceful sem NPU

## Test plan
- Unit: Vulkan matmul vs scalar; WinML graph compile; thermal monitor read.
- Regression: CUDA/DirectML/AVX paths intactos.
- E2E: Vulkan em Radeon RX 7900 gera 32 tokens em <=15s; NPU em Snapdragon X Elite oferece >=1.3x speedup vs CPU para dense layers.
- Correctness: Vulkan vs CPU <= 5e-3; NPU vs CPU <= 5e-3.

## DoD
- Vulkan habilitado como fallback cross-vendor.
- NPU opt-in em FULL/BALANCED quando disponivel.
- Coverage >=80% em `runtime/backends/{vulkan,windows_ml}`.
- ADR-009 NPU offload heuristic; ADR-010 Vulkan vs DirectML choice.

## Riscos
- WinML NPU EP coverage limitado para alguns shapes -> graceful fallback.
