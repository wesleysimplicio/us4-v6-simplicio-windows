# ADR-005: Keep CPU optimized and BitNet/Ternary paths anchored to scalar parity and adapter-owned packed formats

## Status

Aceito

## Data

2026-05-14

## Autores

- us4-core

## Contexto

As Sprints 04 e 05 aumentam bastante a variacao de execucao local: AVX2, AVX-512, AMX, oneDNN, BitNet 1.58-bit e adapters ternarios. Sem uma decisao explicita, o runtime corre dois riscos: espalhar deteccao de ISA e logica de packed format por varios pontos do codigo, ou permitir que otimizacoes de CPU e quantizacao especial mudem o comportamento sem trilha clara de correctness.

O repositorio atual ja tem uma base scalar funcional e usa esse trilho como referencia de paridade. A direcao do projeto tambem separa `core`, `adapters` e `backends`, entao a entrada de BitNet/Ternary precisa respeitar essa divisao em vez de empurrar formatos compactados para o core.

## Decisao

Mantemos os caminhos CPU otimizados e BitNet/Ternary subordinados a uma referencia scalar canonica, com deteccao de ISA centralizada e formatos compactados pertencendo ao boundary de adapters/loaders.

- `HardwareProbe` e `BackendSelector` escolhem AVX2, AVX-512, AMX, oneDNN ou scalar; adapters nao fazem CPUID ad hoc.
- Toda rota vetorizada ou block GEMM precisa de uma referencia scalar equivalente e desligavel para gates de correctness.
- BitNet e Ternary entram como adapters especializados, mas continuam obedecendo ao mesmo contrato de `Attach`, `LoadModel`, `Prefill`, `Generate` e `Reset`.
- Packing, unpacking, LUTs ternarias e metadados de blocos ficam em loaders/adapters e nos kernels do backend; o core consome apenas descricoes normalizadas de layout e quant strategy.
- `MICRO` e `NANO` priorizam BitNet/Ternary quando o budget de memoria justificar, mas nunca removem a possibilidade de degradar para dense scalar ou CPU baseline se a paridade falhar.

## Consequencias

### Positivas (+)

- Acelerar CPU e introduzir modelos compactados nao quebra o contrato comum do runtime.
- Packed formats ficam confinados aos boundaries certos, o que reduz acoplamento no core.
- Correctness continua comparavel porque cada otimizacao ou quantizacao mantem uma referencia scalar clara.

### Negativas (-)

- O custo de implementacao sobe porque cada rota otimizada precisa de baseline e harness de comparacao.
- BitNet/Ternary podem ter throughput inicial menor que o desejado enquanto os kernels especializados amadurecem.
- Formatos compactados ainda vao exigir disciplina forte de loader, versionamento e documentacao para evitar drift entre adapters e backends.

### Neutras / observacoes

- Esta decisao cobre o ownership arquitetural de packed formats; detalhes binarios exatos de um container especifico ainda podem ganhar ADR sucessora se ficarem irreversiveis.
- oneDNN entra como acelerador de CPU dentro do mesmo boundary, nao como substituto do contrato de backend do projeto.

## Alternativas consideradas

### Alternativa A - Deixar cada kernel otimizado definir seu proprio formato e caminho de carga

- Resumo: AVX, AMX, oneDNN, BitNet e ternary escolheriam livremente layouts e loaders.
- Por que foi descartada: produziria fragmentacao, bugs de interoperabilidade e dificultaria fallback seguro.

### Alternativa B - Tratar BitNet/Ternary como excecoes fora do contrato comum de adapters

- Resumo: criar um pipeline paralelo so para modelos compactados.
- Por que foi descartada: enfraquece o runtime universal e multiplica os pontos de manutencao.

## Criterio de revisao

- Revisar quando a referencia scalar deixar de ser viavel como baseline operacional para um grupo relevante de kernels.
- Revisar quando um formato BitNet ou ternario dominante exigir um contrato de loader incompatível com a normalizacao atual.
- Revisar quando dados de benchmark mostrarem que a preferencia de `MICRO` e `NANO` por modelos compactados precisa mudar por classe de hardware.

## Links

- Documentos relacionados: [DESIGN](./DESIGN.md), [PATTERNS](./PATTERNS.md)
- ADRs relacionados: [ADR-001](./ADR-001-adapter-interface.md), [ADR-002](./ADR-002-backend-selection.md), [ADR-003](./ADR-003-runtime-mode.md)
