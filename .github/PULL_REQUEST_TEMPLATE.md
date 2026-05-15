# Pull Request

## Summary

<!-- Descreva em 1-3 frases o que mudou e por que. -->

## Related task

<!-- Referencia obrigatoria: issue, task.md ou ambos. -->

- Task:
- Closes:

## Change type

- [ ] feat - nova feature
- [ ] fix - bug fix
- [ ] refactor - refactor sem mudanca comportamental
- [ ] perf - melhoria de performance
- [ ] docs - documentacao
- [ ] test - apenas testes
- [ ] chore - build, CI ou tooling
- [ ] breaking - quebra contrato

## Scope flags

- [ ] CLI/UX touched
- [ ] Architecture touched
- [ ] Release-relevant

## Validation

- [ ] Build local verde (`cmake -S . -B build -G Ninja` e `cmake --build build`)
- [ ] `us4-cli` validado no fluxo alterado
- [ ] Unit tests passam (`ctest --test-dir build --output-on-failure`)
- [ ] `clang-format --dry-run --Werror` executado, quando disponivel
- [ ] `clang-tidy -p build` executado, quando disponivel
- [ ] Playwright executado quando CLI/UX foi tocado
- [ ] Regression/correctness rodados para os backends tocados, quando o harness existir

## Definition of Done

- [ ] Acceptance criteria da task atendidos
- [ ] Sem TODO/FIXME novos sem dono e prazo
- [ ] Sem segredo hardcoded
- [ ] Commits seguem Conventional Commits
- [ ] Documentacao atualizada quando aplicavel

## Playwright evidence

<!-- Preencha se CLI/UX foi tocado. Caso contrario, escreva N/A. -->

- CLI/UX touched:
- HTML report:
- Trace:
- Screenshots:
- Video:

## Backend validation

<!-- Marque o que foi tocado e descreva abaixo o que foi realmente validado. -->

- [ ] CUDA
- [ ] DirectML
- [ ] Vulkan
- [ ] CPU AVX
- [ ] NPU

Validation notes:

## Bench / correctness notes

<!-- Numeros reais, caminho para relatorio, ou N/A com justificativa. -->

## ADR / architecture notes

<!-- Cite ADR-NNN quando aplicavel, ou N/A. -->

## Release notes

<!-- Se houver impacto em release futura, packaging ou distribuicao manual, descreva. Caso contrario, N/A. -->

## Environment limits

<!-- Registre limitacoes reais de ambiente, por exemplo ausencia de compilador, GPU ou harness. Caso nao haja, escreva N/A. -->

## Reviewer checklist

- [ ] Mudanca segue `.specs/architecture/PATTERNS.md`
- [ ] Escopo esta contido na task
- [ ] Evidencia bate com o que a PR afirma
- [ ] ADR foi citada quando arquitetura mudou
