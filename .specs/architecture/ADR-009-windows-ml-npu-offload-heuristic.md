# ADR-009: Use opt-in hybrid partitioning for Windows ML / NPU offload

## Status

Aceito

## Data

2026-05-15

## Autores

- us4-core

## Contexto

Sprint 11 exige um caminho de Windows ML / NPU que preserve correctness e degrade de forma segura quando a sessao nao estiver em hardware NPU-capable ou quando o caller nao fizer opt-in explicito. O planner atual em `runtime/backends/windows_ml/windows_ml_execution_plan.h` ja decidiu uma heuristica concreta para compile de grafo, prefill, decode e compressao de KV, mas isso ainda nao estava documentado como decisao arquitetural.

Essa decisao precisa ser explicita porque o produto promete NPU como aceleracao opcional, nunca como caminho unico. Ela tambem precisa permanecer alinhada com `ADR-002`, `ADR-003` e `ADR-008`: o core escolhe backend e modo antes do adapter, correctness vence throughput, e fallback observavel e obrigatorio.

## Decisao

Adotamos um planner Windows ML em modo hibrido e opt-in, no qual apenas particoes densas elegiveis sao offloadadas para NPU e todo o restante permanece com assistencia host ou fallback explicito.

- O opt-in para NPU so e considerado satisfeito quando `allowNpu` estiver ativo e a sessao mirar `windows-ml`, `npu` ou `auto` com binding Windows ML.
- `prefill` so e offloadado fora de `NANO`; `decode` so e offloadado quando a sessao pedir baixa latencia ou operar em `ULTRA_LOW` ou `MICRO`.
- Pesos quantizados em `INT8` sao o default fora de requests `FP32`.
- Compressao de KV permanece no host em `MICRO`, `NANO` e contextos longos.
- MoE, speculative e vision continuam criando particoes de host assist ate a cobertura de operadores crescer.
- Sem opt-in satisfeito, o planner sempre materializa particao `cpu-fallback` e issue observavel.

## Consequencias

### Positivas (+)

- O caminho NPU permanece seguro e auditavel sem prometer execucao completa onde o backend ainda nao cobre tudo.
- A politica fica previsivel entre familias de modelo porque a decisao mora no core/planner, nao nos adapters.
- O runtime consegue expor `issues`, `partition_count`, `batch_hint` e `context_hint` de forma estavel para CLI, testes e telemetria.

### Negativas (-)

- O ganho de throughput inicial fica limitado porque parte do pipeline continua no host.
- A matrix de teste cresce com combinacoes de opt-in, modo e capacidades do modelo.
- Existe risco de subutilizacao de NPU em casos onde o device suportaria mais offload do que a heuristica atual permite.

### Neutras / observacoes

- Esta decisao formaliza o estado atual do planner; ela nao implica que a implementacao de execucao completa ja esteja pronta.
- O caminho hibrido continua dependente de fallback GPU ou CPU para fechar correctness end-to-end.

## Alternativas consideradas

### Alternativa A - NPU como backend exclusivo quando presente

- Resumo: promover Windows ML / NPU a caminho primario sempre que houver hardware compativel.
- Por que foi descartada: quebraria a exigencia de correctness-first e deixaria a sessao vulneravel a gaps de operadores ou shapes.

### Alternativa B - Offload manual decidido por cada adapter

- Resumo: cada adapter definiria quais camadas enviar para Windows ML.
- Por que foi descartada: espalha politica, dificulta telemetria e torna os fallbacks menos previsiveis.

## Criterio de revisao

- Revisar quando Windows ML sustentar decode completo sem host assist para pelo menos um adapter dense principal.
- Revisar quando benchmarks mostrarem que `prefill` ou `decode` deveriam subir ou descer na ordem de offload para uma classe relevante de NPUs.
- Revisar quando o backend passar a cobrir MoE, speculative ou vision sem ajuda do host.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-002](./ADR-002-backend-selection.md), [ADR-003](./ADR-003-runtime-mode.md), [ADR-008](./ADR-008-directml-vulkan-windows-ml-fallback-strategy.md)
