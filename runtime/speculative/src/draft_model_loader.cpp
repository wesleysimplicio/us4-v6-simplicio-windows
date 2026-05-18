#include "us4/runtime/speculative/draft_model_loader.h"

namespace us4::runtime::speculative
{

    std::optional<DraftModelHandle> DraftModelLoader::Load(const DraftModelDescriptor& descriptor)
    {
        if (descriptor.modelId.empty() || descriptor.vocabSize == 0)
        {
            return std::nullopt;
        }
        DraftModelHandle handle{};
        handle.modelId = descriptor.modelId;
        handle.parameterCount = descriptor.parameterCount;
        handle.loaded = true;
        lastLoadedId_ = descriptor.modelId;
        loadedCount_ += 1;
        return handle;
    }

    bool DraftModelLoader::Unload(const std::string& modelId)
    {
        if (lastLoadedId_ != modelId || loadedCount_ == 0)
        {
            return false;
        }
        loadedCount_ -= 1;
        if (loadedCount_ == 0)
        {
            lastLoadedId_.clear();
        }
        return true;
    }

    bool DraftModelLoader::IsLoaded(const std::string& modelId) const
    {
        return loadedCount_ > 0 && lastLoadedId_ == modelId;
    }

    std::size_t DraftModelLoader::LoadedCount() const
    {
        return loadedCount_;
    }

} // namespace us4::runtime::speculative
