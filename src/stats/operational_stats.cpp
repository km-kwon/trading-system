#include "stats/operational_stats.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <sstream>

namespace mini_ats::stats {

namespace {

[[nodiscard]] std::chrono::nanoseconds non_negative(
    std::chrono::nanoseconds latency) noexcept {
    return latency < std::chrono::nanoseconds::zero() ? std::chrono::nanoseconds::zero()
                                                      : latency;
}

[[nodiscard]] std::chrono::nanoseconds percentile(
    const std::vector<std::chrono::nanoseconds>& sorted_samples,
    std::uint64_t numerator,
    std::uint64_t denominator) {
    if (sorted_samples.empty()) {
        return std::chrono::nanoseconds::zero();
    }

    const std::uint64_t sample_count = sorted_samples.size();
    std::uint64_t rank = (sample_count * numerator + denominator - 1) / denominator;
    if (rank == 0) {
        rank = 1;
    }

    const std::size_t index = static_cast<std::size_t>(
        std::min<std::uint64_t>(rank - 1, sample_count - 1));
    return sorted_samples[index];
}

[[nodiscard]] LatencyStatisticsSnapshot make_latency_snapshot(
    const std::vector<std::chrono::nanoseconds>& samples) {
    if (samples.empty()) {
        return {};
    }

    auto sorted_samples = samples;
    std::sort(sorted_samples.begin(), sorted_samples.end());

    return LatencyStatisticsSnapshot{
        .sample_count = sorted_samples.size(),
        .min = sorted_samples.front(),
        .max = sorted_samples.back(),
        .p50 = percentile(sorted_samples, 50, 100),
        .p95 = percentile(sorted_samples, 95, 100),
        .p99 = percentile(sorted_samples, 99, 100),
    };
}

}  // namespace

bool ExactVwap::empty() const noexcept {
    return quantity <= 0;
}

domain::Price ExactVwap::floor_price() const noexcept {
    return empty() ? domain::Price{0} : static_cast<domain::Price>(notional / quantity);
}

bool TradeStatisticsSnapshot::empty() const noexcept {
    return trade_count == 0 || traded_quantity <= 0;
}

bool CommandStatisticsSnapshot::empty() const noexcept {
    return received == 0;
}

bool LatencyStatisticsSnapshot::empty() const noexcept {
    return sample_count == 0;
}

bool OperationalStatisticsSnapshot::empty() const noexcept {
    return commands.empty() && trades.empty() && latency.empty();
}

void OperationalStatistics::reset() noexcept {
    commands_ = {};
    trade_count_ = 0;
    traded_quantity_ = 0;
    traded_notional_ = 0;
    latency_samples_.clear();
}

void OperationalStatistics::record_trade(const domain::Trade& trade) {
    if (!trade.has_valid_price() || !trade.has_valid_quantity()) {
        return;
    }

    ++trade_count_;
    traded_quantity_ += trade.quantity;
    traded_notional_ += static_cast<Notional>(trade.notional());
}

void OperationalStatistics::record_trades(std::span<const domain::Trade> trades) {
    for (const auto& trade : trades) {
        record_trade(trade);
    }
}

void OperationalStatistics::record_submit_result(const engine::SubmitOrderResult& result) {
    record_trades(result.trades);
}

void OperationalStatistics::record_cancel_result(
    const engine::CancelOrderResult& result) noexcept {
    (void)result;
}

void OperationalStatistics::record_command(bool accepted,
                                           std::chrono::nanoseconds latency) {
    ++commands_.received;
    if (accepted) {
        ++commands_.accepted;
    } else {
        ++commands_.rejected;
    }

    latency_samples_.push_back(non_negative(latency));
}

void OperationalStatistics::record_command_result(
    bool accepted,
    std::span<const domain::Trade> trades,
    std::chrono::nanoseconds latency) {
    record_command(accepted, latency);
    if (accepted) {
        record_trades(trades);
    }
}

OperationalStatisticsSnapshot OperationalStatistics::snapshot() const {
    TradeStatisticsSnapshot trade_snapshot{
        .trade_count = trade_count_,
        .traded_quantity = traded_quantity_,
        .traded_notional = traded_notional_,
        .vwap = std::nullopt,
    };
    if (!trade_snapshot.empty()) {
        trade_snapshot.vwap = ExactVwap{
            .notional = traded_notional_,
            .quantity = traded_quantity_,
        };
    }

    return OperationalStatisticsSnapshot{
        .commands = commands_,
        .trades = trade_snapshot,
        .latency = make_latency_snapshot(latency_samples_),
    };
}

std::string format_operational_statistics(const OperationalStatisticsSnapshot& snapshot) {
    std::ostringstream output;
    output << "STATS"
           << " commands_received=" << snapshot.commands.received
           << " commands_accepted=" << snapshot.commands.accepted
           << " commands_rejected=" << snapshot.commands.rejected
           << " trades=" << snapshot.trades.trade_count
           << " traded_quantity=" << snapshot.trades.traded_quantity
           << " traded_notional=" << snapshot.trades.traded_notional;

    if (snapshot.trades.vwap.has_value()) {
        output << " vwap_notional=" << snapshot.trades.vwap->notional
               << " vwap_quantity=" << snapshot.trades.vwap->quantity
               << " vwap_floor_price=" << snapshot.trades.vwap->floor_price();
    } else {
        output << " vwap=NONE";
    }

    output << " latency_samples=" << snapshot.latency.sample_count
           << " latency_min_ns=" << snapshot.latency.min.count()
           << " latency_max_ns=" << snapshot.latency.max.count()
           << " latency_p50_ns=" << snapshot.latency.p50.count()
           << " latency_p95_ns=" << snapshot.latency.p95.count()
           << " latency_p99_ns=" << snapshot.latency.p99.count();

    return output.str();
}

}  // namespace mini_ats::stats
