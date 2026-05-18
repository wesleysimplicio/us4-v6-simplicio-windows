---
sprint: sprint-08
status: done
start: 2026-08-20
end: 2026-09-02
owner: us4-core
---

# Sprint 08 — MoE Foundation (Windows)

## Objetivo
DeepSeek + Kimi MoE. Expert pager VRAM->RAM->SSD. Top-k routing. Cold expert offload.

## Tasks
- [x] T08.1 — `runtime/moe/Router` (top-k softmax, load balance loss)
- [x] T08.2 — `runtime/moe/ExpertPager` (VRAM resident hot, RAM warm, SSD cold)
- [x] T08.3 — `runtime/adapters/deepseek/DeepSeekMoEAdapter` (shared + routed experts)
- [x] T08.4 — `runtime/adapters/kimi/KimiMoEAdapter`
- [x] T08.5 — Loader MoE: lazy load expert por demanda (mmap shards)
- [x] T08.6 — Telemetry: expert hit-rate, eviction count, router entropy

## Test plan
- Unit: router correctness; pager evict + re-load; load balance.
- Regression: dense + Llama verdes.
- E2E: DeepSeek V2/V3 Q4 em 2x RTX 4090 (NVLink) gera 100 tokens em <=15s.
- Correctness: diff <= 1e-3.

## DoD
- DeepSeek + Kimi em FULL/BALANCED.
- Coverage >=80% em `runtime/moe` + adapters MoE.
- ADR-007 MoE expert paging strategy.

