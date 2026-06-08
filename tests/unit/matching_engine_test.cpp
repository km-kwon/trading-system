#include "engine/matching_engine.hpp"

#include <gtest/gtest.h>

namespace {

using mini_ats::domain::ExecutionType;
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
using mini_ats::domain::RejectReason;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::TimeInForce;
using mini_ats::engine::MatchingEngine;

Order make_limit_order(OrderId id,
                       Side side,
                       Price price,
                       Quantity quantity,
                       SequenceNumber sequence,
                       TimeInForce time_in_force = TimeInForce::Day,
                       InstrumentId instrument_id = InstrumentId{1001}) {
    return Order{
        .id = id,
        .instrument_id = instrument_id,
        .side = side,
        .type = OrderType::Limit,
        .time_in_force = time_in_force,
        .price = price,
        .quantity = quantity,
        .sequence = sequence,
    };
}

Order make_market_order(OrderId id,
                        Side side,
                        Quantity quantity,
                        SequenceNumber sequence,
                        TimeInForce time_in_force = TimeInForce::IOC,
                        InstrumentId instrument_id = InstrumentId{1001}) {
    return Order{
        .id = id,
        .instrument_id = instrument_id,
        .side = side,
        .type = OrderType::Market,
        .time_in_force = time_in_force,
        .price = Price{0},
        .quantity = quantity,
        .sequence = sequence,
    };
}

TEST(MatchingEngineTest, NonCrossingLimitOrderRestsInBook) {
    MatchingEngine engine{InstrumentId{1001}};

    const auto result = engine.submit_order(
        make_limit_order(OrderId{1}, Side::Buy, Price{73500}, Quantity{10}, SequenceNumber{1}));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].order_id, OrderId{1});
    EXPECT_EQ(result.reports[0].type, ExecutionType::Accepted);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Accepted);
    EXPECT_EQ(result.reports[0].remaining_quantity, Quantity{10});

    const auto* resting_order = engine.order_book().find_order(OrderId{1});
    ASSERT_NE(resting_order, nullptr);
    EXPECT_EQ(resting_order->remaining_quantity, Quantity{10});
}

TEST(MatchingEngineTest, BuyOrderCrossesBestAskAtRestingPrice) {
    MatchingEngine engine{InstrumentId{1001}};

    const auto resting_result = engine.submit_order(
        make_limit_order(OrderId{10}, Side::Sell, Price{73700}, Quantity{3}, SequenceNumber{1}));
    ASSERT_EQ(resting_result.reports.size(), 1U);

    const auto result = engine.submit_order(
        make_limit_order(OrderId{20}, Side::Buy, Price{73800}, Quantity{10}, SequenceNumber{2}));

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].resting_order_id, OrderId{10});
    EXPECT_EQ(result.trades[0].incoming_order_id, OrderId{20});
    EXPECT_EQ(result.trades[0].aggressor_side, Side::Buy);
    EXPECT_EQ(result.trades[0].price, Price{73700});
    EXPECT_EQ(result.trades[0].quantity, Quantity{3});

    ASSERT_EQ(result.reports.size(), 2U);
    EXPECT_EQ(result.reports[0].order_id, OrderId{10});
    EXPECT_EQ(result.reports[0].status, OrderStatus::Filled);
    EXPECT_EQ(result.reports[0].remaining_quantity, Quantity{0});

    EXPECT_EQ(result.reports[1].order_id, OrderId{20});
    EXPECT_EQ(result.reports[1].status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(result.reports[1].filled_quantity, Quantity{3});
    EXPECT_EQ(result.reports[1].remaining_quantity, Quantity{7});

    EXPECT_EQ(engine.order_book().find_order(OrderId{10}), nullptr);
    const auto* incoming_remainder = engine.order_book().find_order(OrderId{20});
    ASSERT_NE(incoming_remainder, nullptr);
    EXPECT_EQ(incoming_remainder->price(), Price{73800});
    EXPECT_EQ(incoming_remainder->remaining_quantity, Quantity{7});
}

TEST(MatchingEngineTest, SellOrderCrossesBestBidAtRestingPrice) {
    MatchingEngine engine{InstrumentId{1001}};

    ASSERT_EQ(engine.submit_order(make_limit_order(OrderId{30}, Side::Buy, Price{73600},
                                                   Quantity{5}, SequenceNumber{1}))
                  .reports.size(),
              1U);

    const auto result = engine.submit_order(
        make_limit_order(OrderId{31}, Side::Sell, Price{73500}, Quantity{2}, SequenceNumber{2}));

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].resting_order_id, OrderId{30});
    EXPECT_EQ(result.trades[0].incoming_order_id, OrderId{31});
    EXPECT_EQ(result.trades[0].aggressor_side, Side::Sell);
    EXPECT_EQ(result.trades[0].price, Price{73600});
    EXPECT_EQ(result.trades[0].quantity, Quantity{2});

    ASSERT_EQ(result.reports.size(), 2U);
    EXPECT_EQ(result.reports[0].order_id, OrderId{30});
    EXPECT_EQ(result.reports[0].status, OrderStatus::PartiallyFilled);
    EXPECT_EQ(result.reports[0].remaining_quantity, Quantity{3});

    EXPECT_EQ(result.reports[1].order_id, OrderId{31});
    EXPECT_EQ(result.reports[1].status, OrderStatus::Filled);
    EXPECT_EQ(result.reports[1].remaining_quantity, Quantity{0});

    const auto* resting_remainder = engine.order_book().find_order(OrderId{30});
    ASSERT_NE(resting_remainder, nullptr);
    EXPECT_EQ(resting_remainder->remaining_quantity, Quantity{3});
    EXPECT_EQ(engine.order_book().find_order(OrderId{31}), nullptr);
}

TEST(MatchingEngineTest, MatchingUsesFifoWithinSamePriceLevel) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto first_resting_result = engine.submit_order(
        make_limit_order(OrderId{40}, Side::Sell, Price{73700}, Quantity{2}, SequenceNumber{1}));
    [[maybe_unused]] const auto second_resting_result = engine.submit_order(
        make_limit_order(OrderId{41}, Side::Sell, Price{73700}, Quantity{2}, SequenceNumber{2}));

    const auto result = engine.submit_order(
        make_limit_order(OrderId{42}, Side::Buy, Price{73700}, Quantity{3}, SequenceNumber{3}));

    ASSERT_EQ(result.trades.size(), 2U);
    EXPECT_EQ(result.trades[0].resting_order_id, OrderId{40});
    EXPECT_EQ(result.trades[0].quantity, Quantity{2});
    EXPECT_EQ(result.trades[1].resting_order_id, OrderId{41});
    EXPECT_EQ(result.trades[1].quantity, Quantity{1});

    EXPECT_EQ(engine.order_book().find_order(OrderId{40}), nullptr);
    const auto* second_resting_order = engine.order_book().find_order(OrderId{41});
    ASSERT_NE(second_resting_order, nullptr);
    EXPECT_EQ(second_resting_order->remaining_quantity, Quantity{1});
    EXPECT_EQ(engine.order_book().find_order(OrderId{42}), nullptr);
}

TEST(MatchingEngineTest, DuplicateOrderIdIsRejectedEvenAfterFill) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto resting_result = engine.submit_order(
        make_limit_order(OrderId{50}, Side::Sell, Price{73700}, Quantity{2}, SequenceNumber{1}));
    [[maybe_unused]] const auto fill_result = engine.submit_order(
        make_limit_order(OrderId{51}, Side::Buy, Price{73700}, Quantity{2}, SequenceNumber{2}));

    const auto duplicate_result = engine.submit_order(
        make_limit_order(OrderId{50}, Side::Buy, Price{73500}, Quantity{1}, SequenceNumber{3}));

    EXPECT_TRUE(duplicate_result.trades.empty());
    ASSERT_EQ(duplicate_result.reports.size(), 1U);
    EXPECT_EQ(duplicate_result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(duplicate_result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(duplicate_result.reports[0].reject_reason, RejectReason::DuplicateOrderId);
}

TEST(MatchingEngineTest, InvalidOrderIsRejected) {
    MatchingEngine engine{InstrumentId{1001}};

    Order invalid_quantity = make_limit_order(OrderId{60}, Side::Buy, Price{73500},
                                              Quantity{0}, SequenceNumber{1});
    const auto result = engine.submit_order(invalid_quantity);

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(result.reports[0].reject_reason, RejectReason::InvalidQuantity);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(MatchingEngineTest, ReferenceDataRejectsOffTickLimitPrice) {
    const InstrumentReference instrument{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = SequenceNumber{1},
    };
    MatchingEngine engine{instrument};

    const auto result = engine.submit_order(
        make_limit_order(OrderId{70}, Side::Buy, Price{73502}, Quantity{10}, SequenceNumber{1}));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(result.reports[0].reject_reason, RejectReason::InvalidPrice);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(MatchingEngineTest, ReferenceDataRejectsPriceOutsideLimitBand) {
    const InstrumentReference instrument{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = SequenceNumber{1},
    };
    MatchingEngine engine{instrument};

    const auto result = engine.submit_order(
        make_limit_order(OrderId{71}, Side::Buy, Price{69000}, Quantity{10}, SequenceNumber{1}));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(result.reports[0].reject_reason, RejectReason::InvalidPrice);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(MatchingEngineTest, ReferenceDataRejectsOrdersWhenMarketIsClosed) {
    const InstrumentReference instrument{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Closed,
        .version = SequenceNumber{1},
    };
    MatchingEngine engine{instrument};

    const auto result = engine.submit_order(
        make_limit_order(OrderId{72}, Side::Buy, Price{73500}, Quantity{10}, SequenceNumber{1}));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(result.reports[0].reject_reason, RejectReason::MarketClosed);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(MatchingEngineTest, ReferenceDataAllowsValidLimitOrder) {
    const InstrumentReference instrument{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = SequenceNumber{1},
    };
    MatchingEngine engine{instrument};

    const auto result = engine.submit_order(
        make_limit_order(OrderId{73}, Side::Buy, Price{73500}, Quantity{10}, SequenceNumber{1}));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Accepted);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Accepted);
    ASSERT_NE(engine.order_book().find_order(OrderId{73}), nullptr);
}

TEST(MatchingEngineTest, MarketOrderSweepsAvailableLiquidityAndCancelsRemainder) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto first_resting_result = engine.submit_order(
        make_limit_order(OrderId{70}, Side::Sell, Price{73700}, Quantity{2}, SequenceNumber{1}));
    [[maybe_unused]] const auto second_resting_result = engine.submit_order(
        make_limit_order(OrderId{71}, Side::Sell, Price{73800}, Quantity{2}, SequenceNumber{2}));

    const auto result = engine.submit_order(
        make_market_order(OrderId{72}, Side::Buy, Quantity{5}, SequenceNumber{3}));

    ASSERT_EQ(result.trades.size(), 2U);
    EXPECT_EQ(result.trades[0].price, Price{73700});
    EXPECT_EQ(result.trades[0].quantity, Quantity{2});
    EXPECT_EQ(result.trades[1].price, Price{73800});
    EXPECT_EQ(result.trades[1].quantity, Quantity{2});

    ASSERT_EQ(result.reports.size(), 5U);
    EXPECT_EQ(result.reports.back().order_id, OrderId{72});
    EXPECT_EQ(result.reports.back().type, ExecutionType::Canceled);
    EXPECT_EQ(result.reports.back().status, OrderStatus::Canceled);
    EXPECT_EQ(result.reports.back().filled_quantity, Quantity{4});
    EXPECT_EQ(result.reports.back().remaining_quantity, Quantity{0});

    EXPECT_TRUE(engine.order_book().empty());
    EXPECT_EQ(engine.order_book().find_order(OrderId{72}), nullptr);
}

TEST(MatchingEngineTest, IocLimitOrderPartiallyFillsAndDoesNotRest) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto resting_result = engine.submit_order(
        make_limit_order(OrderId{80}, Side::Sell, Price{73700}, Quantity{3}, SequenceNumber{1}));

    const auto result = engine.submit_order(make_limit_order(
        OrderId{81}, Side::Buy, Price{73800}, Quantity{5}, SequenceNumber{2}, TimeInForce::IOC));

    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].quantity, Quantity{3});
    ASSERT_EQ(result.reports.size(), 3U);
    EXPECT_EQ(result.reports.back().type, ExecutionType::Canceled);
    EXPECT_EQ(result.reports.back().status, OrderStatus::Canceled);
    EXPECT_EQ(result.reports.back().filled_quantity, Quantity{3});

    EXPECT_TRUE(engine.order_book().empty());
    EXPECT_EQ(engine.order_book().find_order(OrderId{81}), nullptr);
}

TEST(MatchingEngineTest, IocLimitOrderCancelsWhenItDoesNotCross) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto resting_result = engine.submit_order(
        make_limit_order(OrderId{90}, Side::Sell, Price{73900}, Quantity{3}, SequenceNumber{1}));

    const auto result = engine.submit_order(make_limit_order(
        OrderId{91}, Side::Buy, Price{73800}, Quantity{5}, SequenceNumber{2}, TimeInForce::IOC));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].order_id, OrderId{91});
    EXPECT_EQ(result.reports[0].type, ExecutionType::Canceled);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Canceled);
    EXPECT_EQ(result.reports[0].filled_quantity, Quantity{0});

    ASSERT_NE(engine.order_book().find_order(OrderId{90}), nullptr);
    EXPECT_EQ(engine.order_book().find_order(OrderId{91}), nullptr);
}

TEST(MatchingEngineTest, FokLimitOrderRejectsWithoutMutatingBookWhenLiquidityIsInsufficient) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto first_resting_result = engine.submit_order(
        make_limit_order(OrderId{100}, Side::Sell, Price{73700}, Quantity{3}, SequenceNumber{1}));
    [[maybe_unused]] const auto second_resting_result = engine.submit_order(
        make_limit_order(OrderId{101}, Side::Sell, Price{73900}, Quantity{3}, SequenceNumber{2}));

    const auto result = engine.submit_order(make_limit_order(
        OrderId{102}, Side::Buy, Price{73800}, Quantity{5}, SequenceNumber{3}, TimeInForce::FOK));

    EXPECT_TRUE(result.trades.empty());
    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(result.reports[0].reject_reason, RejectReason::WouldNotExecute);

    EXPECT_EQ(engine.order_book().order_count(), 2U);
    EXPECT_EQ(engine.order_book().total_quantity_at(Side::Sell, Price{73700}), Quantity{3});
    EXPECT_EQ(engine.order_book().total_quantity_at(Side::Sell, Price{73900}), Quantity{3});
}

TEST(MatchingEngineTest, FokLimitOrderFullyExecutesWhenLiquidityIsAvailable) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto first_resting_result = engine.submit_order(
        make_limit_order(OrderId{110}, Side::Sell, Price{73700}, Quantity{3}, SequenceNumber{1}));
    [[maybe_unused]] const auto second_resting_result = engine.submit_order(
        make_limit_order(OrderId{111}, Side::Sell, Price{73800}, Quantity{2}, SequenceNumber{2}));

    const auto result = engine.submit_order(make_limit_order(
        OrderId{112}, Side::Buy, Price{73800}, Quantity{5}, SequenceNumber{3}, TimeInForce::FOK));

    ASSERT_EQ(result.trades.size(), 2U);
    EXPECT_EQ(result.trades[0].quantity, Quantity{3});
    EXPECT_EQ(result.trades[1].quantity, Quantity{2});
    ASSERT_EQ(result.reports.size(), 4U);
    EXPECT_EQ(result.reports.back().order_id, OrderId{112});
    EXPECT_EQ(result.reports.back().status, OrderStatus::Filled);

    EXPECT_TRUE(engine.order_book().empty());
    EXPECT_EQ(engine.order_book().find_order(OrderId{112}), nullptr);
}

TEST(MatchingEngineTest, CancelOrderRemovesRestingOrderAndReportsCanceled) {
    MatchingEngine engine{InstrumentId{1001}};

    [[maybe_unused]] const auto resting_result = engine.submit_order(
        make_limit_order(OrderId{120}, Side::Buy, Price{73600}, Quantity{5}, SequenceNumber{1}));

    const auto result = engine.cancel_order(CancelRequest{
        .order_id = OrderId{120},
        .instrument_id = InstrumentId{1001},
        .sequence = SequenceNumber{2},
    });

    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].order_id, OrderId{120});
    EXPECT_EQ(result.reports[0].type, ExecutionType::Canceled);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Canceled);
    EXPECT_EQ(result.reports[0].filled_quantity, Quantity{0});
    EXPECT_EQ(result.reports[0].remaining_quantity, Quantity{0});
    EXPECT_TRUE(engine.order_book().empty());

    const auto duplicate_result = engine.submit_order(
        make_limit_order(OrderId{120}, Side::Buy, Price{73600}, Quantity{1}, SequenceNumber{3}));
    ASSERT_EQ(duplicate_result.reports.size(), 1U);
    EXPECT_EQ(duplicate_result.reports[0].reject_reason, RejectReason::DuplicateOrderId);
}

TEST(MatchingEngineTest, CancelOrderRejectsMissingOrder) {
    MatchingEngine engine{InstrumentId{1001}};

    const auto result = engine.cancel_order(CancelRequest{
        .order_id = OrderId{130},
        .instrument_id = InstrumentId{1001},
        .sequence = SequenceNumber{1},
    });

    ASSERT_EQ(result.reports.size(), 1U);
    EXPECT_EQ(result.reports[0].type, ExecutionType::Rejected);
    EXPECT_EQ(result.reports[0].status, OrderStatus::Rejected);
    EXPECT_EQ(result.reports[0].reject_reason, RejectReason::OrderNotFound);
}

}  // namespace
