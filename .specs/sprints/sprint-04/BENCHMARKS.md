# Sprint 04 Bench Notes

## T04.1 / T04.4 / T04.5 - CPU AVX matmul, dequant, and oneDNN block GEMM

Local benchmark evidence is captured through `build/runtime/benchmarks/cpu_block_gemm_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cpu_block_gemm_bench.json`.

### Historical snapshot

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
- `T04.1` is now implemented with explicit `AVX2` and `AVX-512` kernels plus runtime ISA gating. On this machine (`Intel Core i5-1235U`), only the `AVX2` path is runnable; `AVX-512` is compiled and guarded but not directly executable here.
- `T04.4` is now implemented with `AVX2` group-wise `INT8` and `INT4` dequant paths. The same local benchmark now emits `cpu-q8-avx2` and `cpu-q4-avx2` variants with `tokens_per_second` in the JSON report, so the sprint evidence includes both correctness and quantized throughput.
- Current local evidence after the AVX hot-path landing is written to `runtime/benchmarks/correctness/reports/cpu_block_gemm_bench.json`. On this host, `AVX2` now measures above `11x` speedup vs the naive scalar baseline for `qwen-0.5b` and above `12x` for `gemma-2b`.
- `T04.7` remains open because its acceptance criterion is specifically `>=5x` on a real `AVX-512` host. This repository now records `cpu-avx512-*-unavailable` when the host cannot execute that ISA, instead of reporting misleading fallback numbers.
