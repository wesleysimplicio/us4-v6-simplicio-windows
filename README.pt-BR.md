# US4 V6 - Windows Edition

> Universal State Runtime para inferencia local de LLMs em Windows x86-64 (NVIDIA / AMD / Intel + NPU opcional).
> pt-BR. EN version: [README.md](README.md).

![us4-v6-simplicio-windows](us4-v6-simplicio-windows.PNG)

## O que e

US4 V6 e um runtime C++ para inferencia local de LLMs em Windows x86-64 com dispatch adaptativo entre CPU, CUDA, DirectML, Vulkan e caminhos opcionais via Windows ML / NPU. O projeto ja possui scaffold inicial de runtime, selecao de backend orientada a hardware, uma CLI executavel, testes unitarios iniciais e documentacao de arquitetura/produto preenchida para sustentar os proximos sprints.

Spec master de referencia: [`US4-V6-Windows-Edition.md`](./US4-V6-Windows-Edition.md).

## Status Atual

O repositorio ja saiu do estado de planejamento puro. A base atual inclui:

- scaffold do runtime Windows em CMake dentro de [`runtime/`](./runtime/)
- modelo de capacidades e fluxo de selecao para CUDA, DirectML, Vulkan e fallback CPU
- pipeline de probe de hardware com selecao de modo e relatorio de budgets
- executavel CLI inicial: `us4-cli`
- testes unitarios iniciais para probe e formatacao
- documentacao de arquitetura, patterns, dominio, workflow e sprints preenchida em [`.specs/`](./.specs/)

Na pratica, o repo esta em fase inicial de implementacao: as fundacoes existem, enquanto execucao real de modelos, profundidade dos adapters, harnesses de correctness e especializacao por backend seguem evoluindo sprint a sprint.

## Baseline Implementado

O scaffold atual do runtime cobre estas areas:

- `runtime/core/`: resumo de probe do host e wiring voltado para a CLI
- `runtime/backends/`: descritores de backend, deteccao de capacidades e logica de selecao
- `runtime/adapters/`: contratos de adapter e baseline de null adapter para Windows
- `runtime/memory/`, `runtime/kv/`, `runtime/cache/`, `runtime/moe/`, `runtime/speculative/`, `runtime/tuning/`, `runtime/telemetry/`: esqueletos iniciais para expansao nos proximos sprints
- `runtime/benchmarks/`: scaffold do entrypoint de benchmarks
- `profiles/`: presets de perfil de hardware/runtime

A CLI atual ja suporta probe e estabelece o contrato para os futuros fluxos de `run`.

## Stack

- C++17/20 + CMake + Ninja
- CUDA
- DirectML
- Vulkan compute
- oneDNN / AVX2 / AVX-512 / AMX
- Windows ML para caminhos opcionais de NPU
- GoogleTest para cobertura unitaria
- Playwright para evidencia E2E de CLI/UX
- GitHub Actions para gates de CI / DoD

## Setup Local

Setup recomendado em Windows:

1. Instale o Visual Studio 2022 com MSVC e ferramentas de build C++.
2. Garanta CMake e Ninja disponiveis, normalmente via ambiente de desenvolvimento do Visual Studio.
3. Abra um Developer PowerShell / VS Dev shell.
4. Configure e compile:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

5. Rode o probe:

```powershell
.\build\us4-cli.exe --probe
```

6. Rode os testes unitarios:

```powershell
ctest --test-dir build --output-on-failure
```

Se o foco for apenas a camada do starter/documentacao, [`scripts/test.ps1`](./scripts/test.ps1) continua validando separadamente a parte de bootstrap/packaging.

## Layout do Repo

```text
.specs/                 Documentacao de produto, arquitetura, workflow e sprints
runtime/
  core/                 Probe do host e runtime core voltado para CLI
  backends/             Deteccao de capacidades e selecao de backend
  adapters/             Contratos de runtime adapters e baseline de null adapter
  memory/ kv/ cache/    Esqueletos de memoria e paginacao
  moe/ speculative/     Scaffolds de MoE e decoding
  tuning/ telemetry/    Scaffolds de tuning e observabilidade
  benchmarks/           Scaffold de benchmarks
profiles/               Presets de perfil de hardware/runtime
tests/unit/             Cobertura inicial com GoogleTest
```

## Como Trabalhar Aqui

Este repo segue o contrato de [`AGENTS.md`](./AGENTS.md). As instrucoes canonicas para agentes ficam em [`AGENTS.md`](./AGENTS.md) e sao espelhadas em [`CLAUDE.md`](./CLAUDE.md) e [`.github/copilot-instructions.md`](./.github/copilot-instructions.md).

Para trabalho tecnico, o loop esperado continua sendo: ler a task, carregar o contexto arquitetural, editar de forma cirurgica, rodar format/lint/unit, executar Playwright em mudancas de CLI/UX e anexar evidencia no PR.

## Proximos Marcos

Os proximos marcos principais sao:

- aprofundar o baseline de CPU alem do scaffold inicial
- conectar implementacoes reais de adapters ao caminho de execucao da CLI
- expandir harnesses de correctness e benchmark por backend
- levar CUDA, DirectML e Vulkan de suporte em nivel de seletor para adapters prontos para execucao
- alinhar CI/DoD com o workflow completo do runtime C++

## Fora de Escopo

- Inferencia em nuvem ou distribuida
- Treino ou fine-tuning
- Suporte Linux/macOS nesta edicao
- Aplicacao desktop GUI no lugar do foco em CLI/runtime

## Licenca

A politica de licenciamento ainda nao foi publicada. Trate o repositorio como interno/controlado pelo projeto ate a definicao da primeira politica de release publica.
