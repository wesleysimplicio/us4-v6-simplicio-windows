# TIMELINE — `US4 V6 Windows Edition`

> Consolidated 12-sprint roadmap, dependency graph, critical path, and release milestones for `us4-v6-simplicio-windows`. Authoritative source for sprint scheduling; per-sprint detail lives in `sprint-XX/SPRINT.md`.
> Current checkbox-based progress snapshot is generated into `STATUS.md` by `scripts/render-planning-status.ps1`.

- **Roadmap window**: 2026-05-14 → 2026-10-28 (24 weeks, ~5.5 months)
- **Cadence**: 2 weeks per sprint, 12 sprints
- **v1.0 GA**: end of Sprint 12 (2026-10-28)
- **Owner**: `us4-core`

---

## 1. Sprint window (cronograma)

| Sprint | Janela | Tema | Marco externo |
|---|---|---|---|
| S01 | 2026-05-14 → 2026-05-27 | Foundations & Skeleton | v0.1.0-alpha (internal) |
| S02 | 2026-05-28 → 2026-06-10 | CPU Scalar Baseline (Qwen + Gemma) | v0.2.0-alpha |
| S03 | 2026-06-11 → 2026-06-24 | CUDA + DirectML Skeleton | v0.3.0-alpha |
| S04 | 2026-06-25 → 2026-07-08 | AVX Hot Paths + oneDNN | v0.4.0-alpha |
| S05 | 2026-07-09 → 2026-07-22 | BitNet + Ternary | v0.5.0-alpha |
| S06 | 2026-07-23 → 2026-08-05 | KV Memory Architecture | v0.6.0-alpha |
| S07 | 2026-08-06 → 2026-08-19 | Llama Adapter | **v0.7.0-alpha (public)** |
| S08 | 2026-08-20 → 2026-09-02 | MoE Foundation (DeepSeek + Kimi) | v0.8.0-beta |
| S09 | 2026-09-03 → 2026-09-16 | MoE Advanced (MiniMax + GLM, SP-MoE) | v0.9.0-beta |
| S10 | 2026-09-17 → 2026-09-30 | Batching + Speculative (EAGLE-3) | **v0.10.0-beta (public)** |
| S11 | 2026-10-01 → 2026-10-14 | Vulkan + Windows ML / NPU | v0.11.0-rc.1 |
| S12 | 2026-10-15 → 2026-10-28 | Auto-Tune + Release | **v1.0.0 GA** |

---

## 2. Gantt (text)

```
                    May ────────── Jun ────────── Jul ────────── Aug ────────── Sep ────────── Oct
S01 Foundations     [██]
S02 CPU Scalar          [██]
S03 CUDA + DML               [██]
S04 AVX + oneDNN                  [██]
S05 BitNet                             [██]
S06 KV Memory                               [██]
S07 Llama                                        [██]                         <-- v0.7.0-alpha
S08 MoE Foundation                                    [██]
S09 MoE Advanced                                           [██]
S10 Batch+Spec                                                  [██]          <-- v0.10.0-beta
S11 Vulkan + NPU                                                     [██]
S12 Auto-Tune+GA                                                          [██] <-- v1.0.0 GA
```

Each `[██]` = 2-week sprint window.

---

## 3. Dependency graph (DAG)

```
                       ┌──────── S04 AVX + oneDNN
                       │
S01 ──> S02 ──> S03 ───┼──> S06 KV ──> S08 MoE-F ──> S09 MoE-A ──> S10 Batch+Spec ──> S12 Release
                       │                                                          ▲
                       └──> S05 BitNet ────────────> S07 Llama ───────────────────┘
                                                              S11 Vulkan+NPU ────┘
```

Hard dependencies (cannot start before predecessor closes):

- S02 ⇐ S01 (RuntimeContext skeleton + CMake + MSVC/Clang toolchain)
- S03 ⇐ S02 (Tensor + adapter trait stable; CUDA + DirectML in parallel inside the sprint)
- S04 ⇐ S03 (CPU AVX path as fallback to GPU backends, oneDNN block GEMM contract)
- S05 ⇐ S03 (needs tensor dequant API surface)
- S06 ⇐ S03 (CUDA memory pool + DML resource heap first)
- S07 ⇐ S05, S06 (Llama uses KV cache + quant adapters)
- S08 ⇐ S06 (MoE expert routing assumes KV partition)
- S09 ⇐ S08 (SP-MoE on top of basic MoE)
- S10 ⇐ S09 (speculative needs batched + MoE-aware)
- S11 ⇐ S07 (Vulkan + Windows ML/NPU adapter contract stabilized via Llama path)
- S12 ⇐ S10, S11 (auto-tune covers all backends including NPU)

---

## 4. Critical path

```
S01 → S02 → S03 → S06 → S08 → S09 → S10 → S12
```

- **8 sprints on critical path = 16 weeks of zero-slack work**
- Any slip on S01/S02/S03/S06/S08/S09/S10/S12 pushes v1.0 GA day-for-day.
- **Off-critical (1-week slack each)**: S04 (AVX + oneDNN), S05 (BitNet), S07 (Llama), S11 (Vulkan + NPU).
- Slack does NOT mean optional: each off-critical sprint feeds quality/coverage required for DoD of later sprints.

---

## 5. Buffer & risk

- **No explicit buffer sprint.** Cadence assumes 1 of S04/S05/S07/S11 can absorb a 1-week slip from the critical path.
- **High-risk sprints** (highest chance of slip):
  - S03 — CUDA + DirectML in parallel: two vendor SDKs, dual graph-compile path, kernel correctness across NVIDIA + AMD + Intel.
  - S08 — MoE foundation: routing correctness across DeepSeek + Kimi simultaneously, dual backend (CUDA + DML).
  - S10 — Speculative decoding: EAGLE-3 correctness vs perf trade-off, CUDA Graphs interaction.
  - S11 — Vulkan + Windows ML: Vulkan compute portability across GPU vendors, NPU access on Snapdragon X / Core Ultra / Ryzen AI requires CI hardware availability.
- **Mitigation policy**: if a critical-path sprint slips >3 working days, freeze off-critical work and pull engineers onto the blocker. Document via ADR.
- **Correctness regression** at any sprint boundary blocks the next sprint from starting (logit-diff gate per backend: CUDA / DML / Vulkan / AVX / NPU). See `RELEASE.md` §1.

---

## 6. External milestones

| Date | Milestone | Audience |
|---|---|---|
| 2026-05-27 | v0.1.0-alpha (internal) | `us4-core` only |
| 2026-08-19 | **v0.7.0-alpha (public winget + portable zip, prerelease)** | Early adopters + benchmarkers |
| 2026-09-30 | **v0.10.0-beta (winget + MSIX, prerelease, batched + MoE)** | Beta channel |
| 2026-10-14 | v0.11.0-rc.1 (Vulkan + NPU opt-in) | RC channel, NPU hardware testers |
| 2026-10-28 | **v1.0.0 GA (MSIX signed + winget latest + portable zip)** | Public |

Tag conventions, MSIX signing, winget manifest mechanics: `.specs/workflow/RELEASE.md`.

---

## 7. Out of scope for v1.0

Tracked in `.specs/sprints/BACKLOG.md`. Highlights:

- ARM64 Windows native build (Snapdragon X devices via emulation only in v1.0)
- Quantization-aware fine-tune path
- LoRA / adapter hot-swap at inference time
- Multi-host inference (cluster mode)
- Speculative + MoE co-design beyond EAGLE-3
- Linux WSL2-optimized path (WSL2 works via Windows binary; native Linux is separate edition)

Anything moved into the v1.0 window requires an ADR + sprint reshuffle approved by release manager.

---

## 8. How to update this file

- **Per sprint close**: update `Marco externo` column if a tag shipped or was renamed. No retroactive date edits.
- **Slip**: if a critical-path sprint slips, push the date range here AND in the affected `SPRINT.md` frontmatter AND in `BACKLOG.md`. Open an ADR if v1.0 GA moves.
- **New scope**: do not add sprints. Add tasks to existing sprints or push to `BACKLOG.md`.
- **DAG change**: requires ADR (changes the dependency contract between sprint trains).

CI does not validate this file — release manager owns it. Treat it as the published roadmap.
