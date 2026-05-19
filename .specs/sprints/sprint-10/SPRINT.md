---
sprint: sprint-10
status: in_progress
start: 2026-09-17
end: 2026-09-30
owner: us4-core
---

# Sprint 10 - Batching + Speculative Decoding (Windows)

## Objetivo
Continuous batching multi-sessao. P-EAGLE / EAGLE-3 speculative. Draft model loader. CUDA Graphs reuse cross-step.

## Tasks
- [x] T10.1 - `runtime/scheduler/ContinuousBatcher`
- [x] T10.2 - `runtime/scheduler/SessionPool` (multi-session + KV namespace)
- [x] T10.3 - `runtime/speculative/PEagleDecoder`
- [x] T10.4 - `runtime/speculative/Eagle3Decoder`
- [x] T10.5 - Draft model loader (small companion)
- [x] T10.6 - CUDA Graphs reuse entre steps speculative
- [x] T10.7 - Acceptance rate telemetry

## Test plan
- Unit: batcher fairness; speculative accept/reject; session isolation.
- Regression: single-session intacto.
- E2E: 4 sessoes paralelas em RTX 4090 sem leak; speculative >=1.8x speedup.
- Correctness: tokens identicos a non-speculative.

## DoD
- Batching + speculative em FULL/BALANCED.
- Coverage >=80% em `runtime/{scheduler,speculative}`.
- ADR-008 Speculative decoding choice.
