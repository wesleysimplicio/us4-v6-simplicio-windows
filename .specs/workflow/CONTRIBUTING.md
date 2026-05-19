# CONTRIBUTING - `US4 V6 Windows Edition`

Guia pratico para contribuir no repo `us4-v6-simplicio-windows` sem assumir capacidades que ainda nao existem.

---

## Pre-requisitos

- Windows 11 com toolchain C++ funcional para CMake/Ninja
- Visual Studio 2022 Build Tools ou equivalente com compilador C++
- Node 20+ para Playwright
- leitura previa de:
  - [AGENTS.md](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/AGENTS.md)
  - [`.specs/architecture/PATTERNS.md`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.specs/architecture/PATTERNS.md)
  - task atual em `.specs/sprints/`

Se a maquina nao expuser compilador C++ utilizavel para o `cmake`, registre isso na task ou na PR em vez de fingir validacao completa.

---

## Fluxo padrao

### 1. Criar ou localizar a task

Use uma `task.md` para qualquer mudanca nao trivial:

```text
.specs/sprints/sprint-XX/<id>-<short-desc>.task.md
```

A task deve deixar claro:

- contexto
- acceptance criteria
- escopo
- plano de testes
- ADRs relevantes

### 2. Criar branch

```powershell
git checkout main
git pull --rebase origin main
git checkout -b feat/sprint-XX-<task-id>-<slug>
```

### 3. Implementar com escopo contido

- siga `PATTERNS.md`
- nao adicione dependencia nova sem confirmacao humana
- nao esconda refactor dentro de feature
- mantenha fallback entre backends como contrato do produto

### 4. Rodar validacao local

#### Gate minimo esperado hoje

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

#### Lint quando as ferramentas estiverem disponiveis

```powershell
clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h' '*.cu' '*.hlsl')
clang-tidy -p build (git ls-files 'runtime/**/*.cpp')
```

#### E2E quando a task tocar CLI/UX

```powershell
npx playwright test --reporter=list,html
```

Anexe ou referencie:

- `playwright-report/`
- `test-results/`
- `trace.zip`, screenshots e video quando existirem

#### Validacoes complementares da Sprint 12

Se a task tocar `bench`:

```powershell
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only
.\build\us4-cli.exe bench --model qwen-0.5b --backend cpu --mode cpu-only --format json
```

Se a task tocar `tune`:

```powershell
$env:US4_PROFILE_STORE_PATH="$PWD\\runtime\\tuning\\profiles.dev.json"
.\build\us4-cli.exe tune --model qwen-0.5b --backend cpu --mode cpu-only
Get-Content $env:US4_PROFILE_STORE_PATH
```

Itens que continuam sendo contrato do projeto, mas ainda nao estao universalmente automatizados:

- coverage diff >= 80%
- regression por backend
- correctness diff por backend
- smoke multi-backend publicado

Quando a task tocar essas frentes e o harness existir, rode e documente. Quando o harness ainda nao existir, declare isso na PR.

### 5. Abrir PR

```powershell
git push -u origin <sua-branch>
gh pr create --fill --web
```

Preencha [`.github/PULL_REQUEST_TEMPLATE.md`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/PULL_REQUEST_TEMPLATE.md) com evidencia real.

### 6. Review

- CI verde antes de pedir review
- responda todos os `req:`
- cite ADR quando a mudanca for arquitetural

### 7. Merge

Padrao:

```powershell
gh pr merge --squash --delete-branch
```

### 8. Distribuicao

Hoje merge em `main` valida engenharia, e o repositorio ja possui a trilha local de release em `.github/workflows/release.yml`, `packaging/msix/`, `packaging/winget/`, `scripts/release-dry-run.ps1`, `scripts/preflight-release.ps1`, `scripts/post-publish-smoke.ps1` e `scripts/render-project-status.ps1`.

O que ainda nao esta fechado localmente e depende de ambiente externo eh a assinatura publica do MSIX com certificado confiavel, o smoke de instalacao em host real e a publicacao real de `winget`/GitHub Release.

Veja [`.specs/workflow/RELEASE.md`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.specs/workflow/RELEASE.md).

---

## CI real do repositorio

Hoje o repositorio usa:

- [`.github/workflows/ci.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/ci.yml)
- [`.github/workflows/dod.yml`](C:/Users/wesley.simplicio/Pictures/m/us4-v6-simplicio-windows/.github/workflows/dod.yml)

`ci.yml`:

- builda em `windows-2022`
- roda smoke `us4-cli --probe`
- roda `ctest`
- tenta `clang-format` e `clang-tidy` quando disponiveis
- roda Playwright somente quando a PR toca CLI/UX

`dod.yml`:

- reforca build e `ctest`
- valida titulo da PR
- exige task ou issue no corpo
- exige ADR para mudancas de arquitetura
- exige evidencia Playwright para PRs de CLI/UX

---

## Checklist antes do PR

- [ ] task criada ou referenciada
- [ ] branch nomeada conforme convencao
- [ ] build local verde, quando houver toolchain
- [ ] `ctest` verde, quando houver binarios de teste
- [ ] `clang-format` e `clang-tidy` rodados, quando disponiveis
- [ ] Playwright rodado se CLI/UX foi tocado
- [ ] evidencia anexada para CLI/UX
- [ ] `bench --format json` anexado ou referenciado se o contrato de matriz mudou
- [ ] store persistido anexado ou referenciado se `tune` mudou
- [ ] ADR citada se arquitetura mudou
- [ ] sem dependencia nova nao aprovada
- [ ] sem segredo no diff
- [ ] docs atualizadas quando o comportamento do repo mudou

---

## Erros comuns

- prometer no PR um benchmark ou correctness que nao foi executado
- tratar roadmap de release como capability pronta
- abrir PR sem task ou sem explicar a limitacao de ambiente
- tocar CLI e esquecer evidencia Playwright
- tocar `bench` e nao guardar o JSON validado
- tocar `tune` e nao guardar o store persistido usado no teste
- alterar arquitetura sem ADR
