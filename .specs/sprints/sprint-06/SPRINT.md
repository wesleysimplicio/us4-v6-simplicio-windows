---
sprint: sprint-06
status: in_progress
start: 2026-07-23
end: 2026-08-05
owner: us4-core
---

# Sprint 06 — KV Memory Architecture (Windows)

## Objetivo
Pager hot-cold KV (VRAM hot, RAM warm, SSD cold via mmap). Prefix cache. KV summarization. Eviction policies.

## Tasks
- [x] T06.1 — `runtime/kv/KvPager` (page table, LRU, tiers VRAM/RAM/SSD)
- [x] T06.2 — `runtime/kv/PrefixCache` (shared prefix by hash)
- [ ] T06.3 — `runtime/kv/SsdColdStore` (mmap Windows `CreateFileMapping` + async flush)
- [x] T06.4 — `runtime/kv/Summarizer` (compress old tokens)
- [x] T06.5 — Eviction policy: LRU + frequency hybrid, cost-aware (VRAM mais caro)
- [ ] T06.6 — Adapter hooks: append, lookup, evict, summarize
- [ ] T06.7 — Telemetry: hit-rate VRAM/RAM/SSD/summary

## Test plan
- Unit: pager hit/miss; prefix dedup; SSD round-trip; eviction priority.
- Regression: adapters intactos.
- E2E: prompt 8k tokens com prefix repetido -> hit-rate VRAM >= 80%.
- Correctness: KV restored igual ao mantido em VRAM.

## DoD
- KV tiering por mode (FULL=VRAM, BALANCED=VRAM+RAM, DEGRADED+=SSD, MICRO+=summary).
- Coverage >=80% em `runtime/kv`.
- ADR-006 KV architecture.
