# Goal Result

## Summary

Implemented Sprint 05 issue `#52` by making low-memory hosts automatically prefer `NANO` on CPU-only plans and `MICRO` on accelerated plans when the detected host budget is 8 GiB or less. The CLI `probe` path now reflects that behavior, a focused Playwright smoke test covers it end-to-end, and release/version metadata was synchronized to `0.1.38`.

## Changed Files

- `runtime/core/runtime_mode.cpp`
- `profiles/src/profile_catalog.cpp`
- `tests/unit/hardware_probe_test.cpp`
- `tests/e2e/smoke.spec.ts`
- `CHANGELOG.md`
- `CMakeLists.txt`
- `package.json`
- `package-lock.json`
- `README.md`
- `README.pt-BR.md`

## Validation Commands

```bash
npm run test:cli
npm run pack:dry
cmake -S . -B build-macos -G "Unix Makefiles"
cmake --build build-macos -j 8
ctest --test-dir build-macos --output-on-failure
build-macos/runtime/benchmarks/correctness_diff
build-macos/runtime/benchmarks/hybrid_planner_gate
US4_CLI_PATH="$(pwd)/build-macos/runtime/cli/us4-cli" npx playwright test --project=cli --grep "prints probe summary|exports probe as json|reports nano mode for low-memory cpu-only hosts" --reporter=list
taskflow run /Users/wesleysimplicio/Projetos/ai/ds4/us4-v6-simplicio/us4-v6-simplicio-windows
```

## Validation Results

- build: pass (`build-macos`)
- tests: pass for targeted CLI/runtime validation; partial overall because Windows/PowerShell release smoke cannot run on this host
- lint: not run (repo automation did not detect a local lint command for this environment)

## Remaining Risks

- Full release/package/MSIX smoke still needs a Windows host with `pwsh`, `MakeAppx.exe`, signing prerequisites, and the rest of the PowerShell-based release toolchain.
- `GTest` is not installed on this host, so CTest did not generate the unit test suite locally.

## Suggested PR Title

`fix: prefer micro and nano profiles on low-memory hosts`

## Suggested PR Body

```md
## Summary
- prefer `NANO` on CPU-only hosts and `MICRO` on accelerated hosts when detected host RAM is 8 GiB or less
- expose the low-memory probe behavior through focused unit and Playwright smoke coverage
- bump the CLI/runtime surface to `0.1.38` and sync versioned release examples

## Validation
- [x] npm run test:cli
- [x] npm run pack:dry
- [x] build
- [x] correctness gates
- [x] focused Playwright CLI smoke
- [ ] full Windows/MSIX release smoke (blocked on host tooling)

## Risks
- Windows-only release/package flows still need validation on a Windows host with PowerShell/MSIX tooling.
```
