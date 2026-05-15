# US4 V6 - Windows Edition

> Universal State Runtime for local LLM inference on Windows x86-64 (NVIDIA / AMD / Intel + optional NPU).
> EN. Versao pt-BR: [README.pt-BR.md](README.pt-BR.md).

![us4-v6-simplicio-windows](us4-v6-simplicio-windows.PNG)

## What

US4 V6 is a C++ runtime for local LLM inference on Windows x86-64 with adaptive backend dispatch across CPU, CUDA, DirectML, Vulkan, and optional Windows ML / NPU paths. The repository is already beyond planning-only mode: it contains a working CLI surface, hardware-aware backend selection, a CPU scalar execution baseline, matrix benchmarking/tuning scaffolds, and filled product and architecture documentation to support the remaining runtime sprints.

Reference master spec: [`US4-V6-Windows-Edition.md`](./US4-V6-Windows-Edition.md).

## Current Status

The current baseline already includes:

- a CMake-based Windows runtime scaffold under [`runtime/`](./runtime/)
- hardware capability detection and backend selection for CUDA, DirectML, Vulkan, Windows ML, and CPU fallback
- `us4-cli` commands for `help`, `version`, `probe`, `run`, `bench`, and `tune`
- a CPU-only scalar execution path for `run --backend cpu`
- dry-run planning for CUDA, DirectML, Vulkan, and Windows ML backends
- a `MatrixRunner` that drives matrix planning for `bench` and mini-bench persistence for `tune`
- a persistent per-host profile store at `runtime/tuning/profiles.json`, overridable with `US4_PROFILE_STORE_PATH`
- unit coverage, Playwright CLI E2E coverage, correctness gates, and workflow docs under [`.specs/`](./.specs/)

This means the repo is now in an implementation-heavy stage: the CLI and planner contracts are real, while device-side execution depth, release packaging, and the final runtime polish continue sprint by sprint.

## CLI Surface

The current `us4-cli` contract is:

```text
us4-cli help
us4-cli version
us4-cli probe [--format <text|json>]
us4-cli serve --model <model-id> [--format <text|json>] [--backend <auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode <auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]
us4-cli run --model <model-id> --prompt <text> [--model-path <asset>] [--max-tokens <count>] [--backend <auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode <auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]
us4-cli bench --model <model-id> [--format <text|json>] [--max-tokens <count>] [--backend <auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode <auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]
us4-cli tune --model <model-id> [--format <text|json>] [--max-tokens <count>] [--backend <auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode <auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]
```

Current behavior:

- `probe` prints the hardware/backend summary in text and now also supports `--format json`.
- `serve` prints the serving scaffold for the current host and supports `--format text|json`.
- `run` executes the CPU scalar baseline when the resolved plan is CPU-only, and prints dry-run execution plans for non-CPU backends.
- `bench` evaluates the current benchmark matrix without persisting a profile. It supports `--format text` and `--format json`.
- `tune` runs the mini-bench planner, selects the best profile for the current hardware fingerprint, persists that selection to the profile store, and now also supports `--format json`.
- `version` exists, but the runtime version string is still hardcoded and is not yet a canonical build-generated version source.

Not implemented yet:

- `run` still emits text only; JSON parity for the full run path is still open.
- MSIX packaging, portable zip automation, and release publishing are not finished.

## Matrix Bench And Tune

`bench` and `tune` are powered by the `MatrixRunner` in [`runtime/benchmarks/`](./runtime/benchmarks/):

- `bench` builds the current benchmark matrix for the requested model/backend/mode combination and exports the scored sample set.
- `tune` executes the same planning flow, chooses the best supported profile for the current host, and persists the selection.
- the persisted profile is keyed by a hardware fingerprint built from CPU, GPU, NPU, and memory/storage capability data.

Expected store location:

- default: `runtime/tuning/profiles.json`
- override: `US4_PROFILE_STORE_PATH=<absolute-or-relative-path>`

Typical examples:

```powershell
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only --format json
.\build\us4-cli.exe tune --model qwen-0.5b --backend windows-ml --npu
```

## Stack

- C++17/20 + CMake + Ninja
- CUDA
- DirectML
- Vulkan compute
- oneDNN / AVX2 / AVX-512 / AMX
- Windows ML for optional NPU paths
- GoogleTest for unit coverage
- Playwright for CLI/UX E2E evidence
- GitHub Actions for CI / DoD gates

## Local Setup

Recommended Windows setup:

1. Install Visual Studio 2022 with MSVC and C++ build tools.
2. Ensure CMake and Ninja are available, typically via the Visual Studio developer environment.
3. Open a Developer PowerShell / VS Dev shell.
4. Configure and build:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

5. Exercise the CLI:

```powershell
.\build\us4-cli.exe probe
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi" --backend cpu
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only --format json
.\build\us4-cli.exe tune --model qwen-0.5b --backend cpu --mode cpu-only
```

6. Run validation:

```powershell
ctest --test-dir build --output-on-failure
npx playwright test --project=cli
```

If you are working only on the starter/documentation layer, [`scripts/test.ps1`](./scripts/test.ps1) still validates the packaging/bootstrap side separately.

## Repo Layout

```text
.specs/                 Product, architecture, workflow, and sprint docs
runtime/
  core/                 Probe summary, runtime planning, CPU scalar execution
  backends/             Capability detection, backend selection, dry-run planners
  adapters/             Runtime adapter contracts and model loader scaffolds
  memory/ kv/ cache/    Memory and paging modules
  moe/ speculative/     MoE and decoding scaffolds
  tuning/               AutoTuner and persisted host profile store
  telemetry/            Runtime observability scaffolds
  benchmarks/           MatrixRunner, correctness gates, benchmark registry
profiles/               Hardware/runtime profile presets
tests/unit/             GoogleTest coverage
tests/e2e/              Playwright CLI evidence
```

## Working Here

This repo follows the `AGENTS.md` contract. Canonical agent instructions live in [`AGENTS.md`](./AGENTS.md) and are mirrored in [`CLAUDE.md`](./CLAUDE.md) and [`.github/copilot-instructions.md`](./.github/copilot-instructions.md).

For technical work, the expected loop is still: read the task, load architecture context, edit surgically, run format/lint/unit, run Playwright for CLI-facing changes, re-run correctness and regression gates for touched backends, then attach evidence to the PR.

If the change touches:

- `bench`, include the matrix output used for validation, especially `--format json` when the JSON contract changed.
- `tune`, include both CLI evidence and the persisted `profiles.json` artifact used during validation.
- release or packaging flows, update [`.specs/workflow/RELEASE.md`](./.specs/workflow/RELEASE.md) in the same change.

## Next Milestones

The biggest remaining milestones are:

- deepen device-side execution beyond dry-run behavior for Vulkan and Windows ML
- finish the Sprint 12 CLI/release surface around JSON parity and packaging
- add release automation for portable zip and MSIX
- align the runtime version source, changelog, and release metadata end to end

## Out of Scope

- Cloud or distributed inference
- Training or fine-tuning
- Linux/macOS support in this edition
- GUI desktop application in place of the CLI/runtime focus

## License

License policy has not been published yet. Treat the repository as internal/project-controlled until the first public release policy is added.
