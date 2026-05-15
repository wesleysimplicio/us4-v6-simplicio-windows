#pragma once

#include "us4/runtime/backends/runtime_types.h"
#include "us4/runtime/backends/vulkan/vulkan_execution_plan.h"

#include <string>
#include <vector>

namespace us4::runtime::backends::vulkan
{

    enum class VulkanKernelProgram
    {
        kMatMul,
        kSoftmax,
        kRmsNorm,
    };

    struct VulkanKernelManifestEntry
    {
        VulkanKernelProgram program = VulkanKernelProgram::kMatMul;
        std::string name;
        std::string relativePath;
        std::string entryPoint = "main";
        std::uint32_t workgroupX = 1;
        std::uint32_t workgroupY = 1;
        std::uint32_t workgroupZ = 1;
        std::string source;
    };

    [[nodiscard]] std::string ToString(VulkanKernelProgram program);

    class VulkanKernelLibrary
    {
      public:
        [[nodiscard]] static VulkanKernelLibrary LoadDefault();

        [[nodiscard]] bool Loaded() const;
        [[nodiscard]] std::size_t LoadedCount() const;
        [[nodiscard]] std::size_t Size() const;
        [[nodiscard]] bool HasProgram(VulkanKernelProgram program) const;
        [[nodiscard]] const VulkanKernelManifestEntry*
        FindProgram(VulkanKernelProgram program) const;
        [[nodiscard]] const std::vector<VulkanKernelManifestEntry>& Entries() const;
        [[nodiscard]] const std::vector<RuntimeIssue>& Issues() const;
        [[nodiscard]] std::vector<VulkanKernelProgram>
        ResolveProgramsForPlan(const VulkanExecutionPlan& plan) const;

      private:
        std::vector<VulkanKernelManifestEntry> entries_;
        std::vector<RuntimeIssue> issues_;
    };

} // namespace us4::runtime::backends::vulkan
