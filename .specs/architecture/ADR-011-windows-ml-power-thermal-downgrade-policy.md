#ADR - 011 : Downgrade Windows ML mixed dispatch under power and thermal pressure

## Status

Aceito

## Data

2026-05-15

## Autores

- us4-core

## Contexto

Sprint 11 pede um monitor de energia e temperatura para o caminho `Windows ML + Vulkan`, com degradacao segura do dispatch quando o host entra em battery saver, thermal throttle ou sinal equivalente vindo de ETW. Ate aqui o repositorio ja conseguia montar `WinMlAdapter`, `LayerOffloader` e `MixedDispatchPlanner`, mas ainda tratava o plano misto como se o ambiente estivesse sempre nominal.

Sem uma politica explicita, o runtime ficaria cego a eventos de bateria e temperatura e poderia insistir em densos no NPU mesmo quando o sistema estivesse pedindo contencao.

## Decisao

Adotamos um monitor leve de energia/temperatura no backend `windows_ml`, com leitura real de `GetSystemPowerStatus` quando disponivel e overrides por env para cenarios sinteticos de teste. Esse monitor escolhe uma politica de dispatch que o `MixedDispatchPlanner` aplica sobre os slices densos do NPU.

- `nominal`: nao altera o dispatch misto.
- `prefer-efficiency`: preserva prefill denso no NPU, mas pode demover decode denso sensivel a latencia para o caminho GPU primario.
- `thermal-throttle`: demove slices densos do NPU para o fallback GPU/CPU.
- `critical-fallback`: remove o uso denso do NPU e cai no fallback GPU/CPU de forma mais agressiva.
- O monitor aceita overrides `US4_POWER_SOURCE`, `US4_BATTERY_PERCENT`, `US4_BATTERY_SAVER`, `US4_THERMAL_STATE` e `US4_ETW_THROTTLED` para testes deterministas e evidence em CLI.

## Consequencias

### Positivas (+)

- O planner misto passa a reagir a pressao operacional sem precisar de hardware real em toda suite.
- A CLI consegue explicar claramente quando um plano `Windows ML` foi degradado por energia ou temperatura.
- A cobertura de `Sprint 11` ganha um contrato estavel para futuros coletores reais de ETW.

### Negativas (-)

- O monitor ainda usa `GetSystemPowerStatus` como fonte real principal e trata ETW como sinal sintetico/plugavel nesta fase.
- A heuristica pode ser conservadora em alguns dispositivos, especialmente quando GPU e NPU competem por envelopes termicos diferentes.

### Neutras / observacoes

- Esta ADR complementa `ADR-009` e `ADR-010`.
- O fallback continua priorizando correctness e explicabilidade, nao throughput maximo.

## Alternativas consideradas

### Alternativa A - Ignorar energia/temperatura ate existir ETW completo

- Resumo: manter o planner atual e esperar a integracao final de ETW.
- Por que foi descartada: deixa a Sprint 11 sem a degradacao exigida e sem contrato testavel.

### Alternativa B - Degradar tudo direto para CPU sob qualquer sinal de bateria

- Resumo: simplificar a politica derrubando qualquer offload ao menor sinal.
- Por que foi descartada: agressivo demais para `battery saver` e ruim para a experiencia em hardware com GPU disponivel.

## Criterio de revisao

- Revisar quando a coleta real de ETW estiver integrada e puder substituir os sinais sinteticos.
- Revisar quando benchmarks mostrarem que `prefer-efficiency` deve migrar decode para outro alvo em classes especificas de hardware.
- Revisar quando o runtime tiver telemetria historica suficiente para calibrar thresholds por perfil.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-009](./ADR-009-windows-ml-npu-offload-heuristic.md), [ADR-010](./ADR-010-directml-vs-vulkan-choice.md)
