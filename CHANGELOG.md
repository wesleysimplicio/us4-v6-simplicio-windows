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

## [0.1.17] - 2026-05-15

### Added

- `run --format json` for CPU scalar execution and backend dry-run envelopes
- CLI/E2E regression coverage for the `run` JSON contract

## [0.1.16] - 2026-05-15

### Added

- public `serve` scaffold in the CLI
- JSON output for `probe` and `tune`
- stronger Sprint 12 docs and matrix persistence coverage
