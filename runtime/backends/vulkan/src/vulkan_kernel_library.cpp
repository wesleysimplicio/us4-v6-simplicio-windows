#include "us4/runtime/backends/vulkan/vulkan_kernel_library.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>

namespace us4::runtime::backends::vulkan
{

    namespace
    {
        std::vector<VulkanKernelManifestEntry> DefaultManifest()
        {
            return {
                VulkanKernelManifestEntry{
                    .program = VulkanKernelProgram::kMatMul,
                    .name = "matmul",
                    .relativePath = "matmul.comp",
                    .entryPoint = "main",
                    .workgroupX = 16,
                    .workgroupY = 16,
                    .workgroupZ = 1,
                },
                VulkanKernelManifestEntry{
                    .program = VulkanKernelProgram::kSoftmax,
                    .name = "softmax",
                    .relativePath = "softmax.comp",
                    .entryPoint = "main",
                    .workgroupX = 64,
                    .workgroupY = 1,
                    .workgroupZ = 1,
                },
                VulkanKernelManifestEntry{
                    .program = VulkanKernelProgram::kRmsNorm,
                    .name = "rmsnorm",
                    .relativePath = "rmsnorm.comp",
                    .entryPoint = "main",
                    .workgroupX = 64,
                    .workgroupY = 1,
                    .workgroupZ = 1,
                },
            };
        }

        std::vector<std::filesystem::path> ResolveSearchRoots()
        {
            std::vector<std::filesystem::path> roots;
#if defined(US4_VULKAN_KERNEL_SOURCE_DIR)
            roots.emplace_back(std::filesystem::path(US4_VULKAN_KERNEL_SOURCE_DIR));
#endif
            roots.emplace_back(std::filesystem::current_path() / "runtime" / "backends" / "vulkan" /
                               "kernels");
            roots.emplace_back(std::filesystem::current_path() / "build" / "runtime" / "backends" /
                               "vulkan" / "kernels");
            return roots;
        }

        std::optional<std::string> ReadTextFile(const std::filesystem::path& path)
        {
            std::ifstream input(path, std::ios::binary);
            if (!input.is_open())
            {
                return std::nullopt;
            }

            return std::string(std::istreambuf_iterator<char>(input),
                               std::istreambuf_iterator<char>());
        }
    } // namespace

    std::string ToString(const VulkanKernelProgram program)
    {
        switch (program)
        {
        case VulkanKernelProgram::kMatMul:
            return "matmul";
        case VulkanKernelProgram::kSoftmax:
            return "softmax";
        case VulkanKernelProgram::kRmsNorm:
            return "rmsnorm";
        }

        return "unknown";
    }

    VulkanKernelLibrary VulkanKernelLibrary::LoadDefault()
    {
        VulkanKernelLibrary library;
        library.entries_ = DefaultManifest();
        const auto searchRoots = ResolveSearchRoots();

        for (auto& entry : library.entries_)
        {
            bool loaded = false;
            for (const auto& root : searchRoots)
            {
                const auto candidate = root / entry.relativePath;
                const auto source = ReadTextFile(candidate);
                if (!source.has_value())
                {
                    continue;
                }

                entry.source = *source;
                loaded = true;
                break;
            }

            if (!loaded)
            {
                library.issues_.push_back(
                    {"vulkan.kernel.missing." + entry.name,
                     "Could not locate the Vulkan kernel source for `" + entry.name + "`.", false});
            }
        }

        return library;
    }

    bool VulkanKernelLibrary::Loaded() const
    {
        return std::all_of(entries_.begin(), entries_.end(),
                           [](const auto& entry) { return !entry.source.empty(); });
    }

    std::size_t VulkanKernelLibrary::LoadedCount() const
    {
        return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
                                                      [](const auto& entry)
                                                      { return !entry.source.empty(); }));
    }

    std::size_t VulkanKernelLibrary::Size() const
    {
        return entries_.size();
    }

    bool VulkanKernelLibrary::HasProgram(const VulkanKernelProgram program) const
    {
        return FindProgram(program) != nullptr;
    }

    const VulkanKernelManifestEntry*
    VulkanKernelLibrary::FindProgram(const VulkanKernelProgram program) const
    {
        const auto it = std::find_if(entries_.begin(), entries_.end(), [program](const auto& entry)
                                     { return entry.program == program; });
        return it == entries_.end() ? nullptr : &(*it);
    }

    const std::vector<VulkanKernelManifestEntry>& VulkanKernelLibrary::Entries() const
    {
        return entries_;
    }

    const std::vector<RuntimeIssue>& VulkanKernelLibrary::Issues() const
    {
        return issues_;
    }

    std::vector<VulkanKernelProgram>
    VulkanKernelLibrary::ResolveProgramsForPlan(const VulkanExecutionPlan& plan) const
    {
        std::set<VulkanKernelProgram> required;
        for (const auto& step : plan.steps)
        {
            if (step.stage == VulkanKernelStage::kPrefill ||
                step.stage == VulkanKernelStage::kDecode ||
                step.stage == VulkanKernelStage::kSpeculativeDraft)
            {
                required.insert(VulkanKernelProgram::kMatMul);
                required.insert(VulkanKernelProgram::kSoftmax);
                required.insert(VulkanKernelProgram::kRmsNorm);
            }
        }

        return std::vector<VulkanKernelProgram>(required.begin(), required.end());
    }

} // namespace us4::runtime::backends::vulkan
