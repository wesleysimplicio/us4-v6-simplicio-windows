#include "runtime/cli/command_line.h"

#include <iostream>

int main(int argc, char** argv)
{
    const us4::cli::ParsedCommand command = us4::cli::ParseArguments(argc, argv);
    const us4::cli::CommandOutput result = us4::cli::ExecuteCommand(command);

    if (!result.stdoutText.empty())
    {
        std::cout << result.stdoutText;
    }
    if (!result.stderrText.empty())
    {
        std::cerr << result.stderrText;
    }
    return result.exitCode;
}
