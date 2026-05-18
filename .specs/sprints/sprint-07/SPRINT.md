---
sprint: sprint-07
status: in_progress
start: 2026-08-06
end: 2026-08-19
owner: us4-core
---

# Sprint 07 — Llama Adapter (Windows)

## Objetivo
Llama 3/4 adapter com GQA, RoPE scaling (linear/dynamic/YaRN), ALiBi opcional. Full attention em todos backends.

## Tasks
- [x] T07.1 — `runtime/adapters/llama/LlamaConfig`
- [ ] T07.2 — `runtime/adapters/llama/LlamaAdapter`
- [x] T07.3 — `runtime/core/rope.{h,cpp}` (linear + dynamic + YaRN)
- [x] T07.4 — `runtime/core/gqa_attention.{h,cpp}`
- [ ] T07.5 — Loader Llama GGUF + safetensors + tokenizer.json
- [ ] T07.6 — Bench Llama 3.x 8B em CUDA + DirectML + AVX

## Test plan
- Unit: RoPE vs Python reference (atol 1e-5); GQA shape.
- Regression: outros adapters verdes.
- E2E: Llama 3 8B Q4 em RTX 4090 gera 200 tokens em <=10s; em CPU AVX-512 em <=120s.
- Correctness: diff <= 1e-3.

## DoD
- Llama 3+4 em FULL/BALANCED/DEGRADED.
- Coverage >=80% em `runtime/adapters/llama`.
