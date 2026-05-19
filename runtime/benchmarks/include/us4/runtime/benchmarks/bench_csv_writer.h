#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace us4::runtime::benchmarks
{

    struct BenchRow
    {
        std::string adapterId;
        std::string backend;
        std::string profile;
        std::size_t promptTokens = 0;
        std::size_t generatedTokens = 0;
        double prefillMs = 0.0;
        double decodeMsPerToken = 0.0;
        double tokensPerSecond = 0.0;
        std::size_t peakHostBytes = 0;
        std::size_t peakDeviceBytes = 0;
        std::string notes;
    };

    [[nodiscard]] std::string SerializeBenchHeader();
    [[nodiscard]] std::string SerializeBenchRow(const BenchRow& row);
    [[nodiscard]] std::string SerializeBenchTable(const std::vector<BenchRow>& rows);

} // namespace us4::runtime::benchmarks
