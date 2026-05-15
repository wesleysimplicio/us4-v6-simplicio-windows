#include "us4/runtime/backends/directml/dml_device.h"

#include <sstream>

namespace us4::runtime::backends::directml
{

    namespace
    {
        constexpr std::uint32_t kDirectMlPrimaryRank = 200;
        constexpr std::uint32_t kDirectMlIntegratedBias = 30;

        std::uint32_t ComputeContextHint(const HardwareCapabilities& capabilities)
        {
            constexpr std::size_t kGiB = 1024ULL * 1024ULL * 1024ULL;
            return capabilities.budget.deviceBytes >= 12ULL * kGiB ? 8192U : 4096U;
        }

        BackendVendor NormalizeVendor(const HardwareCapabilities& capabilities,
                                      const DmlDeviceOptions& options)
        {
            if (capabilities.primaryAdapterVendor != BackendVendor::kUnknown)
            {
                return capabilities.primaryAdapterVendor;
            }

            if (options.preferIntegratedGpu)
            {
                return BackendVendor::kIntel;
            }

            return BackendVendor::kMicrosoft;
        }
    } // namespace

    DmlTensorDataType ToDmlDataType(const PrecisionMode precision)
    {
        switch (precision)
        {
        case PrecisionMode::kFp32:
            return DmlTensorDataType::kFp32;
        case PrecisionMode::kFp16:
            return DmlTensorDataType::kFp16;
        case PrecisionMode::kBf16:
            return DmlTensorDataType::kBf16;
        case PrecisionMode::kInt8:
            return DmlTensorDataType::kInt8;
        }

        return DmlTensorDataType::kFp16;
    }

    bool IsPrecisionSupported(const DmlAdapterInfo& adapter, const DmlTensorDataType dataType)
    {
        switch (dataType)
        {
        case DmlTensorDataType::kFp32:
            return true;
        case DmlTensorDataType::kFp16:
            return adapter.supportsFp16;
        case DmlTensorDataType::kBf16:
            return adapter.supportsBf16;
        case DmlTensorDataType::kInt8:
        case DmlTensorDataType::kUInt8:
            return adapter.supportsInt8;
        }

        return false;
    }

    std::string ToString(const DmlQueuePriority priority)
    {
        switch (priority)
        {
        case DmlQueuePriority::kNormal:
            return "normal";
        case DmlQueuePriority::kHigh:
            return "high";
        case DmlQueuePriority::kGlobalRealtime:
            return "global-realtime";
        }

        return "unknown";
    }

    std::string ToString(const DmlTensorDataType dataType)
    {
        switch (dataType)
        {
        case DmlTensorDataType::kFp32:
            return "fp32";
        case DmlTensorDataType::kFp16:
            return "fp16";
        case DmlTensorDataType::kBf16:
            return "bf16";
        case DmlTensorDataType::kInt8:
            return "int8";
        case DmlTensorDataType::kUInt8:
            return "uint8";
        }

        return "unknown";
    }

    std::string ToString(const DmlOperatorKind kind)
    {
        switch (kind)
        {
        case DmlOperatorKind::kMatMul:
            return "matmul";
        case DmlOperatorKind::kAttention:
            return "attention";
        case DmlOperatorKind::kRmsNorm:
            return "rmsnorm";
        case DmlOperatorKind::kElementWise:
            return "element-wise";
        case DmlOperatorKind::kCustom:
            return "custom";
        }

        return "unknown";
    }

    std::string ToString(const DmlExecutionPhase phase)
    {
        switch (phase)
        {
        case DmlExecutionPhase::kPrefill:
            return "prefill";
        case DmlExecutionPhase::kDecode:
            return "decode";
        case DmlExecutionPhase::kValidation:
            return "validation";
        }

        return "unknown";
    }

    std::string ToString(const DmlGraphState state)
    {
        switch (state)
        {
        case DmlGraphState::kEmpty:
            return "empty";
        case DmlGraphState::kRecordable:
            return "recordable";
        case DmlGraphState::kCompiled:
            return "compiled";
        case DmlGraphState::kReady:
            return "ready";
        case DmlGraphState::kFaulted:
            return "faulted";
        }

        return "unknown";
    }

    DmlDevice::DmlDevice(DmlDeviceOptions options) : options_(options) {}

    bool DmlDevice::SupportsSelection(const HardwareCapabilities& capabilities)
    {
        return capabilities.hasDirectMl;
    }

    BackendDescriptor DmlDevice::BuildDescriptor(const HardwareCapabilities& capabilities)
    {
        const DmlAdapterInfo adapter = DescribeAdapter(capabilities);

        BackendDescriptor descriptor;
        descriptor.kind = BackendKind::kDirectML;
        descriptor.name = "directml";
        descriptor.displayName = "DirectML";
        descriptor.deviceClass = adapter.deviceClass;
        descriptor.vendor = adapter.vendor;
        descriptor.availability = capabilities.hasDirectMl ? BackendAvailability::kAvailable
                                                           : BackendAvailability::kUnavailable;
        descriptor.defaultPrecision =
            adapter.supportsFp16
                ? PrecisionMode::kFp16
                : (adapter.supportsBf16 ? PrecisionMode::kBf16 : PrecisionMode::kFp32);
        descriptor.selectionRank = kDirectMlPrimaryRank;
        if (adapter.deviceClass == DeviceClass::kIntegratedGpu)
        {
            descriptor.selectionRank += kDirectMlIntegratedBias;
        }
        descriptor.maxContextTokensHint = ComputeContextHint(capabilities);
        descriptor.supportsGraphCapture = adapter.supportsGraphCompilation;
        descriptor.supportsPagedKv = true;
        descriptor.supportsMoE = true;
        descriptor.supportsUnifiedMemory =
            capabilities.hasUnifiedMemory || adapter.deviceClass == DeviceClass::kIntegratedGpu;
        descriptor.budget = capabilities.budget;
        return descriptor;
    }

    DmlAdapterInfo DmlDevice::DescribeAdapter(const HardwareCapabilities& capabilities,
                                              const DmlDeviceOptions& options)
    {
        DmlAdapterInfo adapter;
        adapter.adapterName = capabilities.primaryAdapterName;
        adapter.vendor = NormalizeVendor(capabilities, options);
        adapter.deviceClass = capabilities.primaryAdapterClass == DeviceClass::kDiscreteGpu
                                  ? DeviceClass::kDiscreteGpu
                                  : DeviceClass::kIntegratedGpu;
        adapter.supportsFp16 = capabilities.hasDirectMl;
        adapter.supportsBf16 = capabilities.hasDirectMl &&
                               (capabilities.primaryAdapterVendor == BackendVendor::kIntel ||
                                capabilities.primaryAdapterVendor == BackendVendor::kAmd);
        adapter.supportsInt8 = capabilities.hasDirectMl;
        adapter.supportsGraphCompilation = capabilities.hasDirectMl;
        adapter.supportsDescriptorReuse = true;
        adapter.queueCount = options.preferLowLatency ? 2U : 1U;
        adapter.dedicatedBytes = capabilities.primaryAdapterClass == DeviceClass::kDiscreteGpu
                                     ? capabilities.budget.deviceBytes
                                     : 0;
        adapter.sharedBytes = capabilities.primaryAdapterClass == DeviceClass::kIntegratedGpu
                                  ? capabilities.budget.deviceBytes
                                  : capabilities.budget.hostBytes;
        return adapter;
    }

    bool DmlDevice::Initialize(const HardwareCapabilities& capabilities)
    {
        descriptor_ = BuildDescriptor(capabilities);
        adapter_ = DescribeAdapter(capabilities, options_);
        lastIssue_.reset();
        ready_ = false;

        if (!capabilities.hasDirectMl)
        {
            lastIssue_ = DmlIssue{
                .code = "directml-unavailable",
                .message = "DirectML is not available on this host.",
                .recoverable = true,
            };
            return false;
        }

        ready_ = true;
        submissionFence_ = 1;
        return true;
    }

    bool DmlDevice::PrepareGraphSession(std::string_view modelId, const DmlExecutionPhase phase,
                                        const std::size_t estimatedPersistentBytes)
    {
        if (!ready_)
        {
            lastIssue_ = DmlIssue{
                .code = "device-not-ready",
                .message = "Initialize the DirectML device before preparing a graph session.",
                .recoverable = true,
            };
            return false;
        }

        if (estimatedPersistentBytes > descriptor_.budget.deviceBytes &&
            descriptor_.budget.deviceBytes > 0)
        {
            lastIssue_ = DmlIssue{
                .code = "persistent-budget-exceeded",
                .message = "Estimated DirectML persistent allocation exceeds device budget.",
                .recoverable = true,
            };
            return false;
        }

        boundModelId_ = std::string(modelId);
        boundPhase_ = phase;
        ++submissionFence_;
        lastIssue_.reset();
        return true;
    }

    void DmlDevice::Reset()
    {
        boundModelId_.clear();
        boundPhase_ = DmlExecutionPhase::kDecode;
        submissionFence_ = 0;
        ready_ = false;
        lastIssue_.reset();
    }

    const DmlDeviceOptions& DmlDevice::Options() const
    {
        return options_;
    }

    const DmlAdapterInfo& DmlDevice::Adapter() const
    {
        return adapter_;
    }

    const BackendDescriptor& DmlDevice::Descriptor() const
    {
        return descriptor_;
    }

    bool DmlDevice::IsReady() const
    {
        return ready_;
    }

    std::uint64_t DmlDevice::SubmissionFence() const
    {
        return submissionFence_;
    }

    std::optional<DmlIssue> DmlDevice::LastIssue() const
    {
        return lastIssue_;
    }

    std::string DmlDevice::DescribeSession() const
    {
        std::ostringstream builder;
        builder << "backend=directml"
                << " adapter=\"" << adapter_.adapterName << "\""
                << " vendor=" << static_cast<int>(adapter_.vendor)
                << " phase=" << ToString(boundPhase_) << " model=\"" << boundModelId_ << "\""
                << " queue_priority=" << ToString(options_.queuePriority)
                << " tensor_reuse=" << (options_.allowTensorReuse ? "on" : "off")
                << " fence=" << submissionFence_;
        return builder.str();
    }

} // namespace us4::runtime::backends::directml
