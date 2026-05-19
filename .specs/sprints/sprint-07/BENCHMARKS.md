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
- Synthetic and planner-only flows remain intact because `LlamaScalarAdapter` now
  falls back to `DenseAdapterBase::LoadModel` whenever a full `Llama` asset load is
  not available.
