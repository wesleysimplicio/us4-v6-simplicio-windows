# GitHub Issues Map ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â US4 V6 Windows Edition

> Mapeamento gerado em 2026-05-18 a partir de `.specs/sprints/sprint-XX/SPRINT.md`.
> Total: **12 sprint epics + 88 task issues = 100 issues**.
> Fechados: **71** (tasks with versioned checkboxes marked as done).

## Sprint Epics

| Sprint | Epic | Theme | Status | Done |
|---|---|---|---|---:|
| 01 | [#1](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/1) | Foundations & Skeleton | done | 10/10 |
| 02 | [#2](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/2) | CPU Scalar Baseline ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Qwen + Gemma | done | 9/9 |
| 03 | [#3](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/3) | CUDA + DirectML Skeleton | in_progress | 3/7 |
| 04 | [#4](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/4) | AVX Hot Paths + oneDNN | in_progress | 2/7 |
| 05 | [#5](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/5) | BitNet + Ternary Adapters | in_progress | 5/7 |
| 06 | [#6](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/6) | KV Memory Architecture | done | 7/7 |
| 07 | [#7](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/7) | Llama Adapter | in_progress | 4/6 |
| 08 | [#8](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/8) | MoE Foundation - DeepSeek + Kimi | done | 6/6 |
| 09 | [#9](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/9) | MoE Advanced Ã¢â‚¬â€ MiniMax + GLM, SP-MoE | done | 6/6 |
| 10 | [#10](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/10) | Batching + Speculative Decoding | in_progress | 6/7 |
| 11 | [#11](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/11) | Vulkan + Windows ML / NPU | in_progress | 7/8 |
| 12 | [#12](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/12) | Auto-Tune + Release v1.0 | in_progress | 6/8 |

## Task Issues

### Sprint 01 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Foundations & Skeleton (#1)
- [#13](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/13) T01.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CMake/Ninja root + runtime skeleton
- [#14](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/14) T01.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Interface IUS4WindowsAdapter
- [#15](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/15) T01.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â HardwareProbe (CUDA/DXGI/Vulkan/AVX/NPU) **done**
- [#16](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/16) T01.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â BackendSelector
- [#17](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/17) T01.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â RuntimeMode enum + selector
- [#18](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/18) T01.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Telemetry skeleton
- [#19](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/19) T01.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CI/DoD workflows
- [#20](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/20) T01.8 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Claude hooks (post-edit + pre-commit)
- [#21](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/21) T01.9 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Playwright config + first smoke
- [#22](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/22) T01.10 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ADR-001/002/003

### Sprint 02 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CPU Scalar Baseline (#2)
- [#23](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/23) T02.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Tensor primitives
- [#24](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/24) T02.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Scalar matmul
- [#25](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/25) T02.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Scalar attention
- [#26](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/26) T02.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DenseAdapterBase
- [#27](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/27) T02.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â QwenAdapter scalar
- [#28](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/28) T02.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â GemmaAdapter scalar
- [#29](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/29) T02.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Loader GGUF/Safetensors
- [#30](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/30) T02.8 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Benchmark harness dense_baseline
- [#31](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/31) T02.9 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Runtime mode CPU_ONLY

### Sprint 03 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CUDA + DirectML Skeleton (#3)
- [#32](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/32) T03.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CudaContext
- [#33](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/33) T03.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CUDA kernels matmul/softmax/rmsnorm
- [#34](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/34) T03.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â cuBLAS/cuBLASLt fallback wrapper
- [#35](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/35) T03.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DmlDevice
- [#36](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/36) T03.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DmlGraph
- [#37](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/37) T03.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Vendor selector wiring
- [#38](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/38) T03.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Qwen + Gemma em CUDA + DML

### Sprint 04 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â AVX Hot Paths + oneDNN (#4)
- [#39](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/39) T04.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â AVX2/AVX-512 matmul
- [#40](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/40) T04.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â AMX BF16/INT8
- [#41](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/41) T04.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â AVX attention fused
- [#42](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/42) T04.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Dequant INT8/INT4
- [#43](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/43) T04.5 - OneDnnBackend block GEMM **done**
- [#44](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/44) T04.6 - Auto-select AVX via cpuid **done**
- [#45](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/45) T04.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Re-bench Qwen/Gemma AVX vs scalar

### Sprint 05 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â BitNet + Ternary (#5)
- [#46](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/46) T05.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CUDA bitnet_matmul.cu
- [#47](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/47) T05.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â AVX2 bitnet_matmul
- [#48](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/48) T05.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â BitNetAdapter
- [#49](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/49) T05.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â TernaryAdapter
- [#50](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/50) T05.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Ternary LUT
- [#51](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/51) T05.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Loader BitNet/ternary
- [#52](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/52) T05.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â RuntimeMode MICRO/NANO trigger

### Sprint 06 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â KV Memory Architecture (#6)
- [#53](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/53) T06.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â KvPager
- [#54](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/54) T06.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â PrefixCache
- [#55](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/55) T06.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SsdColdStore
- [#56](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/56) T06.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Summarizer
- [#57](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/57) T06.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Eviction policy hybrid
- [#58](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/58) T06.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Adapter hooks (append/lookup/evict/summarize)
- [#59](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/59) T06.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Telemetry hit-rate tiered

### Sprint 07 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Llama Adapter (#7)
- [#60](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/60) T07.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â LlamaConfig
- [#61](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/61) T07.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â LlamaAdapter full attention
- [#62](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/62) T07.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â RoPE (linear + dynamic + YaRN)
- [#63](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/63) T07.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â GQA attention
- [#64](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/64) T07.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Loader Llama (GGUF + safetensors + tokenizer) **done**
- [#65](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/65) T07.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Bench Llama 3.x 8B cross-backend

### Sprint 08 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â MoE Foundation (#8)
- [#66](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/66) T08.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â MoE Router
- [#67](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/67) T08.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ExpertPager
- [#68](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/68) T08.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â DeepSeekMoEAdapter
- [#69](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/69) T08.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â KimiMoEAdapter
- [#70](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/70) T08.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Loader MoE lazy **done**
- [#71](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/71) T08.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Telemetry MoE

### Sprint 09 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â MoE Advanced (#9)
- [#72](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/72) T09.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â MiniMaxAdapter
- [#73](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/73) T09.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â GLMAdapter
- [#77](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/77) T09.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SpeculativePrefetch
- [#74](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/74) T09.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SparsityAwareCache
- [#75](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/75) T09.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â MultimodalCache
- [#76](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/76) T09.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Telemetry prefetch + sparsity

### Sprint 10 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Batching + Speculative (#10)
- [#78](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/78) T10.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â ContinuousBatcher
- [#79](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/79) T10.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â SessionPool
- [#80](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/80) T10.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â PEagleDecoder
- [#81](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/81) T10.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Eagle3Decoder
- [#82](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/82) T10.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Draft model loader
- [#83](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/83) T10.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CUDA Graphs reuse speculative
- [#84](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/84) T10.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Acceptance rate telemetry

### Sprint 11 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Vulkan + Windows ML / NPU (#11)
- [#85](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/85) T11.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â VulkanContext
- [#86](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/86) T11.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Vulkan kernels GLSL
- [#87](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/87) T11.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â WinMlAdapter (NPU EP)
- [#88](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/88) T11.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â LayerOffloader (dense ÃƒÂ¢Ã¢â‚¬Â Ã¢â‚¬â„¢ NPU)
- [#89](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/89) T11.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Mixed dispatch GPU+NPU
- [#90](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/90) T11.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Power/thermal monitor
- [#91](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/91) T11.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Bench Vulkan + NPU
- [#92](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/92) T11.8 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Fallback graceful sem NPU

### Sprint 12 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Auto-Tune + Release v1.0 (#12)
- [#93](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/93) T12.1 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â AutoTuner ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â **done**
- [#94](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/94) T12.2 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â profiles.json ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â **done**
- [#95](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/95) T12.3 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â matrix_runner ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â **done**
- [#96](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/96) T12.4 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â CLI polish + JSON + completions ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â **done**
- [#97](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/97) T12.5 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Docs finais ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â **done**
- [#98](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/98) T12.6 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â MSIX installer assinado (open, bloqueado por cert prod)
- [#99](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/99) T12.7 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Release v1.0 tag + GitHub Releases (open)
- [#100](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/100) T12.8 ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â Migration guide + troubleshooting ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â **done**

## ConvenÃƒÆ’Ã‚Â§ÃƒÆ’Ã‚Âµes

- Cada **epic** agrupa as tasks via GitHub sub-issues (linkadas via API).
- Cada **task** tem AC mensurÃƒÆ’Ã‚Â¡vel + DoD herdado do sprint.
- **Status real do cÃƒÆ’Ã‚Â³digo** vs **checkbox versionado**: o repo jÃƒÆ’Ã‚Â¡ implementa partes de muitas tasks anteriores ao sprint atual; mantÃƒÆ’Ã‚Â©m-se conservador no checkbox atÃƒÆ’Ã‚Â© evidÃƒÆ’Ã‚Âªncia (test + correctness diff + E2E).
- AtualizaÃƒÆ’Ã‚Â§ÃƒÆ’Ã‚Â£o de status: edite o checkbox em `.specs/sprints/sprint-XX/SPRINT.md` e rode `scripts/render-planning-status.ps1` para regenerar `STATUS.md`. Issues no GitHub fecham via PR (Conventional Commits) ou manualmente quando DoD verde.

