# US4 V6 — Windows Edition

> Universal State Runtime for local LLM inference on Windows x86-64 (NVIDIA / AMD / Intel + optional NPU).
> EN. Versão pt-BR: [README.pt-BR.md](README.pt-BR.md).

![us4-v6-simplicio-windows](us4-v6-simplicio-windows.PNG)

## What

US4 V6 is a C++ runtime that runs modern LLM families (DeepSeek MoE, Kimi MoE, MiniMax, GLM, Qwen, Llama, Gemma, BitNet, PT-BitNet Ternary) **locally** on Windows x86-64, with adaptive backend dispatch across **CPU scalar / AVX2 / AVX-512 / AMX / oneDNN / CUDA / DirectML / Vulkan / Windows ML (NPU)**, hot-cold KV paging across VRAM/RAM/SSD, MoE expert paging, speculative decoding, continuous batching, CUDA Graphs reuse, and hardware-aware auto-tuning across 8 hardware profiles.

Runtime modes: `FULL`, `BALANCED`, `DEGRADED`, `ULTRA_LOW`, `MICRO`, `NANO`, `CPU_ONLY` — auto-selected by hardware probe at boot, manually overridable via CLI flag.

Backend selector strategy: NVIDIA → CUDA, AMD/Intel discrete GPU → DirectML, fallback Vulkan, CPU AVX always on, NPU (Snapdragon X / Intel Core Ultra / AMD Ryzen AI) opt-in for dense layer offload.

Reference master spec: [`US4-V6-Windows-Edition.md`](../US4-V6-Windows-Edition.md).

## Stack

- C++17/20 + CMake/Ninja
- CUDA (NVIDIA streams, memory pool, CUDA Graphs)
- DirectML (cross-vendor GPU graph compile)
- Vulkan compute (AMD/Intel/cross-vendor fallback)
- oneDNN (block GEMM, AVX/AMX-aware)
- AVX2 / AVX-512 / AMX (CPU SIMD hot paths, BF16/INT8 quant)
- Windows ML (NPU offload — Snapdragon X / Intel Core Ultra / AMD Ryzen AI)
- GoogleTest (unit), Playwright (CLI/UX E2E), Ralph Loop (autonomous fix/verify)
- GitHub Actions (CI + DoD gates), MSIX installer

## Status

**Planning complete.** All 12 sprints scaffolded in [`.specs/sprints/`](.specs/sprints/). Implementation hasn't started yet — architecture `DESIGN.md`/`PATTERNS.md`/ADRs are filled in **during** each sprint's first task, not upfront.

| Sprint | Theme |
|---|---|
| 01 | Foundations & Skeleton |
| 02 | CPU Scalar Baseline |
| 03 | CUDA + DirectML Skeleton |
| 04 | AVX Hot Paths + oneDNN |
| 05 | BitNet + Ternary |
| 06 | KV Memory Architecture (VRAM/RAM/SSD) |
| 07 | Llama Adapter |
| 08 | MoE Foundation (DeepSeek + Kimi) |
| 09 | MoE Advanced (MiniMax + GLM + SP-MoE) |
| 10 | Batching + Speculative Decoding |
| 11 | Vulkan + Windows ML/NPU |
| 12 | Auto-Tune + MSIX Release v1.0 |

Full matrix: [`.specs/sprints/BACKLOG.md`](.specs/sprints/BACKLOG.md).

## How the agent works here

This repo follows the **Agentic Starter** convention. Master instruction file: [`AGENTS.md`](AGENTS.md) (mirrored at [`CLAUDE.md`](CLAUDE.md) and [`.github/copilot-instructions.md`](.github/copilot-instructions.md)).

Every technical task goes through the mandatory loop: read task → plan → load context → edit → lint → unit → Playwright E2E (trace+screenshot+video) → fix → conventional commit (English) → PR with DoD checklist.

DoD gate (enforced by [`.github/workflows/dod.yml`](.github/workflows/dod.yml)): lint green, unit green with ≥80% coverage on touched files, Playwright E2E green with evidence, all AC checked, ADR added if architectural decision, changelog updated if release-relevant.

## Repo layout

```
.specs/
  product/       VISION.md, DOMAIN.md, PERSONAS.md
  architecture/  DESIGN.md, PATTERNS.md, ADR-*.md (filled during sprints)
  workflow/      WORKFLOW.md, CONTRIBUTING.md, RELEASE.md
  sprints/       BACKLOG.md, sprint-01..12/SPRINT.md
.skills/         reusable agent capabilities
.agents/         custom sub-agents (ralph-loop, tdd, reviewer, architect)
.claude/         settings + hooks (post-edit lint, pre-commit gate)
.github/         workflows (ci.yml, dod.yml), PR/Issue templates
runtime/         C++ source (created in sprint-01: core, adapters, memory, kv, cache, moe, cuda, directml, vulkan, avx, onednn, windowsml, speculative, tuning, telemetry, benchmarks)
```

## Out of scope

- Cloud/distributed inference (single-machine focus).
- Training/fine-tuning (inference only).
- Apple Silicon → see [US4 V6 Apple Edition](https://github.com/wesleysimplicio/us4-v6-simplicio-apple).
- Linux/macOS support (Windows-only).
- GUI desktop app (CLI + library only).

## License

TBD (will be declared before v1.0 release in Sprint 12).
