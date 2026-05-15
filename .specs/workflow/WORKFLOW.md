# WORKFLOW - `US4 V6 Windows Edition`

Como o time `us4-core` move codigo de ideia ate merge em `main` neste repo. Este arquivo descreve o fluxo real do repositorio hoje, sem assumir packaging ou release automation que ainda nao existem.

---

## 1. Branch strategy

### Padrao: trunk-based development

- `main` e a unica branch longa.
- Toda mudanca entra por PR pequeno, com escopo unico.
- Feature branches devem ser curtas. Alvo pratico: menos de 2 dias.
- Codigo incompleto precisa ficar atras de flag de build ou de runtime.
- Rebase em cima de `main` antes do merge. Historico linear.

### Naming convention

```text
feat/<short-desc>
fix/<short-desc>
chore/<short-desc>
refactor/<short-desc>
docs/<short-desc>
perf/<short-desc>
hotfix/<short-desc>
```

Para tasks de sprint, use:

```text
feat/sprint-XX-<task-id>-<slug>
```

Exemplo:

```text
feat/sprint-02-t02.2-scalar-gemm
```

---

## 2. Pull request rules

### Tamanho e foco

- Alvo: ate 400 linhas modificadas, sem contar gerados e lockfiles.
- PR acima de 600 linhas precisa justificar o tamanho no corpo.
- Uma PR deve ter um unico objetivo. Refactor oportunista vai em outra PR.

### Titulo

O titulo da PR precisa seguir Conventional Commits:

```text
feat(runtime): add cpu fallback probe summary
fix(cli): reject run without prompt
chore(ci): align windows build gate
docs(workflow): remove release pipeline claims
```

### Corpo

Use o template em [`.github/PULL_REQUEST_TEMPLATE.md`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/PULL_REQUEST_TEMPLATE.md) e preencha:

- task ou issue relacionada
- resumo do que mudou e por que
- evidencias de validacao executadas
- observacoes de backend, arquitetura e release quando aplicavel

Nao invente evidencia. Se um harness ainda nao existe, marque `N/A` e explique.

### Review

- Minimo de 1 reviewer humano.
- Mudancas em runtime, arquitetura, seguranca ou superficie publica pedem 2 reviewers quando possivel.
- Nao faça self-merge sem aprovacao.
- `req:` aberto bloqueia merge.

### Merge

- `Squash and merge` e o padrao.
- Branch pode ser removida depois do merge.
- Se `main` quebrar apos merge, a prioridade e corrigir ou reverter.

---

## 3. Code review protocol

### Reviewer

- Leia a `task.md` antes do diff.
- Use comentarios objetivos:
  - `nit:` sugestao nao bloqueante
  - `q:` pergunta
  - `req:` mudanca bloqueante
  - `praise:` reforco de padrao bom
- Se houver violacao de ADR ou de `PATTERNS.md`, comente com link.

### Autor

- Nao merge com `req:` aberto.
- Durante review ativa, prefira push normal a force-push.
- Se houver mudanca arquitetural, cite ADR no corpo da PR.

---

## 4. CI e gates reais do repo

Os workflows ativos para este repo sao:

- [`.github/workflows/ci.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/ci.yml)
- [`.github/workflows/dod.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/dod.yml)

### O que `ci.yml` faz hoje

Em `windows-2022`, o pipeline:

1. configura o ambiente MSVC
2. roda `cmake -S . -B build -G Ninja`
3. compila o projeto
4. executa smoke `build\\us4-cli.exe --probe`
5. roda `ctest`
6. tenta `clang-format` e `clang-tidy` quando os binarios estiverem disponiveis no runner
7. roda Playwright apenas quando a PR tocar CLI/UX
8. publica artefatos de `ctest`, `playwright-report/` e `test-results/`

### O que `dod.yml` faz hoje

O gate de DoD valida:

- configure + build do projeto
- `ctest`
- titulo da PR em Conventional Commits
- referencia de task ou issue no corpo
- referencia de ADR quando arquivos de arquitetura mudam
- evidencia Playwright no corpo quando a PR tocar CLI/UX

### O que ainda nao existe como gate automatizado

Hoje o repo ainda nao possui automacao completa para:

- coverage diff >= 80%
- regression matrix por backend
- correctness diff em `runtime/benchmarks/correctness/`
- pipeline separado `e2e`
- `release.yml`
- build de MSIX, portable zip ou publicacao via winget

Quando esses itens forem implementados, atualize este documento e o DoD.

### Workflows herdados do starter

Os arquivos abaixo ainda existem, mas estao protegidos para rodar apenas no repo do starter original:

- [`.github/workflows/publish-npm.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/publish-npm.yml)
- [`.github/workflows/scaffold-self-check.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/scaffold-self-check.yml)

Eles nao fazem parte do fluxo do runtime Windows neste repo.

---

## 5. Distribuicao e release status

Estado atual do repo:

- nao existe [`.github/workflows/release.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/release.yml)
- nao existe pasta `packaging/`
- nao existe `CHANGELOG.md`
- nao existe pipeline de assinatura, MSIX ou winget

Portanto, hoje `main` valida build e testes, mas ainda nao produz release instalavel do runtime.

Detalhes e lacunas de release ficam em [`.specs/workflow/RELEASE.md`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.specs/workflow/RELEASE.md).

---

## 6. Hotfix process

Enquanto nao houver pipeline de release, hotfix segue o mesmo fluxo de PR normal:

1. branch curta a partir de `main`
2. fix minimo
3. teste que reproduz o bug
4. CI verde
5. squash merge

Se o repositorio estiver com artefatos distribuidos manualmente, documente rollback no corpo da PR e no incidente correspondente.

---

## 7. Branch protection

Configuracao recomendada para `main`:

- require pull request
- require pelo menos 1 approval
- require status checks do workflow `CI`
- require status checks do workflow `DoD Gate`
- require linear history
- bloquear force-push
- bloquear delete

O job de Playwright em `CI` e condicional. Ele so deve ser exigido quando a PR tocar CLI/UX.

Se houver divergencia entre a configuracao do GitHub e este arquivo, corrija a configuracao ou atualize o documento no mesmo PR.
