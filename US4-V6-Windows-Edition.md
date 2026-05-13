# US4 V6 Windows Edition — Universal State Runtime

**Development Prompt — US4-V6-Windows-Edition**  
**Target:** Windows 11/12 + NVIDIA/AMD/Intel GPUs + CPU AVX/AMX + NPU providers

---

## 1. Project Name & Positioning

```text
US4 V6 Windows Edition
```

US4 V6 Windows Edition is the Windows-native evolution of the US4 V6 Apple Silicon thesis.

Apple edition:

```text
MLX + Metal + NEON + ANE
```

Windows edition:

```text
CUDA + DirectML + Vulkan + oneDNN + AVX2/AVX-512/AMX + Windows ML/NPU providers
```

US4 V6 Windows is **not** a generic slow runner.

It is:

```text
Universal core
+ specialized adapters
+ backend-specific acceleration
+ memory virtualization
+ SSD cold cache
+ hardware-aware auto-tuning
```

Official tagline:

> **US4 V6 Windows: Universal core, specialized adapters, CUDA/DirectML/Vulkan acceleration, local LLMs optimized for real Windows machines.**

---

## 2. Core Objective

Create a universal Windows-native LLM runtime capable of running multiple model families through specialized adapters while optimizing:

- active RAM usage
- VRAM usage
- RAM ↔ VRAM transfer cost
- startup time
- time-to-first-token
- repeated prefill
- prefix cache reuse
- KV cache reuse
- SSD cold storage
- hot/cold KV memory
- model-specific expert paging
- speculative expert prefetch
- continuous batching
- CUDA execution
- DirectML execution
- Vulkan execution
- CPU AVX/AMX fallback
- NPU provider fallback where available
- auto-tuning per machine profile
- correctness validation
- benchmark-driven optimization

---

## 3. Non-Negotiable Rules

Do not make fake benchmark claims.

All performance numbers are **targets to validate**, not guaranteed facts.

Do not claim final superiority without:

```text
real benchmark
hardware profile
model profile
backend profile
correctness validation
before/after metrics
```

Do not implement agent behavior.

Do not introduce RAG as a core runtime feature.

Do not turn US4 into a generic wrapper only.

The runtime must remain:

```text
universal core + specialized adapters + optimized backends
```

---

## 4. Windows Backend Strategy

US4 V6 Windows must support multiple backends.

Backend priority is selected by hardware.

```text
NVIDIA RTX / Data Center GPU:
  1. CUDA
  2. Vulkan fallback
  3. DirectML fallback
  4. CPU AVX fallback

AMD Radeon:
  1. DirectML
  2. Vulkan
  3. CPU AVX fallback

Intel Arc:
  1. DirectML
  2. oneDNN / OpenVINO-style path where applicable
  3. Vulkan
  4. CPU AVX fallback

CPU-only:
  1. oneDNN
  2. AVX-512 / AMX when available
  3. AVX2
  4. scalar safe fallback

NPU-capable Windows machines:
  1. Windows ML / NPU execution provider for supported small ops
  2. DirectML fallback
  3. CPU fallback
```

---

## 5. Target Hardware Profiles

### 5.1 Windows Desktop / Workstation

| Hardware Class | RAM | VRAM | US4 Mode | Target Use |
|---|---:|---:|---|---|
| RTX 4090 desktop | 64GB–128GB | 24GB | Balanced / Full | strong local inference |
| RTX 3090 desktop | 64GB–128GB | 24GB | Balanced | strong local inference |
| RTX 4080 / 5080 class | 64GB+ | 16GB | Balanced / Degraded | large dense models |
| RTX 4070 / 5070 class | 32GB–64GB | 12GB | Degraded | 7B–14B models |
| RTX 4060 / 5060 class | 32GB–64GB | 8GB | Ultra-Low | 3B–9B models |
| AMD Radeon 16GB | 32GB–64GB | 16GB | Degraded | DirectML/Vulkan path |
| Intel Arc 16GB | 32GB–64GB | 16GB | Degraded | DirectML/oneDNN path |
| CPU-only workstation | 64GB–256GB | 0GB | CPU Mode | testing, BitNet, small dense |

### 5.2 Windows Laptops

| Machine Class | RAM | VRAM | US4 Mode | Target Use |
|---|---:|---:|---|---|
| Gaming laptop RTX 4090 | 64GB | 16GB | Balanced | strong local inference |
| Gaming laptop RTX 4080 | 32GB–64GB | 12GB | Degraded | 7B–14B |
| Gaming laptop RTX 4070 | 32GB | 8GB | Ultra-Low | 3B–9B |
| Copilot+ PC / NPU laptop | 16GB–32GB | shared/NPU | Micro / Nano | small dense, BitNet |
| Intel/AMD iGPU laptop | 16GB–32GB | shared | Micro | small dense only |

---

## 6. Runtime Modes — Windows

```cpp
enum class US4WindowsRuntimeMode {
    FULL,
    BALANCED,
    DEGRADED,
    ULTRA_LOW,
    MICRO,
    NANO,
    CPU_ONLY
};
```

Mode mapping:

```text
FULL:
  RAM 128GB+
  VRAM 24GB+
  CUDA preferred

BALANCED:
  RAM 64GB–128GB
  VRAM 16GB–24GB

DEGRADED:
  RAM 32GB–64GB
  VRAM 12GB–16GB

ULTRA_LOW:
  RAM 32GB–64GB
  VRAM 8GB–12GB

MICRO:
  RAM 16GB–32GB
  VRAM 4GB–8GB or shared

NANO:
  RAM 8GB–16GB
  small dense / BitNet only

CPU_ONLY:
  no usable GPU
  AVX/AMX/oneDNN execution
```

---

## 7. Supported Model Families & Adapters

| Adapter | Target Models | Main Optimization |
|---|---|---|
| `us4_adapter_deepseek_moe` | DeepSeek V4 Flash, DeepSeek V3.x MoE | MoE expert paging, KV paging |
| `us4_adapter_kimi_moe` | Kimi K2.6 / K2.5 | MoE + MLA + multimodal cache |
| `us4_adapter_minimax` | MiniMax M2.7 | long sessions, prefix reuse |
| `us4_adapter_glm` | GLM-5.1 / GLM-5 | long-horizon engineering cache |
| `us4_adapter_qwen_dense` | Qwen2.5 / Qwen3 / Qwen-Coder | dense/GQA/RoPE |
| `us4_adapter_llama` | Llama 3.x / Llama 4 | GQA/RoPE, CUDA/Vulkan kernels |
| `us4_adapter_gemma` | Gemma 3 / 4 | small/medium low-memory |
| `us4_adapter_bitnet` | BitNet b1.58 | ternary/1.58-bit kernels |
| `us4_adapter_ternary` | PT-BitNet converted models | post-training ternary |

---

## 8. Compatibility Matrix — Windows

| RAM / VRAM | DeepSeek / Kimi / MiniMax / GLM | Qwen / Llama 14B–32B | Qwen / Gemma 3B–9B | BitNet / Ternary |
|---|---|---|---|---|
| **128GB RAM + 24GB VRAM** | Limited viable / experimental | Excellent | Excellent | Excellent |
| **64GB RAM + 24GB VRAM** | Experimental | Strong | Excellent | Excellent |
| **64GB RAM + 16GB VRAM** | Lab only | Good / limited | Excellent | Excellent |
| **32GB RAM + 12GB VRAM** | No | Limited | Good / strong | Excellent |
| **32GB RAM + 8GB VRAM** | No | Not recommended | Good | Excellent |
| **16GB RAM + 8GB VRAM** | No | No | 1B–4B only | Good |
| **16GB RAM + iGPU/NPU** | No | No | 1B–3B only | Good / limited |
| **8GB RAM** | No | No | tiny only | limited |

---

## 9. Universal Runtime Architecture — Windows

```text
US4 V6 Windows
├── core/
│   ├── Universal Runtime
│   ├── Model Registry
│   ├── Runtime Scheduler
│   ├── Adapter Dispatcher
│   └── Backend Selector
│
├── adapters/
│   ├── DeepSeek MoE Adapter
│   ├── Kimi MoE Adapter
│   ├── MiniMax Adapter
│   ├── GLM Adapter
│   ├── Qwen Dense Adapter
│   ├── Llama Adapter
│   ├── Gemma Adapter
│   ├── BitNet Adapter
│   └── Generic Ternary Adapter
│
├── memory/
│   ├── Windows Memory Governor
│   ├── RAM/VRAM Planner
│   ├── Hot-Cold KV Memory Planner
│   ├── Page-Locked Buffer Manager
│   └── Low-RAM Degradation Engine
│
├── kv/
│   ├── Universal KV Pager
│   ├── Adapter-specific KV Layout
│   ├── Continuous Batching KV Manager
│   ├── CPU↔GPU KV Transfer Planner
│   └── KV Compression Engine
│
├── cache/
│   ├── Prefix Cache
│   ├── SSD Cold Cache
│   ├── Session Checkpoint Cache
│   ├── Content-aware Cache
│   └── Multimodal Cache
│
├── moe/
│   ├── Expert Pager
│   ├── Speculative Expert Prefetch
│   ├── Sparsity-aware Expert Cache
│   └── Expert Routing Telemetry
│
├── backends/
│   ├── cuda/
│   ├── directml/
│   ├── vulkan/
│   ├── onednn/
│   ├── cpu_avx/
│   └── windows_ml/
│
├── tuning/
│   ├── Auto-Tuner
│   ├── Hardware Probe
│   ├── Backend Benchmark
│   └── Profile Store
│
└── telemetry/
    ├── Metrics
    ├── Correctness Validation
    ├── Logit Drift Monitor
    └── Benchmark Reports
```

---

## 10. Required Project Structure

```text
us4-v6-windows/
  CMakeLists.txt
  README.md

  runtime/
    core/
      us4_runtime_windows_v6.cpp
      us4_runtime_windows_v6.h
      us4_scheduler_windows.cpp
      us4_scheduler_windows.h
      us4_model_registry.cpp
      us4_model_registry.h
      us4_backend_selector.cpp
      us4_backend_selector.h

    adapters/
      deepseek/
        us4_adapter_deepseek_moe.cpp
        us4_adapter_deepseek_moe.h
      kimi/
        us4_adapter_kimi_moe.cpp
        us4_adapter_kimi_moe.h
      minimax/
        us4_adapter_minimax.cpp
        us4_adapter_minimax.h
      glm/
        us4_adapter_glm.cpp
        us4_adapter_glm.h
      qwen/
        us4_adapter_qwen_dense.cpp
        us4_adapter_qwen_dense.h
      llama/
        us4_adapter_llama.cpp
        us4_adapter_llama.h
      gemma/
        us4_adapter_gemma.cpp
        us4_adapter_gemma.h
      bitnet/
        us4_adapter_bitnet.cpp
        us4_adapter_bitnet.h
      ternary/
        us4_adapter_ternary.cpp
        us4_adapter_ternary.h

    memory/
      us4_windows_memory_governor.cpp
      us4_windows_memory_governor.h
      us4_vram_governor.cpp
      us4_vram_governor.h
      us4_page_locked_buffers.cpp
      us4_page_locked_buffers.h
      us4_low_ram_degradation.cpp
      us4_low_ram_degradation.h

    kv/
      us4_kv_pager_windows.cpp
      us4_kv_pager_windows.h
      us4_kv_transfer_planner.cpp
      us4_kv_transfer_planner.h
      us4_kv_compression.cpp
      us4_kv_compression.h
      us4_kv_continuous_batching.cpp
      us4_kv_continuous_batching.h

    cache/
      us4_prefix_cache.cpp
      us4_prefix_cache.h
      us4_ssd_cold_cache.cpp
      us4_ssd_cold_cache.h
      us4_session_checkpoint.cpp
      us4_session_checkpoint.h
      us4_multimodal_cache.cpp
      us4_multimodal_cache.h

    moe/
      us4_expert_pager.cpp
      us4_expert_pager.h
      us4_expert_prefetch.cpp
      us4_expert_prefetch.h
      us4_sparsity_expert_cache.cpp
      us4_sparsity_expert_cache.h
      us4_expert_routing_telemetry.cpp
      us4_expert_routing_telemetry.h

    backends/
      cuda/
        us4_cuda_backend.cu
        us4_cuda_backend.h
        us4_cuda_kernels.cu
        us4_cuda_memory.cu
        us4_cuda_graphs.cu

      directml/
        us4_directml_backend.cpp
        us4_directml_backend.h
        us4_directml_graph.cpp
        us4_directml_graph.h

      vulkan/
        us4_vulkan_backend.cpp
        us4_vulkan_backend.h
        us4_vulkan_kernels.cpp
        us4_vulkan_kernels.h

      onednn/
        us4_onednn_backend.cpp
        us4_onednn_backend.h

      cpu_avx/
        us4_avx_dequant.cpp
        us4_avx_dequant.h
        us4_avx_kv.cpp
        us4_avx_kv.h
        us4_amx_kernels.cpp
        us4_amx_kernels.h

      windows_ml/
        us4_windows_ml_backend.cpp
        us4_windows_ml_backend.h
        us4_npu_provider.cpp
        us4_npu_provider.h

    speculative/
      us4_peagle.cpp
      us4_peagle.h
      us4_eagle3.cpp
      us4_eagle3.h
      us4_draft_tree.cpp
      us4_draft_tree.h

    tuning/
      us4_autotune_windows.cpp
      us4_autotune_windows.h
      us4_hardware_probe_windows.cpp
      us4_hardware_probe_windows.h
      us4_backend_benchmark.cpp
      us4_backend_benchmark.h
      us4_profile_store.cpp
      us4_profile_store.h

    telemetry/
      us4_metrics.cpp
      us4_metrics.h
      us4_trace.cpp
      us4_trace.h
      us4_correctness.cpp
      us4_correctness.h
      us4_logit_drift.cpp
      us4_logit_drift.h

    benchmarks/
      us4_bench_windows_matrix.cpp
      us4_bench_cuda.cpp
      us4_bench_directml.cpp
      us4_bench_vulkan.cpp
      us4_bench_avx.cpp
      us4_bench_kv.cpp
      us4_bench_moe.cpp
      us4_bench_end_to_end.cpp

  profiles/
    hardware/
      windows_rtx4090_24gb.json
      windows_rtx3090_24gb.json
      windows_rtx4080_16gb.json
      windows_rtx4070_12gb.json
      windows_rtx4060_8gb.json
      windows_amd_16gb.json
      windows_intel_arc_16gb.json
      windows_cpu_only_64gb.json
      windows_npu_32gb.json

    models/
      deepseek_v4_flash.json
      kimi_k2_6.json
      minimax_m2_7.json
      glm_5_1.json
      qwen_dense.json
      llama.json
      gemma.json
      bitnet.json
      ternary_generic.json

  docs/
    us4-v6-windows-architecture.md
    us4-v6-windows-backends.md
    us4-v6-cuda-backend.md
    us4-v6-directml-backend.md
    us4-v6-vulkan-backend.md
    us4-v6-cpu-avx-amx.md
    us4-v6-windows-build.md
    us4-v6-windows-benchmark-methodology.md
```

---

## 11. Adapter Interface

Every model adapter must implement:

```cpp
class IUS4WindowsAdapter {
public:
    virtual ~IUS4WindowsAdapter() = default;

    virtual const char* family() const = 0;
    virtual const char* model_name() const = 0;
    virtual US4ArchitectureType architecture_type() const = 0;

    virtual bool supports_moe() const = 0;
    virtual bool supports_mla() const = 0;
    virtual bool supports_gqa() const = 0;
    virtual bool supports_multimodal() const = 0;
    virtual bool supports_ternary() const = 0;
    virtual bool supports_cuda() const = 0;
    virtual bool supports_directml() const = 0;
    virtual bool supports_vulkan() const = 0;
    virtual bool supports_cpu_avx() const = 0;

    virtual US4KVLayout kv_layout() const = 0;

    virtual US4QuantStrategy recommended_quant_strategy(
        const US4WindowsHardwareProfile& hardware
    ) const = 0;

    virtual US4MemoryPlan build_memory_plan(
        const US4WindowsHardwareProfile& hardware,
        US4WindowsRuntimeMode mode
    ) const = 0;

    virtual bool configure_backend(US4BackendContext& backend) = 0;
    virtual bool configure_runtime(US4RuntimeContext& context) = 0;
};
```

---

## 12. Backend Selector

The backend selector must inspect:

```text
GPU vendor
GPU model
VRAM
system RAM
CPU instruction support
DirectX 12 support
Vulkan support
CUDA support
NPU provider support
driver version
available memory
```

Expected output:

```text
US4 V6 Windows Hardware Probe
OS: Windows
CPU: AMD / Intel
RAM: 64GB
GPU: NVIDIA RTX 4090
VRAM: 24GB
CUDA: Available
DirectML: Available
Vulkan: Available
AVX2: Available
AVX-512: Not Available
AMX: Not Available
NPU Provider: Not Available

Recommended Backend: CUDA
Recommended Mode: Balanced
Recommended KV Strategy: VRAM hot / RAM warm / SSD cold
Recommended Expert Strategy: VRAM hot experts / RAM warm experts / SSD cold experts
```

---

## 13. Memory Architecture — RAM / VRAM / SSD

Windows machines often have separated RAM and VRAM.

US4 must manage:

```text
VRAM HOT:
  active tensors
  active KV pages
  hot experts
  CUDA/DirectML/Vulkan buffers

RAM WARM:
  compressed KV
  warm experts
  prefix cache metadata
  staging buffers

SSD COLD:
  cold KV pages
  cold experts
  session checkpoints
  prefix snapshots
```

Rules:

```text
Keep decode hot path in VRAM.
Avoid per-token SSD access.
Use SSD only for cold state.
Use RAM as warm cache.
Use pinned/page-locked buffers for GPU transfer where backend supports it.
Avoid uncontrolled Windows paging.
```

---

## 14. CUDA Backend Requirements

CUDA backend is the priority backend for NVIDIA.

Required features:

```text
CUDA memory pool
CUDA streams
CUDA Graphs for repeated decode patterns
fused dequant/matmul kernels
KV cache kernels
expert streaming kernels
pinned host memory
asynchronous H2D/D2H transfers
```

Required interface:

```cpp
class US4CudaBackend {
public:
    bool initialize(const US4CudaConfig& config);
    bool allocate_pools(const US4CudaMemoryPlan& plan);
    bool upload_hot_expert(uint32_t expert_id, const void* data, size_t bytes);
    bool run_decode_step(const US4DecodeStep& step);
    bool run_prefill(const US4PrefillRequest& request);
    bool synchronize();
    US4CudaStats stats() const;
};
```

Acceptance criteria:

```text
CUDA path must be optional.
CUDA path must expose memory usage.
CUDA path must benchmark against DirectML and CPU fallback.
CUDA path must be correctness-tested.
```

---

## 15. DirectML Backend Requirements

DirectML is the broad compatibility backend.

Required features:

```text
DirectX 12 device creation
DirectML operator graph creation
operator fusion where available
GPU buffer reuse
descriptor heap reuse
command queue scheduling
fallback for AMD / Intel / NVIDIA
```

Required interface:

```cpp
class US4DirectMLBackend {
public:
    bool initialize(const US4DirectMLConfig& config);
    bool compile_graph(const US4GraphDescription& graph);
    bool execute_graph(const US4ExecutionRequest& request);
    bool allocate_buffers(const US4MemoryPlan& plan);
    US4DirectMLStats stats() const;
};
```

Acceptance criteria:

```text
Must work without CUDA.
Must support AMD/Intel/NVIDIA where DirectML is available.
Must expose operator timing.
Must fallback safely when unsupported ops appear.
```

---

## 16. Vulkan Backend Requirements

Vulkan is the cross-vendor low-level backend.

Required features:

```text
Vulkan device selection
compute pipeline creation
shader module management
descriptor set reuse
buffer pool
cross-vendor compute kernels
fallback path for unsupported DirectML/CUDA configurations
```

Acceptance criteria:

```text
Vulkan path must be optional.
Vulkan path must benchmark against DirectML.
Vulkan path must expose GPU memory usage where possible.
```

---

## 17. CPU AVX / AMX Backend Requirements

CPU backend is required for:

```text
fallback
small models
BitNet
ternary models
debugging
CPU-only systems
```

Instruction paths:

```text
AVX2
AVX-512
AMX where available
scalar fallback
```

Hot paths:

```text
Q2/Q4/Q8 dequant
ternary unpack
KV compression
KV decompression
small matmul fallback
routing predictor
prefix hashing
```

Acceptance criteria:

```text
Every vectorized path must have a scalar reference.
Every vectorized path must pass correctness tests.
CPU backend must be benchmarked separately.
```

---

## 18. Windows ML / NPU Backend

NPU support is optional and experimental.

Candidate workloads:

```text
small routing predictor
expert prefetch predictor
draft token predictor
embedding helper
softmax helper
small projection blocks
```

Rules:

```text
Do not force NPU path.
Use only when it improves latency or power.
Fallback automatically.
Measure all offload overhead.
```

---

## 19. KV Paging — Windows

KV cache must be tiered:

```text
L0: VRAM hot KV
L1: RAM compressed KV
L2: SSD cold KV
L3: summary/session checkpoint
```

Required interface:

```cpp
class US4WindowsKVPager {
public:
    bool get_page(uint64_t page_id, US4KVAccessMode mode, void** data);
    bool prefetch_pages(const uint64_t* page_ids, size_t count);
    bool promote_to_vram(uint64_t page_id);
    bool demote_to_ram(uint64_t page_id);
    bool flush_to_ssd(uint64_t page_id);
    bool compress_page(uint64_t page_id, US4KVPrecision precision);
    US4KVPagerStats stats() const;
};
```

Acceptance criteria:

```text
Active decode must not block on SSD.
KV page fault latency must be measured.
Repeated prefill must reuse KV.
VRAM pressure must trigger demotion.
RAM pressure must trigger SSD flush.
```

---

## 20. Expert Paging — Windows

For MoE models:

```text
HOT experts:
  VRAM resident

WARM experts:
  RAM compressed / partially dequantized

COLD experts:
  SSD-backed / mmap-backed
```

Required interface:

```cpp
class US4WindowsExpertPager {
public:
    bool request_expert(uint32_t expert_id, US4ExpertPriority priority);
    bool prefetch_expert(uint32_t expert_id, float confidence);
    bool promote_to_vram(uint32_t expert_id);
    bool demote_to_ram(uint32_t expert_id);
    bool flush_to_ssd(uint32_t expert_id);
    US4ExpertPagerStats stats() const;
};
```

Acceptance criteria:

```text
Track expert cache hit rate.
Track false prefetch rate.
Track expert load latency.
Avoid per-token SSD access.
Do not allow low-priority prefetch to block critical expert loads.
```

---

## 21. Continuous Batching

US4 V6 Windows must support multi-session batching.

Required features:

```text
batch compatible decode steps
isolate KV per session
share prefix cache safely
avoid prompt leakage
priority scheduling
backpressure
```

Acceptance criteria:

```text
No cross-session KV corruption.
Batching must be disableable.
Batching must show throughput gain in multi-session benchmark.
```

---

## 22. Speculative Decoding

Implement optional speculative decoding:

```text
P-EAGLE / EAGLE-style draft tree
small draft model or adapter-specific draft path
verification path
accept/reject metrics
safe fallback
```

Metrics:

```text
accepted draft ratio
rejected draft ratio
tokens/s improvement
latency overhead
correctness drift
```

---

## 23. SSD Cold Cache

SSD cold cache stores:

```text
cold KV pages
cold experts
prefix snapshots
session checkpoints
multimodal cache objects
```

Implementation rules:

```text
Use binary cache format.
Use checksums.
Use versioned cache metadata.
Support cleanup policy.
Support max cache size.
Avoid unbounded disk growth.
```

CLI:

```bash
--ssd-cache-dir C:\us4-cache
--ssd-cache-max-gb 256
--ssd-cache-cleanup
```

---

## 24. CLI Design

### Auto backend

```powershell
.\us4.exe `
  --model models\qwen `
  --adapter auto `
  --backend auto `
  --mode auto `
  --enable-kv-paging `
  --enable-ssd-cache `
  --enable-continuous-batching `
  --metrics
```

### CUDA mode

```powershell
.\us4.exe `
  --model models\deepseek-v4-flash `
  --adapter deepseek-moe `
  --backend cuda `
  --mode balanced `
  --enable-cuda-graphs `
  --enable-expert-paging `
  --enable-kv-paging `
  --enable-ssd-cache `
  --metrics
```

### DirectML mode

```powershell
.\us4.exe `
  --model models\gemma `
  --adapter gemma `
  --backend directml `
  --mode ultra-low `
  --enable-kv-paging `
  --enable-ssd-cache `
  --metrics
```

### CPU-only BitNet mode

```powershell
.\us4.exe `
  --model models\bitnet-b1.58-2b `
  --adapter bitnet `
  --backend cpu-avx `
  --mode micro `
  --enable-ternary `
  --metrics
```

---

## 25. Build Instructions — Windows

### 25.1 Prerequisites

Install:

```text
Visual Studio 2022 or newer
CMake 3.28+
Ninja
Windows SDK
Git
Python 3.11+ for scripts
```

Optional backend prerequisites:

```text
CUDA Toolkit for NVIDIA backend
DirectX 12 / DirectML SDK support
Vulkan SDK
oneDNN
Windows ML runtime / provider support
```

### 25.2 Configure

```powershell
cmake -S . -B build `
  -G Ninja `
  -DUS4_BUILD_WINDOWS=ON `
  -DUS4_ENABLE_CUDA=ON `
  -DUS4_ENABLE_DIRECTML=ON `
  -DUS4_ENABLE_VULKAN=ON `
  -DUS4_ENABLE_ONEDNN=ON `
  -DUS4_ENABLE_CPU_AVX=ON
```

### 25.3 Build

```powershell
cmake --build build --config Release
```

### 25.4 Run hardware probe

```powershell
.\build\us4.exe --hardware-probe
```

### 25.5 Run autotune

```powershell
.\build\us4.exe `
  --autotune `
  --backend auto `
  --mode auto `
  --save-profile profiles\hardware\local.auto.json
```

### 25.6 Run benchmark

```powershell
.\build\us4.exe `
  --benchmark `
  --model models\qwen `
  --adapter auto `
  --backend auto `
  --mode auto `
  --metrics `
  --trace-file traces\qwen-run.jsonl
```

---

## 26. Expected Startup Output

```text
US4 V6 Windows Edition Enabled
Build: us4-v6-windows
OS: Windows
CPU: <detected>
RAM: <detected>
GPU: <detected>
VRAM: <detected>
Backend: CUDA / DirectML / Vulkan / oneDNN / CPU-AVX
Runtime Mode: Full / Balanced / Degraded / Ultra-Low / Micro / Nano / CPU-Only
Model: <model>
Adapter: <adapter>
Architecture: <Dense / MoE / MLA / GQA / Ternary>
KV Paging: Enabled
Expert Paging: Enabled / Not Applicable
SSD Cold Cache: Enabled
Continuous Batching: Enabled
Speculative Decoding: Enabled / Disabled
AutoTune Profile: Loaded / Not Found
Metrics: Enabled
```

---

## 27. Telemetry

Collect:

```text
tokens_per_second
time_to_first_token_ms
startup_time_ms
model_load_time_ms
backend_kernel_time_ms
gpu_memory_used_mb
gpu_memory_free_mb
system_ram_used_mb
system_ram_free_mb
ssd_read_mb
ssd_write_mb
kv_page_faults
kv_page_fault_latency_ms
expert_page_faults
expert_page_fault_latency_ms
prefix_cache_hit_rate
expert_cache_hit_rate
expert_prefetch_hit_rate
false_prefetch_rate
continuous_batching_throughput
speculative_accept_rate
speculative_reject_rate
logit_drift
```

Output formats:

```text
console summary
JSON
JSONL trace
CSV benchmark report
```

---

## 28. Benchmarks

Required benchmarks:

```text
startup benchmark
short prompt benchmark
medium prompt benchmark
long context benchmark
repeated prefill benchmark
KV paging benchmark
expert paging benchmark
SSD cold cache benchmark
CUDA vs DirectML benchmark
DirectML vs Vulkan benchmark
CPU AVX benchmark
BitNet/Ternary benchmark
continuous batching benchmark
speculative decoding benchmark
memory pressure benchmark
```

---

## 29. Correctness Validation

Every optimization must be disableable.

Required validation:

```text
same prompt
same seed or greedy mode
baseline output comparison
logit drift measurement
backend parity test
KV layout validation
adapter fallback test
corruption check
cache checksum validation
```

If correctness fails:

```text
disable optimization
fallback to safer backend
log reason
continue execution if safe
```

---

## 30. Implementation Order

Implement in this order:

```text
1. Core Windows runtime skeleton
2. Hardware probe
3. Backend selector
4. Telemetry
5. CPU AVX scalar + vectorized fallback
6. DirectML backend skeleton
7. CUDA backend skeleton
8. SSD cold cache
9. Prefix cache
10. KV pager
11. Qwen/Gemma adapter
12. BitNet adapter
13. Llama adapter
14. CUDA kernels and CUDA Graphs
15. DirectML graph execution
16. Vulkan fallback
17. oneDNN CPU path
18. Expert pager
19. DeepSeek/Kimi/MiniMax/GLM adapters
20. Continuous batching
21. Speculative decoding
22. NPU provider experiment
23. Auto-tuning
24. Full benchmark matrix
25. Documentation
26. Final report
```

Start with Qwen/Gemma/BitNet.

Do not start with the largest MoE models.

---

## 31. Pull Request Requirements

Each PR must include:

```text
problem statement
backend affected
adapter affected
hardware profile used
benchmark method
before/after results
correctness notes
known limitations
rollback path
```

No PR may claim universal performance wins without benchmark evidence.

---

## 32. Final Positioning

```text
US4 V6 Apple Edition:
  MLX + Metal + NEON + ANE

US4 V6 Windows Edition:
  CUDA + DirectML + Vulkan + oneDNN + AVX/AMX + Windows ML/NPU
```

Final tagline:

> **US4 V6 Windows: Universal core, specialized adapters, CUDA/DirectML/Vulkan acceleration, local LLMs optimized for real Windows machines.**
