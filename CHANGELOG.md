# Changelog

All notable changes to this repository will be documented in this file.

The format is based on Keep a Changelog and the project follows Semantic Versioning for the public CLI/runtime surface as it becomes operationally canonical.

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
