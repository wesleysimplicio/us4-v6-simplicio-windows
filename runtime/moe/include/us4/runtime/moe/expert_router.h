#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace us4::runtime::moe
{

    struct RoutingPolicy
    {
        std::size_t topK = 2;
        bool preferDeterministic = true;
        bool allowCrossDeviceExperts = false;
    };

    struct ExpertRoute
    {
        std::size_t expertId = 0;
        float score = 0.0F;
        std::string placement = "device";
    };

    struct RoutingStats
    {
        float entropy = 0.0F;
        float loadBalanceLoss = 0.0F;
        std::size_t deviceRouteCount = 0;
        std::size_t hostRouteCount = 0;
    };

    class ExpertRouter
    {
      public:
        [[nodiscard]] std::vector<ExpertRoute> BuildPlan(const std::string& tokenWindow,
                                                         const RoutingPolicy& policy) const;
        [[nodiscard]] std::vector<ExpertRoute> SelectTopK(const std::string& tokenWindow,
                                                          std::size_t k) const;
        [[nodiscard]] RoutingStats Evaluate(const std::vector<ExpertRoute>& routes) const;
    };

} // namespace us4::runtime::moe
