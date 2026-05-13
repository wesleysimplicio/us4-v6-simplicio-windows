# .specs — Mapa de Navegação

Pasta concentra todo o contexto que o agente AI precisa pra trabalhar em **US4 V6 Windows Edition** (`us4-v6-simplicio-windows`). Quando algo não está aqui, o agente não vê. Specs como código de primeira classe.

Stack alvo: C++17/20 + CMake/Ninja + CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML (NPU) + GoogleTest + Playwright + Ralph Loop.

## Ordem de leitura recomendada

Tanto humano novo no time quanto agente devem percorrer nessa ordem:

1. **`product/VISION.md`** — por que o produto existe. Problema, diferencial, métricas.
2. **`product/PERSONAS.md`** — pra quem o produto existe. Objetivos e frustrações.
3. **`product/DOMAIN.md`** — vocabulário e entidades de runtime/adapters.
4. **`architecture/DESIGN.md`** — diagrama macro, boundaries, stack, backends.
5. **`architecture/PATTERNS.md`** — como escrever código aqui. Naming, estrutura, error handling, kernels.
6. **`architecture/ADR-*.md`** — decisões arquiteturais e suas razões (criadas durante os sprints).
7. **`workflow/WORKFLOW.md`** — branch strategy, PR, deploy, hotfix.
8. **`workflow/CONTRIBUTING.md`** — como adicionar uma feature passo a passo.
9. **`workflow/RELEASE.md`** — versionamento e release.
10. **`sprints/BACKLOG.md`** — matriz dos 12 sprints.
11. **`sprints/sprint-XX/SPRINT.md`** — sprint corrente.
12. **`sprints/sprint-XX/NN-*.task.md`** — tasks ativas (criadas conforme sprint avança).

## Estrutura

```
.specs/
├── README.md
├── product/
│   ├── VISION.md
│   ├── DOMAIN.md
│   └── PERSONAS.md
├── architecture/
│   ├── DESIGN.md            # preenchido no sprint-01 (T01.9)
│   ├── PATTERNS.md          # preenchido incrementalmente nos sprints
│   ├── ADR-template.md
│   └── ADR-XXX-*.md         # criados durante os sprints
├── workflow/
│   ├── WORKFLOW.md
│   ├── CONTRIBUTING.md
│   └── RELEASE.md
└── sprints/
    ├── BACKLOG.md
    ├── task-template.md
    ├── sprint-01/SPRINT.md
    ├── sprint-02/SPRINT.md
    ├── ...
    └── sprint-12/SPRINT.md
```

## Convenções

- Markdown puro, cabeçalho `# Título` claro.
- Diagramas em Mermaid embutido (`mermaid` code block).
- Tabelas pra glossários e listas comparativas.
- Bullets curtos, frases na voz ativa.
- Idioma: pt-BR pro conteúdo, inglês pra nomes técnicos (variáveis, comandos, identifiers).

## Como adicionar nova spec

- Decisão arquitetural irreversível -> nova `ADR-NNN-titulo.md` em `architecture/` baseada em `ADR-template.md`.
- Nova feature grande -> task em `sprints/task-template.md` dentro de `sprints/sprint-XX/`.
- Novo conceito de domínio -> entrada em `product/DOMAIN.md`.
- Nova rotina de processo -> seção em `workflow/WORKFLOW.md` ou doc novo em `workflow/`.

## Pra o agente

Antes de implementar qualquer task:

- Confirmar que leu VISION + DESIGN + PATTERNS + a task atual.
- Procurar ADR relacionada antes de inventar decisão.
- Atualizar DOMAIN se introduzir novo conceito de runtime/adapter.
- Atualizar BACKLOG/SPRINT.md ao fechar/abrir item.
- Logit-diff vs referência é gate de correctness — nunca pula.
