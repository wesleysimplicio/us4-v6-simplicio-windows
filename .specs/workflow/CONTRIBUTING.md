# CONTRIBUTING — `US4 V6 Windows Edition`

Guia step-by-step para adicionar feature, fix ou refactor em **US4 V6 Windows Edition** (`us4-v6-simplicio-windows`). Funciona pra humano e pra agent. Stack: C++17/20 + CMake/Ninja + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + GoogleTest + Playwright + Ralph Loop. Time: `us4-core`.

---

## Pré-requisitos

- Windows 11 22H2+ (x64 ou ARM64). Visual Studio 2022 Build Tools + Windows 11 SDK.
- CUDA Toolkit 12.x (se for tocar backend NVIDIA). DirectX 12 (DirectML) já vem no Windows. Vulkan SDK 1.3+ (se for tocar Vulkan).
- Repo clonado, build verde local (PowerShell):
  ```powershell
  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DUS4_ENABLE_CUDA=ON -DUS4_ENABLE_DIRECTML=ON -DUS4_ENABLE_VULKAN=ON
  cmake --build build -j $env:NUMBER_OF_PROCESSORS
  ctest --test-dir build --output-on-failure
  ```
- Node 20+ pra Playwright (`npx playwright install` já rodado uma vez).
- Leu `AGENTS.md` (raiz), `.specs/architecture/PATTERNS.md` (preenchido incrementalmente) e a `task.md` corrente.
- Acesso pra abrir PR no `wesleysimplicio/us4-v6-simplicio-windows` + rodar CI.

---

## Fluxo padrão (8 passos)

### 1. Criar `task.md` em sprint atual

Toda mudança não-trivial nasce em `task.md`. Caminho:

```
.specs/sprints/sprint-XX/<id>-<short-desc>.task.md
```

Use `task-template.md` como base. Preencha:
- Contexto e problema (qual gap de runtime, adapter, kernel, backend ou DX).
- Acceptance Criteria testáveis (checkboxes — comportamento + métrica).
- Out of scope explícito (especialmente backends — qual está em escopo, qual fica de fora).
- Test plan: unit (GoogleTest), regression por backend tocado, correctness diff (logit), e2e Playwright.
- Definition of Done (espelha `AGENTS.md` seção DoD).
- Pegadinhas conhecidas + ADRs relevantes.

Exemplo: `.specs/sprints/sprint-04/T04.3-cuda-fp16-gemm.task.md`.

> Mudança trivial (typo, bump patch sem risco, `.clang-format` cosmetic) pode pular task. Qualquer coisa que toque kernel, runtime, correctness, adapter, backend (CUDA/DirectML/Vulkan/oneDNN) ou CLI exige task.

### 2. Criar branch

A partir de `main` atualizada:

```powershell
git checkout main
git pull --rebase origin main
git checkout -b feat/sprint-04-t04.3-cuda-fp16-gemm
```

Convenção de nome em `WORKFLOW.md` seção 1. Sprint branches usam `feat/sprint-XX-<task-id>-<slug>`.

### 3. Implementar seguindo PATTERNS

- **Não invente padrão novo.** Se `.specs/architecture/PATTERNS.md` define como criar kernel CUDA/DirectML, header com `#pragma once`, RAII pra cuStream, dispatch por backend — segue.
- Mudou padrão? Abre ADR **antes** (`.specs/architecture/ADR-template.md`) e linka na task.
- Não adiciona dependência (CMake `FetchContent`, vcpkg port, MSVC runtime, CUDA library nova) sem confirmar com `us4-core`.
- Edita só o pedido. Sem refactor oportunista — refactor é PR separado em branch `refactor/<slug>`.
- Hardware-aware: NVIDIA → CUDA, AMD/Intel → DirectML, fallback Vulkan, CPU AVX always-on, NPU opt-in via Windows ML. **Nunca** "só CUDA, sem fallback". `BackendSelector` decide por hardware detectado.
- Stream/queue hygiene: toda cuStream/D3D12CommandQueue alocada precisa de RAII wrapper. Vazamento de handle = bug bloqueante.

### 4. Testes (unit + regression + correctness + e2e)

Antes do PR, todo verde local (PowerShell):

```powershell
# format + lint
clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h' '*.cu' '*.hlsl')
clang-tidy -p build (git ls-files 'runtime/**/*.cpp')

# unit (GoogleTest via CTest) — coverage do diff >= 80% (OpenCppCoverage)
ctest --test-dir build --output-on-failure

# regression POR BACKEND tocado (CUDA, DirectML, Vulkan, oneDNN)
ctest --test-dir build -L regression-cuda --output-on-failure
ctest --test-dir build -L regression-directml --output-on-failure
ctest --test-dir build -L regression-vulkan --output-on-failure

# correctness diff (logit vs referência, tolerância da task)
.\build\runtime\benchmarks\correctness_logit_diff.exe --model qwen-0.5b --tokens 32 --backend cuda

# bench (se task pede número)
.\build\runtime\benchmarks\dense_baseline.exe --backend cuda

# e2e Playwright (OBRIGATÓRIO em toda task que toca CLI/UX)
npx playwright test --reporter=list,html
```

- Bug fix sem teste regressivo é **inaceitável**. Escreve GoogleTest que falha sem o fix.
- E2E tem evidência: `playwright-report/index.html` + `test-results/<spec>/trace.zip` + screenshot + video em retry.
- Regression suite roda **por backend tocado** — alterou kernel CUDA, re-roda regression-cuda. Alterou seletor, re-roda todas.
- Coverage caiu? Justifica no PR ou adiciona teste.
- Logit diff fora da tolerância = bloqueia. Sem exceção sem ADR.

### 5. Abrir PR usando template

Push e abre PR via gh:

```powershell
git push -u origin feat/sprint-04-t04.3-cuda-fp16-gemm
gh pr create --fill --web
```

Template em `.github/PULL_REQUEST_TEMPLATE.md` é preenchido automático. Complete:
- Link para `task.md`.
- Resumo (3-5 bullets — *o que* + *por que*).
- Bench numbers por backend (tokens/s cold/warm, VRAM/RAM peak, SM occupancy) se aplicável.
- Correctness diff vs referência (link pro relatório em `runtime/benchmarks/correctness/`).
- Evidência E2E (link pro report Playwright + `trace.zip`).
- Checklist DoD marcada.
- Riscos e plano de rollback (especialmente backend novo atrás de `-DUS4_ENABLE_<X>=OFF`).
- Hardware testado: GPU(s), CPU (AVX2/AVX-512/AMX), NPU (se opt-in).

Title segue Conventional Commits: `perf(cuda): fuse epilogue into FP16 GEMM kernel`.

### 6. Review

- CI verde é pré-requisito. PR com vermelho não vai pra review humana.
- Reviewer humano em até 4h úteis. PR de hotfix tem SLA 30min.
- PRs tocando kernel/runtime/segurança exigem 2 reviewers (1 com tag `runtime` ou `security`).
- PRs tocando backend específico precisam de 1 reviewer com tag desse backend (`cuda`, `directml`, `vulkan`).
- Endereça todos `req:` antes de pedir re-review.
- Discussão de design vai no diff. Discussão arquitetural ampla vira ADR.

### 7. Merge squash

Após aprovação:

```powershell
gh pr merge --squash --delete-branch
```

Mensagem squash = title do PR + corpo enxuto. Histórico de `main` fica linear.

### 8. Distribuição

- Merge em `main` dispara `ci.yml` + `dod.yml` (runner `windows-2022` + self-hosted GPU runner pra CUDA/Vulkan). Verde => artifact (binário + Playwright report) em GitHub Actions.
- Pra cortar release de produção: bump versão, atualiza `CHANGELOG.md`, tag SemVer. Detalhes em `RELEASE.md`.
- Distribuição: MSIX installer assinado (Authenticode) + portable zip + winget manifest.

---

## Skills/Agents que você pode usar

Skills e agents reduzem trabalho repetitivo e enforçam padrão. Ver `.skills/` e `.agents/` no repo.

### Skills disponíveis (em `.skills/`)

| Skill | Quando trigar | Caminho |
|-------|---------------|---------|
| `caveman` | Default da sessão. Output terse. Não afeta código/PR/commit. | `.skills/caveman/SKILL.md` |
| `ralph-loop` | **Obrigatório** em toda task técnica. Loop `read → plan → execute → format → lint → unit → e2e → regression → fix → repeat` até DoD verde. | `.skills/ralph-loop/SKILL.md` |
| `everything-claude-code` | Bundle ~60 agents + ~221 skills. Padrão: máximo de agents ECC em paralelo. | `.skills/everything-claude-code/SKILL.md` |
| `playwright-e2e` | Escrever ou ajustar teste E2E pro CLI `us4-cli.exe`. Fixtures, process wrapper, evidências. | `.skills/playwright-e2e/SKILL.md` |
| `conventional-commits` | Compor mensagem de commit ou title de PR. Cobre `feat`, `fix`, `perf`, breaking change. | `.skills/conventional-commits/SKILL.md` |
| `_template` | Base pra criar skill nova. | `.skills/_template/SKILL.md` |

### Agents customizados (em `.agents/`)

| Agent | Uso |
|-------|-----|
| `ralph-loop.agent.md` | **Executor autônomo padrão.** Loop até DoD verde. Aciona em toda task técnica. |
| `tdd.agent.md` | TDD specialist. GoogleTest falhando antes do código. Loop red-green-refactor. |
| `reviewer.agent.md` | Code review C++ sem editar. Memory safety, modern C++, concurrency, CUDA/DirectML/Vulkan idioms. |
| `architect.agent.md` | Desenha arquitetura, cria ADRs, popula `PATTERNS.md`. Não escreve código de produção. |

> `.agents/` é a fonte canônica (padrão AGENTS.md ecosystem). Espelhado em `.github/copilot/agents/` pro GitHub Copilot Workspace. Detalhes em [`.agents/README.md`](../../.agents/README.md).

### Como invocar

- Em Claude Code: `Skill(playwright-e2e)` ou referência em prompt.
- Em Copilot Agent Mode: seleciona agent custom no chat.
- Skills com trigger explícito têm prefixo `$skill-name` em comentário ou prompt.

---

## Checklist rápido antes do PR

- [ ] `task.md` criado e linkado.
- [ ] Branch nome segue convenção (`feat/sprint-XX-<task-id>-<slug>`).
- [ ] Build verde em todas as configs (Release/Debug, x64/ARM64) sem warning novo.
- [ ] `clang-format --dry-run --Werror` + `clang-tidy -p build` verdes.
- [ ] Unit (`ctest`) verde, coverage do diff >= 80% (OpenCppCoverage).
- [ ] Regression suite verde **em todos os backends tocados** (CUDA/DirectML/Vulkan/oneDNN/CPU).
- [ ] Correctness diff dentro da tolerância da task **por backend**.
- [ ] E2E Playwright verde com **evidência anexada** (`playwright-report/` + `trace.zip` + screenshots + video em retry).
- [ ] PATTERNS.md respeitado, ou ADR aberta.
- [ ] Sem dependência nova não-aprovada.
- [ ] PR title em Conventional Commits.
- [ ] Template de PR preenchido (task link, bench por backend, correctness, E2E, hardware testado).
- [ ] Sem `std::cout` / `printf` / `OutputDebugString` de debug deixado pra trás.
- [ ] Sem secrets em diff (`git diff | findstr /i "secret token key"`).
- [ ] CHANGELOG atualizado se mudança visível ao usuário.
- [ ] CUDA stream / D3D12 queue / VkCommandBuffer com RAII (sem vazamento de handle).

---

## Erros comuns (não faça)

- Branch que vive 2 semanas: quebra task em pedaços menores (alvo < 2 dias).
- PR de 2000 linhas com "vários ajustes": split em PRs de até 400 linhas.
- GoogleTest que mocka kernel/runtime pra esconder falha: falso verde, vai voltar como incidente.
- Mudar padrão sem ADR: dívida invisível.
- Force-push em branch de PR aberto: reviewer perde contexto.
- Merge com CI vermelho: bloqueia o time inteiro depois.
- Esquecer de remover flag `-DUS4_ENABLE_<X>=OFF` quando backend atinge GA: lixo composto.
- Quebrar correctness sem ADR + sign-off do owner: rollback imediato em `main`.
- **"Só roda em CUDA, sem fallback"**: violação do BackendSelector. Sempre ter caminho DirectML/Vulkan/CPU.
- Alocar cuStream/D3D12CommandQueue/VkCommandBuffer sem RAII: vazamento de handle => crash em prod.
- Testar só na máquina com RTX: regression precisa rodar em AMD/Intel via DirectML antes do merge.

Em dúvida, pergunte em `us4-core` antes de abrir PR. Custo de pergunta < custo de revert.
