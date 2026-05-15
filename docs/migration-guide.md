# Migration Guide

## Moving to the current Sprint 12 CLI/release baseline

This repository now exposes a broader operational surface than the early scaffold-only revisions.
If you are updating local scripts, CI jobs, or downstream automation, align to these rules.

## 1. Runtime version source

- `us4-cli version` now comes from the CMake project version generated at build time.
- Do not treat `package.json` as the only canonical runtime version source anymore.
- Release automation should validate the CLI version from the built binary, not from JavaScript metadata alone.

## 2. CLI JSON contracts

The following commands now have explicit JSON output contracts:

- `probe --format json`
- `run --format json`
- `serve --format json`
- `bench --format json`
- `tune --format json`

If your automation was parsing text output, migrate to JSON for stable machine-readable behavior.

## 3. Persisted tuning profiles

- The default persisted store is `runtime/tuning/profiles.json`.
- Override it with `US4_PROFILE_STORE_PATH` when you need per-job isolation in CI or test automation.
- `bench` does not persist profiles.
- `tune` persists the selected profile for the current hardware fingerprint.

## 4. PowerShell completions

- PowerShell completion support now lives in `scripts/completions/us4-cli.ps1`.
- Use `scripts/install-completions.ps1` to append the loader line into the current PowerShell profile.
- The installer is expected to be idempotent and safe to rerun.

## 5. Release artifacts

The repository now includes local release scaffolding:

- `scripts/build-portable-zip.ps1`
- `scripts/build-msix.ps1`
- `.github/workflows/release.yml`
- `packaging/msix/`
- `CHANGELOG.md`

Current limitations still apply:

- MSIX signing is not configured
- `MakeAppx.exe` must be present to build MSIX locally
- post-publish smoke and winget packaging are still pending
