# Sprint 03 Bench Notes

## T03.2 - CUDA kernels matmul/softmax/rmsnorm

Local evidence is captured through `build/runtime/benchmarks/cuda_kernel_correctness_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cuda_kernel_correctness_bench.json`.

### Historical snapshot

| Kernel | Precision | Max abs diff vs scalar CPU |
|---|---|---:|
| `matmul` | `fp16` | `0.000000` |
| `softmax` | `bf16` | `0.001366` |
| `rmsnorm` | `fp16` | `0.000922` |
| `rmsnorm` | `bf16` | `0.007696` |

## Reading

- `T03.2` is satisfied locally as a host-compiled CUDA kernel scaffold: `matmul`, `softmax`, and `rmsnorm` now expose CUDA-surface entry points and deterministic correctness evidence without depending on `nvcc` or a discrete CUDA device in this workspace.
- The hard correctness gate stays aligned to the issue contract: `matmul fp16 <= 1e-2`, stable `softmax`, and `rmsnorm fp16 <= 5e-3`. The `bf16` `rmsnorm` path is still validated for numerical stability and stays close to reference (`0.007696`) even though the hard local gate remains on the `fp16` path.
- This bench remains a local correctness bench, not a device-side throughput benchmark. Real CUDA throughput work still depends on a later hardware-backed pass of Sprint 03.

## T03.3 - cuBLAS/cuBLASLt fallback wrapper

Local evidence is captured through `build/runtime/benchmarks/cuda_fallback_policy_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cuda_fallback_policy_bench.json`.

### Historical snapshot

| Case | Primary | Fallback | Primary latency (us) | Fallback latency (us) | Speedup vs fallback |
|---|---|---|---:|---:|---:|
| `aligned_fp16_decode` | `custom-kernel` | `cublas` | `9.640439` | `20.684355` | `2.145582x` |
| `irregular_batched_prefill` | `cublaslt` | `cublas` | `562.134621` | `733.483735` | `1.304819x` |
| `deterministic_small_step` | `cublas` | `cublaslt` | `18.671089` | `15.500473` | `0.830186x` |

- `T03.3` is satisfied locally as a policy-and-wrapper scaffold: aligned `FP16/BF16` shapes stay on the custom-kernel lane, while irregular or deterministic shapes degrade to `cuBLAS` / `cuBLASLt` without hiding the fallback reason.
- The current bench is intentionally a dispatch-policy bench, not a device-side CUDA throughput benchmark. It documents the local expected policy delta while Sprint 03 still keeps the shared CUDA runtime on the deterministic reference path.
- The deterministic case intentionally favors `cuBLAS` over the synthetic `cuBLASLt` estimate, which is why `speedup_vs_fallback` can fall below `1.0x` for that row.
