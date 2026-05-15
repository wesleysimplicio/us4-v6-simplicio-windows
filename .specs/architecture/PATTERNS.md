# Patterns — US4 V6 Windows Edition

> Como escrever código aqui. Curto, opinativo, executável e alinhado ao contrato de runtime Windows.
> Audiência: dev humano, reviewer e agent AI. Se a regra não estiver aqui, vale o restante do `AGENTS.md` e revisão técnica.

---

## 1. Naming

| Item | Convenção | Exemplo bom | Exemplo ruim |
|---|---|---|---|
| Classes / structs / enums | `PascalCase` | `HardwareProbe`, `RuntimeMode` | `hardware_probe`, `runtime_mode_t` |
| Funções / métodos | `camelCase` | `selectBackend`, `buildMemoryPlan` | `Select_Backend`, `build_memory_plan` |
| Constantes | `kSnakeCase` | `kMaxKvTiers` | `MAX_KV_TIERS`, `maxKvTiers` |
| Arquivos | `snake_case` | `hardware_probe.cpp`, `kv_pager.h` | `HardwareProbe.cpp`, `kv-pager.hpp` |
| Namespaces | curto e técnico | `us4::runtime`, `us4::cuda` | `myNamespace`, `helpers` |
| Flags CLI | `kebab-case` | `--enable-kv-paging` | `--enable_kv_paging` |
| Testes | alvo + `_test.cpp` | `backend_selector_test.cpp` | `test1.cpp` |
| Benchmarks | alvo + `_benchmark.cpp` | `kv_pager_benchmark.cpp` | `bench.cpp` |

Regra de ouro: nome deve revelar papel técnico e boundary. `plan` é vago; `memoryPlan` ou `decodePlan` é claro.

---

## 2. Estrutura de pastas

Estrutura alvo do código nativo:

```text
runtime/
  core/
  adapters/
    deepseek/
    kimi/
    minimax/
    glm/
    qwen/
    llama/
    gemma/
    bitnet/
    ternary/
  memory/
  kv/
  cache/
  moe/
  backends/
    cuda/
    directml/
    vulkan/
    onednn/
    cpu_avx/
    windows_ml/
  speculative/
  tuning/
  telemetry/
  benchmarks/
tests/
  unit/
  integration/
  e2e/
```

Regra:
- `core/` define contratos e orquestração.
- `adapters/` contêm lógica específica de família de modelo.
- `backends/` contêm execução dependente de dispositivo.
- `telemetry/` mede; não decide fluxo.
- Arquivo `.h` vive ao lado do `.cpp` ou `.cu`.

---

## 3. Como adicionar um adapter novo

1. Ler `DOMAIN.md`, `DESIGN.md` e a task do sprint.
2. Criar pasta em `runtime/adapters/<family>/`.
3. Implementar interface de adapter comum do projeto.
4. Declarar capacidades reais: `supports_moe`, `supports_mla`, `supports_gqa`, `supports_multimodal`, `supports_ternary`.
5. Definir `KVLayout`, `QuantStrategy` e `MemoryPlan` sem hardcode de vendor.
6. Registrar o adapter no `ModelRegistry`.
7. Cobrir com testes unitários de seleção, planejamento de memória e fallback seguro.
8. Se tocar CLI/UX, adicionar ou atualizar E2E Playwright com evidência.

Critério de feito:
- o adapter gera config coerente em CUDA, DirectML/Vulkan e CPU conforme escopo
- o caminho otimizado é desligável
- correctness diff fica dentro da tolerância

---

## 4. Como adicionar ou alterar um backend

1. Alterar apenas a pasta do backend e os contratos estritamente necessários em `core/`.
2. Expor inicialização, alocação, execução e stats; não esconder estado em singleton global.
3. Implementar fallback claro quando feature não existir no hardware.
4. Adicionar benchmark dedicado e teste de correctness vs referência.
5. Atualizar `BackendSelector` somente se a ordem de prioridade ou detecção realmente mudar.

Backend novo ou alterado sempre precisa responder:
- como inicializa dispositivo
- como aloca pools e buffers
- como reporta uso de memória
- como falha sem corromper sessão

---

## 5. Ownership e memória

- `Tensor` é move-only por padrão.
- `TensorView` representa empréstimo sem ownership.
- `std::unique_ptr` é default; `std::shared_ptr` só quando ownership compartilhado for parte do design.
- Nada de ponteiro cru como owner.
- Alocação de VRAM precisa passar por planejadores/pools do backend, não por chamadas dispersas.
- Estruturas de hot path evitam cópias implícitas.

Exemplo:

```cpp
class KvPager {
public:
    std::expected<TensorView, Error> mapPage(PageId pageId);
    std::expected<void, Error> promote(PageId pageId);
};
```

---

## 6. Erros e boundaries

Princípio: sem exceções atravessando ABI ou boundary de backend.

- Interfaces públicas de runtime/adapters/backends retornam `std::expected<T, Error>` ou equivalente do projeto.
- `Error` precisa carregar código, boundary e contexto mínimo para log.
- Função que falha precisa dizer se o caller pode degradar, repetir ou abortar.
- `catch (...)` só em bordas de processo/CLI para converter em erro estruturado.

Exemplo:

```cpp
std::expected<BackendPlan, Error> BackendSelector::select(
    const HardwareProfile& hardware,
    const RuntimePreferences& preferences);
```

Nunca use retorno booleano nu quando a causa da falha importa.

---

## 7. Regras específicas de CUDA

- Streams sempre passados explicitamente.
- `cudaDeviceSynchronize()` é proibido em hot path.
- Use Graphs apenas em padrões repetidos de decode/prefill.
- H2D/D2H assíncrono com pinned memory quando aplicável.
- Kernels vetorizados precisam de caminho de referência e teste de correctness.
- Métricas de pool, alocação e transferências precisam ser expostas.

Smells:
- sincronização global para depurar e esquecida em produção
- alocação `cudaMalloc` por token
- kernel acoplado a um adapter quando o problema é genérico do backend

---

## 8. Regras específicas de DirectML, Vulkan e CPU

DirectML:
- compilar grafo uma vez, executar N vezes
- reusar descriptor heaps e buffers
- fallback claro para operador não suportado

Vulkan:
- pipelines e descriptor sets reutilizáveis
- nada de recriar shader module por request
- rastrear uso de memória quando a API permitir

CPU / oneDNN / AVX:
- toda rota vetorizada tem referência scalar
- detectar AVX2, AVX-512 e AMX em `HardwareProbe`, não em cada chamada dispersa
- otimização de quant/dequant não pode mudar saída fora da tolerância definida

---

## 9. Logging e telemetria

- Logs estruturados, nunca `std::cout` de debug permanente.
- Cada evento relevante inclui `backend`, `adapter`, `model`, `runtime_mode`, `trace_id`.
- Benchmarks e correctness exportam artefatos comparáveis por hardware profile.
- `debug` pode ser verboso; `info` precisa ser enxuto.
- Não logar prompts completos ou caminhos sensíveis sem necessidade real.

Exemplo de campos úteis:
- `tokens_per_second`
- `time_to_first_token_ms`
- `gpu_memory_used_mb`
- `kv_page_faults`
- `expert_cache_hit_rate`
- `fallback_reason`

---

## 10. Testes

Pirâmide do projeto:

- Unit: `GoogleTest` para selector, planner, registry, adapters e utilitários puros.
- Integration: execução real de componentes com backend ou fixture controlada.
- E2E: `Playwright` para fluxos CLI com artefatos.
- Regression/correctness: benchmark de paridade por backend tocado.

Toda task que toca CLI/UX precisa:
- `npx playwright test --reporter=list,html`
- `playwright-report/`
- `test-results/` com trace e evidência

Toda task que toca backend/runtime precisa:
- `ctest --test-dir build --output-on-failure`
- rerun de correctness/benchmark no backend afetado

---

## 11. Formatação e escopo de diff

- Rodar `clang-format` apenas nos arquivos tocados ou no recorte mínimo necessário.
- Não reformatar um subsistema inteiro num PR pequeno.
- Comentário de código só quando o bloco não for óbvio.
- Refactor sem relação com a task vira PR separado.
- Arquivo grande demais é sinal para dividir por boundary, não para empilhar helpers privados.

Heurísticas:
- função > 60 linhas merece revisão
- classe com múltiplas responsabilidades merece split
- arquivo > 400 linhas precisa justificativa

---

## 12. Checklist para mudança técnica

Antes de fechar uma task:

1. Confirmar que leu task + `DESIGN.md` + este arquivo.
2. Verificar se introduziu conceito novo em `DOMAIN.md`.
3. Rodar format/lint.
4. Rodar unit.
5. Rodar E2E se tocou CLI/UX.
6. Rodar correctness/regression no backend afetado.
7. Garantir que não sobraram logs de debug, TODO sem dono ou fallback quebrado.

Regra do time us4-core: simplicidade operacional ganha de cleverness. O código precisa ser previsível sob pressão de memória e múltiplos backends.
---

## 13. Release and packaging scripts

- Script de release precisa falhar com mensagem explÃ­cita quando faltar ferramenta, assinatura ou trust.
- `portable zip`, `checksums`, `winget`, `preflight`, `sign` e `install smoke` sÃ£o superfÃ­cies testÃ¡veis; nÃ£o tratÃ¡-las como shell glue descartÃ¡vel.
- ValidaÃ§Ã£o de release deve produzir saÃ­da estruturada quando o consumidor for automaÃ§Ã£o.
- Placeholder de URL, certificado ou publisher precisa ser evidente e fÃ¡cil de detectar em teste.
- Script que depende de segredo nunca pode embutir fallback silencioso para valor fake.
