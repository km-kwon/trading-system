#pragma once

#include "domain/trade.hpp"
#include "engine/matching_engine.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace mini_ats::stats {

using Notional = std::int64_t;

struct ExactVwap {
    Notional notional{};
    domain::Quantity quantity{};

    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] domain::Price floor_price() const noexcept;
};

struct TradeStatisticsSnapshot {
    std::uint64_t trade_count{};
    domain::Quantity traded_quantity{};
    Notional traded_notional{};
    std::optional<ExactVwap> vwap{};

    [[nodiscard]] bool empty() const noexcept;
};

struct CommandStatisticsSnapshot {
    std::uint64_t received{};
    std::uint64_t accepted{};
    std::uint64_t rejected{};

    [[nodiscard]] bool empty() const noexcept;
};

struct LatencyStatisticsSnapshot {
    std::uint64_t sample_count{};
    std::chrono::nanoseconds min{};
    std::chrono::nanoseconds max{};
    std::chrono::nanoseconds p50{};
    std::chrono::nanoseconds p95{};
    std::chrono::nanoseconds p99{};

    [[nodiscard]] bool empty() const noexcept;
};

struct OperationalStatisticsSnapshot {
    CommandStatisticsSnapshot commands{};
    TradeStatisticsSnapshot trades{};
    LatencyStatisticsSnapshot latency{};

    [[nodiscard]] bool empty() const noexcept;
};

class OperationalStatistics {
public:
    void reset() noexcept;

    void record_trade(const domain::Trade& trade);
    void record_trades(std::span<const domain::Trade> trades);
    void record_submit_result(const engine::SubmitOrderResult& result);
    void record_cancel_result(const engine::CancelOrderResult& result) noexcept;
    void record_command(bool accepted, std::chrono::nanoseconds latency);
    void record_command_result(bool accepted,
                               std::span<const domain::Trade> trades,
                               std::chrono::nanoseconds latency);

    [[nodiscard]] OperationalStatisticsSnapshot snapshot() const;

private:
    CommandStatisticsSnapshot commands_{};
    std::uint64_t trade_count_{};
    domain::Quantity traded_quantity_{};
    Notional traded_notional_{};
    std::vector<std::chrono::nanoseconds> latency_samples_{};
};

[[nodiscard]] std::string format_operational_statistics(
    const OperationalStatisticsSnapshot& snapshot);

}  // namespace mini_ats::stats
