# Architecture Map

## System Shape

- Type: `BACKEND_ONLY`
- Frontend: none
- Backend: CLI nativa C++ + DLL Windows (`us4-cli`, `us4-v6.dll`)
- Database: none
- Jobs/workers: none dedicados; benchmark/telemetry são processos de runtime
- External integrations: CUDA Toolkit, DirectML/D3D12, Vulkan SDK, oneDNN, Windows ML, GitHub Actions

## Local URLs

| Service | URL | Notes |
|---|---|---|
| Runtime CLI | none | Processo local, sem servidor HTTP obrigatório. |
| Playwright report | `playwright-report/index.html` | Evidência local após E2E. |

## Request Path

Fluxo mais comum do runtime:

```text
PowerShell/CLI -> us4-cli -> HardwareProbe -> BackendSelector -> RuntimeMode selector ->
Adapter Dispatcher -> Scheduler -> KV/Expert pagers -> Backend (CUDA/DirectML/Vulkan/CPU/NPU) ->
Telemetry -> stdout/json/jsonl
```

## Key Directories

| Directory | Purpose |
|---|---|
| `.specs/` | Fonte da verdade de produto, arquitetura, workflow e sprints. |
| `.agents/` | Sub-agentes especializados (`ralph-loop`, `tdd`, `reviewer`, `architect`). |
| `.skills/` | Capacidades reutilizáveis acionadas por agentes. |
| `docs/` | Documentação operacional curta para humanos e agentes. |
| `scripts/` | Atalhos locais de start/test/evidence/update. |
| `runtime/` | Árvore C++ prevista para core, adapters, memory, kv, cache, moe, backends e benchmarks. |
| `profiles/` | Perfis de hardware e de modelos previstos pela spec. |

## Authentication

- Flow: none
- Local/demo credentials: none
- Token/session storage: none
- Common failure mode: não se aplica; o produto é CLI + biblioteca

## Observability

- App logs: stdout/stderr do `us4-cli`
- API logs: none
- Job logs: traces e métricas futuras em `runtime/telemetry/` e `runtime/benchmarks/`
- How to identify current request: CLI args, `--trace-file`, logs de telemetry e benchmark report

## Deployment

- Environments: workstation local, CI `windows-2022`, runner GPU self-hosted opcional
- CI/CD: `.github/workflows/ci.yml`, `.github/workflows/dod.yml`
- Release notes/changelog: `.specs/workflow/RELEASE.md`
