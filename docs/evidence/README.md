# Evidence

Em `US4 V6 Windows Edition`, evidência não é opcional.
Toda mudança em CLI/UX ou comportamento observável precisa deixar rastro reproduzível.

## When To Capture

- Mudança em CLI/UX: screenshot + video + trace Playwright.
- Mudança em backend/runtime: saída de benchmark + correctness diff.
- Mudança em memória/KV/expert paging: métricas de page fault + logs relevantes.
- Mudança em release/instalação: screenshot do fluxo e artefatos gerados.

## Default Output

```text
playwright-report/
test-results/
.runtime-logs/evidence/
  <feature>-<scenario>-<timestamp>.json
  <feature>-<scenario>-<timestamp>.txt
```

## Naming

Use nomes minúsculos e hifenizados:

```text
backend-selector-amd-directml-20260514-173000.png
kv-paging-long-context-20260514-173500-trace.zip
```

## Acceptance Checklist

- [ ] A evidência mostra exatamente o cenário pedido.
- [ ] Nenhum segredo, token ou path sensível aparece.
- [ ] O resultado esperado está visível ou assertado.
- [ ] O backend/adapters/perfil tocado está identificado.
- [ ] O caminho do artefato foi incluído na resposta final ou PR.

## Playwright

Fluxo mínimo para mudanças de CLI/UX:

```powershell
npx playwright test --reporter=list,html
```

Artefatos esperados:

- `playwright-report/index.html`
- `test-results/<spec>/trace.zip`
- screenshots
- video de retry/quando configurado

## Runtime Metrics

Para mudanças em performance/correctness, inclua pelo menos:

- `tokens_per_second`
- `time_to_first_token_ms`
- `gpu_memory_used_mb` e/ou `system_ram_used_mb`
- `kv_page_faults` quando relevante
- `expert_cache_hit_rate` quando relevante
- `logit_drift` vs referência
