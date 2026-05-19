#include "us4/runtime/benchmarks/bench_csv_writer.h"

#include <sstream>

namespace us4::runtime::benchmarks
{
    namespace
    {
        std::string EscapeCsv(const std::string& field)
        {
            const bool needsQuote = field.find(',') != std::string::npos
                                       || field.find('"') != std::string::npos
                                       || field.find('\n') != std::string::npos;
            if (!needsQuote)
            {
                return field;
            }
            std::string escaped;
            escaped.reserve(field.size() + 2);
            escaped.push_back('"');
            for (const auto c : field)
            {
                if (c == '"')
                {
                    escaped.push_back('"');
                }
                escaped.push_back(c);
            }
            escaped.push_back('"');
            return escaped;
        }
    } // namespace

    std::string SerializeBenchHeader()
    {
        return "adapterId,backend,profile,promptTokens,generatedTokens,prefillMs,"
               "decodeMsPerToken,tokensPerSecond,peakHostBytes,peakDeviceBytes,notes\n";
    }

    std::string SerializeBenchRow(const BenchRow& row)
    {
        std::ostringstream out;
        out << EscapeCsv(row.adapterId) << ',' << EscapeCsv(row.backend) << ','
            << EscapeCsv(row.profile) << ',' << row.promptTokens << ',' << row.generatedTokens
            << ',' << row.prefillMs << ',' << row.decodeMsPerToken << ',' << row.tokensPerSecond
            << ',' << row.peakHostBytes << ',' << row.peakDeviceBytes << ','
            << EscapeCsv(row.notes) << '\n';
        return out.str();
    }

    std::string SerializeBenchTable(const std::vector<BenchRow>& rows)
    {
        std::string out = SerializeBenchHeader();
        for (const auto& row : rows)
        {
            out += SerializeBenchRow(row);
        }
        return out;
    }

} // namespace us4::runtime::benchmarks
