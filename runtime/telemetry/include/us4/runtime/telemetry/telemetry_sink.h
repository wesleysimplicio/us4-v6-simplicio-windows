#pragma once

#include <string>
#include <vector>

namespace us4::runtime::telemetry
{

    struct TelemetryEvent
    {
        std::string name;
        std::string value;
        std::string category = "runtime";
        double numericValue = 0.0;
    };

    class TelemetrySink
    {
      public:
        void Record(TelemetryEvent event);
        [[nodiscard]] std::size_t Count(const std::string& name) const;
        [[nodiscard]] std::vector<TelemetryEvent> Drain();
        [[nodiscard]] std::vector<TelemetryEvent> Snapshot() const;

      private:
        std::vector<TelemetryEvent> events_;
    };

} // namespace us4::runtime::telemetry
