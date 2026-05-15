# Domain Map

## Product Context

- App: `US4 V6 Windows Edition`
- Main users: ML engineers em Windows, researchers, app developers Windows-native, sysadmins corporativos
- Main business goal: entregar um runtime universal de inferência local para LLMs em Windows x86-64 com seleção automática de backend e memória tiered

## Core Concepts

| Concept | Meaning | Source of truth |
|---|---|---|
| Adapter | Módulo que traduz uma família de modelos para a interface do runtime. | `.specs/product/DOMAIN.md`, `US4-V6-Windows-Edition.md` |
| Backend | Implementação de execução: CUDA, DirectML, Vulkan, oneDNN, AVX/AMX ou Windows ML. | `US4-V6-Windows-Edition.md`, `.specs/product/DOMAIN.md` |
| Runtime Mode | Perfil global de degradação e orçamento de memória (`FULL` até `CPU_ONLY`). | `US4-V6-Windows-Edition.md`, `.specs/product/DOMAIN.md` |
| Hardware Profile | Combinação canônica de CPU/GPU/RAM/VRAM usada pelo auto-tuner e benchmark matrix. | `US4-V6-Windows-Edition.md`, `.specs/sprints/BACKLOG.md` |
| KV Pager | Gerenciador de KV cache entre VRAM quente, RAM morna e SSD frio. | `US4-V6-Windows-Edition.md`, `.specs/product/DOMAIN.md` |
| Expert Pager | Camada de paginação de experts para modelos MoE. | `US4-V6-Windows-Edition.md`, `.specs/product/DOMAIN.md` |
| Correctness Diff | Diferença de logits/tokens versus implementação de referência; gate duro antes de otimizar. | `AGENTS.md`, `.specs/sprints/BACKLOG.md` |

## Critical Rules

| Rule | Expected behavior | Where implemented | How to test |
|---|---|---|---|
| Correctness before performance | Qualquer otimização precisa manter diff dentro da tolerância definida. | `AGENTS.md`, `.specs/sprints/BACKLOG.md`, `runtime/benchmarks/correctness/` no futuro | Comparar saída baseline vs otimizada no mesmo prompt/seed. |
| Backend selection is vendor-aware | NVIDIA prioriza CUDA; AMD/Intel priorizam DirectML; Vulkan é fallback; CPU sempre existe. | `AGENTS.md`, `US4-V6-Windows-Edition.md` | Rodar probe de hardware e conferir backend recomendado. |
| No CUDA-only features | Toda feature de adapter precisa de caminho equivalente em DirectML/Vulkan/CPU, salvo ADR explícita. | `AGENTS.md`, `.specs/architecture/PATTERNS.md` | Revisão de design + cobertura por backend tocado. |
| SSD never blocks active decode | SSD é tier frio; o hot path deve permanecer em VRAM/RAM. | `US4-V6-Windows-Edition.md`, `.specs/product/DOMAIN.md` | Bench de page fault + trace de decode. |
| NPU is opt-in | Offload para NPU nunca pode ser obrigatório nem quebrar fallback. | `AGENTS.md`, `US4-V6-Windows-Edition.md` | Executar com e sem `--npu` e verificar fallback seguro. |

## Main Entities

| Entity | Description | Storage |
|---|---|---|
| `IUS4WindowsAdapter` | Contrato por família de modelo. | Código C++ futuro em `runtime/adapters/` |
| `BackendSelector` | Decide backend e modo com base no hardware. | Código C++ futuro em `runtime/core/` |
| `HardwareProbe` | Coleta capacidades do host Windows. | Código C++ futuro em `runtime/tuning/` |
| `KvPager` | Pagina KV entre VRAM/RAM/SSD. | Código C++ futuro em `runtime/kv/` |
| `ExpertPager` | Carrega e move experts MoE por prioridade. | Código C++ futuro em `runtime/moe/` |
| `AutoTuner` | Persiste perfis e escolhe parâmetros por máquina. | Código C++ futuro em `runtime/tuning/` |
| `Telemetry` | Emite métricas, traces e relatórios. | Código C++ futuro em `runtime/telemetry/` |

## Important Flows

### Backend Selection

1. User/system action: iniciar `us4-cli` ou chamar a DLL.
2. Entry point: `HardwareProbe`.
3. Main modules: `HardwareProbe` -> `BackendSelector` -> `RuntimeMode Selector`.
4. Output: backend recomendado e plano de memória inicial.
5. Evidence: saída de `--probe`, logs estruturados, benchmark report.

### Dense Inference

1. User/system action: rodar modelo Qwen/Gemma/Llama.
2. Entry point: `Adapter Dispatcher`.
3. Main modules: adapter denso -> scheduler -> backend escolhido -> telemetry.
4. Output: tokens gerados com métricas de latência/memória.
5. Evidence: Playwright CLI flow, JSON metrics, correctness diff.

### MoE Inference

1. User/system action: rodar DeepSeek/Kimi/MiniMax/GLM.
2. Entry point: adapter MoE.
3. Main modules: router -> expert pager -> KV pager -> backend -> telemetry.
4. Output: geração com experts aquecidos/paginados sem bloquear decode ativo.
5. Evidence: traces de page fault, bench de expert hit-rate, correctness report.

## Edge Cases

- VRAM insuficiente: degradar `Runtime Mode`, demover KV/experts e manter execução correta.
- Driver CUDA indisponível: fallback para Vulkan/DirectML/CPU conforme hardware.
- DirectML com op não suportado: fallback seguro sem corromper output.
- Laptop com NPU sem ganho real: manter NPU desabilitado ou recuar automaticamente.
- Long context com pressão de RAM: comprimir KV e usar SSD frio sem page fault por token.

## Open Questions

- Assinatura final do MSIX: depende da definição de release no Sprint 12.
- Estratégia final de OpenCppCoverage e thresholds por backend: consolidar quando o código de runtime existir.
