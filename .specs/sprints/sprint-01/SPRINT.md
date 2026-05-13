---
sprint: sprint-01
status: todo
start: TBD
end: TBD
owner: us4-core
---

# Sprint 01 — Foundations & Skeleton (Windows)

## Objetivo
Bootstrap C++ runtime skeleton, hardware probe (CUDA, DirectML, Vulkan, AVX2/AVX-512/AMX, NPU), backend selector, runtime mode selector (FULL/BALANCED/DEGRADED/ULTRA_LOW/MICRO/NANO/CPU_ONLY), telemetry skeleton, CI/DoD pipeline.

## Tasks
- [ ] T01.1 — CMake/Ninja root + `runtime/{core,adapters,memory,kv,cache,moe,backends/{cuda,directml,vulkan,onednn,cpu_avx,windows_ml},speculative,tuning,telemetry,benchmarks}` skeleton
- [ ] T01.2 — `runtime/core/IUS4WindowsAdapter.h` interface
- [ ] T01.3 — `runtime/core/HardwareProbe` (CUDA driver, DXGI for DirectML, Vulkan ICD, AVX caps via cpuid, NPU via Windows ML)
- [ ] T01.4 — `runtime/core/BackendSelector` (NVIDIA->CUDA, AMD/Intel->DirectML, fallback Vulkan, AVX always-on, NPU opt-in)
- [ ] T01.5 — `runtime/core/RuntimeMode` enum + selector heuristics (RAM+VRAM tier -> mode)
- [ ] T01.6 — `runtime/telemetry` skeleton
- [ ] T01.7 — `.github/workflows/{ci,dod}.yml` (Win runner, MSVC + clang-cl, GoogleTest, Playwright smoke, coverage gate 80%)
- [ ] T01.8 — `.claude/hooks/{post-edit,pre-commit}.sh` (lint/format + block red commits)
- [ ] T01.9 — Playwright config + first smoke (`us4.exe --version` + `--probe` outputs vendor/backend chosen)
- [ ] T01.10 — ADR-001 Adapter interface; ADR-002 Backend selection; ADR-003 Runtime mode

## Test plan
- Unit (GoogleTest): probe returns valid vendor + capabilities; backend selector chooses correctly per vendor; mode selector picks per RAM+VRAM.
- Regression: n/a.
- E2E (Playwright): `us4.exe --version`, `--probe`, `--mode auto` com trace+screenshot+video.

## DoD
- Build green em Win11 (MSVC).
- CI gates green, coverage >=80% em `runtime/core`.
- ADR-001/002/003 merged.
- Demo: `us4.exe --probe` lista GPU + RAM + backend escolhido + mode.

## Riscos
- DirectML headers via WinSDK podem variar por versao -> pinar SDK >= 10.0.22621.
- NPU detection nao disponivel em todo Windows ML -> degrade silencioso.
