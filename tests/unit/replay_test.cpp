#include "replay/replay_log.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using mini_ats::domain::CancelRequest;
using mini_ats::domain::InstrumentId;
using mini_ats::domain::InstrumentReference;
using mini_ats::domain::MarketSession;
using mini_ats::domain::Order;
using mini_ats::domain::OrderId;
using mini_ats::domain::OrderStatus;
using mini_ats::domain::OrderType;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::TimeInForce;
using mini_ats::engine::MatchingEngine;
using mini_ats::replay::ReplayCommandType;
using mini_ats::replay::ReplayEvent;
using mini_ats::replay::ReplayEventStatus;
using mini_ats::replay::apply_replay_event;
using mini_ats::replay::replay_events;

Order make_limit_order(OrderId id,
                       Side side,
                       Price price,
                       Quantity quantity,
                       SequenceNumber sequence,
                       InstrumentId instrument_id = InstrumentId{1001}) {
    return Order{
        .id = id,
        .instrument_id = instrument_id,
        .side = side,
        .type = OrderType::Limit,
        .time_in_force = TimeInForce::Day,
        .price = price,
        .quantity = quantity,
        .sequence = sequence,
    };
}

InstrumentReference make_instrument(SequenceNumber version = SequenceNumber{7}) {
    return InstrumentReference{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = version,
    };
}

TEST(ReplayTest, ReplayEventStoresSubmitOrderMetadata) {
    const auto order = make_limit_order(OrderId{1}, Side::Buy, Price{73500}, Quantity{10},
                                        SequenceNumber{11});
    const auto event = ReplayEvent::submit_order(SequenceNumber{11}, SequenceNumber{7}, order);

    EXPECT_EQ(event.type(), ReplayCommandType::SubmitOrder);
    EXPECT_EQ(event.input_sequence, SequenceNumber{11});
    EXPECT_EQ(event.reference_version, SequenceNumber{7});
    EXPECT_EQ(event.instrument_id(), InstrumentId{1001});
    EXPECT_EQ(event.command_sequence(), SequenceNumber{11});
    EXPECT_TRUE(event.has_valid_input_sequence());
    EXPECT_TRUE(event.has_valid_reference_version());
    EXPECT_TRUE(event.has_matching_command_sequence());
}

TEST(ReplayTest, AppliesSubmitAndCancelEventsInOrder) {
    MatchingEngine engine{make_instrument()};

    const std::vector<ReplayEvent> events{
        ReplayEvent::submit_order(
            SequenceNumber{1}, SequenceNumber{7},
            make_limit_order(OrderId{10}, Side::Buy, Price{73500}, Quantity{10},
                             SequenceNumber{1})),
        ReplayEvent::cancel_order(
            SequenceNumber{2}, SequenceNumber{7},
            CancelRequest{
                .order_id = OrderId{10},
                .instrument_id = InstrumentId{1001},
                .sequence = SequenceNumber{2},
            }),
    };

    const auto result = replay_events(engine, events);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.applied_count(), 2U);
    EXPECT_EQ(result.steps.size(), 2U);
    ASSERT_EQ(result.reports.size(), 2U);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Accepted);
    EXPECT_EQ(result.reports[1].status, OrderStatus::Canceled);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(ReplayTest, RejectsReferenceVersionMismatchWithoutMutatingBook) {
    MatchingEngine engine{make_instrument(SequenceNumber{7})};

    const auto event = ReplayEvent::submit_order(
        SequenceNumber{1}, SequenceNumber{6},
        make_limit_order(OrderId{20}, Side::Buy, Price{73500}, Quantity{10}, SequenceNumber{1}));

    const auto result = apply_replay_event(engine, event);

    EXPECT_FALSE(result.applied());
    EXPECT_EQ(result.status, ReplayEventStatus::ReferenceVersionMismatch);
    EXPECT_TRUE(result.trades.empty());
    EXPECT_TRUE(result.reports.empty());
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(ReplayTest, RejectsCommandSequenceMismatchWithoutMutatingBook) {
    MatchingEngine engine{make_instrument()};

    const auto event = ReplayEvent::submit_order(
        SequenceNumber{99}, SequenceNumber{7},
        make_limit_order(OrderId{30}, Side::Buy, Price{73500}, Quantity{10}, SequenceNumber{1}));

    const auto result = apply_replay_event(engine, event);

    EXPECT_FALSE(result.applied());
    EXPECT_EQ(result.status, ReplayEventStatus::CommandSequenceMismatch);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(ReplayTest, ReplaysSameInputStreamDeterministically) {
    const std::vector<ReplayEvent> events{
        ReplayEvent::submit_order(
            SequenceNumber{1}, SequenceNumber{7},
            make_limit_order(OrderId{40}, Side::Sell, Price{73700}, Quantity{3},
                             SequenceNumber{1})),
        ReplayEvent::submit_order(
            SequenceNumber{2}, SequenceNumber{7},
            make_limit_order(OrderId{41}, Side::Sell, Price{73800}, Quantity{4},
                             SequenceNumber{2})),
        ReplayEvent::submit_order(
            SequenceNumber{3}, SequenceNumber{7},
            make_limit_order(OrderId{42}, Side::Buy, Price{73800}, Quantity{10},
                             SequenceNumber{3})),
    };

    MatchingEngine first_engine{make_instrument()};
    MatchingEngine second_engine{make_instrument()};

    const auto first = replay_events(first_engine, events);
    const auto second = replay_events(second_engine, events);

    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());
    ASSERT_EQ(first.trades.size(), 2U);
    ASSERT_EQ(second.trades.size(), 2U);
    EXPECT_EQ(first.trades[0].price, second.trades[0].price);
    EXPECT_EQ(first.trades[0].quantity, second.trades[0].quantity);
    EXPECT_EQ(first.trades[1].price, second.trades[1].price);
    EXPECT_EQ(first.trades[1].quantity, second.trades[1].quantity);
    EXPECT_EQ(first.reports.size(), second.reports.size());

    const auto first_snapshot = first_engine.order_book().snapshot();
    const auto second_snapshot = second_engine.order_book().snapshot();
    ASSERT_EQ(first_snapshot.bids.size(), 1U);
    ASSERT_EQ(second_snapshot.bids.size(), 1U);
    EXPECT_EQ(first_snapshot.bids[0].price, second_snapshot.bids[0].price);
    EXPECT_EQ(first_snapshot.bids[0].total_quantity, Quantity{3});
    EXPECT_EQ(second_snapshot.bids[0].total_quantity, Quantity{3});
}

}  // namespace
