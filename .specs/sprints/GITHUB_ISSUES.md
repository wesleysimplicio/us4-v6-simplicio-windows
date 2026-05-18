# GitHub Issues Map â€” US4 V6 Windows Edition

> Mapeamento gerado em 2026-05-18 a partir de `.specs/sprints/sprint-XX/SPRINT.md`.
> Total: **12 sprint epics + 88 task issues = 100 issues**.
> Fechados: **63** (tasks com checkbox versionado marcado como concluÃ­do).

## Sprint Epics

| Sprint | Epic | Theme | Status | Done |
|---|---|---|---|---:|
| 01 | [#1](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/1) | Foundations & Skeleton | in_progress | 9/10 |
| 02 | [#2](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/2) | CPU Scalar Baseline â€” Qwen + Gemma | done | 9/9 |
| 03 | [#3](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/3) | CUDA + DirectML Skeleton | in_progress | 3/7 |
| 04 | [#4](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/4) | AVX Hot Paths + oneDNN | todo | 0/7 |
| 05 | [#5](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/5) | BitNet + Ternary Adapters | in_progress | 5/7 |
| 06 | [#6](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/6) | KV Memory Architecture | in_progress | 5/7 |
| 07 | [#7](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/7) | Llama Adapter | in_progress | 3/6 |
| 08 | [#8](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/8) | MoE Foundation â€” DeepSeek + Kimi | in_progress | 4/6 |
| 09 | [#9](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/9) | MoE Advanced — MiniMax + GLM, SP-MoE | done | 6/6 |
| 10 | [#10](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/10) | Batching + Speculative Decoding | in_progress | 6/7 |
| 11 | [#11](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/11) | Vulkan + Windows ML / NPU | in_progress | 7/8 |
| 12 | [#12](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/12) | Auto-Tune + Release v1.0 | in_progress | 6/8 |

## Task Issues

### Sprint 01 â€” Foundations & Skeleton (#1)
- [#13](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/13) T01.1 â€” CMake/Ninja root + runtime skeleton
- [#14](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/14) T01.2 â€” Interface IUS4WindowsAdapter
- [#15](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/15) T01.3 â€” HardwareProbe (CUDA/DXGI/Vulkan/AVX/NPU)
- [#16](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/16) T01.4 â€” BackendSelector
- [#17](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/17) T01.5 â€” RuntimeMode enum + selector
- [#18](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/18) T01.6 â€” Telemetry skeleton
- [#19](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/19) T01.7 â€” CI/DoD workflows
- [#20](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/20) T01.8 â€” Claude hooks (post-edit + pre-commit)
- [#21](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/21) T01.9 â€” Playwright config + first smoke
- [#22](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/22) T01.10 â€” ADR-001/002/003

### Sprint 02 â€” CPU Scalar Baseline (#2)
- [#23](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/23) T02.1 â€” Tensor primitives
- [#24](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/24) T02.2 â€” Scalar matmul
- [#25](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/25) T02.3 â€” Scalar attention
- [#26](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/26) T02.4 â€” DenseAdapterBase
- [#27](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/27) T02.5 â€” QwenAdapter scalar
- [#28](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/28) T02.6 â€” GemmaAdapter scalar
- [#29](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/29) T02.7 â€” Loader GGUF/Safetensors
- [#30](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/30) T02.8 â€” Benchmark harness dense_baseline
- [#31](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/31) T02.9 â€” Runtime mode CPU_ONLY

### Sprint 03 â€” CUDA + DirectML Skeleton (#3)
- [#32](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/32) T03.1 â€” CudaContext
- [#33](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/33) T03.2 â€” CUDA kernels matmul/softmax/rmsnorm
- [#34](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/34) T03.3 â€” cuBLAS/cuBLASLt fallback wrapper
- [#35](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/35) T03.4 â€” DmlDevice
- [#36](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/36) T03.5 â€” DmlGraph
- [#37](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/37) T03.6 â€” Vendor selector wiring
- [#38](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/38) T03.7 â€” Qwen + Gemma em CUDA + DML

### Sprint 04 â€” AVX Hot Paths + oneDNN (#4)
- [#39](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/39) T04.1 â€” AVX2/AVX-512 matmul
- [#40](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/40) T04.2 â€” AMX BF16/INT8
- [#41](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/41) T04.3 â€” AVX attention fused
- [#42](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/42) T04.4 â€” Dequant INT8/INT4
- [#43](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/43) T04.5 â€” OneDnnBackend block GEMM
- [#44](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/44) T04.6 â€” Auto-select AVX via cpuid
- [#45](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/45) T04.7 â€” Re-bench Qwen/Gemma AVX vs scalar

### Sprint 05 â€” BitNet + Ternary (#5)
- [#46](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/46) T05.1 â€” CUDA bitnet_matmul.cu
- [#47](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/47) T05.2 â€” AVX2 bitnet_matmul
- [#48](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/48) T05.3 â€” BitNetAdapter
- [#49](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/49) T05.4 â€” TernaryAdapter
- [#50](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/50) T05.5 â€” Ternary LUT
- [#51](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/51) T05.6 â€” Loader BitNet/ternary
- [#52](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/52) T05.7 â€” RuntimeMode MICRO/NANO trigger

### Sprint 06 â€” KV Memory Architecture (#6)
- [#53](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/53) T06.1 â€” KvPager
- [#54](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/54) T06.2 â€” PrefixCache
- [#55](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/55) T06.3 â€” SsdColdStore
- [#56](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/56) T06.4 â€” Summarizer
- [#57](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/57) T06.5 â€” Eviction policy hybrid
- [#58](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/58) T06.6 â€” Adapter hooks (append/lookup/evict/summarize)
- [#59](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/59) T06.7 â€” Telemetry hit-rate tiered

### Sprint 07 â€” Llama Adapter (#7)
- [#60](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/60) T07.1 â€” LlamaConfig
- [#61](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/61) T07.2 â€” LlamaAdapter full attention
- [#62](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/62) T07.3 â€” RoPE (linear + dynamic + YaRN)
- [#63](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/63) T07.4 â€” GQA attention
- [#64](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/64) T07.5 â€” Loader Llama (GGUF + safetensors + tokenizer)
- [#65](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/65) T07.6 â€” Bench Llama 3.x 8B cross-backend

### Sprint 08 â€” MoE Foundation (#8)
- [#66](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/66) T08.1 â€” MoE Router
- [#67](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/67) T08.2 â€” ExpertPager
- [#68](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/68) T08.3 â€” DeepSeekMoEAdapter
- [#69](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/69) T08.4 â€” KimiMoEAdapter
- [#70](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/70) T08.5 â€” Loader MoE lazy
- [#71](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/71) T08.6 â€” Telemetry MoE

### Sprint 09 â€” MoE Advanced (#9)
- [#72](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/72) T09.1 â€” MiniMaxAdapter
- [#73](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/73) T09.2 â€” GLMAdapter
- [#77](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/77) T09.3 â€” SpeculativePrefetch
- [#74](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/74) T09.4 â€” SparsityAwareCache
- [#75](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/75) T09.5 â€” MultimodalCache
- [#76](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/76) T09.6 â€” Telemetry prefetch + sparsity

### Sprint 10 â€” Batching + Speculative (#10)
- [#78](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/78) T10.1 â€” ContinuousBatcher
- [#79](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/79) T10.2 â€” SessionPool
- [#80](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/80) T10.3 â€” PEagleDecoder
- [#81](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/81) T10.4 â€” Eagle3Decoder
- [#82](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/82) T10.5 â€” Draft model loader
- [#83](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/83) T10.6 â€” CUDA Graphs reuse speculative
- [#84](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/84) T10.7 â€” Acceptance rate telemetry

### Sprint 11 â€” Vulkan + Windows ML / NPU (#11)
- [#85](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/85) T11.1 â€” VulkanContext
- [#86](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/86) T11.2 â€” Vulkan kernels GLSL
- [#87](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/87) T11.3 â€” WinMlAdapter (NPU EP)
- [#88](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/88) T11.4 â€” LayerOffloader (dense â†’ NPU)
- [#89](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/89) T11.5 â€” Mixed dispatch GPU+NPU
- [#90](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/90) T11.6 â€” Power/thermal monitor
- [#91](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/91) T11.7 â€” Bench Vulkan + NPU
- [#92](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/92) T11.8 â€” Fallback graceful sem NPU

### Sprint 12 â€” Auto-Tune + Release v1.0 (#12)
- [#93](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/93) T12.1 â€” AutoTuner â€” **done**
- [#94](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/94) T12.2 â€” profiles.json â€” **done**
- [#95](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/95) T12.3 â€” matrix_runner â€” **done**
- [#96](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/96) T12.4 â€” CLI polish + JSON + completions â€” **done**
- [#97](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/97) T12.5 â€” Docs finais â€” **done**
- [#98](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/98) T12.6 â€” MSIX installer assinado (open, bloqueado por cert prod)
- [#99](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/99) T12.7 â€” Release v1.0 tag + GitHub Releases (open)
- [#100](https://github.com/wesleysimplicio/us4-v6-simplicio-windows/issues/100) T12.8 â€” Migration guide + troubleshooting â€” **done**

## ConvenÃ§Ãµes

- Cada **epic** agrupa as tasks via GitHub sub-issues (linkadas via API).
- Cada **task** tem AC mensurÃ¡vel + DoD herdado do sprint.
- **Status real do cÃ³digo** vs **checkbox versionado**: o repo jÃ¡ implementa partes de muitas tasks anteriores ao sprint atual; mantÃ©m-se conservador no checkbox atÃ© evidÃªncia (test + correctness diff + E2E).
- AtualizaÃ§Ã£o de status: edite o checkbox em `.specs/sprints/sprint-XX/SPRINT.md` e rode `scripts/render-planning-status.ps1` para regenerar `STATUS.md`. Issues no GitHub fecham via PR (Conventional Commits) ou manualmente quando DoD verde.
