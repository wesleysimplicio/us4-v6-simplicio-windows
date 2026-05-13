---
sprint: sprint-09
status: todo
start: 2026-09-03
end: 2026-09-16
owner: us4-core
---

# Sprint 09 — MoE Advanced (Windows)

## Objetivo
MiniMax + GLM. SP-MoE prefetch. Sparsity-aware cache. Multimodal cache.

## Tasks
- [ ] T09.1 — `runtime/adapters/minimax/MiniMaxAdapter`
- [ ] T09.2 — `runtime/adapters/glm/GLMAdapter`
- [ ] T09.3 — `runtime/moe/SpeculativePrefetch` (predict next experts + preload)
- [ ] T09.4 — `runtime/cache/SparsityAwareCache`
- [ ] T09.5 — `runtime/cache/MultimodalCache` (image/audio tokens)
- [ ] T09.6 — Telemetry: prefetch hit ratio, sparsity hit ratio

## Test plan
- Unit: speculative prefetch correctness; sparsity hit/miss.
- Regression: DeepSeek/Kimi + dense verdes.
- E2E: MiniMax M2 multimodal em <=30s em RTX 4090.
- Correctness: diff <= 1e-3.

## DoD
- 4 MoE adapters funcionando.
- Coverage >=80% em `runtime/cache` + adapters novos.
- SP-MoE reduz latencia per-token >=20%.
