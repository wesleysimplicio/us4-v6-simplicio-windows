# Changelog

All notable changes to this repository will be documented in this file.

The format is based on Keep a Changelog and the project follows Semantic Versioning for the public CLI/runtime surface as it becomes operationally canonical.

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
