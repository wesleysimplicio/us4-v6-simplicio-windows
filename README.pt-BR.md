# US4 V6 — Windows Edition

> Universal State Runtime pra inferência LLM local em Windows x86-64 (NVIDIA / AMD / Intel + NPU opcional).
> pt-BR. EN version: [README.md](README.md).

![us4-v6-simplicio-windows](us4-v6-simplicio-windows.PNG)

## O que é

US4 V6 é um runtime C++ que roda famílias modernas de LLM (DeepSeek MoE, Kimi MoE, MiniMax, GLM, Qwen, Llama, Gemma, BitNet, PT-BitNet Ternary) **localmente** em Windows x86-64, com dispatch adaptativo de backend entre **CPU scalar / AVX2 / AVX-512 / AMX / oneDNN / CUDA / DirectML / Vulkan / Windows ML (NPU)**, paginação hot-cold de KV entre VRAM/RAM/SSD, paginação de experts MoE, decodificação especulativa, batching contínuo, reuso de CUDA Graphs e auto-tuning ciente de hardware em 8 perfis.

Modos de runtime: `FULL`, `BALANCED`, `DEGRADED`, `ULTRA_LOW`, `MICRO`, `NANO`, `CPU_ONLY` — auto-selecionados pelo probe de hardware no boot, sobrescrevíveis via flag de CLI.

Estratégia do seletor de backend: NVIDIA → CUDA, GPU dedicada AMD/Intel → DirectML, fallback Vulkan, CPU AVX sempre ligado, NPU (Snapdragon X / Intel Core Ultra / AMD Ryzen AI) opt-in pra offload de camada densa.

Spec master de referência: [`US4-V6-Windows-Edition.md`](../US4-V6-Windows-Edition.md).

## Stack

- C++17/20 + CMake/Ninja
- CUDA (streams NVIDIA, memory pool, CUDA Graphs)
- DirectML (compile de grafo cross-vendor)
- Vulkan compute (fallback AMD/Intel/cross-vendor)
- oneDNN (block GEMM, ciente de AVX/AMX)
- AVX2 / AVX-512 / AMX (caminhos quentes SIMD de CPU, quant BF16/INT8)
- Windows ML (offload pra NPU — Snapdragon X / Intel Core Ultra / AMD Ryzen AI)
- GoogleTest (unit), Playwright (CLI/UX E2E), Ralph Loop (fix/verify autônomo)
- GitHub Actions (CI + gates de DoD), instalador MSIX

## Status

**Planejamento completo.** Os 12 sprints estão scaffolded em [`.specs/sprints/`](.specs/sprints/). A implementação ainda não começou — `DESIGN.md`/`PATTERNS.md`/ADRs de arquitetura são preenchidos **durante** a primeira task de cada sprint, não antes.

| Sprint | Tema |
|---|---|
| 01 | Foundations & Skeleton |
| 02 | CPU Scalar Baseline |
| 03 | CUDA + DirectML Skeleton |
| 04 | AVX Hot Paths + oneDNN |
| 05 | BitNet + Ternary |
| 06 | KV Memory Architecture (VRAM/RAM/SSD) |
| 07 | Llama Adapter |
| 08 | MoE Foundation (DeepSeek + Kimi) |
| 09 | MoE Advanced (MiniMax + GLM + SP-MoE) |
| 10 | Batching + Speculative Decoding |
| 11 | Vulkan + Windows ML/NPU |
| 12 | Auto-Tune + MSIX Release v1.0 |

Matriz completa: [`.specs/sprints/BACKLOG.md`](.specs/sprints/BACKLOG.md).

## Como o agente trabalha aqui

Este repo segue a convenção **Agentic Starter**. Arquivo master de instruções: [`AGENTS.md`](AGENTS.md) (espelhado em [`CLAUDE.md`](CLAUDE.md) e [`.github/copilot-instructions.md`](.github/copilot-instructions.md)).

Toda task técnica passa pelo loop obrigatório: ler task → planejar → carregar contexto → editar → lint → unit → Playwright E2E (trace+screenshot+video) → corrigir → commit convencional (em inglês) → PR com checklist DoD.

Gate de DoD (forçado por [`.github/workflows/dod.yml`](.github/workflows/dod.yml)): lint verde, unit verde com ≥80% de cobertura nos arquivos tocados, Playwright E2E verde com evidência, todos os AC marcados, ADR adicionado se decisão arquitetural, changelog atualizado se release-relevant.

## Layout do repo

```
.specs/
  product/       VISION.md, DOMAIN.md, PERSONAS.md
  architecture/  DESIGN.md, PATTERNS.md, ADR-*.md (preenchidos durante os sprints)
  workflow/      WORKFLOW.md, CONTRIBUTING.md, RELEASE.md
  sprints/       BACKLOG.md, sprint-01..12/SPRINT.md
.skills/         capacidades reutilizáveis dos agentes
.agents/         sub-agentes customizados (ralph-loop, tdd, reviewer, architect)
.claude/         settings + hooks (post-edit lint, pre-commit gate)
.github/         workflows (ci.yml, dod.yml), templates de PR/Issue
runtime/         código C++ (criado no sprint-01: core, adapters, memory, kv, cache, moe, cuda, directml, vulkan, avx, onednn, windowsml, speculative, tuning, telemetry, benchmarks)
```

## Fora de escopo

- Inferência em nuvem/distribuída (foco em máquina única).
- Treino/fine-tuning (somente inferência).
- Apple Silicon → ver [US4 V6 Apple Edition](https://github.com/wesleysimplicio/us4-v6-simplicio-apple).
- Suporte Linux/macOS (Windows-only).
- App desktop GUI (somente CLI + biblioteca).

## Licença

TBD (declarada antes do release v1.0 no Sprint 12).
