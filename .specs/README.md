# .specs — Mapa de Navegação

Pasta concentra o contexto operacional e arquitetural que guia a implementação de **US4 V6 Windows Edition** (`us4-v6-simplicio-windows`). Aqui vivem a visão do produto, o contrato de runtime, o fluxo de trabalho e o backlog por sprint.

Stack alvo: C++17/20 + CMake/Ninja + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + GoogleTest + Playwright + Ralph Loop.

## Ordem de leitura recomendada

Humano novo no time e agent AI devem percorrer nesta ordem:

1. `product/VISION.md` — por que o runtime existe e qual problema ele resolve.
2. `product/PERSONAS.md` — quem usa o sistema e quais máquinas reais precisam ser suportadas.
3. `product/DOMAIN.md` — vocabulário de runtime, adapters, backends, KV/cache e modos.
4. `architecture/DESIGN.md` — macroarquitetura, boundaries, topologia de memória e seleção de backend.
5. `architecture/PATTERNS.md` — convenções de código C++/CUDA/DirectML/Vulkan/CPU, testes e fallback.
6. `architecture/ADR-*.md` — decisões irreversíveis do sistema, quando existirem.
7. `workflow/WORKFLOW.md` — branch strategy, review, hooks e fluxo de execução.
8. `workflow/CONTRIBUTING.md` — como abrir task, implementar, validar e preparar PR.
9. `workflow/RELEASE.md` — como gerar MSIX, portable zip e evidências de release.
10. `sprints/BACKLOG.md` — mapa dos 12 sprints.
11. `sprints/sprint-XX/SPRINT.md` — objetivo e capacidade da sprint atual.
12. `sprints/sprint-XX/*.task.md` — contexto, AC, plano de teste e DoD da task em andamento.

## Estrutura

```text
.specs/
  README.md
  product/
    VISION.md
    PERSONAS.md
    DOMAIN.md
  architecture/
    DESIGN.md
    PATTERNS.md
    ADR-template.md
    ADR-XXX-*.md
  workflow/
    WORKFLOW.md
    CONTRIBUTING.md
    RELEASE.md
  sprints/
    BACKLOG.md
    task-template.md
    sprint-01/
    ...
    sprint-12/
```

## Convenções

- Conteúdo em pt-BR; nomes técnicos, flags, identifiers e comandos em inglês.
- Diagramas em Mermaid quando ajudarem a explicar boundaries ou fluxo.
- Tabelas para glossário, matrizes de backend e checklists comparativos.
- Specs descrevem o contrato real do projeto, não textos genéricos de scaffold.
- Mudança arquitetural importante deve manter coerência entre `DOMAIN.md`, `DESIGN.md` e `PATTERNS.md`.

## Como adicionar nova spec

- Decisão arquitetural irreversível: criar `architecture/ADR-NNN-<slug>.md` a partir de `ADR-template.md`.
- Novo conceito de runtime, adapter ou backend: atualizar `product/DOMAIN.md`.
- Mudança em boundary, memória ou fluxo de execução: atualizar `architecture/DESIGN.md`.
- Nova convenção de código ou teste: atualizar `architecture/PATTERNS.md`.
- Nova feature planejada: adicionar task na sprint correta a partir de `sprints/task-template.md`.

## Para o agente

Antes de implementar qualquer task:

- Ler a task atual e o contexto mínimo `VISION + DOMAIN + DESIGN + PATTERNS`.
- Checar se a mudança toca backend, adapter, KV/cache, CLI ou release.
- Procurar ADR existente antes de improvisar decisão estrutural.
- Atualizar specs no mesmo PR quando o vocabulário ou o contrato técnico mudar.
- Tratar correctness diff, regressão por backend e evidência Playwright como gates reais, não opcionais.
