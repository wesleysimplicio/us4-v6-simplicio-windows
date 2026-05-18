# Local Setup

Este repositorio tem duas camadas ativas:

- a camada do `llm-project-mapper`, usada para bootstrap, mapeamento automatico, docs, packaging e Playwright
- a camada do runtime `US4 V6 Windows Edition` em C++/CMake, com CLI, benchmark matrix, tuning persistente e gates de evidencia

Rastro atual do mapper:

- proveniencia: [`.llm-project-mapper.json`](../.llm-project-mapper.json)
- inspecao mais recente: [`.specs/journal/inspection-2026-05-18.md`](../.specs/journal/inspection-2026-05-18.md)

## Prerequisites

- Node.js `>=16.7` para tooling do starter e Playwright
- Visual Studio 2022 com workload `Desktop development with C++`
- CMake `>=3.27`
- Ninja
- Git

Backends opcionais conforme o escopo da task:

- CUDA Toolkit para path NVIDIA
- Vulkan SDK para fallback cross-vendor
- DirectML / DirectX 12 no Windows
- oneDNN para otimizacoes CPU
- Windows ML / NPU provider em hardware compativel

## Toolchain Notes

Para compilar localmente, abra um `Developer PowerShell for VS` ou shell equivalente que exponha:

- `cmake`
- `ninja`
- `cl` ou `clang-cl`

Se `scripts/start.ps1` falhar dizendo que o compilador nao foi encontrado, a causa mais comum e ausencia do workload C++ do Visual Studio ou shell fora do ambiente de desenvolvimento.

## Environment Variables

| Variable | Required | Example | Notes |
|---|---:|---|---|
| `BASE_URL` | no | `http://127.0.0.1:4173` | Usada por Playwright quando houver fluxo CLI/UX com evidencia. |
| `LLM_PROJECT_MAPPER_SOURCE` | no | `C:\Users\wesley\src\llm-project-mapper` | Faz `scripts/update-starter.ps1` atualizar a partir de um clone local do mapeador. |
| `TEST_COMMAND` | no | `npm run test:cli` | Override de validacao auxiliar do starter. |
| `US4_CLI_PATH` | no | `C:\abs\path\us4-cli.exe` | Override do binario usado pelos testes Playwright e scripts de evidencia. |
| `US4_PROFILE_STORE_PATH` | no | `C:\temp\us4-profiles.json` | Override do arquivo persistido por `tune`. O padrao e `runtime/tuning/profiles.json`. |
| `US4_HAS_CUDA` | no | `1` | Forca o scaffold do probe a simular host com CUDA. |
| `US4_HAS_DIRECTML` | no | `1` | Forca o scaffold do probe a simular host com DirectML. |
| `US4_HAS_VULKAN` | no | `1` | Forca o scaffold do probe a simular host com Vulkan. |
| `US4_HAS_NPU` | no | `1` | Forca o scaffold do probe a simular host com NPU. |
| `US4_CPU_NAME` | no | `Ryzen AI 9 HX 370` | Override do nome de CPU no probe scaffold. |
| `US4_GPU_NAME` | no | `GeForce RTX 4080 Laptop GPU` | Override do nome de GPU no probe scaffold. |
| `US4_HOST_GIB` | no | `32` | Memoria host em GiB usada no probe scaffold. |
| `US4_DEVICE_GIB` | no | `12` | Memoria device em GiB usada no probe scaffold. |
| `US4_STORAGE_GIB` | no | `512` | Budget de storage em GiB usado no probe scaffold. |

## Install

Camada JS do starter:

```powershell
npm install
```

Runtime C++:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUS4_ENABLE_CUDA=ON `
  -DUS4_ENABLE_DIRECTML=ON `
  -DUS4_ENABLE_VULKAN=ON `
  -DUS4_ENABLE_WINDOWS_ML=ON
cmake --build build -j
```

## Start

Helper local:

```powershell
.\scripts\start.ps1
```

Esse helper valida:

- presenca de `CMakeLists.txt`
- `cmake` no `PATH`
- `ninja` no `PATH`
- `cl`, `clang-cl` ou `clang++` no `PATH`

## Smoke The CLI

Depois do build:

```powershell
.\build\us4-cli.exe probe
.\build\us4-cli.exe version
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi" --backend cpu
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi" --backend cpu --format json
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only --format json
.\build\us4-cli.exe tune --model qwen-0.5b --backend cpu --mode cpu-only
```

O que esperar hoje:

- `probe` retorna resumo textual do host.
- `run --backend cpu` executa o baseline CPU scalar.
- `run --format json` expõe o mesmo contrato em JSON para CPU scalar e dry-run.
- `run` para CUDA, DirectML, Vulkan e Windows ML retorna planos dry-run.
- `bench --format json` exporta a matriz atual sem persistir profile.
- `tune` persiste o melhor profile para o host atual em `runtime/tuning/profiles.json` ou em `US4_PROFILE_STORE_PATH`.

## Validate

Validacao consolidada do repo:

```powershell
.\scripts\test.ps1
```

Esse script executa:

- `npm run test:cli`
- `npm run pack:dry`
- parse sintatico de `bootstrap.ps1`
- `ctest --test-dir build --output-on-failure` quando ja existir diretorio de build
- `npx playwright test --project=cli --reporter=list` quando `us4-cli` estiver presente
- gates de correctness e hybrid planner expostos pelo build atual

Validacao local detalhada do runtime:

```powershell
clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h')
clang-tidy -p build (git ls-files 'runtime/**/*.cpp')
ctest --test-dir build --output-on-failure
npx playwright test --reporter=list,html
```

## Validate Bench And Tune

Para exercitar explicitamente o bloco da Sprint 12:

```powershell
$env:US4_PROFILE_STORE_PATH = "$PWD\\runtime\\tuning\\profiles.dev.json"
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only --format json
.\build\us4-cli.exe tune --model qwen-0.5b --backend cpu --mode cpu-only
Get-Content $env:US4_PROFILE_STORE_PATH
```

Use um path temporario quando nao quiser poluir o store padrao do workspace.

## Evidence

Starter / packaging:

```powershell
npm run test:cli
npm run pack:dry
```

CLI/UX runtime:

```powershell
.\scripts\evidence.ps1
```

Quando a task tocar `bench` ou `tune`, a evidencia minima deve incluir:

- `playwright-report/index.html`
- `test-results/`
- stdout/stderr do comando exercitado
- JSON de `bench` quando o contrato de matriz for alterado
- arquivo do profile store persistido quando `tune` for alterado

## Demo Access

- Flow: none
- Demo user: none
- Demo password location: none

Projeto focado em CLI + biblioteca nativa. Nao ha autenticacao de demo nesta fase.
