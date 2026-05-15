#include "us4/runtime/tuning/profile_store.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>

namespace us4::runtime::tuning
{
    namespace
    {
        std::optional<std::string> ReadEnv(const char* key)
        {
#if defined(_WIN32)
            char* value = nullptr;
            std::size_t size = 0;
            if (_dupenv_s(&value, &size, key) != 0 || value == nullptr || value[0] == '\0')
            {
                if (value != nullptr)
                {
                    std::free(value);
                }
                return std::nullopt;
            }

            const std::string result(value);
            std::free(value);
            return result;
#else
            const char* value = std::getenv(key);
            if (value == nullptr || value[0] == '\0')
            {
                return std::nullopt;
            }
            return std::string(value);
#endif
        }

        std::string NormalizeToken(std::string value)
        {
            std::string normalized;
            normalized.reserve(value.size());
            for (const char character : value)
            {
                if (std::isalnum(static_cast<unsigned char>(character)) != 0)
                {
                    normalized.push_back(
                        static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
                }
                else
                {
                    normalized.push_back('_');
                }
            }

            while (!normalized.empty() && normalized.back() == '_')
            {
                normalized.pop_back();
            }
            return normalized.empty() ? "none" : normalized;
        }

        std::string ToString(const backends::BackendVendor vendor)
        {
            switch (vendor)
            {
            case backends::BackendVendor::kNvidia:
                return "nvidia";
            case backends::BackendVendor::kAmd:
                return "amd";
            case backends::BackendVendor::kIntel:
                return "intel";
            case backends::BackendVendor::kQualcomm:
                return "qualcomm";
            case backends::BackendVendor::kMicrosoft:
                return "microsoft";
            case backends::BackendVendor::kUnknown:
                break;
            }

            return "unknown";
        }

        std::string ToString(const backends::DeviceClass deviceClass)
        {
            switch (deviceClass)
            {
            case backends::DeviceClass::kDiscreteGpu:
                return "discrete-gpu";
            case backends::DeviceClass::kIntegratedGpu:
                return "integrated-gpu";
            case backends::DeviceClass::kCpuOnly:
                return "cpu-only";
            case backends::DeviceClass::kNpu:
                return "npu";
            }

            return "unknown";
        }
    } // namespace

    ProfileStore::ProfileStore(std::filesystem::path path) : path_(std::move(path)) {}

    std::filesystem::path ProfileStore::DefaultPath()
    {
        if (const auto overridePath = ReadEnv("US4_PROFILE_STORE_PATH"); overridePath.has_value())
        {
            return std::filesystem::path(*overridePath);
        }

#if defined(US4_PROFILE_STORE_PATH)
        return std::filesystem::path(US4_PROFILE_STORE_PATH);
#else
        return std::filesystem::current_path() / "runtime" / "tuning" / "profiles.json";
#endif
    }

    std::string
    ProfileStore::BuildHardwareFingerprint(const backends::HardwareCapabilities& capabilities)
    {
        std::ostringstream builder;
        builder << "cpu=" << NormalizeToken(capabilities.cpuName) << ';'
                << "gpu=" << NormalizeToken(capabilities.primaryAdapterName) << ';'
                << "gpu_vendor=" << NormalizeToken(ToString(capabilities.primaryAdapterVendor))
                << ';' << "gpu_class=" << NormalizeToken(ToString(capabilities.primaryAdapterClass))
                << ';' << "host_bytes=" << capabilities.budget.hostBytes << ';'
                << "device_bytes=" << capabilities.budget.deviceBytes << ';'
                << "storage_bytes=" << capabilities.budget.storageBytes << ';'
                << "npu=" << (capabilities.hasNpu ? "yes" : "no") << ';'
                << "npu_name=" << NormalizeToken(capabilities.npuName) << ';'
                << "npu_vendor=" << NormalizeToken(ToString(capabilities.npuVendor)) << ';'
                << "npu_count=" << capabilities.npuCount;
        return builder.str();
    }

    std::optional<std::string>
    ProfileStore::LoadProfileId(const backends::HardwareCapabilities& capabilities) const
    {
        const std::string fingerprint = BuildHardwareFingerprint(capabilities);
        for (const auto& entry : LoadEntries())
        {
            if (entry.fingerprint == fingerprint)
            {
                return entry.profileId;
            }
        }
        return std::nullopt;
    }

    std::vector<StoredProfileEntry> ProfileStore::LoadEntries() const
    {
        std::vector<StoredProfileEntry> entries;
        if (!std::filesystem::exists(path_))
        {
            return entries;
        }

        std::ifstream input(path_);
        if (!input.is_open())
        {
            return entries;
        }

        const std::string content((std::istreambuf_iterator<char>(input)),
                                  std::istreambuf_iterator<char>());
        const std::regex entryRegex(
            R"json(\{\s*"fingerprint"\s*:\s*"([^"]+)"\s*,\s*"profile_id"\s*:\s*"([^"]+)"\s*\})json");
        for (std::sregex_iterator it(content.begin(), content.end(), entryRegex), end; it != end;
             ++it)
        {
            entries.push_back(StoredProfileEntry{
                .fingerprint = (*it)[1].str(),
                .profileId = (*it)[2].str(),
            });
        }

        return entries;
    }

    bool ProfileStore::SaveProfileId(const backends::HardwareCapabilities& capabilities,
                                     std::string_view profileId) const
    {
        std::vector<StoredProfileEntry> entries = LoadEntries();
        const std::string fingerprint = BuildHardwareFingerprint(capabilities);

        const auto existing = std::find_if(entries.begin(), entries.end(),
                                           [&fingerprint](const StoredProfileEntry& entry)
                                           { return entry.fingerprint == fingerprint; });
        if (existing != entries.end())
        {
            existing->profileId = std::string(profileId);
        }
        else
        {
            entries.push_back(StoredProfileEntry{
                .fingerprint = fingerprint,
                .profileId = std::string(profileId),
            });
        }

        std::sort(entries.begin(), entries.end(),
                  [](const StoredProfileEntry& left, const StoredProfileEntry& right)
                  { return left.fingerprint < right.fingerprint; });
        return PersistEntries(entries);
    }

    bool ProfileStore::PersistEntries(const std::vector<StoredProfileEntry>& entries) const
    {
        std::error_code errorCode;
        std::filesystem::create_directories(path_.parent_path(), errorCode);
        std::ofstream output(path_, std::ios::trunc);
        if (!output.is_open())
        {
            return false;
        }

        output << "{\n  \"entries\": [\n";
        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            const auto& entry = entries[index];
            output << "    {\"fingerprint\": \"" << entry.fingerprint << "\", "
                   << "\"profile_id\": \"" << entry.profileId << "\"}";
            if (index + 1U < entries.size())
            {
                output << ',';
            }
            output << '\n';
        }
        output << "  ]\n}\n";
        return output.good();
    }

} // namespace us4::runtime::tuning
