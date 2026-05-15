#include "us4/runtime/adapters/adapter_factory.h"

#include "us4/runtime/adapters/scalar_family_adapters.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace us4::runtime::adapters
{
    namespace
    {
        std::string Normalize(std::string_view value)
        {
            std::string lowered;
            lowered.reserve(value.size());
            for (const char character : value)
            {
                lowered.push_back(
                    static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
            }
            return lowered;
        }
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateAdapterForModelId(std::string_view modelId)
    {
        const std::string lowered = Normalize(modelId);
        if (lowered.find("qwen") != std::string::npos)
        {
            return CreateQwenScalarAdapter();
        }
        if (lowered.find("gemma") != std::string::npos)
        {
            return CreateGemmaScalarAdapter();
        }
        if (lowered.find("llama") != std::string::npos)
        {
            return CreateLlamaScalarAdapter();
        }
        if (lowered.find("bitnet") != std::string::npos)
        {
            return CreateBitNetScalarAdapter();
        }
        if (lowered.find("ternary") != std::string::npos)
        {
            return CreateTernaryScalarAdapter();
        }
        if (lowered.find("deepseek") != std::string::npos)
        {
            return CreateDeepSeekMoEScalarAdapter();
        }
        if (lowered.find("kimi") != std::string::npos)
        {
            return CreateKimiMoEScalarAdapter();
        }
        return CreateNullAdapter();
    }

} // namespace us4::runtime::adapters
