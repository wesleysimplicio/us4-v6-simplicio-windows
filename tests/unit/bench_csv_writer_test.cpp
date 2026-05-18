#include "us4/runtime/benchmarks/bench_csv_writer.h"

#include <gtest/gtest.h>

namespace us4::runtime::tests
{

    TEST(BenchCsvWriterTest, HeaderHasExpectedColumns)
    {
        const auto header = benchmarks::SerializeBenchHeader();
        EXPECT_NE(header.find("adapterId"), std::string::npos);
        EXPECT_NE(header.find("tokensPerSecond"), std::string::npos);
        EXPECT_NE(header.find("peakDeviceBytes"), std::string::npos);
    }

    TEST(BenchCsvWriterTest, RowSerializesAllNumericFields)
    {
        benchmarks::BenchRow row{};
        row.adapterId = "qwen-0.5b";
        row.backend = "cuda";
        row.profile = "rtx-4090";
        row.promptTokens = 32U;
        row.generatedTokens = 64U;
        row.prefillMs = 12.5;
        row.decodeMsPerToken = 4.25;
        row.tokensPerSecond = 235.3;
        row.peakHostBytes = 100U;
        row.peakDeviceBytes = 2048U;
        row.notes = "smoke";
        const auto line = benchmarks::SerializeBenchRow(row);
        EXPECT_NE(line.find("qwen-0.5b"), std::string::npos);
        EXPECT_NE(line.find("cuda"), std::string::npos);
        EXPECT_NE(line.find("235.3"), std::string::npos);
        EXPECT_NE(line.find("smoke"), std::string::npos);
        EXPECT_NE(line.find('\n'), std::string::npos);
    }

    TEST(BenchCsvWriterTest, EscapesFieldsWithCommas)
    {
        benchmarks::BenchRow row{};
        row.adapterId = "llama,8b";
        row.backend = "cuda";
        const auto line = benchmarks::SerializeBenchRow(row);
        EXPECT_NE(line.find("\"llama,8b\""), std::string::npos);
    }

    TEST(BenchCsvWriterTest, TableConcatenatesHeaderAndRows)
    {
        std::vector<benchmarks::BenchRow> rows;
        rows.push_back({.adapterId = "qwen"});
        rows.push_back({.adapterId = "gemma"});
        const auto table = benchmarks::SerializeBenchTable(rows);
        EXPECT_NE(table.find("adapterId"), std::string::npos);
        EXPECT_NE(table.find("qwen"), std::string::npos);
        EXPECT_NE(table.find("gemma"), std::string::npos);
    }

} // namespace us4::runtime::tests
