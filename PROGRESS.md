# Progress Log

## Current Status

Validation complete for issue #52 and stale GitHub issue triage in progress.

## Checkpoints

### Checkpoint 1

Status: completed

Task:
Sync `main`, verify stale open issues, implement Sprint 05 T05.7 (`RAM<=8GB -> MICRO/NANO`), and validate the CLI/runtime surface locally.

Result:
`RuntimeMode` and `ProfileCatalog` now prefer low-memory profiles automatically, `probe` exposes the new `NANO` CPU-only fallback on constrained hosts, version metadata was bumped to `0.1.38`, and release docs/examples were synchronized.

Validation:
- `npm run test:cli`
- `npm run pack:dry`
- `cmake -S . -B build-macos -G "Unix Makefiles"`
- `cmake --build build-macos -j 8`
- `ctest --test-dir build-macos --output-on-failure` (`No tests were found!!!` because `GTest` is not installed on this host)
- `build-macos/runtime/benchmarks/correctness_diff`
- `build-macos/runtime/benchmarks/hybrid_planner_gate`
- `US4_CLI_PATH="$(pwd)/build-macos/runtime/cli/us4-cli" npx playwright test --project=cli --grep "prints probe summary|exports probe as json|reports nano mode for low-memory cpu-only hosts" --reporter=list`
- `taskflow run /Users/wesleysimplicio/Projetos/ai/ds4/us4-v6-simplicio/us4-v6-simplicio-windows`

Next:
Commit and push the validated patch, then close/update the relevant GitHub issues with evidence.

## Blockers

- Full Playwright release/packaging smoke is not runnable on this macOS host because `pwsh` is missing and the suite expects Windows/MSIX tooling paths.

## Validation History

| Command | Result | Notes |
|---|---|---|
| `npm run test:cli` | pass | CLI help surfaced `llm-project-mapper v0.1.38`. |
| `npm run pack:dry` | pass | Dry package metadata aligned to `0.1.38`. |
| `cmake -S . -B build-macos -G "Unix Makefiles"` | pass | Used `build-macos` because local `Ninja` is unavailable. |
| `cmake --build build-macos -j 8` | pass | Built `us4-cli`, correctness gates, and runtime libraries on AppleClang. |
| `ctest --test-dir build-macos --output-on-failure` | partial | No tests generated because `GTest` is not installed locally. |
| `build-macos/runtime/benchmarks/correctness_diff` | pass | CPU scalar correctness report generated successfully. |
| `build-macos/runtime/benchmarks/hybrid_planner_gate` | pass | Vulkan/Windows ML hybrid planner scenarios passed. |
| `US4_CLI_PATH=... npx playwright test --project=cli --grep "prints probe summary|exports probe as json|reports nano mode for low-memory cpu-only hosts" --reporter=list` | pass | 3 focused CLI smoke tests passed. |
| `US4_CLI_PATH=... npx playwright test --project=cli --reporter=list` | partial | 20 core CLI tests passed; 29 release/package/MSIX tests failed due missing `pwsh`/Windows tooling on this host. |
| `taskflow run /Users/wesleysimplicio/Projetos/ai/ds4/us4-v6-simplicio/us4-v6-simplicio-windows` | pass | Review checklist generated under `~/.config/taskflow/reports/...`. |
