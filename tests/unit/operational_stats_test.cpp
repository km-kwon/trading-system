#include "stats/operational_stats.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

namespace {

using mini_ats::domain::InstrumentId;
using mini_ats::domain::Order;
using mini_ats::domain::OrderId;
using mini_ats::domain::OrderType;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::TimeInForce;
using mini_ats::domain::Trade;
using mini_ats::engine::MatchingEngine;
using mini_ats::stats::OperationalStatistics;
using mini_ats::stats::format_operational_statistics;

Order make_order(OrderId id,
                 Side side,
                 Price price,
                 Quantity quantity,
                 SequenceNumber sequence) {
    return Order{
        .id = id,
        .instrument_id = InstrumentId{1001},
        .side = side,
        .type = OrderType::Limit,
        .time_in_force = TimeInForce::Day,
        .price = price,
        .quantity = quantity,
        .sequence = sequence,
    };
}

Trade make_trade(Price price, Quantity quantity, SequenceNumber sequence) {
    return Trade{
        .id = sequence,
        .instrument_id = InstrumentId{1001},
        .resting_order_id = OrderId{10 + sequence},
        .incoming_order_id = OrderId{20 + sequence},
        .aggressor_side = Side::Buy,
        .price = price,
        .quantity = quantity,
        .sequence = sequence,
    };
}

}  // namespace

TEST(OperationalStatsTest, AccumulatesTradeVolumeNotionalAndExactVwapFromEngineResults) {
    MatchingEngine engine{InstrumentId{1001}};
    OperationalStatistics stats{};

    const auto resting_100 = engine.submit_order(
        make_order(OrderId{10}, Side::Sell, Price{100}, Quantity{10}, SequenceNumber{1}));
    const auto resting_110 = engine.submit_order(
        make_order(OrderId{11}, Side::Sell, Price{110}, Quantity{4}, SequenceNumber{2}));
    const auto incoming = engine.submit_order(
        make_order(OrderId{20}, Side::Buy, Price{110}, Quantity{14}, SequenceNumber{3}));

    stats.record_submit_result(resting_100);
    stats.record_submit_result(resting_110);
    stats.record_submit_result(incoming);

    const auto snapshot = stats.snapshot();
    EXPECT_EQ(snapshot.trades.trade_count, 2U);
    EXPECT_EQ(snapshot.trades.traded_quantity, Quantity{14});
    EXPECT_EQ(snapshot.trades.traded_notional, 1440);
    ASSERT_TRUE(snapshot.trades.vwap.has_value());
    EXPECT_EQ(snapshot.trades.vwap->notional, 1440);
    EXPECT_EQ(snapshot.trades.vwap->quantity, Quantity{14});
    EXPECT_EQ(snapshot.trades.vwap->floor_price(), Price{102});
    EXPECT_TRUE(snapshot.commands.empty());
    EXPECT_TRUE(snapshot.latency.empty());
}

TEST(OperationalStatsTest, RecordsCommandCountsLatencyPercentilesAndAcceptedTrades) {
    using namespace std::chrono_literals;

    OperationalStatistics stats{};
    const std::vector<Trade> accepted_trades{
        make_trade(Price{100}, Quantity{3}, SequenceNumber{1}),
        make_trade(Price{105}, Quantity{2}, SequenceNumber{2}),
    };
    const std::vector<Trade> rejected_trades{
        make_trade(Price{200}, Quantity{99}, SequenceNumber{3}),
    };

    stats.record_command_result(true, accepted_trades, 40us);
    stats.record_command_result(false, rejected_trades, 10us);
    stats.record_command(true, 30us);
    stats.record_command(false, 20us);
    stats.record_command(true, 50us);

    const auto snapshot = stats.snapshot();
    EXPECT_EQ(snapshot.commands.received, 5U);
    EXPECT_EQ(snapshot.commands.accepted, 3U);
    EXPECT_EQ(snapshot.commands.rejected, 2U);

    EXPECT_EQ(snapshot.trades.trade_count, 2U);
    EXPECT_EQ(snapshot.trades.traded_quantity, Quantity{5});
    EXPECT_EQ(snapshot.trades.traded_notional, 510);

    EXPECT_EQ(snapshot.latency.sample_count, 5U);
    EXPECT_EQ(snapshot.latency.min, 10us);
    EXPECT_EQ(snapshot.latency.max, 50us);
    EXPECT_EQ(snapshot.latency.p50, 30us);
    EXPECT_EQ(snapshot.latency.p95, 50us);
    EXPECT_EQ(snapshot.latency.p99, 50us);

    EXPECT_EQ(format_operational_statistics(snapshot),
              "STATS commands_received=5 commands_accepted=3 commands_rejected=2 "
              "trades=2 traded_quantity=5 traded_notional=510 vwap_notional=510 "
              "vwap_quantity=5 vwap_floor_price=102 latency_samples=5 "
              "latency_min_ns=10000 latency_max_ns=50000 latency_p50_ns=30000 "
              "latency_p95_ns=50000 latency_p99_ns=50000");
}

TEST(OperationalStatsTest, IgnoresInvalidTradesClampsNegativeLatencyAndResets) {
    using namespace std::chrono_literals;

    OperationalStatistics stats{};
    const std::vector<Trade> trades{
        make_trade(Price{0}, Quantity{5}, SequenceNumber{1}),
        make_trade(Price{100}, Quantity{0}, SequenceNumber{2}),
        make_trade(Price{100}, Quantity{4}, SequenceNumber{3}),
    };

    stats.record_trades(trades);
    stats.record_command(false, -5ns);

    auto snapshot = stats.snapshot();
    EXPECT_EQ(snapshot.trades.trade_count, 1U);
    EXPECT_EQ(snapshot.trades.traded_quantity, Quantity{4});
    EXPECT_EQ(snapshot.trades.traded_notional, 400);
    EXPECT_EQ(snapshot.latency.min, 0ns);
    EXPECT_EQ(snapshot.latency.p99, 0ns);

    stats.reset();
    snapshot = stats.snapshot();
    EXPECT_TRUE(snapshot.empty());
    EXPECT_EQ(format_operational_statistics(snapshot),
              "STATS commands_received=0 commands_accepted=0 commands_rejected=0 "
              "trades=0 traded_quantity=0 traded_notional=0 vwap=NONE "
              "latency_samples=0 latency_min_ns=0 latency_max_ns=0 latency_p50_ns=0 "
              "latency_p95_ns=0 latency_p99_ns=0");
}
