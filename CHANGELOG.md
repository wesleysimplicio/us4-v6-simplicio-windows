# Changelog

All notable changes to this repository will be documented in this file.

The format is based on Keep a Changelog and the project follows Semantic Versioning for the public CLI/runtime surface as it becomes operationally canonical.

## [0.1.57] - 2026-05-18

### Added

- `AVX2` fused attention with online softmax-rescale, a dedicated `cpu_attention_bench`, and unit coverage that checks the accelerated path against the scalar attention reference

### Changed

- switched the CPU runtime to prefer the new fused attention path on `AVX2` hosts while keeping the scalar fallback and recording latency evidence in `runtime/benchmarks/correctness/reports/cpu_attention_bench.json`

## [0.1.56] - 2026-05-18

### Added

- `AVX2` group-wise `INT8` and `INT4` CPU dequant paths with dedicated unit coverage and local `Q8/Q4` throughput evidence in the Sprint 04 benchmark report

### Changed

- extended `cpu_block_gemm_bench` to emit `cpu-q8-avx2` and `cpu-q4-avx2` samples with `tokens_per_second`, keeping Sprint 04 quantized CPU evidence committed alongside the AVX matmul work

## [0.1.55] - 2026-05-18

### Added

- explicit `AVX2` and guarded `AVX-512` matmul kernels for the CPU hot path, with benchmark evidence that records unavailable `AVX-512` variants honestly on hosts that cannot execute that ISA

### Changed

- taught the CPU block GEMM benchmark, oneDNN planning, and Sprint 04 benchmark notes to distinguish validated `AVX2` speedups from pending `AVX-512` host evidence

## [0.1.54] - 2026-05-18

### Changed
- completed the local starter-to-mapper branding pass in `bootstrap.ps1`, `bootstrap.sh`, `_BOOTSTRAP.md`, and `PROGRESS.md` so the packaged overlay/handoff flow now consistently presents `LLM Project Mapper`
- kept backward-compatible detection for legacy `Agentic Starter` markers in bootstrap preservation and `.gitignore` append checks, avoiding destructive rewrites on older overlays
- refreshed release examples and versioned smoke commands in `README*` and the runtime version surface to track the `0.1.54` snapshot

## [0.1.53] - 2026-05-18

### Added
- a concrete `OneDnnBackend` wrapper for block GEMM planning/execution on top of the existing oneDNN-compatible contract
- a committed local CPU block GEMM benchmark runner plus a versioned Sprint 04 benchmark note snapshot

### Changed
- `scripts/test.ps1` and `scripts/test.sh` now execute the CPU block GEMM benchmark when the binary is available so oneDNN evidence stays in the local validation loop

## [0.1.52] - 2026-05-18

### Added
- explicit CPU fallback selection coverage for AVX2, AVX-512 and AMX capability levels

### Changed
- `BackendSelector` now exposes a reusable CPU fallback descriptor builder so runtime planning and backend catalogs share the same cpuid-driven CPU backend choice
- `RuntimeContext` now uses the cpuid-driven CPU fallback selector instead of hardcoding `cpu-avx2`

## [0.1.51] - 2026-05-18

### Added
- added a real-first `HardwareProbe` path with CPUID, DXGI adapter discovery and runtime presence checks for CUDA, Vulkan and Windows ML
- added deterministic unit coverage for real-probe mode and environment override precedence

### Changed
- introduced automatic synthetic probe mode whenever `US4_*` scenario variables are present so CLI smoke and regression fixtures stay deterministic
- hardened `scripts/test.ps1` so native command failures stop the validation loop immediately
- increased Playwright E2E timeout for the heavier Sprint 12 release and project-status flows

## [0.1.50] - 2026-05-18

### Added
- introduced a Windows memory-mapped MoE shard loader for `expert_NNN.safetensors` directories
- added unit coverage proving only requested experts are mapped and that CPU MoE runs touch only routed experts

### Changed
- marked Sprint 08 `T08.5` as complete and advanced the versioned planning snapshots to a fully done Sprint 08

## [0.1.49] - 2026-05-18

### Added
- introduced a dedicated `LlamaModelLoader` with concrete `tokenizer.json` parsing, `gguf` metadata extraction and `safetensors` shape validation
- added unit coverage for Llama tokenizer round-trip, GGUF metadata loading and safetensors shape mismatch rejection

### Changed
- marked Sprint 07 `T07.5` as complete in the versioned planning snapshots and GitHub issue map

## [0.1.48] - 2026-05-18

### Added

- imported the upstream `auto-map.js` engine from `llm-project-mapper` and wired the local CLI to run the automatic mapping pass immediately after bootstrap metadata is written

### Changed

- re-aligned the starter surface from `agentic-starter` to `llm-project-mapper` across the CLI handoff contract, bootstrap/update scripts, install docs, package metadata and local-setup guidance while preserving the legacy `agentic-starter` bin alias for compatibility

## [0.1.47] - 2026-05-18

### Added

- Kimi MoE CPU benchmark and correctness gate coverage, including a dedicated `moe_kimi_cpu_only` registry case
- explicit MoE load-balance telemetry on the `run` text/JSON contract, plus a Kimi CLI smoke path that proves the adapter route end to end

### Changed

- completed Sprint 08 task `T08.4` in the versioned planning files and aligned the planning snapshot with the newly validated Kimi adapter evidence

## [0.1.46] - 2026-05-18

### Added

- adapter-level KV hooks on `IUS4WindowsAdapter`, wired through the dense and null adapter scaffolds with pager-backed append, lookup, evict and summarize operations
- KV telemetry preview in `probe`, plus per-tier hit-rate reporting in CPU scalar `run` text/JSON surfaces

### Changed

- completed Sprint 06 in the versioned planning files and aligned the local evidence with adapter hook + KV telemetry coverage across unit, correctness and Playwright smoke flows

## [0.1.45] - 2026-05-18

### Changed

- reconciled Sprint 06, Sprint 07, Sprint 09 and Sprint 10 planning status with the newer `main` implementation, including SsdColdStore, LlamaConfig, MiniMax/GLM/SP-MoE, scheduler, draft loader and acceptance telemetry coverage
- updated the planning-status Playwright assertions to the new `63 done / 25 remaining` snapshot and relaxed the local MSIX package smoke timeout to better fit slower Windows hosts
- synchronized another GitHub issue closure pass, including the Sprint 09 epic and the newly validated task issues that already had code and tests on `main`

## [0.1.44] - 2026-05-18

### Changed

- reconciled the versioned sprint checklists with the implemented runtime and release surface, bringing planning status up to date across S01, S02, S03, S05, S06, S07, S08, S11 and S12
- refreshed the planning status Playwright assertions so release and project-status flows validate the new `49 done / 39 remaining` planning snapshot
- synchronized GitHub issue closure with the versioned sprint status for the tasks now fully evidenced by unit, regression, correctness and CLI E2E gates

## [0.1.43] - 2026-05-18

### Added

- synthetic cold offload and on-demand reload support in `runtime/moe/ExpertPager`, including checksum-preserving reload coverage for post-eviction correctness

### Changed

- the CLI `probe` MoE preview now surfaces cold offload and reload counts so ExpertPager behavior can be validated locally on the macOS scaffold host

## [0.1.42] - 2026-05-18

### Added

- MoE probe telemetry preview in the CLI, including hot/warm/cold hit-rate, eviction count, and router entropy surfaced in both text and JSON modes

### Changed

- the hardware probe now emits deterministic MoE telemetry events through the existing sink so local validation can cover Sprint 08 telemetry without Windows-only accelerators

## [0.1.41] - 2026-05-18

### Added

- MiniMax multimodal cache telemetry in the CPU scalar CLI scaffold, including text and JSON evidence for image/audio token reuse signals

### Changed

- the MiniMax MoE path now wires the existing `MultimodalCache` scaffold into `us4-cli run` so local smoke validation can prove cache hit/miss behavior on `minimax-m2`

## [0.1.40] - 2026-05-18

### Added

- MoE prefetch and sparsity telemetry to the CPU scalar CLI scaffold, including ratio output in both text and JSON modes for local smoke validation

### Changed

- the CPU scalar MoE path now simulates speculative prefetch outcomes and sparsity-aware cache lookups so `us4-cli run` emits deterministic telemetry for Sprint 09 smoke coverage

## [0.1.39] - 2026-05-18

### Added

- speculative acceptance telemetry in the CPU scalar baseline, including per-step delta and token acceptance trace data surfaced through `us4-cli run` text and JSON output

### Changed

- `SpeculativeEngine` now tracks partial prefix acceptance instead of only all-or-nothing draft matches, which makes acceptance-rate reporting meaningful for the speculative decoder scaffolds

## [0.1.38] - 2026-05-18

### Changed

- low-memory host selection now prefers `NANO` on CPU-only paths and `MICRO` on accelerated paths when the detected host budget is 8 GiB or less, aligning Sprint 05 T05.7 with `ProbeHardware`, `RuntimeContext`, and `ProfileCatalog`
- synchronized the versioned CLI/runtime surface across CMake, npm metadata, lockfile metadata, tests, and release command examples

## [0.1.37] - 2026-05-18

### Added

- WIP skeletons for Sprint 06/07/09/10 sub-systems mapped from GitHub issues
- `runtime/kv/SsdColdStore` Windows-flavoured cold KV staging contract (S06 T06.3)
- `runtime/adapters/LlamaConfig` dedicated config struct with GQA/RoPE validation (S07 T07.1)
- `runtime/adapters/{MiniMax,Glm}ScalarAdapter` scalar parity scaffolds for the S09 MoE pair (T09.1, T09.2)
- `runtime/moe/SpeculativePrefetch` frequency-based expert prefetcher with windowed history (S09 T09.3)
- `runtime/cache/{SparsityAwareCache,MultimodalCache}` per-channel cache contracts (S09 T09.4, T09.5)
- `runtime/scheduler/{ContinuousBatcher,SessionPool}` new scheduler module for batched multi-session decode (S10 T10.1, T10.2)
- `runtime/speculative/{PEagleDecoder,Eagle3Decoder,DraftModelLoader}` speculative decode scaffolds layered over `SpeculativeEngine` (S10 T10.3, T10.4, T10.5)
- 12 unit test suites covering the new scaffolds (28 tests) — all green on the host build

### Changed

- `post-publish-smoke.ps1` now supports a dev-only `.msix` path that signs a temporary copy with a self-signed certificate, runs install smoke, and cleans up local trust state

## [0.1.36] - 2026-05-18

### Changed

- `post-publish-smoke.ps1` now supports a dev-only `.msix` path that signs a temporary copy with a self-signed certificate, runs install smoke, and cleans up local trust state
- `sign-msix.ps1` now allows local signing without a timestamp URL so dev-only validation does not depend on external timestamp infrastructure

## [0.1.35] - 2026-05-18

### Changed

- `release-dry-run.ps1` now blocks on a mismatched `vX.Y.Z` tag before reporting local release readiness
- `release-dry-run.ps1` and `render-project-status.ps1` now expose dev-only MSIX smoke intent and preflight more explicitly in their structured payloads

## [0.1.34] - 2026-05-17

### Changed

- `render-project-status.ps1` now exposes `dev_msix_preflight` so local SDK/tooling blockers are visible separately from external production blockers
- `dev-msix-smoke.ps1 -PreflightOnly` no longer requires a certificate password because it performs no signing side effects

## [0.1.33] - 2026-05-17

### Added

- `scripts/create-dev-signing-cert.ps1` and `scripts/remove-dev-signing-cert.ps1` for a reproducible local MSIX signing scaffold
- `scripts/dev-msix-smoke.ps1` for dev-only `build -> sign -> install smoke -> cleanup`, plus E2E coverage for the new local MSIX path

### Changed

- release docs now distinguish self-signed local MSIX validation from the external blocker of a trusted production signing certificate

## [0.1.32] - 2026-05-17

### Added

- E2E regression coverage to guarantee that local `release-dry-run` and `render-project-status` use ephemeral winget manifests by default

### Changed

- `release-dry-run.ps1` now defaults `ManifestDir` to an ephemeral working directory instead of `packaging/winget/manifests`
- `render-project-status.ps1` now uses an ephemeral manifest directory for its optional local release dry-run unless the caller passes `-ManifestDir`

## [0.1.31] - 2026-05-17

### Added

- `scripts/validate-publish-layout.ps1` to guarantee that the final release output only contains publishable artifacts and metadata
- E2E regression coverage for clean and dirty publish-layout scenarios

### Changed

- release packaging now stages portable zip and MSIX work outside the publishable output directory
- `release.yml` now uploads and releases only the canonical artifact set instead of `dist/**`

## [0.1.30] - 2026-05-17

### Added

- `scripts/render-project-status.ps1` to consolidate planning, release, and evidence state in one status contract
- E2E regression coverage for project status JSON and Markdown rendering

### Changed

- release docs now expose `render-project-status.ps1` as the main high-level status snapshot for Sprint 12

## [0.1.18] - 2026-05-15

### Added

- build-generated runtime version consumed by `us4-cli version`
- PowerShell completion script for `us4-cli`
- starter release scaffolding for changelog and packaging handoff

### Changed

- aligned CMake project version with the published CLI/runtime surface

## [0.1.19] - 2026-05-15

### Added

- E2E regression coverage for portable zip packaging, MSIX prerequisite handling, and PowerShell completion installation
- migration guide and stronger release smoke coverage in CI

### Fixed

- idempotent completion installation for empty PowerShell profile files

## [0.1.20] - 2026-05-15

### Added

- release checksum generation via `scripts/generate-checksums.ps1`
- local post-publish smoke for portable zip artifacts via `scripts/post-publish-smoke.ps1`
- renderable winget manifest scaffold via `scripts/render-winget-manifests.ps1`

### Changed

- release workflow now generates checksums and validates the portable release artifact before upload

## [0.1.21] - 2026-05-15

### Added

- MSIX signing scaffold via `scripts/sign-msix.ps1`
- CI-aware release signing hook in `release.yml`
- E2E regression for missing signing certificate prerequisites

## [0.1.22] - 2026-05-15

### Added

- release readiness preflight via `scripts/preflight-release.ps1`
- CI release preflight gate before artifact packaging
- E2E regression coverage for ready vs blocked preflight states

## [0.1.23] - 2026-05-15

### Added

- MSIX install smoke scaffold via `scripts/install-msix-smoke.ps1`
- stronger release architecture/pattern docs for packaging and distribution boundaries
- E2E regression for unsigned MSIX install smoke failure

## [0.1.24] - 2026-05-15

### Added

- winget manifest validation via `scripts/validate-winget-manifests.ps1`
- release workflow rendering and validation for publishable winget manifests
- E2E regression coverage for valid vs placeholder winget manifest URLs

## [0.1.25] - 2026-05-15

### Added

- release asset set validation via `scripts/validate-release-assets.ps1`
- CI gate that cross-checks generated artifacts, checksums, and winget manifests before upload
- E2E regression coverage for coherent vs blocked release asset sets

## [0.1.26] - 2026-05-15

### Added

- `release-manifest.json` generation via `scripts/render-release-manifest.ps1`
- CI release metadata render step after asset validation
- E2E regression coverage for release manifest generation from artifacts, checksums, and distribution URLs

## [0.1.29] - 2026-05-17

### Added

- `scripts/release-dry-run.ps1` to execute the local release preparation flow as a single structured contract
- E2E regression coverage for the chained local release dry-run summary

### Changed

- release docs and examples now point to the dry-run entrypoint for Sprint 12 packaging evidence

## [0.1.28] - 2026-05-16

### Added

- `scripts/render-planning-status.ps1` for JSON and Markdown planning snapshots
- E2E regression coverage for generated planning status output and artifact rendering

### Changed

- planning docs now point to the generated `STATUS.md` companion snapshot

## [0.1.27] - 2026-05-16

### Added

- release tag validation via `scripts/validate-release-tag.ps1`
- release notes generation via `scripts/render-release-notes.ps1`
- E2E regression coverage for release tag validation and release notes rendering

### Changed

- planning docs now record the current sprint/task count snapshot and mark Sprint 12 docs as complete

## [0.1.17] - 2026-05-15

### Added

- `run --format json` for CPU scalar execution and backend dry-run envelopes
- CLI/E2E regression coverage for the `run` JSON contract

## [0.1.16] - 2026-05-15

### Added

- public `serve` scaffold in the CLI
- JSON output for `probe` and `tune`
- stronger Sprint 12 docs and matrix persistence coverage
