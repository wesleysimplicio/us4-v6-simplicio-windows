---
sprint: sprint-03
status: in_progress
start: 2026-06-11
end: 2026-06-24
owner: us4-core
---

# Sprint 03 — CUDA + DirectML Skeleton (Windows)

## Objetivo
Backends GPU principais. CUDA: streams + memory pool + CUDA Graphs + GEMM kernels. DirectML: device init + graph compile + dispatch.

## Tasks
- [ ] T03.1 — `runtime/backends/cuda/CudaContext` (device, streams, memory pool, CUDA Graphs)
- [ ] T03.2 — `runtime/backends/cuda/kernels/{matmul,softmax,rmsnorm}.cu` (FP16/BF16)
- [ ] T03.3 — cuBLAS/cuBLASLt fallback wrapper
- [x] T03.4 — `runtime/backends/directml/DmlDevice` (D3D12, command queue)
- [x] T03.5 — `runtime/backends/directml/DmlGraph` (compile + dispatch, FP16/BF16)
- [x] T03.6 — Vendor selector: NVIDIA->CUDA, AMD/Intel->DirectML
- [ ] T03.7 — Qwen + Gemma adapters: CUDA + DirectML paths

## Test plan
- Unit: CUDA matmul vs scalar (atol 1e-2 FP16); DirectML graph compile vs reference; CUDA Graphs reuse.
- Regression: CPU scalar paths intactos.
- E2E: `us4.exe run --backend cuda` gera 32 tokens em <=5s em RTX 4090; `--backend directml` gera 32 tokens em <=15s em Radeon RX 7900.
- Correctness: diff CUDA vs CPU <= 5e-3; DirectML vs CPU <= 5e-3.

## DoD
- CUDA + DirectML em FULL/BALANCED.
- Coverage >=80% em ambos backends.
- ADR-004 GPU backend strategy.

## Riscos
- DirectML graph compile pode falhar em modelos grandes -> chunkar.
