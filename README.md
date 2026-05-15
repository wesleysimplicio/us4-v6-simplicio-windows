# US4 V6 - Windows Edition

> Universal State Runtime for local LLM inference on Windows x86-64 (NVIDIA / AMD / Intel + optional NPU).
> EN. Versao pt-BR: [README.pt-BR.md](README.pt-BR.md).

![us4-v6-simplicio-windows](us4-v6-simplicio-windows.PNG)

## What

US4 V6 is a C++ runtime for local LLM inference on Windows x86-64 with adaptive backend dispatch across CPU, CUDA, DirectML, Vulkan, and optional Windows ML / NPU paths. The project already includes the initial runtime scaffold, hardware-aware backend selection, a bootable CLI, early unit coverage, and filled architecture/product documentation to support the next implementation sprints.

Reference master spec: [`US4-V6-Windows-Edition.md`](./US4-V6-Windows-Edition.md).

## Current Status

The repository is past planning-only mode. The current baseline already includes:

- CMake-based Windows runtime scaffold under [`runtime/`](./runtime/)
- Backend capability model and selector flow for CUDA, DirectML, Vulkan, and CPU fallback
- Hardware probe pipeline with mode selection and budget reporting
- Initial CLI executable: `us4-cli`
- Initial unit tests for probe and formatting behavior
- Project architecture, patterns, domain, workflow, and sprint docs filled in under [`.specs/`](./.specs/)

This means the repo is now in an early implementation stage: foundations are in place, while model execution, adapter depth, correctness harnesses, and backend specialization continue sprint by sprint.

## Implemented Baseline

The runtime scaffold currently covers these areas:

- `runtime/core/`: host probe summary and CLI-facing runtime wiring
- `runtime/backends/`: backend descriptors, host capability detection, selector logic
- `runtime/adapters/`: adapter contracts and a null Windows adapter baseline
- `runtime/memory/`, `runtime/kv/`, `runtime/cache/`, `runtime/moe/`, `runtime/speculative/`, `runtime/tuning/`, `runtime/telemetry/`: initial module skeletons for later sprint expansion
- `runtime/benchmarks/`: benchmark entrypoint scaffold
- `profiles/`: hardware/runtime profile presets

The current CLI baseline supports probing and establishes the contract for future `run` flows.

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

5. Run the probe:

```powershell
.\build\us4-cli.exe --probe
```

6. Run unit tests:

```powershell
ctest --test-dir build --output-on-failure
```

If you are working only on the starter/documentation layer, [`scripts/test.ps1`](./scripts/test.ps1) still validates the packaging/bootstrap side separately.

## Repo Layout

```text
.specs/                 Product, architecture, workflow, and sprint docs
runtime/
  core/                 Probe summary and CLI-facing runtime core
  backends/             Capability detection and backend selection
  adapters/             Runtime adapter contracts and null adapter baseline
  memory/ kv/ cache/    Memory and paging module skeletons
  moe/ speculative/     MoE and decoding scaffolds
  tuning/ telemetry/    Runtime tuning and observability scaffolds
  benchmarks/           Benchmark scaffold
profiles/               Hardware/runtime profile presets
tests/unit/             Initial GoogleTest coverage
```

## Working Here

This repo follows the `AGENTS.md` contract. Canonical agent instructions live in [`AGENTS.md`](./AGENTS.md) and are mirrored in [`CLAUDE.md`](./CLAUDE.md) and [`.github/copilot-instructions.md`](./.github/copilot-instructions.md).

For technical work, the expected loop is still: read task, load architecture context, edit surgically, run format/lint/unit, run Playwright for CLI-facing changes, then attach evidence to the PR.

## Next Milestones

The next major milestones are:

- deepen the CPU baseline beyond scaffold-only behavior
- connect real adapter implementations to the CLI execution path
- expand correctness and benchmark harnesses per backend
- bring CUDA, DirectML, and Vulkan paths from selector-level support to execution-ready adapters
- align CI/DoD automation with the C++ runtime workflow end to end

## Out of Scope

- Cloud or distributed inference
- Training or fine-tuning
- Linux/macOS support in this edition
- GUI desktop application in place of the CLI/runtime focus

## License

License policy has not been published yet. Treat the repository as internal/project-controlled until the first public release policy is added.
