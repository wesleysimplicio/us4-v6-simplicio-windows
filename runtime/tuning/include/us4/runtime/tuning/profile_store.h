#pragma once

#include "us4/runtime/backends/runtime_types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::tuning
{

    struct StoredProfileEntry
    {
        std::string fingerprint;
        std::string profileId;
    };

    class ProfileStore
    {
      public:
        explicit ProfileStore(std::filesystem::path path = DefaultPath());

        [[nodiscard]] static std::filesystem::path DefaultPath();
        [[nodiscard]] static std::string
        BuildHardwareFingerprint(const backends::HardwareCapabilities& capabilities);

        [[nodiscard]] std::optional<std::string>
        LoadProfileId(const backends::HardwareCapabilities& capabilities) const;
        [[nodiscard]] std::vector<StoredProfileEntry> LoadEntries() const;
        bool SaveProfileId(const backends::HardwareCapabilities& capabilities,
                           std::string_view profileId) const;

      private:
        bool PersistEntries(const std::vector<StoredProfileEntry>& entries) const;

        std::filesystem::path path_;
    };

} // namespace us4::runtime::tuning
