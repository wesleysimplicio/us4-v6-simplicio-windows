# Sprint 05 Bench Notes

## T05.2 - AVX2 BitNet matmul

Local benchmark evidence is captured through `build/runtime/benchmarks/cpu_bitnet_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cpu_bitnet_bench.json`.

### Current local evidence

| Variant | Width | Iterations | Elapsed (ms) | Speedup vs scalar | Tokens/s |
|---|---:|---:|---:|---:|---:|
| `scalar-bitnet` | `4096` | `4096` | `11.879` | `1.000x` | `1412354342.574` |
| `avx2-bitnet` | `4096` | `4096` | `2.232` | `5.323x` | `7518358055.120` |

## Reading

- `T05.2` is now satisfied locally with a real `AVX2` lookup-driven path instead of the previous scalar-style placeholder implementation.
- Correctness is paired with the unit diff in `tests/unit/bitnet_ternary_test.cpp`, comparing `DotPackedBitNetAvx2` directly against `DotPackedBitNet`.
- The bench now runs enough work (`4096 x 4096` logical token steps) to avoid sub-millisecond noise and records `tokens_per_second` together with speedup vs scalar.
- Hosts without `AVX2` record `avx2-bitnet-unavailable` instead of faking fallback numbers.
