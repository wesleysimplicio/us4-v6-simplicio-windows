#pragma once

#include "us4/runtime/adapters/i_us4_windows_adapter.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace us4::runtime::adapters
{

    struct DenseAdapterConfig
    {
        std::string adapterId;
        std::string family;
        std::string revision;
        std::vector<std::string> modelAliases;
        AdapterFeatureFlags features;
        backends::PrecisionMode preferredPrecision = backends::PrecisionMode::kFp16;
        std::size_t maxContextTokens = 8192;
        std::size_t maxBatchSize = 1;
        std::size_t hostBytesPerToken = 4096;
        std::size_t deviceBytesPerToken = 2048;
        std::size_t kvBytesPerToken = 512;
        std::size_t prefillScratchBytesPerToken = 256;
        std::size_t decodeScratchBytesPerToken = 128;
        std::int32_t terminalTokenBase = 0;
        std::size_t promptTokenDivisor = 4;
        bool supportsGqa = false;
        bool supportsSlidingWindowAttention = false;
        bool supportsRoPE = true;
    };

    class DenseAdapterBase : public IUS4WindowsAdapter
    {
      public:
        explicit DenseAdapterBase(DenseAdapterConfig config);
        ~DenseAdapterBase() override = default;

        [[nodiscard]] AdapterDescriptor Describe() const override;
        [[nodiscard]] backends::RuntimeStatus Status() const override;
        [[nodiscard]] AdapterHealth Health() const override;
        [[nodiscard]] std::optional<RuntimeBinding> Binding() const override;
        [[nodiscard]] bool CanServe(const backends::SessionRequest& request) const override;
        [[nodiscard]] ModelResidencyPlan
        BuildResidencyPlan(const backends::SessionRequest& request) const override;
        bool Attach(RuntimeBinding binding) override;
        bool LoadModel(const std::string& modelPath) override;
        [[nodiscard]] backends::TokenChunk Prefill(const std::string& prompt) override;
        [[nodiscard]] backends::TokenChunk
        Generate(const backends::SessionRequest& request) override;
        [[nodiscard]] GenerationFrame LastGenerationFrame() const override;
        void Reset() override;

      protected:
        [[nodiscard]] const DenseAdapterConfig& Config() const;
        [[nodiscard]] virtual bool AcceptsModelId(std::string_view modelId) const;
        [[nodiscard]] virtual std::int32_t EncodePromptTokenEstimate(std::string_view prompt) const;
        [[nodiscard]] virtual std::int32_t
        EmitTerminalToken(const backends::SessionRequest& request) const;
        [[nodiscard]] virtual std::size_t
        EstimateHostBytes(const backends::SessionRequest& request) const;
        [[nodiscard]] virtual std::size_t
        EstimateDeviceBytes(const backends::SessionRequest& request) const;

      private:
        DenseAdapterConfig config_;
        std::optional<RuntimeBinding> binding_;
        std::string loadedModelPath_;
        std::size_t lastPrefillTokens_ = 0;
        GenerationFrame lastFrame_{};
        AdapterLifecycle lifecycle_ = AdapterLifecycle::kDetached;
        backends::RuntimeStatus status_ = backends::RuntimeStatus::kIdle;
    };

} // namespace us4::runtime::adapters
