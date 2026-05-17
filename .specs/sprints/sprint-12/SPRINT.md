---
sprint: sprint-12
status: in_progress
start: 2026-10-15
end: 2026-10-28
owner: us4-core
---

# Sprint 12 - Auto-Tune + Benchmark + Release v1.0 (Windows)

## Objetivo
Auto-tuning hardware-aware (tile/batch por profile). Matriz benchmark (8 hardware profiles x 9 adapters). CLI PowerShell-friendly. Docs finais. MSIX installer. Release v1.0.

## Tasks
- [x] T12.1 - `runtime/tuning/AutoTuner` (mini-bench at startup)
- [x] T12.2 - `runtime/tuning/profiles.json` (cached profile por GPU+CPU+RAM+VRAM)
- [x] T12.3 - Matriz benchmark `runtime/benchmarks/matrix_runner.cpp` (8 hardware profiles x 9 adapters)
- [x] T12.4 - CLI polish: `us4.exe serve|run|probe|bench|tune` + JSON output; PowerShell completions
- [x] T12.5 - Docs finais: `README.md` + `.specs/architecture/{DESIGN,PATTERNS}.md`
- [ ] T12.6 - MSIX installer + signed binary x64
- [ ] T12.7 - Release v1.0: tag, changelog, GitHub Releases
- [x] T12.8 - Migration guide + troubleshooting

## Status snapshot (2026-05-16)

- Planejamento versionado: 12 sprints, 88 tasks totais.
- Checkboxes marcados hoje: 6 concluidas, 82 ainda abertas.
- Sprint 12 segue em progresso porque os blocos restantes dependem de assinatura/publicacao real, apesar da trilha local de release ja cobrir preflight, manifests, checksums, release manifest e release notes.

## Test plan
- Unit: auto-tuner converges; profile cache load/save.
- Regression: full matrix verde nos 8 profiles.
- E2E: MSIX install + `us4.exe --probe` em laptop NPU, desktop NVIDIA, desktop AMD, CPU_ONLY.
- Correctness: tolerancia em todos adapters.

## DoD
- Tag `v1.0.0` + MSIX publicado.
- Coverage total >=80%.
- README + docs finais merged.
- Demo gravado em 4 perfis (NVIDIA/AMD/Intel/NPU).
