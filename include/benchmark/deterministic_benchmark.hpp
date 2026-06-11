#pragma once

#include "stats/operational_stats.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mini_ats::benchmark {

struct BenchmarkEnvironment {
    std::string compiler{};
    std::string cpp_standard{};
    std::string build_mode{};
    std::string operating_system{};
    std::string architecture{};
    std::uint32_t hardware_threads{};
};

struct DeterministicBenchmarkResult {
    std::string scenario{"deterministic_gateway"};
    std::size_t iterations{};
    std::size_t commands_submitted{};
    std::chrono::nanoseconds elapsed{};
    BenchmarkEnvironment environment{};
    stats::OperationalStatisticsSnapshot stats{};
};

[[nodiscard]] BenchmarkEnvironment collect_benchmark_environment();
[[nodiscard]] DeterministicBenchmarkResult run_deterministic_gateway_benchmark(
    std::size_t iterations);
[[nodiscard]] std::string format_deterministic_benchmark_result(
    const DeterministicBenchmarkResult& result);

}  // namespace mini_ats::benchmark
