#include "us4/runtime/telemetry/telemetry_sink.h"

#include <utility>

namespace us4::runtime::telemetry
{

    void TelemetrySink::Record(TelemetryEvent event)
    {
        events_.push_back(std::move(event));
    }

    std::size_t TelemetrySink::Count(const std::string& name) const
    {
        std::size_t count = 0;
        for (const TelemetryEvent& event : events_)
        {
            if (event.name == name)
            {
                ++count;
            }
        }
        return count;
    }

    std::vector<TelemetryEvent> TelemetrySink::Drain()
    {
        std::vector<TelemetryEvent> snapshot = events_;
        events_.clear();
        return snapshot;
    }

    std::vector<TelemetryEvent> TelemetrySink::Snapshot() const
    {
        return events_;
    }

} // namespace us4::runtime::telemetry
