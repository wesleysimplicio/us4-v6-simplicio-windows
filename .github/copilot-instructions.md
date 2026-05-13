# copilot-instructions.md

> Espelho slim de [../AGENTS.md](../AGENTS.md) pro GitHub Copilot (Chat + Workspace + CLI). Quando GitHub Copilot abre este repositório, lê este arquivo e segue as mesmas regras de [`AGENTS.md`](../AGENTS.md). Para detalhes completos: ler `AGENTS.md`.
>
> Quando mudar `AGENTS.md`, atualize aqui também (ou trate `AGENTS.md` como única fonte da verdade e mantenha este arquivo como pointer).

---

## Projeto

- **Nome**: US4 V6 Windows Edition (`us4-v6-simplicio-windows`).
- **Domínio**: Universal State Runtime — inferência local de LLMs em Windows x86-64 (NVIDIA / AMD / Intel + NPU opcional).
- **Time**: us4-core.

## Stack

C++17/20 + CMake/Ninja + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + GoogleTest + Playwright (CLI E2E) + Ralph Loop. CI em `windows-2022` + GPU self-hosted opcional. Distribuição: MSIX installer assinado + portable zip.

## Comandos importantes

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DUS4_ENABLE_CUDA=ON -DUS4_ENABLE_DIRECTML=ON -DUS4_ENABLE_VULKAN=ON
cmake --build build -j
clang-format --dry-run --Werror (git ls-files '*.cpp' '*.h')
clang-tidy -p build (git ls-files 'runtime/**/*.cpp')
ctest --test-dir build --output-on-failure
npx playwright test
```

## Workflow OBRIGATÓRIO

`ler task -> planejar -> editar -> format -> lint -> unit -> e2e (trace+screenshot+video) -> regression + correctness por backend -> fix loop -> conventional commit (inglês) -> PR`.

## Definition of Done

- Build verde (`cmake --build build` sem warning novo, em todos backends habilitados)
- Unit verde (`ctest --test-dir build`)
- Lint verde (`clang-tidy` + `clang-format --dry-run`)
- E2E Playwright verde com **evidência salva** em TODA task que toca CLI/UX (`playwright-report/` + `test-results/<spec>/trace.zip` + screenshots + video). Hard rule.
- Coverage do diff >= 80% (OpenCppCoverage)
- Regression suite verde **por backend tocado** (CUDA / DirectML / Vulkan / AVX / NPU)
- Correctness diff dentro da tolerância (`runtime/benchmarks/correctness/`)
- AC marcados
- PR template preenchido
- Conventional commit
- ADR se decisão arquitetural
- Changelog se release-relevant
- Sem `std::cout` / `printf` / `OutputDebugString` de debug deixado pra trás
- Sem TODO órfão

## Proibido

- Pular testes
- Mockar pra esconder falha
- Commit vermelho
- Ignorar ADR
- Adicionar dependência sem perguntar
- Editar arquivo não lido
- Refactor escondido em PR de feature
- Force push em `main`
- Commitar segredo
- Reformatar arquivo inteiro num PR pequeno
- Quebrar correctness sem ADR
- CUDA-only sem fallback (toda feature de adapter precisa de caminho DirectML/Vulkan/CPU equivalente, salvo ADR explícito)

## Padrões

Detalhes em [`../.specs/architecture/PATTERNS.md`](../.specs/architecture/PATTERNS.md). Resumo: `PascalCase` classes / `camelCase` métodos / `kSnakeCase` constantes / `snake_case` arquivos. `#pragma once`. `std::unique_ptr` default. `Tensor` move-only. `std::expected<T,Error>` em ABI. No global mutable state fora de `RuntimeContext`. CUDA streams sempre explícitos. DirectML graph compile-once-execute-N.

## Skills disponíveis

Em `../.skills/`:

- **caveman** (default, modo terse)
- **ralph-loop** (loop autônomo, obrigatório em task com AC mensurável)
- **everything-claude-code** (~60 agents + ~221 skills, paralelo)
- **playwright-e2e** (sob demanda)
- **conventional-commits** (sob demanda)

## Custom agents

Em `../.agents/`:

- **ralph-loop.agent.md** — Ralph Loop (mapeia `/ralph-loop "<prompt>"`, `/goal`, `--autopilot`, "Autopilot", Background Agent).
- **tdd.agent.md** — TDD red-green-refactor com GoogleTest.
- **reviewer.agent.md** — Code Reviewer C++/CUDA read-only.
- **architect.agent.md** — Architect (ADRs, `PATTERNS.md`).

## Idioma

- Respostas/docs: pt-BR.
- Código (vars/funções/classes): inglês.
- Commits: inglês.
- Sem emoji em código. Sem resumo no final. Sem estimativa de tempo.

## Contexto

- Backlog: `../.specs/sprints/BACKLOG.md`.
- Sprint atual: `../.specs/sprints/sprint-XX/SPRINT.md`.
- Padrões: `../.specs/architecture/PATTERNS.md`.
- Decisões: `../.specs/architecture/ADR-*.md`.
