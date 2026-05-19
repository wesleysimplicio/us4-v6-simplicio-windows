# Sprint 07 Bench Notes

## T07.2 - Llama adapter real-tokenizer scalar path

Local evidence is captured through `tests/unit/llama_runtime_test.cpp`, especially
`LlamaScalarAdapterUsesLoadedTokenizerWhenModelIsAvailable`, and exercised inside the
full validation loop driven by `scripts/test.ps1`.

### Reading

- `T07.2` is not closed yet because the original acceptance criteria still require
  full attention across `CUDA`, `DirectML`, `AVX`, and `Vulkan`, plus end-to-end
  throughput on target hardware.
- This local milestone materially reduces adapter risk by proving that the scalar
  `Llama` runtime path now consumes a concrete `tokenizer.json`, `config.json`, and
  `model.safetensors` fixture instead of relying only on the generic byte-oriented
  fallback tokenizer.
- The new runtime test locks in prompt tokenization for `"hello llama runtime"` as
  `{0, 1, 2, 1, 3}` and verifies deterministic detokenization back to the original
  prompt.
- The dry-run CLI surface now also proves that `Vulkan` and `Windows ML` bind a real
  `dense-llama` adapter contract instead of emitting backend-only planning output.
- Synthetic and planner-only flows remain intact because `LlamaScalarAdapter` now
  falls back to `DenseAdapterBase::LoadModel` whenever a full `Llama` asset load is
  not available.

### Current dry-run contract snapshot

| Backend | Model | Adapter | Contract evidence |
|---|---|---|---|
| `cuda` | `qwen-0.5b` | `dense-qwen` | adapter id, residency estimates, prefill telemetry |
| `directml` | `qwen-0.5b` | `dense-qwen` | adapter id, residency estimates, prefill telemetry |
| `vulkan` | `qwen-0.5b` | `dense-qwen` | adapter id, residency estimates, prefill telemetry |
| `windows-ml` | `llama-3.2-3b` | `dense-llama` | adapter id, residency estimates, prefill telemetry |

## T07.6 - Llama 3.x 8B cross-backend contract bench

Local evidence is captured through `build/runtime/benchmarks/llama_backend_contract_bench.exe`
and written to `runtime/benchmarks/correctness/reports/llama_backend_contract_bench.json`.

### Historical snapshot

| Variant | Backend | Status | Elapsed (ms) | Tokens/s | Latency per token (ms) | Peak bytes | Detail |
|---|---|---|---:|---:|---:|---:|---|
| `llama_8b_cpu_avx_contract` | `cpu` | `pass` | `1.460100` | `5479.076776` | `0.182512` | `16384` | `cpu-scalar-avx-contract` |
| `llama_8b_cuda_contract` | `cuda` | `pass` | `0.028200` | `2269503.546099` | `0.000441` | `11077156864` | `cuda-graph-captured` |
| `llama_8b_directml_contract` | `directml` | `pass` | `0.001800` | `35555555.555556` | `0.000028` | `6484395520` | `directml-graph-contract` |

### Reading

- `T07.6` is satisfied locally as a benchmark harness and committed evidence surface for
  `Llama 3.x 8B` across `CPU/AVX`, `CUDA`, and `DirectML`.
- The measurements are explicitly scoped as `synthetic-local-contract`, not as claims of
  real RTX 4090, Radeon RX 7900, or AVX-512 production throughput.
- `CPU/AVX` runs through `RuntimeContext` + `ExecuteCpuScalarRun`, while `CUDA` and
  `DirectML` run through their current deterministic backend contract paths.
- The report preserves `tokens/s`, `latency`, `peak_bytes`, `issue_codes`, and
  `confidence` so later hardware-backed reruns can replace these baselines without
  changing the schema.
