#include "benchmark/deterministic_benchmark.hpp"

#include "domain/instrument.hpp"
#include "gateway/order_gateway.hpp"

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <thread>

namespace mini_ats::benchmark {

namespace {

[[nodiscard]] std::string compiler_text() {
#if defined(__clang__)
    std::ostringstream output;
    output << "clang-" << __clang_major__ << '.' << __clang_minor__ << '.'
           << __clang_patchlevel__;
    return output.str();
#elif defined(__GNUC__)
    std::ostringstream output;
    output << "gcc-" << __GNUC__ << '.' << __GNUC_MINOR__ << '.' << __GNUC_PATCHLEVEL__;
    return output.str();
#elif defined(_MSC_VER)
    std::ostringstream output;
    output << "msvc-" << _MSC_VER;
    return output.str();
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string cpp_standard_text() {
    std::ostringstream output;
    output << __cplusplus;
    return output.str();
}

[[nodiscard]] std::string build_mode_text() {
#ifdef NDEBUG
    return "release";
#else
    return "debug";
#endif
}

[[nodiscard]] std::string operating_system_text() {
#if defined(__linux__)
    return "linux";
#elif defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "apple";
#else
    return "unknown";
#endif
}

[[nodiscard]] std::string architecture_text() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

[[nodiscard]] std::uint64_t commands_per_second_floor(
    const DeterministicBenchmarkResult& result) noexcept {
    if (result.commands_submitted == 0 || result.elapsed.count() <= 0) {
        return 0;
    }

    const auto commands = static_cast<long double>(result.commands_submitted);
    const auto nanoseconds = static_cast<long double>(result.elapsed.count());
    return static_cast<std::uint64_t>((commands * 1'000'000'000.0L) / nanoseconds);
}

[[nodiscard]] domain::InstrumentReference benchmark_instrument() noexcept {
    return domain::InstrumentReference{
        .id = domain::InstrumentId{1001},
        .tick_size = domain::Price{5},
        .lower_price_limit = domain::Price{70000},
        .upper_price_limit = domain::Price{80000},
        .session = domain::MarketSession::Open,
        .version = domain::SequenceNumber{7},
    };
}

[[nodiscard]] std::string submit_command(domain::SequenceNumber sequence,
                                         domain::OrderId order_id,
                                         const char* side,
                                         domain::Price price,
                                         domain::Quantity quantity) {
    std::ostringstream output;
    output << "SUBMIT seq=" << sequence
           << " ref=7"
           << " order_id=" << order_id
           << " instrument_id=1001"
           << " side=" << side
           << " type=LIMIT"
           << " tif=DAY"
           << " price=" << price
           << " quantity=" << quantity;
    return output.str();
}

void run_command(engine::MatchingEngine& engine,
                 stats::OperationalStatistics& stats,
                 const std::string& command) {
    const auto started_at = std::chrono::steady_clock::now();
    const auto response = gateway::handle_text_command(engine, command);
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started_at);
    stats.record_command_result(response.accepted(), response.trades, elapsed);
}

}  // namespace

BenchmarkEnvironment collect_benchmark_environment() {
    return BenchmarkEnvironment{
        .compiler = compiler_text(),
        .cpp_standard = cpp_standard_text(),
        .build_mode = build_mode_text(),
        .operating_system = operating_system_text(),
        .architecture = architecture_text(),
        .hardware_threads = std::thread::hardware_concurrency(),
    };
}

DeterministicBenchmarkResult run_deterministic_gateway_benchmark(std::size_t iterations) {
    engine::MatchingEngine engine{benchmark_instrument()};
    stats::OperationalStatistics stats{};

    const auto started_at = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < iterations; ++index) {
        const auto base_sequence = static_cast<domain::SequenceNumber>(index * 3 + 1);
        const auto base_order_id = static_cast<domain::OrderId>(index * 10 + 1000);

        run_command(engine, stats,
                    submit_command(base_sequence, base_order_id, "SELL",
                                   domain::Price{73700}, domain::Quantity{3}));
        run_command(engine, stats,
                    submit_command(base_sequence + 1, base_order_id + 1, "BUY",
                                   domain::Price{73700}, domain::Quantity{3}));
        run_command(engine, stats,
                    submit_command(base_sequence + 2, base_order_id + 2, "BUY",
                                   domain::Price{73702}, domain::Quantity{1}));
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started_at);

    return DeterministicBenchmarkResult{
        .scenario = "deterministic_gateway",
        .iterations = iterations,
        .commands_submitted = iterations * 3,
        .elapsed = elapsed,
        .environment = collect_benchmark_environment(),
        .stats = stats.snapshot(),
    };
}

std::string format_deterministic_benchmark_result(
    const DeterministicBenchmarkResult& result) {
    std::ostringstream output;
    output << "BENCHMARK"
           << " scenario=" << result.scenario
           << " iterations=" << result.iterations
           << " commands=" << result.commands_submitted
           << " elapsed_ns=" << result.elapsed.count()
           << " commands_per_second_floor=" << commands_per_second_floor(result)
           << " compiler=" << result.environment.compiler
           << " cpp_standard=" << result.environment.cpp_standard
           << " build_mode=" << result.environment.build_mode
           << " os=" << result.environment.operating_system
           << " architecture=" << result.environment.architecture
           << " hardware_threads=" << result.environment.hardware_threads
           << ' ' << stats::format_operational_statistics(result.stats);
    return output.str();
}

}  // namespace mini_ats::benchmark
