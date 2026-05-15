# ADR-008: Use a capability-driven fallback ladder for DirectML, Vulkan, and Windows ML

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

O contrato do produto promete aceleracao fora de NVIDIA sem prender o runtime a um unico backend proprietario. A direcao ja registrada em `DESIGN.md` e `ADR-002` diz que AMD e Intel priorizam DirectML, Vulkan e o fallback cross-vendor, e Windows ML/NPU fica como opt-in. O que faltava era formalizar como essa cadeia de degradacao deve funcionar quando um backend nao cobre um operador, um grafo fica grande demais ou um dispositivo tem suporte parcial.

Essa decisao tambem precisa conversar com a realidade atual do repositorio: o core ja escolhe backend e modo antes de entrar no adapter, e correctness tem precedencia sobre throughput. Logo, a estrategia de fallback nao pode depender de heuristicas privadas por familia de modelo.

## Decisao

Adotamos uma cadeia de fallback orientada por capacidade em que DirectML e a rota principal cross-vendor, Vulkan e o fallback de GPU de menor nivel, e Windows ML participa apenas como opt-in para offload denso e com fallback obrigatorio.

- Em hardware AMD ou Intel com GPU util, DirectML e a primeira tentativa para dense e MoE quando houver cobertura de operadores e budgets compativeis.
- Vulkan entra como fallback de GPU quando DirectML nao suportar o operador, falhar no compile do grafo ou nao couber no budget operacional esperado.
- Windows ML/NPU e sempre opt-in e nunca o unico caminho de correctness; ele serve para offload de subgrafos densos e workloads pequenos ou moderados em dispositivos NPU-capable.
- O runtime registra `fallback_reason`, backend final, modo final e impacto de budget sempre que houver degradacao de DirectML, Vulkan ou Windows ML.
- Adapters continuam declarando capacidades do modelo; a escolha e a troca entre DirectML, Vulkan, Windows ML e CPU pertencem ao core e aos backends.

## Consequencias

### Positivas (+)

- O runtime preserva a promessa de funcionar em AMD, Intel e NPU-capable sem virar um conjunto de excecoes por modelo.
- Fallback fica observavel e auditavel, o que reforca gates de correctness e suporte operacional.
- Windows ML entra de forma pragmatica, ajudando offload onde fizer sentido sem virar dependencia obrigatoria.

### Negativas (-)

- A matriz de teste cresce porque a mesma familia de modelo pode degradar entre varios backends.
- Graph compile e cobertura de operadores em DirectML/Windows ML podem introduzir comportamentos nao triviais de chunking ou fallback parcial.
- Vulkan como fallback exige manutencao mesmo quando nao e o backend primario da maioria das maquinas.

### Neutras / observacoes

- Esta decisao ratifica a direcao de Sprint 03 e prepara Sprint 11; ela nao exige que todos esses backends estejam completos no mesmo marco de implementacao.
- NPU continua subordinada a correctness e a fallback seguro para GPU ou CPU.

## Alternativas consideradas

### Alternativa A - Escolher um unico backend cross-vendor e abandonar os demais

- Resumo: padronizar tudo em DirectML ou tudo em Vulkan.
- Por que foi descartada: reduz superficie, mas enfraquece resiliencia, cobertura de hardware e espaco para NPU opt-in.

### Alternativa B - Deixar fallback ser decidido dentro de cada adapter

- Resumo: cada familia de modelo faria sua propria cadeia DirectML/Vulkan/Windows ML/CPU.
- Por que foi descartada: espalha politica, dificulta telemetria e torna o comportamento menos previsivel.

## Criterio de revisao

- Revisar quando Windows ML deixar de ser apenas opt-in e passar a sustentar sessoes completas com cobertura de operadores suficiente.
- Revisar quando benchmarks mostrarem que Vulkan deve subir na ordem para uma classe importante de hardware AMD ou Intel.
- Revisar quando DirectML ou Vulkan introduzirem capacidades que reduzam a necessidade de degradacao por operator gap ou limite de compile.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-002](./ADR-002-backend-selection.md), [ADR-003](./ADR-003-runtime-mode.md), [ADR-004](./ADR-004-cuda-execution-strategy.md)
