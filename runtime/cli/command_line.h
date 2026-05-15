#pragma once

#include "runtime/core/cpu_scalar_runtime.h"
#include "runtime/core/hardware_probe.h"
#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/runtime/adapters/model_loader.h"
#include "us4/runtime/backends/hardware_probe.h"

#include <algorithm>
#include <cctype>
#include <charconv>
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

        output << "execution: scaffold-only\n";

        return CommandOutput{
            kNotImplementedExitCode,
            output.str(),
            "Run pipeline scaffolding is ready, but model execution is not implemented yet.\n",
        };
    }

} // namespace us4::cli
