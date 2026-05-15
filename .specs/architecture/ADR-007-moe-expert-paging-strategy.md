# ADR-007: Establish staged MoE expert paging with router telemetry

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

Os adapters MoE do roadmap precisam decidir rapidamente quais experts ficam quentes, mornos ou frios sem espalhar essa lógica por backend e por família de modelo. A Sprint 08 exige um `Router`, um `ExpertPager` e telemetria de hit-rate/eviction. Sem esse contrato, DeepSeek e Kimi tenderiam a divergir cedo demais e o runtime perderia previsibilidade.

Também precisamos de uma fundação verificável no trilho CPU antes de conectar paginação real entre VRAM, RAM e SSD nos backends GPU.

## Decisão

Adotamos uma estratégia em duas camadas para MoE: o `ExpertRouter` escolhe top-k e produz métricas de distribuição, enquanto o `ExpertPager` mantém experts em estágios `hot`, `warm` e `cold`.

- O router é dono de score, top-k e telemetria de entropia/load-balance.
- O pager é dono de residência, promoções e evicções.
- Adapters MoE observam ambos os contratos, mas não redefinem a política base.
- O caminho CPU scalar usa essa infraestrutura como scaffold executável para DeepSeek e Kimi.

## Consequências

### Positivas (+)

- DeepSeek e Kimi compartilham a mesma base de paginação desde o início.
- Telemetria de MoE nasce junto do contrato, não como pós-processamento.
- A promoção para backends GPU depois fica incremental em vez de arquitetural.

### Negativas (-)

- A residência atual ainda é lógica; não há movimentação física entre VRAM/RAM/SSD.
- A política inicial usa heurísticas simples de score e LRU/frequência.
- Expert weights lazy-loaded por shard ainda ficam para o próximo passo.

### Neutras / observações

- `hot/warm/cold` já modela o desenho final, mesmo quando a execução atual roda toda em CPU.

## Alternativas consideradas

### Alternativa A — Cada adapter MoE gerencia experts por conta própria

- Resumo: DeepSeek e Kimi teriam pagers independentes.
- Por que foi descartada: duplicaria política, métricas e bugs.

### Alternativa B — Só router sem pager

- Resumo: escolher experts, mas assumir que todos ficam residentes.
- Por que foi descartada: não prepara o runtime para budgets reais nem para cold experts.

## Critério de revisão

- Revisar quando o backend GPU real exigir paginação assíncrona por device.
- Revisar quando expert load/unload lazy por shard entrar em produção.
- Revisar quando métricas de entropia e hit-rate mostrarem desequilíbrio sistemático por família de modelo.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-006](./ADR-006-kv-architecture.md)
