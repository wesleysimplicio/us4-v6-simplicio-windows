---
sprint: sprint-05
status: done
start: 2026-07-09
end: 2026-07-22
owner: us4-core
---

# Sprint 05 — BitNet + Ternary Adapters (Windows)

## Objetivo
BitNet 1.58-bit (CUDA + AVX) + Ternary (PT-BitNet). MICRO + NANO modes.

## Tasks
- [x] T05.1 — `runtime/backends/cuda/kernels/bitnet_matmul.cu` (packed 1.58-bit)
- [x] T05.2 — `runtime/backends/cpu_avx/bitnet_matmul.cpp` (AVX2 popcount + lookup)
- [x] T05.3 — `runtime/adapters/bitnet/BitNetAdapter` (load packed weights)
- [x] T05.4 — `runtime/adapters/ternary/TernaryAdapter` (PT-BitNet -1/0/+1)
- [x] T05.5 — Ternary LUT (256-entry para 4-ternary chunks, AVX shuffle)
- [x] T05.6 — Loader: BitNet GGUF variant + ternary safetensors
- [x] T05.7 — RuntimeMode MICRO/NANO trigger (RAM<=8GB -> ternary preferido)

## Test plan
- Unit: BitNet CUDA vs scalar reference (atol 5e-3); ternary LUT correctness; AVX popcount path.
- Regression: dense adapters intactos.
- E2E: `us4.exe run --model bitnet-b1.58-2b --mode micro` gera 64 tokens em <=20s em CPU AVX-512.
- Correctness: diff <= 1e-2 vs HF reference.

## DoD
- BitNet + Ternary em MICRO + NANO.
- Coverage >=80% adapters novos.
- ADR-005 BitNet packed format.
