# ADR-004: Stage CUDA execution around explicit streams, pooled memory, and graph-safe kernels

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

A Sprint 03 introduz o primeiro backend GPU principal do produto. O contrato do projeto ja define NVIDIA como rota primaria quando houver hardware compativel, mas ainda faltava formalizar como esse backend deve evoluir sem quebrar o boundary entre core, adapters e execucao de dispositivo.

Tambem precisamos proteger uma regra ja presente em `PATTERNS.md`: CUDA nao pode virar um conjunto de chamadas soltas de alocacao, sincronizacao global e kernels acoplados a adapters. O runtime atual ja centraliza escolha de backend e modo em `HardwareProbe`, `BackendSelector` e `RuntimeMode`; o backend CUDA precisa seguir essa mesma filosofia de politica centralizada e execucao observavel.

## Decisao

Adotamos uma estrategia de execucao CUDA baseada em streams explicitas, memory pool por sessao e captura de grafos apenas em regioes estaveis de prefill/decode.

- `CudaContext` e o boundary dono de device, streams, allocator/pool e telemetria de memoria.
- Kernels customizados e wrappers de cuBLAS/cuBLASLt ficam no backend CUDA; adapters descrevem planos e formatos, mas nao chamam primitives CUDA diretamente.
- Toda operacao quente recebe stream explicitamente; sincronizacao global fica restrita a bordas de teste, teardown ou recuperacao de falha.
- CUDA Graphs entram apenas em trechos repetitivos e shape-stable, principalmente loops de decode e blocos de prefill que nao exijam recaptura por token.
- Quando um kernel otimizado nao estiver disponivel ou falhar no gate de correctness, o backend degrada para wrapper de biblioteca ou para backend alternativo sem corromper a sessao.

## Consequencias

### Positivas (+)

- O caminho NVIDIA fica alinhado com a proposta de throughput alto sem espalhar conhecimento de CUDA pelo core.
- Streams, pools e grafos passam a ter donos claros, o que facilita teste, benchmark e telemetria.
- Fallback para cuBLAS/cuBLASLt ou backend alternativo preserva correctness enquanto kernels customizados amadurecem.

### Negativas (-)

- O custo inicial de infra do backend cresce antes de todos os kernels estarem prontos.
- Captura de grafos exige disciplina de shapes e buffers, o que aumenta a complexidade de integracao.
- Bugs de concorrencia e lifetime de memoria ficam mais provaveis se o boundary do `CudaContext` for violado.

### Neutras / observacoes

- O trilho CPU scalar continua sendo a referencia canonica de correctness para gates e benchmarks comparativos.
- Esta decisao cobre a arquitetura do backend CUDA; detalhes de kernel especifico podem exigir ADR adicional se mudarem formato ou ABI.

## Alternativas consideradas

### Alternativa A - Chamar CUDA diretamente a partir de adapters

- Resumo: cada adapter controlaria streams, buffers e kernels conforme sua familia de modelo.
- Por que foi descartada: mistura politica de modelo com politica de dispositivo e enfraquece fallback, teste e telemetria.

### Alternativa B - Usar apenas cuBLAS/cuBLASLt sem kernels proprios nem graph capture

- Resumo: limitar o backend a wrappers de biblioteca.
- Por que foi descartada: simplifica o bootstrap, mas nao atende a meta de controlar decode quente, custom kernels e reuse de grafos.

## Criterio de revisao

- Revisar quando a captura de grafos se mostrar incompativel com batching continuo ou speculative decoding.
- Revisar quando benchmarks mostrarem que pools por sessao nao sao suficientes e um pool por processo trouxer ganho material sem regressao operacional.
- Revisar quando uma familia de modelo exigir kernels CUDA com contrato estruturalmente diferente do restante do backend.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-001](./ADR-001-adapter-interface.md), [ADR-002](./ADR-002-backend-selection.md), [ADR-003](./ADR-003-runtime-mode.md)

## Evidencia local de fallback policy

O wrapper de fallback `cuBLAS/cuBLASLt` agora tem evidencia local versionada em
`runtime/benchmarks/correctness/reports/cuda_fallback_policy_bench.json`, gerada por
`build/runtime/benchmarks/cuda_fallback_policy_bench.exe`.

Snapshot local mais recente:

| Caso | Primario | Fallback | Latencia primaria (us) | Latencia fallback (us) | Delta |
|---|---|---|---:|---:|---:|
| `aligned_fp16_decode` | `custom-kernel` | `cublas` | `9.640439` | `20.684355` | `2.145582x` mais rapido no custom kernel |
| `irregular_batched_prefill` | `cublaslt` | `cublas` | `562.134621` | `733.483735` | `1.304819x` mais rapido no `cublaslt` |
| `deterministic_small_step` | `cublas` | `cublaslt` | `18.671089` | `15.500473` | `0.830186x` - politica prioriza determinismo sobre throughput |

Esses numeros documentam a politica de dispatch do wrapper nesta fase do projeto. Eles nao substituem benchmark device-side real de CUDA kernels, que continua dependente de completar `T03.1` e `T03.2`.
