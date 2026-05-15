#pragma once

#include "us4/runtime/adapters/adapter_contracts.h"
#include "us4/runtime/backends/runtime_types.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace us4::runtime::adapters
{

    class IUS4WindowsAdapter
    {
      public:
        virtual ~IUS4WindowsAdapter() = default;

        [[nodiscard]] virtual AdapterDescriptor Describe() const = 0;
        [[nodiscard]] virtual backends::RuntimeStatus Status() const = 0;
        [[nodiscard]] virtual AdapterHealth Health() const = 0;
        [[nodiscard]] virtual std::optional<RuntimeBinding> Binding() const = 0;
        [[nodiscard]] virtual bool CanServe(const backends::SessionRequest& request) const = 0;
        [[nodiscard]] virtual ModelResidencyPlan
        BuildResidencyPlan(const backends::SessionRequest& request) const = 0;
        virtual bool Attach(RuntimeBinding binding) = 0;
        virtual bool LoadModel(const std::string& modelPath) = 0;
        [[nodiscard]] virtual backends::TokenChunk Prefill(const std::string& prompt) = 0;
        [[nodiscard]] virtual backends::TokenChunk
        Generate(const backends::SessionRequest& request) = 0;
        [[nodiscard]] virtual GenerationFrame LastGenerationFrame() const = 0;
        virtual void Reset() = 0;
    };

    std::unique_ptr<IUS4WindowsAdapter> CreateNullAdapter();

} // namespace us4::runtime::adapters
