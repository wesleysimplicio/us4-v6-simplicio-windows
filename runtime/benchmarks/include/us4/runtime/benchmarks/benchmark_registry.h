#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::benchmarks
{

    struct BenchmarkCase
    {
        std::string name;
        std::string backend;
        std::string scenario;
        std::string modelId;
        std::string adapterId;
        std::string profileId;
        std::string runtimeMode;
        std::size_t promptTokens = 0;
        std::size_t generationTokens = 0;
        bool requiresGpu = false;
        bool participatesInCorrectnessGate = true;
        bool touchesCli = false;
    };

    class BenchmarkRegistry
    {
      public:
        [[nodiscard]] static std::vector<BenchmarkCase> DefaultCases();
        [[nodiscard]] static std::vector<BenchmarkCase> CasesForBackend(const std::string& backend);
        [[nodiscard]] static std::optional<BenchmarkCase> FindByName(const std::string& name);
    };

} // namespace us4::runtime::benchmarks
