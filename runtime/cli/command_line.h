#pragma once

#include "runtime/core/hardware_probe.h"
#include "runtime/core/runtime_context.h"
#include "runtime/core/runtime_mode.h"
#include "us4/runtime/backends/hardware_probe.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>

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
        std::string prompt;
        bool allowNpu = false;
        std::string mode = "auto";
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
        output
            << "us4-cli\n"
            << "Usage:\n"
            << "  us4-cli help\n"
            << "  us4-cli version\n"
            << "  us4-cli probe\n"
            << "  us4-cli run --model <model-id> --prompt <text> [--mode "
               "<auto|full|balanced|degraded|ultra_low|micro|nano|cpu_only>] [--npu]\n"
            << "\n"
            << "Commands:\n"
            << "  help, --help, -h    Show this help\n"
            << "  version, --version  Show CLI version\n"
            << "  probe, --probe      Print detected hardware/backend summary\n"
            << "  run                 Validate runtime intent and show scaffolded execution plan\n"
            << "\n"
            << "Examples:\n"
            << "  us4-cli probe\n"
            << "  us4-cli version\n"
            << "  us4-cli run --model qwen-0.5b --prompt \"hi\" --mode auto\n";
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
        request.allowNpu = command.allowNpu;

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

        const us4::runtime::backends::HardwareCapabilities capabilities =
            us4::runtime::backends::HardwareProbe::DetectHost();
        const us4::core::RuntimePlan plan =
            us4::core::RuntimeContext::BuildPlan(request, capabilities);

        std::ostringstream output;
        output << us4::core::FormatRuntimePlan(plan);
        output << "prompt_chars: " << command.prompt.size() << '\n';
        output << "execution: scaffold-only\n";

        return CommandOutput{
            kNotImplementedExitCode,
            output.str(),
            "Run pipeline scaffolding is ready, but model execution is not implemented yet.\n",
        };
    }

} // namespace us4::cli
