# Sprint 11 Bench Notes

## T11.7 - Vulkan and Windows ML benchmark evidence

Local hybrid-planner evidence is captured through
`build/runtime/benchmarks/hybrid_planner_gate.exe` and written to
`runtime/benchmarks/correctness/reports/hybrid_planner_gate.json`.

### Current local scope

- `vulkan_qwen_balanced`
- `vulkan_llama_balanced`
- `windows_ml_qwen_opt_in`
- `windows_ml_llama_opt_in`
- `windows_ml_qwen_thermal_throttle`
- `windows_ml_qwen_no_npu_fallback`

### Current local snapshot

| Case | Generation tokens | Tokens/s | Latency/token (ms) |
|---|---:|---:|---:|
| `vulkan_qwen_balanced` | `32` | `94646.554274` | `0.010566` |
| `vulkan_llama_balanced` | `32` | `216508.795670` | `0.004619` |
| `windows_ml_qwen_opt_in` | `32` | `6666666.666667` | `0.000150` |
| `windows_ml_llama_opt_in` | `32` | `18823529.411765` | `0.000053` |
| `windows_ml_qwen_thermal_throttle` | `32` | `24615384.615385` | `0.000041` |
| `windows_ml_qwen_no_npu_fallback` | `32` | `22857142.857143` | `0.000044` |

### Reading

- The current repository commits local timing and planner fingerprints for both `Qwen` and `Llama`
  on the `Vulkan` path, plus opt-in and fallback coverage for `Windows ML`.
- The JSON evidence now carries benchmark-shaped metrics directly in the committed report instead of
  requiring downstream derivation from raw timing blobs.
- `windows_ml_llama_opt_in` extends the local Sprint 11 evidence so the NPU offload surface is no
  longer `Qwen`-only in the versioned benchmark notes.
- This remains a local synthetic/planner gate, not a substitute for the real hardware acceptance
  criteria in `Radeon RX 7900` and `Snapdragon X Elite`.
- Until those hosts are exercised, `T11.7` should stay open even if the local gate remains green.
