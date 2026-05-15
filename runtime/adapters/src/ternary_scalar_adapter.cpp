#include "us4/runtime/adapters/ternary_adapter_contracts.h"
#include "us4/runtime/adapters/ternary_model_loader.h"
#include "us4/runtime/adapters/ternary_reference_hooks.h"

#include <algorithm>
#include <memory>
#include <string_view>

namespace us4::runtime::adapters
{
    namespace
    {
        class TernaryScalarAdapter final : public DenseAdapterBase
        {
          public:
            TernaryScalarAdapter() : DenseAdapterBase(MakeTernaryDenseAdapterConfig()) {}

            bool LoadModel(const std::string& modelPath) override
            {
                const auto loadResult = LoadTernaryModelAsset(
                    modelPath, Binding().has_value() ? Binding()->modelId : "ternary");
                if (!loadResult.ok)
                {
                    return false;
                }

                descriptor_ = loadResult.descriptor;
                referenceFootprintBytes_ = EstimateTernaryPackedBytes(*descriptor_);
                return DenseAdapterBase::LoadModel(modelPath);
            }

          protected:
            [[nodiscard]] std::int32_t
            EncodePromptTokenEstimate(std::string_view prompt) const override
            {
                const auto baseEstimate = DenseAdapterBase::EncodePromptTokenEstimate(prompt);
                return baseEstimate + 111;
            }

            [[nodiscard]] std::size_t
            EstimateHostBytes(const backends::SessionRequest& request) const override
            {
                const std::size_t baseEstimate = DenseAdapterBase::EstimateHostBytes(request);
                return std::max(baseEstimate, referenceFootprintBytes_);
            }

            [[nodiscard]] std::size_t
            EstimateDeviceBytes(const backends::SessionRequest& request) const override
            {
                const std::size_t baseEstimate = DenseAdapterBase::EstimateDeviceBytes(request);
                return std::min(baseEstimate, referenceFootprintBytes_ / 6U);
            }

          private:
            std::optional<TernaryModelDescriptor> descriptor_;
            std::size_t referenceFootprintBytes_ = 0;
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateTernaryScalarAdapter()
    {
        return std::make_unique<TernaryScalarAdapter>();
    }

} // namespace us4::runtime::adapters
