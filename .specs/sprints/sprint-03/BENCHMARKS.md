# Sprint 03 Bench Notes

## T03.3 - cuBLAS/cuBLASLt fallback wrapper

Local evidence is captured through `build/runtime/benchmarks/cuda_fallback_policy_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cuda_fallback_policy_bench.json`.

### Historical snapshot

| Case | Primary | Fallback | Primary latency (us) | Fallback latency (us) | Speedup vs fallback |
|---|---|---|---:|---:|---:|
| `aligned_fp16_decode` | `custom-kernel` | `cublas` | `9.640439` | `20.684355` | `2.145582x` |
| `irregular_batched_prefill` | `cublaslt` | `cublas` | `562.134621` | `733.483735` | `1.304819x` |
| `deterministic_small_step` | `cublas` | `cublaslt` | `18.671089` | `15.500473` | `0.830186x` |

## Reading

- `T03.3` is satisfied locally as a policy-and-wrapper scaffold: aligned `FP16/BF16` shapes stay on the custom-kernel lane, while irregular or deterministic shapes degrade to `cuBLAS` / `cuBLASLt` without hiding the fallback reason.
- The current bench is intentionally a dispatch-policy bench, not a device-side CUDA throughput benchmark. It documents the local expected policy delta while Sprint 03 still keeps the shared CUDA runtime on the deterministic reference path.
- The deterministic case intentionally favors `cuBLAS` over the synthetic `cuBLASLt` estimate, which is why `speedup_vs_fallback` can fall below `1.0x` for that row.
