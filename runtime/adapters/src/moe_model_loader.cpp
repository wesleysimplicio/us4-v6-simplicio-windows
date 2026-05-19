#include "us4/runtime/adapters/moe_model_loader.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#else
#include <cstdint>
using HANDLE = void*;
inline HANDLE GetInvalidHandle()
{
    return reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(-1));
}
#define INVALID_HANDLE_VALUE GetInvalidHandle()
inline bool UnmapViewOfFile(void* /*view*/) { return false; }
inline bool CloseHandle(HANDLE /*h*/) { return false; }
#endif

namespace us4::runtime::adapters
{
    namespace
    {
        namespace fs = std::filesystem;

        struct KnownExpertShard
        {
            fs::path path;
            std::size_t fileBytes = 0;
        };

        struct ActiveMapping
        {
            HANDLE fileHandle = INVALID_HANDLE_VALUE;
            HANDLE mappingHandle = nullptr;
            void* mappedView = nullptr;
            std::size_t mappedBytes = 0;
            fs::path path;
        };

        [[nodiscard]] bool StartsWith(std::string_view value, std::string_view prefix)
        {
            return value.substr(0, prefix.size()) == prefix;
        }

        [[nodiscard]] fs::path ResolveRootDirectory(const fs::path& modelPath)
        {
            if (fs::exists(modelPath) && fs::is_directory(modelPath))
            {
                return modelPath;
            }
            return modelPath.parent_path();
        }

        [[nodiscard]] std::optional<std::size_t> TryParseExpertId(const fs::path& path)
        {
            const std::string stem = path.stem().string();
            if (!StartsWith(stem, "expert_"))
            {
                return std::nullopt;
            }

            const std::string suffix = stem.substr(7U);
            if (suffix.empty())
            {
                return std::nullopt;
            }

            if (!std::all_of(suffix.begin(), suffix.end(), [](const char character)
                             { return std::isdigit(static_cast<unsigned char>(character)) != 0; }))
            {
                return std::nullopt;
            }

            return static_cast<std::size_t>(std::stoull(suffix));
        }

        void CloseMapping(ActiveMapping& mapping)
        {
            if (mapping.mappedView != nullptr)
            {
                UnmapViewOfFile(mapping.mappedView);
                mapping.mappedView = nullptr;
            }
            if (mapping.mappingHandle != nullptr)
            {
                CloseHandle(mapping.mappingHandle);
                mapping.mappingHandle = nullptr;
            }
            if (mapping.fileHandle != INVALID_HANDLE_VALUE)
            {
                CloseHandle(mapping.fileHandle);
                mapping.fileHandle = INVALID_HANDLE_VALUE;
            }
            mapping.mappedBytes = 0;
            mapping.path.clear();
        }
    } // namespace

    struct MoeModelLoader::State
    {
        fs::path rootDirectory;
        std::map<std::size_t, KnownExpertShard> knownShards;
        std::map<std::size_t, ActiveMapping> activeMappings;
    };

    MoeModelLoader::MoeModelLoader() : state_(std::make_unique<State>()) {}

    MoeModelLoader::~MoeModelLoader()
    {
        Close();
    }

    MoeModelLoader::MoeModelLoader(MoeModelLoader&& other) noexcept = default;

    MoeModelLoader& MoeModelLoader::operator=(MoeModelLoader&& other) noexcept = default;

    bool MoeModelLoader::Open(const std::string_view modelPath)
    {
        Close();

        const fs::path root = ResolveRootDirectory(fs::path(std::string(modelPath)));
        if (root.empty() || !fs::exists(root) || !fs::is_directory(root))
        {
            return false;
        }

        state_->rootDirectory = root;
        for (const auto& entry : fs::directory_iterator(root))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".safetensors")
            {
                continue;
            }

            const auto expertId = TryParseExpertId(entry.path());
            if (!expertId.has_value())
            {
                continue;
            }

            state_->knownShards[*expertId] = KnownExpertShard{
                .path = entry.path(),
                .fileBytes = static_cast<std::size_t>(entry.file_size()),
            };
        }

        return !state_->knownShards.empty();
    }

    std::optional<MappedExpertShard> MoeModelLoader::LoadExpert(const std::size_t expertId)
    {
        const auto known = state_->knownShards.find(expertId);
        if (known == state_->knownShards.end())
        {
            return std::nullopt;
        }

        const auto active = state_->activeMappings.find(expertId);
        if (active != state_->activeMappings.end())
        {
            return MappedExpertShard{
                .expertId = expertId,
                .resolvedPath = active->second.path.string(),
                .mappedBytes = active->second.mappedBytes,
            };
        }

        ActiveMapping mapping{};
#if defined(_WIN32)
        mapping.fileHandle =
            CreateFileW(known->second.path.wstring().c_str(), GENERIC_READ, FILE_SHARE_READ,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (mapping.fileHandle == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        mapping.mappingHandle =
            CreateFileMappingW(mapping.fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (mapping.mappingHandle == nullptr)
        {
            CloseMapping(mapping);
            return std::nullopt;
        }

        mapping.mappedView = MapViewOfFile(mapping.mappingHandle, FILE_MAP_READ, 0, 0, 0);
        if (mapping.mappedView == nullptr)
        {
            CloseMapping(mapping);
            return std::nullopt;
        }
#else
        // Non-Windows host scaffold: surface the shard registry without touching the OS mapping
        // API so the host test suite can still exercise discovery + bookkeeping behaviour.
        mapping.mappedView = reinterpret_cast<void*>(static_cast<std::intptr_t>(expertId + 1));
#endif

        mapping.mappedBytes = known->second.fileBytes;
        mapping.path = known->second.path;
        state_->activeMappings.emplace(expertId, std::move(mapping));

        return MappedExpertShard{
            .expertId = expertId,
            .resolvedPath = known->second.path.string(),
            .mappedBytes = known->second.fileBytes,
        };
    }

    bool MoeModelLoader::UnloadExpert(const std::size_t expertId)
    {
        const auto active = state_->activeMappings.find(expertId);
        if (active == state_->activeMappings.end())
        {
            return false;
        }

        CloseMapping(active->second);
        state_->activeMappings.erase(active);
        return true;
    }

    void MoeModelLoader::Close()
    {
        for (auto& [expertId, mapping] : state_->activeMappings)
        {
            static_cast<void>(expertId);
            CloseMapping(mapping);
        }
        state_->activeMappings.clear();
        state_->knownShards.clear();
        state_->rootDirectory.clear();
    }

    bool MoeModelLoader::IsMapped(const std::size_t expertId) const
    {
        return state_->activeMappings.find(expertId) != state_->activeMappings.end();
    }

    std::size_t MoeModelLoader::KnownExpertCount() const
    {
        return state_->knownShards.size();
    }

    std::size_t MoeModelLoader::MappedExpertCount() const
    {
        return state_->activeMappings.size();
    }

    std::vector<std::size_t> MoeModelLoader::KnownExpertIds() const
    {
        std::vector<std::size_t> ids;
        ids.reserve(state_->knownShards.size());
        for (const auto& [expertId, shard] : state_->knownShards)
        {
            static_cast<void>(shard);
            ids.push_back(expertId);
        }
        return ids;
    }

    std::vector<std::size_t> MoeModelLoader::MappedExpertIds() const
    {
        std::vector<std::size_t> ids;
        ids.reserve(state_->activeMappings.size());
        for (const auto& [expertId, mapping] : state_->activeMappings)
        {
            static_cast<void>(mapping);
            ids.push_back(expertId);
        }
        return ids;
    }

} // namespace us4::runtime::adapters
