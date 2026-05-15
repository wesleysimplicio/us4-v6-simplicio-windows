# Feature Notes

Este projeto ainda não possui código de runtime scaffoldado, então este diretório funciona como índice das features centrais previstas pela spec.

## Features principais

| Feature | Goal | Main future areas |
|---|---|---|
| Backend selection | Escolher CUDA/DirectML/Vulkan/CPU/NPU de forma automática e segura. | `runtime/core/`, `runtime/tuning/` |
| Runtime modes | Degradar consumo e capacidade por perfil de hardware. | `runtime/core/`, `runtime/memory/` |
| KV tiering | Manter KV em VRAM/RAM/SSD sem bloquear decode ativo. | `runtime/kv/`, `runtime/cache/` |
| MoE paging | Paginar experts e prever prefetch de MoE. | `runtime/moe/`, `runtime/adapters/` |
| Continuous batching | Compartilhar throughput entre múltiplas sessões. | `runtime/core/`, `runtime/kv/` |
| Speculative decoding | Acelerar geração com P-EAGLE/EAGLE-3. | `runtime/speculative/`, `runtime/core/` |
| Auto-tuning | Ajustar parâmetros por perfil de máquina. | `runtime/tuning/`, `profiles/` |

## Feature docs recomendadas

Crie um arquivo específico quando a feature sair do campo “planejada” para “em implementação”, por exemplo:

```text
docs/features/backend-selection.md
docs/features/kv-tiering.md
docs/features/moe-paging.md
docs/features/speculative-decoding.md
docs/features/msix-release.md
```

## O que cada feature doc deve conter

- objetivo funcional
- perfis de hardware impactados
- adapters impactados
- backends impactados
- módulos e arquivos principais
- riscos de correctness
- benchmark esperado
- evidência exigida

## Evidência mínima por tipo de feature

- CLI/UX: screenshot + trace + video Playwright
- backend/kernel: benchmark + correctness diff + logs
- memory paging: page fault stats + latency report
- release/install: screenshots do fluxo + artefatos gerados
