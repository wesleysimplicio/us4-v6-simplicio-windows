# Evidence

Em `US4 V6 Windows Edition`, evidencia nao e opcional.
Toda mudanca em CLI, runtime observavel, correctness ou release precisa deixar rastro reproduzivel.

## When To Capture

- mudanca em CLI/UX: trace + stdout/stderr + `playwright-report/`
- mudanca em `bench`: saida textual ou JSON da matriz validada
- mudanca em `tune`: stdout do comando + store persistido
- mudanca em backend/runtime: benchmark + correctness diff
- mudanca em memoria/KV/expert paging: metricas e logs relevantes
- mudanca em release/instalacao: artefatos gerados + screenshots do fluxo quando houver UI

## Default Output

```text
playwright-report/
test-results/
.runtime-logs/evidence/
  <feature>-<scenario>-<timestamp>.json
  <feature>-<scenario>-<timestamp>.txt
runtime/benchmarks/correctness/reports/
runtime/tuning/
  profiles.json
```

## Naming

Use nomes minusculos e hifenizados:

```text
backend-selector-amd-directml-20260514-173000.txt
bench-matrix-qwen-cpu-only-20260515-101500.json
tune-profile-store-qwen-cpu-only-20260515-101900.json
```

## Acceptance Checklist

- [ ] A evidencia mostra exatamente o cenario pedido.
- [ ] Nenhum segredo, token ou path sensivel aparece.
- [ ] O resultado esperado esta visivel ou assertado.
- [ ] O backend/adapters/perfil tocado esta identificado.
- [ ] O caminho do artefato foi incluido na resposta final ou PR.

## Playwright

Fluxo minimo para mudancas de CLI/UX:

```powershell
npx playwright test --reporter=list,html
```

Artefatos esperados:

- `playwright-report/index.html`
- `test-results/<spec>/trace.zip`
- stdout/stderr anexados pelo spec
- screenshots
- video de retry/quando configurado

## Bench Matrix Evidence

Quando a mudanca tocar `bench`, inclua pelo menos:

- o comando executado
- backend, mode e model usados
- `bench --format json` quando o contrato JSON mudar
- `selectedProfileId`, `selectedBackend` e `selectedScore`
- as amostras ou decisoes relevantes quando a selecao mudar

Se o benchmark foi validado apenas em texto, deixe isso explicito na PR.

## Tune Persistence Evidence

Quando a mudanca tocar `tune`, inclua pelo menos:

- o stdout do `tune`
- o path do store usado na execucao
- o arquivo persistido resultante
- confirmacao de que a escrita aconteceu no host fingerprint esperado

Para evitar ruido no workspace, prefira `US4_PROFILE_STORE_PATH` apontando para um path temporario do teste.

## Runtime Metrics

Para mudancas em performance ou correctness, inclua pelo menos:

- `tokens_per_second`
- `time_to_first_token_ms`
- `gpu_memory_used_mb` e/ou `system_ram_used_mb`
- `kv_page_faults` quando relevante
- `expert_cache_hit_rate` quando relevante
- `logit_drift` vs referencia

## Release Evidence

Quando release ou packaging entrar no escopo, a evidencia minima deve crescer para incluir:

- nome e hash do artefato
- versao publicada
- smoke do binario publicado
- changelog correspondente

Hoje esse fluxo ainda nao esta completo; veja [C:\Users\wesley.simplicio\Pictures\m\us4-v6-simplicio-windows\.specs\workflow\RELEASE.md](C:\Users\wesley.simplicio\Pictures\m\us4-v6-simplicio-windows\.specs\workflow\RELEASE.md).
