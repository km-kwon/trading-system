#include "benchmark/deterministic_benchmark.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using mini_ats::benchmark::collect_benchmark_environment;
using mini_ats::benchmark::format_deterministic_benchmark_result;
using mini_ats::benchmark::run_deterministic_gateway_benchmark;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;

}  // namespace

TEST(DeterministicBenchmarkTest, RunsFixedGatewayScenarioAndCollectsStats) {
    const auto result = run_deterministic_gateway_benchmark(2);

    EXPECT_EQ(result.scenario, "deterministic_gateway");
    EXPECT_EQ(result.iterations, 2U);
    EXPECT_EQ(result.commands_submitted, 6U);
    EXPECT_EQ(result.stats.commands.received, 6U);
    EXPECT_EQ(result.stats.commands.accepted, 4U);
    EXPECT_EQ(result.stats.commands.rejected, 2U);
    EXPECT_EQ(result.stats.trades.trade_count, 2U);
    EXPECT_EQ(result.stats.trades.traded_quantity, Quantity{6});
    EXPECT_EQ(result.stats.trades.traded_notional, 442200);
    ASSERT_TRUE(result.stats.trades.vwap.has_value());
    EXPECT_EQ(result.stats.trades.vwap->floor_price(), Price{73700});
    EXPECT_EQ(result.stats.latency.sample_count, 6U);
    EXPECT_FALSE(result.environment.compiler.empty());
    EXPECT_FALSE(result.environment.cpp_standard.empty());
    EXPECT_FALSE(result.environment.build_mode.empty());
    EXPECT_FALSE(result.environment.operating_system.empty());
    EXPECT_FALSE(result.environment.architecture.empty());
}

TEST(DeterministicBenchmarkTest, CollectsBenchmarkEnvironmentMetadata) {
    const auto environment = collect_benchmark_environment();

    EXPECT_FALSE(environment.compiler.empty());
    EXPECT_FALSE(environment.cpp_standard.empty());
    EXPECT_FALSE(environment.build_mode.empty());
    EXPECT_FALSE(environment.operating_system.empty());
    EXPECT_FALSE(environment.architecture.empty());
}

TEST(DeterministicBenchmarkTest, FormatsBenchmarkSummaryWithStatsPayload) {
    const auto result = run_deterministic_gateway_benchmark(1);
    const auto text = format_deterministic_benchmark_result(result);

    EXPECT_NE(text.find("BENCHMARK scenario=deterministic_gateway iterations=1 commands=3"),
              std::string::npos);
    EXPECT_NE(text.find("commands_per_second_floor="), std::string::npos);
    EXPECT_NE(text.find("compiler="), std::string::npos);
    EXPECT_NE(text.find("cpp_standard="), std::string::npos);
    EXPECT_NE(text.find("build_mode="), std::string::npos);
    EXPECT_NE(text.find("os="), std::string::npos);
    EXPECT_NE(text.find("architecture="), std::string::npos);
    EXPECT_NE(text.find("hardware_threads="), std::string::npos);
    EXPECT_NE(text.find("STATS commands_received=3 commands_accepted=2 commands_rejected=1"),
              std::string::npos);
    EXPECT_NE(text.find("trades=1 traded_quantity=3 traded_notional=221100"),
              std::string::npos);
    EXPECT_NE(text.find("vwap_floor_price=73700"), std::string::npos);
}
