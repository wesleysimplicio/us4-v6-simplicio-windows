# ADR-001: Establish a single Windows adapter interface for all model families

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

O runtime precisa executar famílias de modelos heterogêneas sem espalhar conhecimento de arquitetura de modelo pelo core ou pelos backends. A Sprint 01 exige um contrato explícito para adapters, e o design já separa política de runtime, detalhes de dispositivo e detalhes de modelo em boundaries distintos.

Sem um contrato único, cada família tenderia a expor superfícies ad hoc, dificultando seleção de backend, testes, telemetria e evolução de `run`, batching e speculative decoding.

## Decisão

Adotamos `IUS4WindowsAdapter` como interface única de integração entre core e adapters.

- O core fala com adapters por meio de `Describe`, `Attach`, `LoadModel`, `Prefill`, `Generate` e `Reset`.
- O adapter descreve capacidades de família e recebe `RuntimeBinding` já resolvido pelo core.
- Seleção de hardware continua fora do adapter.
- O contrato mínimo inicial aceita implementação nula (`NullWindowsAdapter`) para scaffolding e testes.

## Consequências

### Positivas (+)

- Boundary claro entre core e famílias de modelo.
- Facilita introduzir adapters densos, MoE e ternários sem alterar a CLI a cada família.
- Permite testes e telemetria consistentes por adapter.

### Negativas (-)

- A interface inicial ainda é simplificada e precisará evoluir para batching, streaming e erros tipados.
- Existe risco de inflar a interface cedo demais se novas famílias trouxerem necessidades conflitantes.
- O `NullWindowsAdapter` pode mascarar profundidade funcional ausente se for usado além do scaffold.

### Neutras / observações

- A evolução para `std::expected<T, Error>` continua planejada, mas não é bloqueadora da Sprint 01.

## Alternativas consideradas

### Alternativa A — Core chamar implementações concretas diretamente

- Resumo: o core instancia e chama cada adapter sem interface estável.
- Por que foi descartada: acopla o runtime a detalhes de família e torna evolução/testes mais caros.

### Alternativa B — Interface separada por família de modelo

- Resumo: cada família teria sua própria interface.
- Por que foi descartada: aumenta a fragmentação exatamente onde o produto quer unificar comportamento.

## Critério de revisão

- Revisar quando batching contínuo, speculative decoding ou streaming exigirem ampliar o contrato.
- Revisar se duas famílias diferentes exigirem semânticas incompatíveis na mesma interface.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
