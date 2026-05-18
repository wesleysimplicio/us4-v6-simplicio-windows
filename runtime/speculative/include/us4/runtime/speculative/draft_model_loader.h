#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace us4::runtime::speculative
{

    struct DraftModelDescriptor
    {
        std::string modelId;
        std::filesystem::path path;
        std::size_t parameterCount = 0;
        std::size_t hiddenSize = 0;
        std::size_t numLayers = 0;
        std::size_t vocabSize = 0;
        bool sharedTokenizer = true;
    };

    struct DraftModelHandle
    {
        std::string modelId;
        std::size_t parameterCount = 0;
        bool loaded = false;
    };

    class DraftModelLoader
    {
      public:
        [[nodiscard]] std::optional<DraftModelHandle> Load(const DraftModelDescriptor& descriptor);
        [[nodiscard]] bool Unload(const std::string& modelId);
        [[nodiscard]] bool IsLoaded(const std::string& modelId) const;
        [[nodiscard]] std::size_t LoadedCount() const;

      private:
        std::size_t loadedCount_ = 0;
        std::string lastLoadedId_;
    };

} // namespace us4::runtime::speculative
