#pragma once

#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/hardware_probe.h"
#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/runtime/adapters/model_loader.h"
#include "us4/runtime/backends/cuda/cuda_backend.h"
#include "us4/runtime/backends/directml/dml_device.h"
#include "us4/runtime/backends/directml/dml_graph.h"
#include "us4/runtime/backends/hardware_probe.h"
#include "us4/runtime/backends/vulkan/vulkan_context.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"
#include "us4/runtime/backends/windows_ml/layer_offloader.h"
#include "us4/runtime/backends/windows_ml/mixed_dispatch_planner.h"
#include "us4/runtime/backends/windows_ml/power_thermal_monitor.h"
#include "us4/runtime/backends/windows_ml/windows_ml_execution_plan.h"
#include "us4/runtime/backends/windows_ml/winml_adapter.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace us4::cli
{

    inline constexpr int kSuccessExitCode = 0;
    inline constexpr int kUsageExitCode = 1;
    inline constexpr int kNotImplementedExitCode = 2;

    enum class CommandKind
    {
        kHelp,
        kVersion,
        kProbe,
        kRun,
        kInvalid,
    };

    struct ParsedCommand
    {
        CommandKind kind = CommandKind::kHelp;
        std::string modelId;
        std::string modelPath;
        std::string prompt;
        std::string backend = "auto";
        bool allowNpu = false;
        std::string mode = "auto";
        std::uint32_t maxTokens = 32;
        std::string error;
    };

    struct CommandOutput
    {
        int exitCode = kSuccessExitCode;
        std::string stdoutText;
        std::string stderrText;
    };

    inline std::uint32_t EstimatePromptTokens(std::string_view prompt)
    {
        return std::max<std::uint32_t>(1U, static_cast<std::uint32_t>((prompt.size() + 3U) / 4U));
    }

    inline void AppendIssueCodes(std::ostringstream& output, std::string_view prefix,
                                 const std::vector<us4::runtime::backends::RuntimeIssue>& issues)
    {
        if (issues.empty())
        {
            return;
        }

        output << prefix << ".issue_codes: ";
        for (std::size_t index = 0; index < issues.size(); ++index)
        {
            if (index > 0U)
            {
                output << ',';
            }
            output << issues[index].code;
        }
        output << '\n';
    }

    inline void AppendCudaDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                                 const us4::runtime::backends::HardwareCapabilities& capabilities,
                                 std::string_view prompt)
    {
        const auto executionPlan = us4::runtime::backends::cuda::CudaBackend::BuildExecutionPlan(
            plan.request, capabilities);
        const auto prefill =
            us4::runtime::backends::cuda::CudaBackend::PreparePrefill(executionPlan, prompt);
        const auto decode = us4::runtime::backends::cuda::CudaBackend::PrepareDecode(
            executionPlan, prefill.chunk, 0U);

        output << "execution: cuda-dry-run\n";
        output << "cuda.architecture: "
               << us4::runtime::backends::cuda::ToString(executionPlan.device.architecture) << '\n';
        output << "cuda.compute_capability: " << executionPlan.device.computeCapabilityMajor << '.'
               << executionPlan.device.computeCapabilityMinor << '\n';
        output << "cuda.execution_flavor: "
               << us4::runtime::backends::cuda::ToString(executionPlan.flavor) << '\n';
        output << "cuda.residency: "
               << us4::runtime::backends::cuda::ToString(executionPlan.memory.residency) << '\n';
        output << "cuda.prefill_streams: " << executionPlan.streams.prefillStreams << '\n';
        output << "cuda.decode_streams: " << executionPlan.streams.decodeStreams << '\n';
        output << "cuda.graph_capture: " << (executionPlan.graph.enableGraphCapture ? "yes" : "no")
               << '\n';
        output << "cuda.prefill_chunk_tokens: " << prefill.chunk.tokens.size() << '\n';
        output << "cuda.decode_preview_tokens: " << decode.chunk.tokens.size() << '\n';
        output << "cuda.prefill_bytes_touched: " << prefill.deviceBytesTouched << '\n';
        output << "cuda.decode_bytes_touched: " << decode.deviceBytesTouched << '\n';
        output << "cuda.plan_issues: " << executionPlan.issues.size() << '\n';
        AppendIssueCodes(output, "cuda", executionPlan.issues);
    }

    inline void
    AppendDirectMlDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                         const us4::runtime::backends::HardwareCapabilities& capabilities,
                         std::string_view prompt)
    {
        using namespace us4::runtime::backends::directml;

        DmlDevice device({
            .preferIntegratedGpu =
                plan.backend.deviceClass == us4::runtime::backends::DeviceClass::kIntegratedGpu,
            .preferLowLatency = plan.request.preferLowLatency,
        });
        const bool initialized = device.Initialize(capabilities);
        const std::size_t persistentBytes =
            std::max<std::size_t>(plan.residency.expectedDeviceBytes / 4U, 1024U * 1024U);
        const bool prepared =
            initialized &&
            device.PrepareGraphSession(plan.model.id, DmlExecutionPhase::kPrefill, persistentBytes);

        DmlGraph graph(&device);
        graph.RecordNode({
            .name = "prefill.matmul",
            .kind = DmlOperatorKind::kMatMul,
            .dataType = ToDmlDataType(plan.backend.defaultPrecision),
            .inputCount = 2,
            .outputCount = 1,
            .usesPersistentWeights = true,
            .allowChunking = true,
            .temporaryBytes =
                std::max<std::size_t>(plan.residency.expectedPrefillScratchBytes, 512U * 1024U),
            .persistentBytes = persistentBytes / 2U,
        });
        graph.RecordNode({
            .name = "decode.attention",
            .kind = DmlOperatorKind::kAttention,
            .dataType = ToDmlDataType(plan.backend.defaultPrecision),
            .inputCount = 3,
            .outputCount = 1,
            .usesPersistentWeights = false,
            .allowChunking = true,
            .temporaryBytes =
                std::max<std::size_t>(plan.residency.expectedDecodeScratchBytes, 256U * 1024U),
            .persistentBytes = persistentBytes / 4U,
        });

        const auto compile = graph.Compile({
            .precision = plan.backend.defaultPrecision,
            .enableOperatorFusion = true,
            .enablePersistentDescriptors = true,
            .enableChunkedCompilation = true,
            .maxTemporaryBytes =
                std::max<std::size_t>(plan.residency.expectedPrefillScratchBytes, 1024U * 1024U),
            .maxPersistentBytes = persistentBytes,
        });
        const auto dispatch = graph.Dispatch({
            .phase = DmlExecutionPhase::kPrefill,
            .tokenCount = EstimatePromptTokens(prompt),
            .batchSize =
                static_cast<std::uint32_t>(std::max<std::size_t>(plan.profile.targetBatchSize, 1U)),
            .sequenceLength = std::max<std::uint32_t>(EstimatePromptTokens(prompt), 1U),
            .allowAsyncCompletion = true,
        });

        output << "execution: directml-dry-run\n";
        output << "directml.initialized: " << (initialized ? "yes" : "no") << '\n';
        output << "directml.session_prepared: " << (prepared ? "yes" : "no") << '\n';
        output << "directml.graph_state: " << ToString(graph.State()) << '\n';
        output << "directml.compile_ok: " << (compile.compiled ? "yes" : "no") << '\n';
        output << "directml.dispatch_ok: " << (dispatch.submitted ? "yes" : "no") << '\n';
        output << "directml.node_count: " << graph.Nodes().size() << '\n';
        output << "directml.dispatch_count: " << graph.Stats().dispatchCount << '\n';
        output << "directml.fence: " << graph.Stats().lastFenceValue << '\n';
    }

    inline void AppendVulkanDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                                   const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        const auto executionPlan = us4::runtime::backends::vulkan::BuildVulkanExecutionPlan({
            .binding = plan.binding,
            .request = plan.request,
            .adapterId = plan.model.adapterId,
            .targetBatchSize = plan.profile.targetBatchSize,
            .modelSupportsMoE = plan.model.supportsMoE,
            .modelSupportsSpeculative = plan.model.supportsSpeculative,
        });
        us4::runtime::backends::vulkan::VulkanContext context({
            .preferIntegratedGpu =
                plan.backend.deviceClass == us4::runtime::backends::DeviceClass::kIntegratedGpu,
            .enableValidationLayer = false,
            .allowDescriptorBuffer = true,
            .preferAsyncTransfers = true,
        });
        const bool initialized = context.Initialize(capabilities);
        const bool bound = initialized && context.BindExecutionPlan(plan.model.id, executionPlan);

        output << "execution: vulkan-dry-run\n";
        output << "vulkan.context_initialized: " << (initialized ? "yes" : "no") << '\n';
        output << "vulkan.context_bound: " << (bound ? "yes" : "no") << '\n';
        output << "vulkan.context_state: "
               << us4::runtime::backends::vulkan::ToString(context.State()) << '\n';
        output << "vulkan.step_count: " << executionPlan.steps.size() << '\n';
        output << "vulkan.decode_micro_batch: " << executionPlan.decodeMicroBatchSize << '\n';
        output << "vulkan.prefill_batches: " << executionPlan.maxConcurrentPrefillBatches << '\n';
        output << "vulkan.descriptor_sets: " << context.DescriptorArena().setCount << '\n';
        output << "vulkan.persistent_bytes: " << context.DescriptorArena().persistentBytes << '\n';
        output << "vulkan.transient_bytes: " << context.DescriptorArena().transientBytes << '\n';
        output << "vulkan.kernel_manifest_loaded: "
               << (context.KernelLibrary().Loaded() ? "yes" : "no") << '\n';
        output << "vulkan.kernel_count: " << context.Stats().loadedKernelCount << '\n';
        output << "vulkan.required_kernel_count: " << context.Stats().requiredKernelCount << '\n';
        output << "vulkan.timeline_semaphores: "
               << (executionPlan.useTimelineSemaphores ? "yes" : "no") << '\n';
        output << "vulkan.cpu_attention_fallback: "
               << (executionPlan.fallbackToCpuAttention ? "yes" : "no") << '\n';
        output << "vulkan.host_moe_route: " << (executionPlan.routeMoEOnHost ? "yes" : "no")
               << '\n';
        output << "vulkan.plan_issues: " << executionPlan.issues.size() << '\n';
        AppendIssueCodes(output, "vulkan", executionPlan.issues);
        if (!executionPlan.steps.empty())
        {
            output << "vulkan.first_stage: "
                   << us4::runtime::backends::vulkan::ToString(executionPlan.steps.front().stage)
                   << '\n';
            output << "vulkan.last_stage: "
                   << us4::runtime::backends::vulkan::ToString(executionPlan.steps.back().stage)
                   << '\n';
        }
    }

    inline void
    AppendWindowsMlDryRun(std::ostringstream& output, const us4::core::RuntimePlan& plan,
                          const us4::runtime::backends::HardwareCapabilities& capabilities)
    {
        const auto executionPlan = us4::runtime::backends::windows_ml::BuildWindowsMlExecutionPlan({
            .binding = plan.binding,
            .request = plan.request,
            .adapterId = plan.model.adapterId,
            .targetBatchSize = plan.profile.targetBatchSize,
            .modelSupportsMoE = plan.model.supportsMoE,
            .modelSupportsSpeculative = plan.model.supportsSpeculative,
            .modelSupportsVision = plan.model.supportsVision,
        });
        us4::runtime::backends::windows_ml::WinMlAdapter adapter({
            .allowCpuFallback = true,
            .preferStaticShapes = true,
            .enableTelemetry = true,
        });
        const std::vector<us4::runtime::backends::windows_ml::LayerDescriptor> sampleLayers = {
            {
                .name = "embed",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kEmbedding,
            },
            {
                .name = "prefill.ffn",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kDensePrefill,
            },
            {
                .name = "decode.ffn",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kDenseDecode,
                .latencySensitive = true,
            },
            {
                .name = "attention",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kAttention,
            },
            {
                .name = "kv-compress",
                .kind = us4::runtime::backends::windows_ml::LayerKind::kKvCompression,
            },
        };
        const bool initialized = adapter.Initialize(capabilities);
        const bool compiled = initialized && adapter.CompileGraph(plan.model.id, executionPlan);
        const auto powerSnapshot =
            us4::runtime::backends::windows_ml::PowerThermalMonitor::Sample();
        const auto powerIssues =
            us4::runtime::backends::windows_ml::PowerThermalMonitor::BuildIssues(powerSnapshot);
        const auto dispatchTable =
            us4::runtime::backends::windows_ml::LayerOffloader::BuildDispatchTable(executionPlan,
                                                                                   sampleLayers);

        output << "execution: windows-ml-dry-run\n";
        output << "windows_ml.adapter_initialized: " << (initialized ? "yes" : "no") << '\n';
        output << "windows_ml.graph_compiled: " << (compiled ? "yes" : "no") << '\n';
        output << "windows_ml.adapter_state: "
               << us4::runtime::backends::windows_ml::ToString(adapter.State()) << '\n';
        output << "windows_ml.opt_in_satisfied: " << (executionPlan.optInSatisfied ? "yes" : "no")
               << '\n';
        output << "windows_ml.prefill_offload: " << (executionPlan.offloadPrefill ? "yes" : "no")
               << '\n';
        output << "windows_ml.decode_offload: " << (executionPlan.offloadDecode ? "yes" : "no")
               << '\n';
        output << "windows_ml.kv_host_compression: "
               << (executionPlan.compressKvOnHost ? "yes" : "no") << '\n';
        output << "windows_ml.partition_count: " << executionPlan.partitions.size() << '\n';
        output << "windows_ml.npu_partitions: " << adapter.Stats().npuPartitionCount << '\n';
        output << "windows_ml.host_partitions: " << adapter.Stats().hostPartitionCount << '\n';
        output << "windows_ml.cpu_fallback_partitions: "
               << adapter.Stats().cpuFallbackPartitionCount << '\n';
        output << "windows_ml.dispatch_table_size: " << dispatchTable.size() << '\n';
        output << "windows_ml.batch_hint: " << executionPlan.batchSizeHint << '\n';
        output << "windows_ml.context_hint: " << executionPlan.contextTokenHint << '\n';
        output << "windows_ml.plan_issues: " << executionPlan.issues.size() << '\n';
        output << "windows_ml.power_source: "
               << us4::runtime::backends::windows_ml::ToString(powerSnapshot.powerSource) << '\n';
        output << "windows_ml.battery_percent: " << powerSnapshot.batteryPercent << '\n';
        output << "windows_ml.battery_saver: " << (powerSnapshot.batterySaverActive ? "yes" : "no")
               << '\n';
        output << "windows_ml.thermal_state: "
               << us4::runtime::backends::windows_ml::ToString(powerSnapshot.thermalState) << '\n';
        output << "windows_ml.etw_throttled: " << (powerSnapshot.etwThrottleActive ? "yes" : "no")
               << '\n';
        output << "windows_ml.power_policy: "
               << us4::runtime::backends::windows_ml::ToString(
                      us4::runtime::backends::windows_ml::PowerThermalMonitor::SelectPolicy(
                          powerSnapshot))
               << '\n';
        output << "windows_ml.synthetic_power_telemetry: "
               << (powerSnapshot.usingSyntheticTelemetry ? "yes" : "no") << '\n';
        AppendIssueCodes(output, "windows_ml", executionPlan.issues);
        AppendIssueCodes(output, "windows_ml.power", powerIssues);
        if (!dispatchTable.empty())
        {
            output << "windows_ml.first_dispatch_target: "
                   << us4::runtime::backends::windows_ml::ToString(dispatchTable.front().target)
                   << '\n';
            output << "windows_ml.last_dispatch_target: "
                   << us4::runtime::backends::windows_ml::ToString(dispatchTable.back().target)
                   << '\n';
        }
        if (!executionPlan.partitions.empty())
        {
            output << "windows_ml.last_partition: "
                   << us4::runtime::backends::windows_ml::ToString(
                          executionPlan.partitions.back().kind)
                   << '\n';
        }

        output << "windows_ml.mixed_dispatch_active: " << (capabilities.hasVulkan ? "yes" : "no")
               << '\n';
        if (capabilities.hasVulkan)
        {
            auto gpuBinding = plan.binding;
            gpuBinding.backend = us4::runtime::backends::BackendDescriptor{
                .kind = us4::runtime::backends::BackendKind::kVulkan,
                .name = "vulkan",
                .displayName = "Vulkan Compute",
                .deviceClass = capabilities.primaryAdapterClass,
                .vendor = capabilities.primaryAdapterVendor,
                .availability = us4::runtime::backends::BackendAvailability::kAvailable,
                .defaultPrecision = us4::runtime::backends::PrecisionMode::kFp16,
                .maxContextTokensHint = std::max<std::uint32_t>(
                    plan.request.maxContextTokens, plan.binding.backend.maxContextTokensHint == 0
                                                       ? 4096U
                                                       : plan.binding.backend.maxContextTokensHint),
                .supportsPagedKv = true,
                .supportsSpeculative = true,
                .supportsUnifiedMemory = capabilities.hasUnifiedMemory ||
                                         capabilities.primaryAdapterClass ==
                                             us4::runtime::backends::DeviceClass::kIntegratedGpu,
                .budget = capabilities.budget,
            };
            const auto gpuPlan = us4::runtime::backends::vulkan::BuildVulkanExecutionPlan({
                .binding = gpuBinding,
                .request = plan.request,
                .adapterId = plan.model.adapterId,
                .targetBatchSize = plan.profile.targetBatchSize,
                .modelSupportsMoE = plan.model.supportsMoE,
                .modelSupportsSpeculative = plan.model.supportsSpeculative,
            });
            const auto mixedDispatchPlan =
                us4::runtime::backends::windows_ml::MixedDispatchPlanner::Build(
                    gpuPlan, executionPlan, sampleLayers, powerSnapshot);
            output << "windows_ml.mixed_dispatch_slice_count: " << mixedDispatchPlan.slices.size()
                   << '\n';
            output << "windows_ml.mixed_dispatch_gpu_primary: "
                   << (mixedDispatchPlan.gpuPrimaryActive ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_npu_dense: "
                   << (mixedDispatchPlan.npuDenseActive ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_host_assist: "
                   << (mixedDispatchPlan.hostAssistRequired ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_cpu_fallback: "
                   << (mixedDispatchPlan.cpuFallbackPresent ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_policy: "
                   << us4::runtime::backends::windows_ml::ToString(mixedDispatchPlan.policy)
                   << '\n';
            output << "windows_ml.mixed_dispatch_policy_degraded: "
                   << (mixedDispatchPlan.degradedByPolicy ? "yes" : "no") << '\n';
            output << "windows_ml.mixed_dispatch_npu_demotions: "
                   << mixedDispatchPlan.npuDemotionCount << '\n';
            if (!mixedDispatchPlan.slices.empty())
            {
                output << "windows_ml.mixed_dispatch_first_target: "
                       << us4::runtime::backends::windows_ml::ToString(
                              mixedDispatchPlan.slices.front().target)
                       << '\n';
                output << "windows_ml.mixed_dispatch_last_target: "
                       << us4::runtime::backends::windows_ml::ToString(
                              mixedDispatchPlan.slices.back().target)
                       << '\n';
            }
        }
    }

    inline std::string RenderHelp()
    {
        std::ostringstream output;
        output << "us4-cli\n"
               << "Usage:\n"
               << "  us4-cli help\n"
               << "  us4-cli version\n"
               << "  us4-cli probe\n"
               << "  us4-cli run --model <model-id> --prompt <text> [--model-path <asset>] "
                  "[--max-tokens <count>] [--backend "
                  "<auto|cpu|cuda|directml|vulkan|windows-ml|npu>] [--mode "
                  "<auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]\n"
               << "\n"
               << "Commands:\n"
               << "  help, --help, -h    Show this help\n"
               << "  version, --version  Show CLI version\n"
               << "  probe, --probe      Print detected hardware/backend summary\n"
               << "  run                 Execute CPU_ONLY baseline or print the scaffolded "
                  "execution plan\n"
               << "\n"
               << "Examples:\n"
               << "  us4-cli probe\n"
               << "  us4-cli version\n"
               << "  us4-cli run --model qwen-0.5b --prompt \"hi\" --backend cpu\n";
        return output.str();
    }

    inline ParsedCommand ParseArguments(const int argc, char** argv)
    {
        if (argc <= 1)
        {
            return {};
        }

        ParsedCommand command{};
        const std::string_view verb = argv[1];

        if (verb == "--help" || verb == "-h" || verb == "help")
        {
            return command;
        }

        if (verb == "--version" || verb == "version")
        {
            command.kind = CommandKind::kVersion;
            return command;
        }

        if (verb == "--probe" || verb == "probe")
        {
            command.kind = CommandKind::kProbe;
            return command;
        }

        if (verb != "run")
        {
            command.kind = CommandKind::kInvalid;
            command.error = "Unknown command: " + std::string(verb);
            return command;
        }

        command.kind = CommandKind::kRun;
        for (int index = 2; index < argc; ++index)
        {
            const std::string_view option = argv[index];
            if (option == "--model")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --model.";
                    return command;
                }
                command.modelId = argv[++index];
                continue;
            }
            if (option == "--prompt")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --prompt.";
                    return command;
                }
                command.prompt = argv[++index];
                continue;
            }
            if (option == "--model-path")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --model-path.";
                    return command;
                }
                command.modelPath = argv[++index];
                continue;
            }
            if (option == "--mode")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --mode.";
                    return command;
                }
                command.mode = argv[++index];
                continue;
            }
            if (option == "--backend")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --backend.";
                    return command;
                }
                command.backend = argv[++index];
                continue;
            }
            if (option == "--max-tokens")
            {
                if (index + 1 >= argc)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "Missing value for --max-tokens.";
                    return command;
                }

                std::uint32_t parsedValue = 0;
                const std::string_view rawValue = argv[++index];
                const auto [ptr, ec] = std::from_chars(
                    rawValue.data(), rawValue.data() + rawValue.size(), parsedValue);
                if (ec != std::errc{} || ptr != rawValue.data() + rawValue.size() ||
                    parsedValue == 0U)
                {
                    command.kind = CommandKind::kInvalid;
                    command.error = "The --max-tokens value must be a positive integer.";
                    return command;
                }
                command.maxTokens = parsedValue;
                continue;
            }
            if (option == "--npu")
            {
                command.allowNpu = true;
                continue;
            }

            command.kind = CommandKind::kInvalid;
            command.error = "Unknown run option: " + std::string(option);
            return command;
        }

        if (command.modelId.empty())
        {
            command.kind = CommandKind::kInvalid;
            command.error = "The run command requires --model <model-id>.";
            return command;
        }
        if (command.prompt.empty())
        {
            command.kind = CommandKind::kInvalid;
            command.error = "The run command requires --prompt <text>.";
            return command;
        }

        return command;
    }

    inline CommandOutput ExecuteCommand(const ParsedCommand& command)
    {
        const auto isAutoMode = [](std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
                           { return static_cast<char>(std::tolower(character)); });
            return value == "auto";
        };
        const auto normalizeBackend = [](std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char character)
                           {
                               if (character == '_' || character == ' ')
                               {
                                   return '-';
                               }
                               return static_cast<char>(std::tolower(character));
                           });
            return value;
        };

        if (command.kind == CommandKind::kInvalid)
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                command.error + '\n',
            };
        }

        if (command.kind == CommandKind::kHelp)
        {
            return CommandOutput{
                kSuccessExitCode,
                RenderHelp(),
                {},
            };
        }

        if (command.kind == CommandKind::kVersion)
        {
            return CommandOutput{
                kSuccessExitCode,
                "us4-cli 0.1.0-alpha\n",
                {},
            };
        }

        if (command.kind == CommandKind::kProbe)
        {
            return CommandOutput{
                kSuccessExitCode,
                us4::core::FormatProbeSummary(us4::core::ProbeHardware()),
                {},
            };
        }

        us4::runtime::backends::SessionRequest request{};
        request.modelId = command.modelId;
        request.preferredBackend = normalizeBackend(command.backend);
        request.allowNpu = command.allowNpu;
        request.maxGenerationTokens = command.maxTokens;

        if (request.preferredBackend != "auto" && request.preferredBackend != "cpu" &&
            request.preferredBackend != "cuda" && request.preferredBackend != "directml" &&
            request.preferredBackend != "vulkan" && request.preferredBackend != "windows-ml" &&
            request.preferredBackend != "npu")
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                "Unknown value for --backend.\n",
            };
        }

        const auto parsedMode = us4::core::ParseRuntimeMode(command.mode);
        if (!parsedMode.has_value() && !isAutoMode(command.mode))
        {
            return CommandOutput{
                kUsageExitCode,
                RenderHelp(),
                "Unknown value for --mode.\n",
            };
        }
        if (parsedMode.has_value())
        {
            request.mode = *parsedMode;
        }
        else if (request.preferredBackend == "cpu")
        {
            request.mode = us4::runtime::backends::RuntimeMode::kCpuOnly;
        }

        const us4::runtime::backends::HardwareCapabilities capabilities =
            us4::runtime::backends::HardwareProbe::DetectHost();
        const us4::core::RuntimePlan plan =
            us4::core::RuntimeContext::BuildPlan(request, capabilities);

        std::ostringstream output;
        output << us4::core::FormatRuntimePlan(plan);
        output << "prompt_chars: " << command.prompt.size() << '\n';
        if (plan.mode == us4::runtime::backends::RuntimeMode::kCpuOnly ||
            plan.backend.kind == us4::runtime::backends::BackendKind::kCpu)
        {
            const auto runResult =
                us4::core::ExecuteCpuScalarRun(plan, command.prompt, command.modelPath);
            if (!runResult.ok)
            {
                output << "execution: cpu-scalar-failed\n";
                return CommandOutput{
                    kNotImplementedExitCode,
                    output.str(),
                    runResult.error + '\n',
                };
            }

            output << "execution: cpu-scalar\n";
            output << "model_path: " << runResult.report.modelPath << '\n';
            output << "model_format: "
                   << us4::runtime::adapters::ToString(runResult.report.modelFormat) << '\n';
            output << "prefill_tokens:";
            for (const auto token : runResult.report.prefillTokens)
            {
                output << ' ' << token;
            }
            output << '\n';
            output << "generated_tokens:";
            for (const auto token : runResult.report.generatedTokens)
            {
                output << ' ' << token;
            }
            output << '\n';
            output << "generated_text: " << runResult.report.generatedText << '\n';
            output << "generation.prefill_tokens: " << runResult.report.frame.prefillTokens << '\n';
            output << "generation.decode_tokens: " << runResult.report.frame.decodeTokens << '\n';
            output << "generation.reached_eos: "
                   << (runResult.report.frame.reachedEos ? "yes" : "no") << '\n';
            output << "kernel.scalar_matmul_checksum: " << runResult.report.scalarMatMulChecksum
                   << '\n';
            output << "kernel.scalar_attention_checksum: "
                   << runResult.report.scalarAttentionChecksum << '\n';
            output << "kv.segment_count: " << runResult.report.kvStats.segmentCount << '\n';
            output << "kv.pinned_segment_count: " << runResult.report.kvStats.pinnedSegmentCount
                   << '\n';
            output << "kv.host_bytes: " << runResult.report.kvStats.hostBytes << '\n';
            output << "kv.storage_bytes: " << runResult.report.kvStats.storageBytes << '\n';
            output << "kv.summary_bytes: " << runResult.report.kvStats.summaryBytes << '\n';
            output << "kv.evictions: " << runResult.report.kvStats.evictionCount << '\n';
            output << "prefix_cache.entries: " << runResult.report.prefixCacheEntries << '\n';
            output << "prefix_cache.warm_entries: " << runResult.report.prefixCacheWarmEntries
                   << '\n';
            output << "moe.route_count: " << runResult.report.moeRouteCount << '\n';
            output << "moe.host_route_count: " << runResult.report.moeHostRouteCount << '\n';
            output << "moe.router_entropy: " << runResult.report.moeRouterEntropy << '\n';
            output << "moe.hot_experts: " << runResult.report.moeHotExperts << '\n';
            output << "moe.warm_experts: " << runResult.report.moeWarmExperts << '\n';
            output << "moe.cold_experts: " << runResult.report.moeColdExperts << '\n';
            output << "telemetry.events: " << runResult.report.telemetryEventCount << '\n';
            output << "run_status: completed\n";
            return CommandOutput{
                kSuccessExitCode,
                output.str(),
                {},
            };
        }

        switch (plan.backend.kind)
        {
        case us4::runtime::backends::BackendKind::kCuda:
            AppendCudaDryRun(output, plan, capabilities, command.prompt);
            break;
        case us4::runtime::backends::BackendKind::kDirectML:
            AppendDirectMlDryRun(output, plan, capabilities, command.prompt);
            break;
        case us4::runtime::backends::BackendKind::kVulkan:
            AppendVulkanDryRun(output, plan, capabilities);
            break;
        case us4::runtime::backends::BackendKind::kWindowsMl:
            AppendWindowsMlDryRun(output, plan, capabilities);
            break;
        case us4::runtime::backends::BackendKind::kCpu:
            output << "execution: scaffold-only\n";
            break;
        }

        return CommandOutput{
            kNotImplementedExitCode,
            output.str(),
            "Run pipeline scaffolding is ready, but model execution is not implemented yet.\n",
        };
    }

} // namespace us4::cli
