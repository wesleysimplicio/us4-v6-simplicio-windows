#include "us4/runtime/benchmarks/benchmark_registry.h"

#include <iostream>

int main()
{
    for (const auto& benchmark :
         us4::runtime::benchmarks::BenchmarkRegistry::CasesForBackend("cuda"))
    {
        if (benchmark.name == "moe_throughput")
        {
            std::cout << benchmark.name << " " << benchmark.backend << " " << benchmark.scenario
                      << '\n';
        }
    }
    return 0;
}
