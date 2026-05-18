# Backlog — US4 V6 Windows Edition

Universal State Runtime for local LLM inference on Windows x86-64 (NVIDIA / AMD / Intel + optional NPU).
Stack: C++17/20 + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + CMake/Ninja + GoogleTest + Playwright (CLI/UX) + Ralph Loop.

Reference master spec: `../../US4-V6-Windows-Edition.md`.

## Cadence
- Sprint length: 2 weeks. 12 sprints total.
- DoD gate: lint + unit (>=80% cov on touched code) + Playwright E2E + correctness diff vs reference within tolerance.
- Loop: read -> plan -> edit -> lint -> unit -> e2e -> regression -> ralph fix -> commit.

## Status snapshot (2026-05-18)

- Planning files currently define `12` sprints and `88` tasks.
- Versioned checkboxes currently mark `69` tasks as done and `19` as still open.
- The repo implementation and the versioned sprint checkboxes are now aligned for the tasks already evidenced by unit, E2E, regression and correctness gates.
- The generated companion snapshot lives in `STATUS.md` and can be refreshed via `scripts/render-planning-status.ps1`.

## Sprint Matrix

| Sprint | Theme | Key Deliverables | Adapters | Backends |
|---|---|---|---|---|
| 01 | Foundations & Skeleton | Project scaffolding, core runtime contracts, hardware probe (CUDA/DirectML/Vulkan/AVX/NPU), backend selector, mode selector, telemetry skeleton, CI/DoD pipeline | — | — |
| 02 | CPU Scalar Baseline | C++ tensor primitives, scalar matmul, scalar attention, dense adapter base, smoke benchmark, CPU_ONLY mode | Qwen dense, Gemma | CPU scalar |
| 03 | CUDA + DirectML Skeleton | CUDA streams + memory pool + CUDA Graphs, DirectML device init, GEMM kernels (CUDA), DirectML graph compile, vendor selector | Qwen, Gemma | CUDA, DirectML |
| 04 | AVX Hot Paths + oneDNN | AVX2/AVX-512 matmul, AMX BF16/INT8 path, oneDNN block GEMM, AVX dequant (INT8/INT4) | Qwen, Gemma | CPU AVX/AMX, oneDNN |
| 05 | BitNet + Ternary | BitNet 1.58-bit CUDA + AVX kernels, PT-BitNet ternary adapter, ternary lookup tables, MICRO + NANO modes | BitNet, Ternary | CUDA, AVX |
| 06 | KV Memory Architecture | Hot-cold KV pager (VRAM/RAM/SSD), prefix cache, SSD cold tier with mmap, KV summarization, eviction policies | All current | CUDA, DirectML, AVX |
| 07 | Llama Adapter | Llama 3/4 adapter, GQA, RoPE scaling, ALiBi optional, full attention path on all backends | Llama | All |
| 08 | MoE Foundation | DeepSeek MoE adapter, Kimi MoE adapter, expert pager (VRAM->RAM->SSD), top-k routing | DeepSeek, Kimi | CUDA, DirectML, AVX |
| 09 | MoE Advanced | MiniMax adapter, GLM adapter, SP-MoE speculative expert prefetch, sparsity-aware cache, multimodal cache | MiniMax, GLM | CUDA, DirectML, AVX |
| 10 | Batching + Speculative | Continuous batching multi-session, P-EAGLE / EAGLE-3 speculative decoding, draft model loader, CUDA Graphs reuse | All | All |
| 11 | Vulkan + Windows ML/NPU | Vulkan compute backend (AMD/Intel/cross-vendor), Windows ML adapter for NPU (Snapdragon X / Intel Core Ultra / AMD Ryzen AI), dense offload to NPU | Dense (Qwen/Gemma/Llama) | Vulkan, Windows ML |
| 12 | Auto-Tune + Release v1.0 | Hardware-aware auto-tuning (8 profiles), benchmark matrix (profiles x 9 adapters), CLI polish (PowerShell), docs final, MSIX installer, release | All | All |

## Cross-Cutting (every sprint)
- Unit tests (GoogleTest) on every new public API.
- Regression suite re-run on existing adapters + modes + backends.
- Playwright E2E on CLI flows + telemetry surface (trace+screenshot+video).
- Ralph Loop autonomous run after merge to validate green.
- Correctness diff vs reference impl logged in `runtime/benchmarks/correctness/`.
- Telemetry expand: latency, tokens/s, RAM/VRAM peak, KV hit-rate, expert hit-rate, mode transitions, backend usage breakdown.

## Definition of Ready
- Linked to master spec section.
- Acceptance criteria measurable (numeric or boolean).
- Test plan declared (unit + e2e + regression).
- Hardware profile target identified (1 of 8 profiles).

## Definition of Done
- Code merged via PR, conventional commit (English).
- All gates green (lint/unit/e2e/regression/correctness).
- Coverage >=80% on touched files.
- ADR added if architectural decision was made.
- Changelog entry under correct version.
- Sprint-end demo recorded if user-visible.

## Out of Scope
- Cloud/distributed inference (single-machine focus).
- Training/fine-tuning (inference only).
- Apple Silicon on this edition (see Apple Edition).
- Linux/macOS support (Windows-only).
- GUI desktop app (CLI + library only).
