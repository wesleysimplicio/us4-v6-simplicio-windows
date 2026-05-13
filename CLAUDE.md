# CLAUDE.md

> Este arquivo espelha [AGENTS.md](./AGENTS.md). Edite ambos juntos OU mantenha apenas `AGENTS.md` e symlink `CLAUDE.md` -> `AGENTS.md` (`ln -sf AGENTS.md CLAUDE.md`). O Claude Code lê arquivo regular, não símbolo.

---

# AGENTS.md

> Master instruction file lido por **Claude Code**, **Codex CLI**, **GitHub Copilot**, **Hermes Agent** (Nous Research), **OpenClaw**, **Cursor**, **Aider** e qualquer outro agent que respeite o padrão `AGENTS.md`. Contrato entre humano e IA neste repositório.
>
> Mudou algo aqui? Reflete em `CLAUDE.md` e `.github/copilot-instructions.md` (mantém os três alinhados ou usa symlink).

Este arquivo dá ao agent **tudo que ele precisa saber pra entregar uma task** sem perguntar: stack, comandos, fluxo, padrões, proibições, skills e atalhos. Lê ele inteiro antes da primeira linha de código.

---

## Projeto

- **Nome**: US4 V6 Windows Edition (`us4-v6-simplicio-windows`)
- **Domínio**: Universal State Runtime — inferência local de LLMs em Windows x86-64 (NVIDIA / AMD / Intel + NPU opcional).
- **Time**: us4-core.
- **Edição irmã**: [`us4-v6-simplicio-apple`](https://github.com/wesleysimplicio/us4-v6-simplicio-apple) (mesmo runtime contract, stack Apple Silicon).

Mono-projeto. Raiz do repo = projeto. Ignora `projects/` se vazia.

---

## Stack

C++17/20 + CMake/Ninja + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + GoogleTest + Playwright (CLI E2E) + Ralph Loop.

Detalhes:

- **Linguagem principal**: C++17/20 (MSVC 17.10+ ou Clang 16+).
- **Build system**: CMake >= 3.27 + Ninja generator (multi-config opcional).
- **Compute backends**:
  - **CUDA** (NVIDIA path, primary on RTX/A-series). CUDA Graphs + streams + memory pool.
  - **DirectML** (AMD / Intel iGPU / cross-vendor fallback). DML graph compile.
  - **Vulkan compute** (AMD/Intel/cross-vendor secondary).
  - **CPU AVX2/AVX-512/AMX** (oneDNN block GEMM + custom hot paths).
  - **Windows ML** (NPU: Snapdragon X / Intel Core Ultra / AMD Ryzen AI, opt-in).
- **Test runner unit**: GoogleTest (gtest + gmock) via CTest.
- **Test runner E2E**: **Playwright** (CLI flow tests; config em `playwright.config.ts`).
- **Bench harness**: Google Benchmark em `runtime/benchmarks/`.
- **Linter / formatter**: clang-tidy (`.clang-tidy`) + clang-format (`.clang-format`).
- **CI/CD**: GitHub Actions (`.github/workflows/{ci,dod}.yml`). Runner: `windows-2022` + GPU self-hosted opcional.
- **Distribuição**: MSIX installer assinado + portable zip. Detalhes em `.specs/workflow/RELEASE.md`.

> Antes de adicionar dependência nova (CMake `FetchContent`, vcpkg, NuGet, oneDNN release): **pergunta ao usuário**. Sem exceção.

---

## Comandos importantes

```powershell
# bootstrap (1a vez, PowerShell)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUS4_ENABLE_CUDA=ON -DUS4_ENABLE_DIRECTML=ON -DUS4_ENABLE_VULKAN=ON
cmake --build build -j

# desenvolvimento
cmake --build build --target us4-cli
.\build\us4-cli.exe --probe
.\build\us4-cli.exe run --model qwen-0.5b --prompt "hi"

# qualidade
clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h')
clang-format -i (git ls-files '*.cpp' '*.h')
clang-tidy -p build (git ls-files 'runtime/**/*.cpp')
ctest --test-dir build --output-on-failure
ctest --test-dir build --output-on-failure -L coverage

# E2E
npx playwright install
npx playwright test
npx playwright test --ui
npx playwright show-report

# bench
.\build\runtime\benchmarks\dense_baseline.exe
.\build\runtime\benchmarks\moe_throughput.exe

# git/PR
git checkout -b feat/<task-id>-<slug>
gh pr create --fill
gh run watch
```

---

## Workflow loop OBRIGATÓRIO

Toda task técnica passa por esses passos. Não pula etapa.

1. **Ler task** — `.specs/sprints/sprint-XX/<task-id>.task.md`. Contexto + AC + test plan + DoD.
2. **Planejar** — plano interno curto. Task ambígua => pergunta antes de codar.
3. **Carregar contexto** — `.specs/architecture/PATTERNS.md` + ADRs relevantes. Verifica skills em `.skills/`.
4. **Editar** — edits cirúrgicos. Só toca o que a task pede.
5. **Format + Lint** — `clang-format --dry-run --Werror` + `clang-tidy -p build`. Vermelho = corrige.
6. **Unit** — `ctest --test-dir build --output-on-failure`. Vermelho = corrige. Coverage do diff >= 80%.
7. **E2E (OBRIGATÓRIO em TODA task que toca CLI/UX)** — `npx playwright test --reporter=list,html`. Captura **trace + screenshot + video**. Sem evidência em `playwright-report/` + `test-results/` = task não fechada.
8. **Regression + Correctness** — re-roda suíte de sprints anteriores + `runtime/benchmarks/correctness/` (logit diff dentro da tolerância da task). Re-roda em **cada backend** que toca (CUDA + DirectML + Vulkan + AVX conforme escopo).
9. **Fix loop** — falhou => volta passo 4.
10. **Commit** — Conventional Commits em **inglês**. Body explica *why*.
11. **PR** — `gh pr create`. Template: link da task, evidências Playwright, bench numbers por backend, DoD marcado.

---

## Definition of Done

PR só faz merge quando **todos** os itens estão marcados:

- [ ] Build verde (`cmake --build build` sem warning novo, em todos os backends habilitados)
- [ ] Unit tests passam (`ctest --test-dir build` verde)
- [ ] Lint passa (`clang-tidy` + `clang-format --dry-run` verde)
- [ ] E2E Playwright passa com **evidência anexada em TODA task que toca CLI/UX** — `playwright-report/index.html` + `test-results/<spec>/trace.zip` + screenshots + video (when retry). Hard rule.
- [ ] Coverage do diff >= 80% (OpenCppCoverage em CI)
- [ ] Regression suite verde por backend tocado (CUDA / DirectML / Vulkan / AVX / NPU)
- [ ] Correctness diff dentro da tolerância (`runtime/benchmarks/correctness/`)
- [ ] AC da task: todos os checkboxes marcados
- [ ] PR template preenchido
- [ ] Conventional commit no merge
- [ ] ADR criado em `.specs/architecture/` se mudou decisão arquitetural
- [ ] Changelog atualizado se release-relevant
- [ ] Sem `std::cout` / `printf` / `OutputDebugString` de debug deixado pra trás
- [ ] Sem TODO sem dono e sem prazo

CI bloqueia merge se DoD falhar (`.github/workflows/dod.yml`).

---

## Padrões de código

Padrões completos em `.specs/architecture/PATTERNS.md` (preenchido incrementalmente nos sprints). Resumo:

- **Naming**: `PascalCase` classes, `camelCase` métodos/funcs, `kSnakeCase` constantes, `snake_case` arquivos.
- **Headers**: `#pragma once`. `.h` em mesma pasta do `.cpp` / `.cu`.
- **Smart pointers**: `std::unique_ptr` default, `std::shared_ptr` só com ownership compartilhada real.
- **No exceptions across ABI boundary** — adapters retornam `std::expected<T,Error>` (polyfill enquanto C++23 não obrigatório).
- **No global mutable state** fora do `RuntimeContext` singleton.
- **Tensor ownership**: `Tensor` move-only. `TensorView` pra empréstimo.
- **CUDA**: streams sempre passados explicitamente. Nada de `cudaDeviceSynchronize` em hot path.
- **DirectML**: graph compile uma vez, execute N vezes.
- Decisões irreversíveis viram **ADR**.
- Antes de código novo: lê `PATTERNS.md` da seção relevante.

---

## Onde encontrar contexto

| Pergunta | Onde olha |
|---|---|
| Por que esse produto existe? | `.specs/product/VISION.md` |
| Quem é o usuário? | `.specs/product/PERSONAS.md` |
| Quais entidades de runtime/adapters? | `.specs/product/DOMAIN.md` |
| Como o sistema é desenhado? | `.specs/architecture/DESIGN.md` |
| Como escrever código aqui? | `.specs/architecture/PATTERNS.md` |
| Por que decidimos X? | `.specs/architecture/ADR-*.md` |
| Como faço PR/branch/release? | `.specs/workflow/{WORKFLOW,RELEASE,CONTRIBUTING}.md` |
| O que tá no backlog? | `.specs/sprints/BACKLOG.md` |
| Sprint atual? | `.specs/sprints/sprint-XX/SPRINT.md` |
| Tasks abertas? | `.specs/sprints/sprint-XX/*.task.md` |
| Skills/capacidades reutilizáveis? | `.skills/README.md` + `.skills/*/SKILL.md` |
| Custom agents (sub-agents)? | `.agents/README.md` + `.agents/*.agent.md` |

---

## Proibido

- **Pular testes** — sem unit/E2E/regression = sem merge.
- **Mockar pra fazer passar** — mock só pra isolar dependência externa, nunca pra esconder falha em kernel/runtime.
- **Commit com vermelho** — Hook `.claude/hooks/pre-commit.sh` bloqueia.
- **Ignorar ADR** — decisão registrada é lei. Reverter exige novo ADR ("Supersedes ADR-XXX").
- **Adicionar dependência sem perguntar** — toda nova dep passa por confirmação humana.
- **Editar arquivo não lido** — lê antes.
- **Refactor escondido em PR de feature** — refactor = PR separado.
- **Force push em `main`** — bloqueado.
- **Commitar segredo** — `.env`, token HF, signing cert MSIX, senha => nunca.
- **Reformatar arquivo inteiro num PR pequeno** — diff polui review.
- **Quebrar correctness sem ADR** — regressão de logit-diff > tolerância exige ADR + sign-off do owner.
- **CUDA-only sem fallback** — toda feature de adapter precisa ter caminho DirectML/Vulkan/CPU equivalente (mesmo se inicialmente mais lento), salvo ADR explícito.

---

## Skills disponíveis

Skills em `.skills/<nome>/SKILL.md`.

### Ativadas por padrão no início da sessão

Via `.claude/settings.json` SessionStart hook:

- **`caveman`** — modo terse. ~65% menos tokens de output sem perder substância. Default level: `full`. Boundaries: código, commits, PRs e docs canônicos em prosa normal. Desativa com `stop caveman` / `normal mode`.
- **`ralph-loop`** — loop autônomo `read => plan => execute => format => lint => unit => e2e => regression => fix => repeat` até DoD verde. **Obrigatório** em TODA task técnica com AC mensurável.
- **`everything-claude-code`** — bundle ~60 agents + ~221 skills. Padrão: **máximo de agents ECC em paralelo** a cada alteração. `cpp-reviewer` + `security-reviewer` **obrigatórios** após edits.

### Sob demanda

- **`playwright-e2e`** — teste Playwright pro CLI. Fixtures, CLI process wrapper (PowerShell), evidências.
- **`conventional-commits`** — regras de commit.
- **`_template`** — base pra criar skill nova.

Detalhes: `.skills/README.md`.

---

## Custom agents disponíveis

Sub-agents em `.agents/<slug>.agent.md` (padrão **AGENTS.md ecosystem**). Espelhados em `.github/copilot/agents/` pro Copilot Workspace.

- **`ralph-loop.agent.md`** — Ralph Loop autônomo. Loop até DoD verde. Mapeia: Claude Code => `/ralph-loop "<prompt>"`; Codex CLI >=0.128 => `/goal <objective>`; Copilot CLI => `copilot --autopilot --max-autopilot-continues N`; VS Code Agent Mode => "Autopilot"; Cursor >=3.0 => Background Agent / `/multitask`. Tools: `edit`, `terminal`, `search`.
- **`tdd.agent.md`** — TDD Specialist. Teste falhando antes do código (GoogleTest). Loop red-green-refactor.
- **`reviewer.agent.md`** — Code Reviewer C++/CUDA. Read-only. Memory safety, modern C++, concurrency, performance, CUDA stream hygiene.
- **`architect.agent.md`** — Architect. ADRs, `PATTERNS.md`. Não escreve código de produção.
- **`_template.agent.md`** — base.

Detalhes: `.agents/README.md`.

---

## Comandos especiais

### Criar nova ADR

```powershell
ls .specs/architecture/ADR-*.md | Select-Object -Last 1
Copy-Item .specs/architecture/ADR-template.md .specs/architecture/ADR-XXX-<slug>.md
```

### Abrir PR

```powershell
git push -u origin (git branch --show-current)
gh pr create --fill
gh pr view --web
gh run watch
```

### Criar task nova

```powershell
Copy-Item .specs/sprints/task-template.md .specs/sprints/sprint-XX/<id>-<slug>.task.md
```

### Criar skill nova

```powershell
Copy-Item -Recurse .skills/_template .skills/<nome-da-skill>
```

### Rodar checklist DoD localmente antes de PR

```powershell
cmake --build build `
  && clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h') `
  && clang-tidy -p build (git ls-files 'runtime/**/*.cpp') `
  && ctest --test-dir build --output-on-failure `
  && npx playwright test
```

---

## Notas finais pro agent

- **Idioma**: respostas/docs em **pt-BR**, código (vars/funções/classes) em **inglês**, commits em **inglês**.
- **Sem emoji em código**. README/slides ok.
- **Sem resumo no final** de uma resposta.
- **Sem estimativa de tempo**.
- **Pergunta apenas em ambiguidade real** do pedido.
- **Paralelo é o padrão** — research + read + review independentes rodam simultâneos.
- **Hooks do `.claude/hooks/`** rodam automaticamente: post-edit faz format/lint, pre-commit bloqueia commit vermelho.
- **Backend selector** é responsabilidade do `HardwareProbe` + `BackendSelector`. NVIDIA => CUDA primary. AMD/Intel => DirectML primary. Vulkan = fallback cross-vendor. CPU AVX sempre disponível. NPU é opt-in (`--npu`).
- **Correctness > performance**. Logit diff vs referência é gate duro em **todo backend**.
