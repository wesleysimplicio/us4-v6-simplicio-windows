# ADR-010: Prefer DirectML first and keep Vulkan as the explicit cross-vendor fallback

## Status

Aceito

## Data

2026-05-15

## Autores

- us4-core

## Contexto

Sprint 11 pede Vulkan e Windows ML, mas o repositorio ja carrega uma cadeia de escolha mais detalhada entre DirectML e Vulkan do que a descrita superficialmente em `ADR-008`. O planner Vulkan em `runtime/backends/vulkan/include/us4/runtime/backends/vulkan/vulkan_execution_plan.h` incorpora regras de spill de KV, micro-batch, semaforos e fallback de attention em `NANO`. Ao mesmo tempo, o selector e os testes continuam tratando DirectML como primeira opcao para AMD e Intel.

Como essa preferencia interfere em correctness, budgets e telemetria de fallback, a relacao entre DirectML e Vulkan precisa virar ADR propria.

## Decisao

Preferimos DirectML como primeira escolha cross-vendor para AMD e Intel, e mantemos Vulkan como fallback explicito de GPU quando o compile de grafo, a cobertura de operadores ou o budget operacional do DirectML nao forem suficientes.

- DirectML continua sendo a primeira tentativa para sessoes dense e mistas em GPUs AMD ou Intel.
- Vulkan entra como fallback consciente, nao como empate, e precisa expor o motivo da degradacao por `issues` e telemetria.
- O planner Vulkan pode usar semaforos de timeline, descriptor buffer e KV persistente quando o modo e o device permitirem.
- Em `NANO`, Vulkan pode demover decode attention para CPU para preservar correctness no menor budget.
- Pedidos de NPU nao mudam a ordem DirectML -> Vulkan; quando Vulkan entra, ele ignora opt-in de NPU e permanece em rota GPU/CPU hibrida.

## Consequencias

### Positivas (+)

- A ordem de escolha fica consistente com o contrato do produto e com as expectativas para hardware AMD/Intel no Windows.
- Vulkan continua valioso como fallback cross-vendor sem competir com DirectML em todos os cenarios.
- O runtime consegue explicar claramente por que caiu de DirectML para Vulkan ou de Vulkan para CPU em modos restritos.

### Negativas (-)

- A validacao precisa cobrir mais combinacoes porque o fallback de GPU vira parte do contrato.
- Vulkan carrega custo de manutencao mesmo quando nao e o backend primario da maior parte das sessoes.
- Pode haver casos em que Vulkan performe melhor, mas ainda assim entre como fallback por decisao de produto e operabilidade.

### Neutras / observacoes

- Esta decisao complementa, e nao substitui, `ADR-008`.
- A ordem aqui nao impede futuras promocoes de Vulkan para casos especificos, desde que novo ADR documente a excecao.

## Alternativas consideradas

### Alternativa A - Escolha puramente por benchmark local sem backend preferencial

- Resumo: medir DirectML e Vulkan em toda sessao e escolher sempre o mais rapido.
- Por que foi descartada: aumenta custo de startup, complica operacao e dificulta previsibilidade em ambientes heterogeneos.

### Alternativa B - Vulkan como backend primario para toda GPU nao-NVIDIA

- Resumo: simplificar a matriz elevando Vulkan acima de DirectML.
- Por que foi descartada: conflita com o posicionamento atual do produto e com a melhor integracao de DirectML no stack Windows.

## Criterio de revisao

- Revisar quando benchmarks sustentados mostrarem vantagem recorrente de Vulkan sobre DirectML em uma classe importante de GPUs AMD ou Intel.
- Revisar quando DirectML perder cobertura de operadores relevante para adapters principais e tornar o fallback frequente demais.
- Revisar quando o runtime tiver metricas historicas suficientes de `fallback_reason` para reordenar a ladder por perfil de hardware.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-002](./ADR-002-backend-selection.md), [ADR-008](./ADR-008-directml-vulkan-windows-ml-fallback-strategy.md), [ADR-009](./ADR-009-windows-ml-npu-offload-heuristic.md)
