# ADR-012: Windows ML Compiled Fallback Artifact

- Status: Accepted
- Date: 2026-05-15
- Supersedes: None

## Context

Na Sprint 11, o `Windows ML` já tinha planner, layer offload e dry-run híbrido, mas ainda faltava um contrato explícito para responder o que de fato foi compilado quando:

- o usuário faz opt-in com `--npu`
- o host não tem NPU disponível
- o runtime precisa manter a sessão utilizável sem cair silenciosamente para outro backend antes do adapter

Sem esse contrato, a CLI, o hybrid gate e o mixed dispatch inferiam o estado final a partir de `plan`, `stats` e heurísticas separadas.

## Decision

Introduzimos um `WinMlSessionArtifact` como produto do `WinMlAdapter::CompileGraph()`.

O artifact passa a registrar:

- `compileTarget` (`npu` ou `cpu-fallback`)
- `fallbackReason`
- `cpuFallbackArmed`
- `hostAssistRequired`
- `reusableGraph`
- `requiresStaticShapes`
- `batch/context hints`
- `cacheKey`
- contagem efetiva de partições compiladas

Também adotamos a seguinte política:

1. Pedido explícito de `windows-ml --npu` sem NPU disponível não deve cair direto para CPU no selector.
2. O backend `windows-ml` permanece selecionável quando o pedido é explícito.
3. O `WinMlAdapter` compila uma sessão `cpu-fallback` quando `allowCpuFallback=true`.
4. O `MixedDispatchPlanner` deve respeitar o artifact compilado e desarmar `npuDenseActive` quando o target efetivo for `cpu-fallback`.

## Consequences

Positivas:

- CLI consegue expor `compile_target`, `fallback_reason`, `graph_reusable` e `session_artifact`.
- O hybrid correctness gate passa a validar o cenário `windows_ml_qwen_no_npu_fallback`.
- O runtime fica mais honesto sobre fallback compilado versus degradação por policy térmica/energética.

Negativas:

- O `Windows ML` agora tem mais estado explícito no adapter.
- O planner híbrido passa a depender opcionalmente do artifact compilado, não só do plano lógico.

## Alternatives Considered

- Deixar o selector cair para CPU imediatamente quando `hasNpu=false`.
  - Rejeitado porque esconde o comportamento do backend explicitamente pedido pelo usuário.
- Inferir fallback apenas pelos `stats` do adapter.
  - Rejeitado porque não preserva motivo, target efetivo nem reusabilidade da sessão.
