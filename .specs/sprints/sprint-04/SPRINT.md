---
sprint: sprint-04
status: in_progress
start: 2026-06-25
end: 2026-07-08
owner: us4-core
---

# Sprint 04 — AVX Hot Paths + oneDNN (Windows)

## Objetivo
Caminhos quentes CPU: AVX2, AVX-512, AMX (Intel BF16/INT8). oneDNN para block GEMM. Dequant INT8/INT4.

## Tasks
- [ ] T04.1 — `runtime/backends/cpu_avx/avx2_matmul.cpp` + `avx512_matmul.cpp`
- [ ] T04.2 — `runtime/backends/cpu_avx/amx_bf16_int8.cpp` (Intel Sapphire Rapids+)
- [ ] T04.3 — `runtime/backends/cpu_avx/avx_attention.cpp` (fused softmax-rescale)
- [ ] T04.4 — `runtime/backends/cpu_avx/dequant_{int8,int4}.cpp` (group-wise scales)
- [x] T04.5 — `runtime/backends/onednn/OneDnnBackend` (block GEMM via primitives)
- [x] T04.6 — Auto-select AVX level via cpuid (AVX2 / AVX-512 / AMX)
- [ ] T04.7 — Re-bench Qwen/Gemma CPU AVX vs scalar

## Test plan
- Unit: AVX matmul vs scalar (atol 1e-3); AMX path vs AVX-512 fallback; oneDNN block GEMM correctness.
- Regression: scalar path + GPU paths verdes.
- E2E: `us4.exe run --backend cpu` em CPU AVX-512 >= 5x speedup vs scalar.
- Correctness: diff AVX vs scalar <= 1e-3.

## DoD
- AVX path em DEGRADED + ULTRA_LOW + CPU_ONLY modes.
- Coverage >=80% em `runtime/backends/{cpu_avx,onednn}`.
