# Sprint 04 Bench Notes

## T04.5 — oneDNN block GEMM backend

Local benchmark evidence captured through `build/runtime/benchmarks/cpu_block_gemm_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cpu_block_gemm_bench.json`.

### Snapshot

| Model | Variant | Elapsed (ms) | Speedup vs scalar |
|---|---:|---:|---:|
| `qwen-0.5b` | `scalar` | `5061.152` | `1.000x` |
| `qwen-0.5b` | `cpu-avx512` | `2181.934` | `2.320x` |
| `qwen-0.5b` | `onednn` | `3268.804` | `1.548x` |
| `gemma-2b` | `scalar` | `9072.181` | `1.000x` |
| `gemma-2b` | `cpu-avx512` | `3358.202` | `2.701x` |
| `gemma-2b` | `onednn` | `4115.990` | `2.204x` |

## Reading

- `T04.5` is satisfied locally: the backend exists, matches scalar correctness, and emits a committed bench delta against the direct CPU AVX path.
- `T04.7` remains open: current local AVX-512 speedups are below the issue acceptance criterion of `>=5x` vs scalar, so the re-bench task stays honest until the AVX hot paths (`T04.1`–`T04.4`) land.
