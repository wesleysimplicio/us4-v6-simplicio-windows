# ADR-003: Derive runtime mode from hardware budget with explicit overrides

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

O roadmap depende de modos operacionais previsíveis (`FULL`, `BALANCED`, `DEGRADED`, `ULTRA_LOW`, `MICRO`, `NANO`, `CPU_ONLY`) para acomodar diferenças fortes de VRAM, RAM e dispositivo. A Sprint 01 exige enumeração desses modos e heurísticas iniciais de escolha.

Sem uma política de modo explícita, o runtime perde previsibilidade, perfis ficam soltos e o comportamento entre máquinas deixa de ser auditável.

## Decisão

Adotamos `RuntimeMode` como política explícita do core, escolhida por heurística de hardware e passível de override via CLI.

- `RuntimeMode` é parte do contrato de `SessionRequest`.
- O modo padrão é derivado pelo core a partir do backend selecionado e do budget disponível.
- `--mode` permite override explícito.
- Perfis de runtime em `profiles/` mapeiam diretamente para esses modos.

## Consequências

### Positivas (+)

- Comportamento previsível por classe de hardware.
- Facilita autotune futuro e presets persistentes.
- Simplifica comunicação em docs, telemetria e suporte.

### Negativas (-)

- Heurística inicial é conservadora e ainda não usa benchmark real.
- Modos podem precisar ser recalibrados quando backends reais entrarem em produção.
- Existe risco de overfitting da heurística ao scaffold atual.

### Neutras / observações

- `auto` na CLI significa “deixar o core escolher”.

## Alternativas consideradas

### Alternativa A — Só perfis nomeados, sem enum de modo

- Resumo: o runtime só trabalharia com IDs de perfil.
- Por que foi descartada: perfis ajudam, mas o modo ainda precisa existir como contrato transversal.

### Alternativa B — Sempre usar `BALANCED`

- Resumo: um único modo padrão para todas as máquinas.
- Por que foi descartada: não representa a realidade de budgets muito diferentes.

## Critério de revisão

- Revisar quando benchmark/correctness mostrarem que a heurística de budget gera escolhas sistematicamente ruins.
- Revisar quando perfis persistentes passarem a sobrescrever a heurística automática.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
