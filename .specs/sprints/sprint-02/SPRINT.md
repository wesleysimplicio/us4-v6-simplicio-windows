---
sprint: sprint-02
status: todo
start: TBD
end: TBD
owner: us4-core
---

# Sprint 02 — CPU Scalar Baseline (Windows)

## Objetivo
Tensor primitives + baseline escalar (matmul, attention) em C++ puro. Adapter dense base com Qwen + Gemma em CPU_ONLY mode.

## Tasks
- [ ] T02.1 — `runtime/core/Tensor.{h,cpp}` (shape, dtype FP32/FP16/BF16/INT8/INT4)
- [ ] T02.2 — `runtime/backends/cpu_avx/scalar_matmul.cpp` (reference scalar GEMM, FP32)
- [ ] T02.3 — `runtime/backends/cpu_avx/scalar_attention.cpp` (causal mask, softmax stable, KV concat)
- [ ] T02.4 — `runtime/adapters/DenseAdapterBase`
- [ ] T02.5 — `runtime/adapters/qwen/QwenAdapter` (scalar path)
- [ ] T02.6 — `runtime/adapters/gemma/GemmaAdapter` (scalar path)
- [ ] T02.7 — Loader GGUF/Safetensors minimal (FP32/FP16)
- [ ] T02.8 — Benchmark harness `runtime/benchmarks/dense_baseline.cpp`
- [ ] T02.9 — Runtime mode CPU_ONLY funcional

## Test plan
- Unit: matmul vs naive reference (atol 1e-4); attention shape/mask; tokenizer round-trip.
- Regression: Sprint 01 probe/selector verde.
- E2E: `us4.exe run --model qwen-0.5b --backend cpu --prompt "hi"` gera >= 5 tokens em <=120s.
- Correctness: logit diff vs reference <= 1e-3 nos primeiros 32 tokens.

## DoD
- 2 adapters em CPU scalar.
- Coverage >=80% em `runtime/backends/cpu_avx` + adapters.
