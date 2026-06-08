#include "marketdata/market_data.hpp"

#include <gtest/gtest.h>

#include <variant>

namespace {

using mini_ats::domain::CancelRequest;
using mini_ats::domain::InstrumentId;
using mini_ats::domain::InstrumentReference;
using mini_ats::domain::MarketSession;
using mini_ats::domain::Order;
using mini_ats::domain::OrderId;
using mini_ats::domain::OrderType;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::TimeInForce;
using mini_ats::engine::MatchingEngine;
using mini_ats::marketdata::BookUpdateEvent;
using mini_ats::marketdata::MarketDataEventType;
using mini_ats::marketdata::TradeEvent;
using mini_ats::marketdata::event_type;
using mini_ats::marketdata::market_data_events_for;

InstrumentReference make_instrument() {
    return InstrumentReference{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = SequenceNumber{7},
    };
}

Order make_order(OrderId id,
                 Side side,
                 OrderType type,
                 TimeInForce time_in_force,
                 Price price,
                 Quantity quantity,
                 SequenceNumber sequence) {
    return Order{
        .id = id,
        .instrument_id = InstrumentId{1001},
        .side = side,
        .type = type,
        .time_in_force = time_in_force,
        .price = price,
        .quantity = quantity,
        .sequence = sequence,
    };
}

}  // namespace

TEST(MarketDataTest, LimitOrderRestingCreatesTopOfBookUpdate) {
    MatchingEngine engine{make_instrument()};

    const auto submit_result = engine.submit_order(
        make_order(OrderId{10}, Side::Buy, OrderType::Limit, TimeInForce::Day,
                   Price{73500}, Quantity{10}, SequenceNumber{1}));
    const auto events = market_data_events_for(submit_result, engine.order_book().snapshot());

    ASSERT_EQ(events.size(), 1U);
    EXPECT_EQ(event_type(events[0]), MarketDataEventType::BookUpdate);

    const auto& book_update = std::get<BookUpdateEvent>(events[0]);
    EXPECT_EQ(book_update.instrument_id, InstrumentId{1001});
    EXPECT_TRUE(book_update.has_best_bid());
    EXPECT_FALSE(book_update.has_best_ask());
    ASSERT_EQ(book_update.bids.size(), 1U);
    EXPECT_EQ(book_update.best_bid->side, Side::Buy);
    EXPECT_EQ(book_update.best_bid->price, Price{73500});
    EXPECT_EQ(book_update.best_bid->total_quantity, Quantity{10});
    EXPECT_EQ(book_update.sequence, submit_result.reports[0].sequence);
}

TEST(MarketDataTest, TradeCreatesTradeEventAndFinalBookUpdate) {
    MatchingEngine engine{make_instrument()};

    const auto resting_result = engine.submit_order(
        make_order(OrderId{20}, Side::Sell, OrderType::Limit, TimeInForce::Day,
                   Price{73700}, Quantity{3}, SequenceNumber{1}));
    ASSERT_EQ(market_data_events_for(resting_result, engine.order_book().snapshot()).size(), 1U);

    const auto incoming_result = engine.submit_order(
        make_order(OrderId{21}, Side::Buy, OrderType::Limit, TimeInForce::Day,
                   Price{73700}, Quantity{5}, SequenceNumber{2}));
    const auto events = market_data_events_for(incoming_result, engine.order_book().snapshot());

    ASSERT_EQ(events.size(), 2U);
    EXPECT_EQ(event_type(events[0]), MarketDataEventType::Trade);
    EXPECT_EQ(event_type(events[1]), MarketDataEventType::BookUpdate);

    const auto& trade = std::get<TradeEvent>(events[0]);
    EXPECT_EQ(trade.trade_id, 1U);
    EXPECT_EQ(trade.resting_order_id, OrderId{20});
    EXPECT_EQ(trade.incoming_order_id, OrderId{21});
    EXPECT_EQ(trade.aggressor_side, Side::Buy);
    EXPECT_EQ(trade.price, Price{73700});
    EXPECT_EQ(trade.quantity, Quantity{3});

    const auto& book_update = std::get<BookUpdateEvent>(events[1]);
    ASSERT_TRUE(book_update.has_best_bid());
    EXPECT_FALSE(book_update.has_best_ask());
    EXPECT_EQ(book_update.best_bid->price, Price{73700});
    EXPECT_EQ(book_update.best_bid->total_quantity, Quantity{2});
    EXPECT_EQ(book_update.sequence, incoming_result.reports.back().sequence);
}

TEST(MarketDataTest, RejectedAndNoFillIocSubmitProduceNoMarketData) {
    MatchingEngine engine{make_instrument()};

    const auto rejected_result = engine.submit_order(
        make_order(OrderId{30}, Side::Buy, OrderType::Limit, TimeInForce::Day,
                   Price{73502}, Quantity{10}, SequenceNumber{1}));
    EXPECT_TRUE(market_data_events_for(rejected_result, engine.order_book().snapshot()).empty());

    const auto ioc_result = engine.submit_order(
        make_order(OrderId{31}, Side::Buy, OrderType::Limit, TimeInForce::IOC,
                   Price{73500}, Quantity{10}, SequenceNumber{2}));
    EXPECT_TRUE(market_data_events_for(ioc_result, engine.order_book().snapshot()).empty());
}

TEST(MarketDataTest, CancelCreatesBookUpdateAfterRemovingRestingOrder) {
    MatchingEngine engine{make_instrument()};

    const auto submit_result = engine.submit_order(
        make_order(OrderId{40}, Side::Sell, OrderType::Limit, TimeInForce::Day,
                   Price{73700}, Quantity{3}, SequenceNumber{1}));
    ASSERT_FALSE(market_data_events_for(submit_result, engine.order_book().snapshot()).empty());

    const auto cancel_result = engine.cancel_order(CancelRequest{
        .order_id = OrderId{40},
        .instrument_id = InstrumentId{1001},
        .sequence = SequenceNumber{2},
    });
    const auto events = market_data_events_for(cancel_result, engine.order_book().snapshot());

    ASSERT_EQ(events.size(), 1U);
    ASSERT_EQ(event_type(events[0]), MarketDataEventType::BookUpdate);

    const auto& book_update = std::get<BookUpdateEvent>(events[0]);
    EXPECT_FALSE(book_update.has_best_bid());
    EXPECT_FALSE(book_update.has_best_ask());
    EXPECT_TRUE(book_update.bids.empty());
    EXPECT_TRUE(book_update.asks.empty());
    EXPECT_EQ(book_update.sequence, cancel_result.reports[0].sequence);
}
