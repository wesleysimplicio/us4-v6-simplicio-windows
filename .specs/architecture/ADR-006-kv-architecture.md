# ADR-006: Establish tiered KV residency with prefix reuse and summary fallback

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

As sprints seguintes dependem de uma base de KV que funcione em budgets muito diferentes sem quebrar correctness ou tornar o runtime imprevisível. A Sprint 06 pede paginação quente/fria, prefix cache, sumarização e telemetria. Sem uma arquitetura mínima comum, cada adapter ou backend tenderia a carregar sua própria política local e o comportamento deixaria de ser auditável.

Também precisamos de uma base que já sirva ao caminho `CPU_ONLY` atual, porque é o trilho funcional disponível para validar contratos antes de CUDA, DirectML, MoE e Llama completos entrarem.

## Decisão

Adotamos uma arquitetura de KV em camadas com tiers explícitos (`device`, `host`, `storage`, `summary`), prefix cache por chave estável de prompt e summarizer determinístico para modos degradados.

- `KvPager` é o dono da residência e do rebalanço entre tiers.
- `PrefixCache` é responsável apenas por reaproveitamento de prefixos, não por política de residência.
- `Summarizer` reduz contexto antigo mantendo janela de cabeça/cauda determinística.
- O runtime reporta estatísticas de KV e cache no caminho de execução para alimentar telemetria e gates futuros.

## Consequências

### Positivas (+)

- O contrato de KV fica auditável desde já no runtime CPU.
- Adapters futuros reaproveitam a mesma política base em vez de duplicar regras.
- Correctness e telemetria passam a ter pontos claros de integração.

### Negativas (-)

- A implementação inicial ainda é conservadora e simplificada frente ao desenho final com VRAM real e SSD mmap.
- Regras de rebalanço ainda não usam custo observado por hardware real.
- A sumarização atual é estrutural, não semântica.

### Neutras / observações

- `device` pode permanecer lógico no caminho CPU até backends GPU reais assumirem o tier quente.

## Alternativas consideradas

### Alternativa A — Cada adapter gerencia o próprio KV

- Resumo: Qwen, Gemma, Llama e MoE teriam políticas independentes.
- Por que foi descartada: fragmenta o comportamento e enfraquece a telemetria transversal.

### Alternativa B — Só cache em RAM sem tiers

- Resumo: manter tudo em host memory e adiar paginação.
- Por que foi descartada: não prepara o runtime para budgets baixos nem para futuras rotas GPU/NPU.

## Critério de revisão

- Revisar quando CUDA/DirectML reais exigirem page movement assíncrono ou budgets por dispositivo físico.
- Revisar quando a sumarização estrutural causar drift de correctness acima da tolerância da sprint.
- Revisar quando MoE e Llama precisarem de política diferenciada por expert ou por attention group.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-001](./ADR-001-adapter-interface.md), [ADR-003](./ADR-003-runtime-mode.md)
