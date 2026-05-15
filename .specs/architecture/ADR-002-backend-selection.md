# ADR-002: Centralize backend selection in hardware-aware core policy

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

O produto promete um contrato único para NVIDIA, AMD, Intel, CPU e NPU opcional. Essa promessa só é sustentável se a escolha de backend for feita em um lugar único, previsível e testável. A Sprint 01 exige `HardwareProbe` e `BackendSelector` com heurísticas claras.

Deixar adapters ou entrypoints tomarem decisões de hardware fragmentaria a política de fallback e enfraqueceria a telemetria de correctness.

## Decisão

Centralizamos a escolha de backend no core por meio de `HardwareProbe` e `BackendSelector`.

- `HardwareProbe` coleta capacidades e budgets do host.
- `BackendSelector` monta catálogo e seleciona o backend primário.
- Ordem padrão: NVIDIA prioriza CUDA; AMD/Intel priorizam DirectML; Vulkan atua como fallback cross-vendor; CPU sempre existe; NPU é opt-in.
- Adapters recebem o binding resolvido e não inferem hardware por conta própria.

## Consequências

### Positivas (+)

- Política de seleção testável e observável em um único lugar.
- Fallback previsível entre backends.
- Menos acoplamento entre famílias de modelo e detalhes de dispositivo.

### Negativas (-)

- Heurísticas iniciais podem simplificar demais cenários reais de hardware híbrido.
- Regras de seleção precisarão amadurecer junto com autotune e dados de benchmark.
- NPU opt-in adiciona um ramo extra de comportamento para validar.

### Neutras / observações

- Nesta fase o probe ainda é scaffoldado via ambiente; integração com SDKs reais vem em sprints posteriores.

## Alternativas consideradas

### Alternativa A — Deixar cada adapter escolher o backend

- Resumo: adapters tomariam a decisão com base em suas próprias necessidades.
- Por que foi descartada: política de hardware ficaria espalhada e inconsistente.

### Alternativa B — Escolha manual obrigatória via CLI

- Resumo: usuário sempre define o backend explicitamente.
- Por que foi descartada: piora UX e contradiz a proposta de runtime adaptativo.

## Critério de revisão

- Revisar quando benchmarks mostrarem que a ordem padrão erra sistematicamente em uma classe de hardware.
- Revisar quando autotune persistente passar a influenciar a seleção primária.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
