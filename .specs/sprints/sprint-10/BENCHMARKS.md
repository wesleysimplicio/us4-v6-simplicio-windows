# Sprint 10 Bench Notes

## T10.6 - CUDA Graph reuse cross-step speculative

Local evidence is captured through `build/runtime/benchmarks/cuda_graph_reuse_bench.exe`
and written to `runtime/benchmarks/correctness/reports/cuda_graph_reuse_bench.json`.

### Historical snapshot

| Variant | Steps | Graph captures | Cache hits | Graph replays | Elapsed (ms) | Speedup vs reset |
|---|---:|---:|---:|---:|---:|---:|
| `reset-per-step` | `512` | `512` | `0` | `512` | `0.818900` | `1.000000x` |
| `reuse-cross-step` | `512` | `1` | `511` | `512` | `0.695700` | `1.177088x` |

## Reading

- `T10.6` is satisfied locally as a synthetic integration over `CudaContext`: speculative decode steps now acquire graph executables by stable fingerprint and reuse them instead of re-capturing every step.
- The local bench proves the intended delta of the scaffold path: one capture plus `511` cache hits for the reuse run, versus `512` captures for the reset-per-step baseline.
- This evidence is deliberately scoped to the current local CUDA scaffold and does not claim a device-side RTX throughput number. Real GPU speedup remains a later runtime milestone once the backend moves past the deterministic reference path.
