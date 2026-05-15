#include "us4/runtime/adapters/bitnet_adapter_contracts.h"
#include "us4/runtime/adapters/bitnet_model_loader.h"
#include "us4/runtime/adapters/bitnet_reference_hooks.h"

#include <algorithm>
#include <memory>
#include <string_view>

namespace us4::runtime::adapters
{
    namespace
    {
        class BitNetScalarAdapter final : public DenseAdapterBase
        {
          public:
            BitNetScalarAdapter() : DenseAdapterBase(MakeBitNetDenseAdapterConfig()) {}

            bool LoadModel(const std::string& modelPath) override
            {
                const auto loadResult = LoadBitNetModelAsset(
                    modelPath, Binding().has_value() ? Binding()->modelId : "bitnet");
                if (!loadResult.ok)
                {
                    return false;
                }

                descriptor_ = loadResult.descriptor;
                referenceFootprintBytes_ = EstimateBitNetPackedBytes(*descriptor_);
                return DenseAdapterBase::LoadModel(modelPath);
            }

          protected:
            [[nodiscard]] std::int32_t
            EncodePromptTokenEstimate(std::string_view prompt) const override
            {
                const auto baseEstimate = DenseAdapterBase::EncodePromptTokenEstimate(prompt);
                return baseEstimate + 158;
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
                return std::min(baseEstimate, referenceFootprintBytes_ / 8U);
            }

          private:
            std::optional<BitNetModelDescriptor> descriptor_;
            std::size_t referenceFootprintBytes_ = 0;
        };
    } // namespace

    std::unique_ptr<IUS4WindowsAdapter> CreateBitNetScalarAdapter()
    {
        return std::make_unique<BitNetScalarAdapter>();
    }

} // namespace us4::runtime::adapters
