#include "engine/order_book.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using mini_ats::domain::CancelRequest;
using mini_ats::domain::InstrumentId;
using mini_ats::domain::Order;
using mini_ats::domain::OrderId;
using mini_ats::domain::OrderType;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::TimeInForce;
using mini_ats::engine::OrderBook;
using mini_ats::engine::format_order_book;

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

TEST(OrderBookTest, AddsLimitOrdersAndFindsBestPrices) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{1}, Side::Buy, Price{73500},
                                                Quantity{10}, SequenceNumber{1})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{2}, Side::Buy, Price{73600},
                                                Quantity{5}, SequenceNumber{2})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{3}, Side::Sell, Price{73800},
                                                Quantity{4}, SequenceNumber{3})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{4}, Side::Sell, Price{73700},
                                                Quantity{6}, SequenceNumber{4})));

    ASSERT_TRUE(book.best_bid_price().has_value());
    ASSERT_TRUE(book.best_ask_price().has_value());
    EXPECT_EQ(*book.best_bid_price(), Price{73600});
    EXPECT_EQ(*book.best_ask_price(), Price{73700});

    ASSERT_NE(book.best_bid_order(), nullptr);
    ASSERT_NE(book.best_ask_order(), nullptr);
    EXPECT_EQ(book.best_bid_order()->id(), OrderId{2});
    EXPECT_EQ(book.best_ask_order()->id(), OrderId{4});
}

TEST(OrderBookTest, KeepsFifoOrderWithinSamePriceLevel) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{10}, Side::Buy, Price{73500},
                                                Quantity{10}, SequenceNumber{1})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{11}, Side::Buy, Price{73500},
                                                Quantity{5}, SequenceNumber{2})));

    ASSERT_NE(book.best_bid_order(), nullptr);
    EXPECT_EQ(book.best_bid_order()->id(), OrderId{10});
    EXPECT_EQ(book.total_quantity_at(Side::Buy, Price{73500}), Quantity{15});

    EXPECT_TRUE(book.reduce_order(OrderId{10}, Quantity{10}));

    ASSERT_NE(book.best_bid_order(), nullptr);
    EXPECT_EQ(book.best_bid_order()->id(), OrderId{11});
    EXPECT_EQ(book.total_quantity_at(Side::Buy, Price{73500}), Quantity{5});
}

TEST(OrderBookTest, PartialReduceKeepsRemainingQuantityInBook) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{20}, Side::Buy, Price{73500},
                                                Quantity{10}, SequenceNumber{1})));

    EXPECT_TRUE(book.reduce_order(OrderId{20}, Quantity{3}));

    const auto* resting_order = book.find_order(OrderId{20});
    ASSERT_NE(resting_order, nullptr);
    EXPECT_EQ(resting_order->remaining_quantity, Quantity{7});
    EXPECT_EQ(book.total_quantity_at(Side::Buy, Price{73500}), Quantity{7});
    EXPECT_EQ(book.order_count(), 1U);
}

TEST(OrderBookTest, FullReduceRemovesOrderAndEmptyPriceLevel) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{30}, Side::Sell, Price{74000},
                                                Quantity{8}, SequenceNumber{1})));

    EXPECT_TRUE(book.reduce_order(OrderId{30}, Quantity{8}));

    EXPECT_EQ(book.find_order(OrderId{30}), nullptr);
    EXPECT_EQ(book.total_quantity_at(Side::Sell, Price{74000}), Quantity{0});
    EXPECT_EQ(book.price_level_count(Side::Sell), 0U);
    EXPECT_EQ(book.best_ask_order(), nullptr);
}

TEST(OrderBookTest, CancelRequestRemovesRestingOrder) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{40}, Side::Sell, Price{74000},
                                                Quantity{8}, SequenceNumber{1})));

    const CancelRequest wrong_instrument{
        .order_id = OrderId{40},
        .instrument_id = InstrumentId{2002},
        .sequence = SequenceNumber{2},
    };
    EXPECT_FALSE(book.cancel_order(wrong_instrument));
    EXPECT_TRUE(book.contains_order(OrderId{40}));

    const CancelRequest request{
        .order_id = OrderId{40},
        .instrument_id = InstrumentId{1001},
        .sequence = SequenceNumber{3},
    };
    EXPECT_TRUE(book.cancel_order(request));
    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, RejectsOrdersThatShouldNotRest) {
    OrderBook book{InstrumentId{1001}};

    Order market_order = make_limit_order(OrderId{50}, Side::Buy, Price{0}, Quantity{10},
                                          SequenceNumber{1});
    market_order.type = OrderType::Market;
    EXPECT_FALSE(book.add_order(market_order));

    EXPECT_FALSE(book.add_order(make_limit_order(OrderId{51}, Side::Buy, Price{73500},
                                                 Quantity{10}, SequenceNumber{2},
                                                 TimeInForce::IOC)));
    EXPECT_FALSE(book.add_order(make_limit_order(OrderId{52}, Side::Buy, Price{0},
                                                 Quantity{10}, SequenceNumber{3})));
    EXPECT_FALSE(book.add_order(make_limit_order(OrderId{53}, Side::Buy, Price{73500},
                                                 Quantity{0}, SequenceNumber{4})));
    EXPECT_FALSE(book.add_order(make_limit_order(OrderId{54}, Side::Buy, Price{73500},
                                                 Quantity{10}, SequenceNumber{5},
                                                 TimeInForce::Day, InstrumentId{2002})));

    EXPECT_TRUE(book.empty());
}

TEST(OrderBookTest, RejectsDuplicateOrderIds) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{60}, Side::Buy, Price{73500},
                                                Quantity{10}, SequenceNumber{1})));
    EXPECT_FALSE(book.add_order(make_limit_order(OrderId{60}, Side::Sell, Price{74000},
                                                 Quantity{2}, SequenceNumber{2})));

    EXPECT_EQ(book.order_count(), 1U);
    ASSERT_NE(book.best_bid_order(), nullptr);
    EXPECT_EQ(book.best_bid_order()->id(), OrderId{60});
}

TEST(OrderBookTest, SnapshotShowsBookStateInPriorityOrder) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{70}, Side::Buy, Price{73500},
                                                Quantity{10}, SequenceNumber{1})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{71}, Side::Buy, Price{73600},
                                                Quantity{5}, SequenceNumber{2})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{72}, Side::Sell, Price{73800},
                                                Quantity{4}, SequenceNumber{3})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{73}, Side::Sell, Price{73700},
                                                Quantity{6}, SequenceNumber{4})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{74}, Side::Buy, Price{73500},
                                                Quantity{5}, SequenceNumber{5})));

    EXPECT_TRUE(book.reduce_order(OrderId{70}, Quantity{3}));

    const auto snapshot = book.snapshot();

    ASSERT_EQ(snapshot.asks.size(), 2U);
    EXPECT_EQ(snapshot.asks[0].price, Price{73700});
    EXPECT_EQ(snapshot.asks[0].total_quantity, Quantity{6});
    EXPECT_EQ(snapshot.asks[0].orders[0].order_id, OrderId{73});
    EXPECT_EQ(snapshot.asks[1].price, Price{73800});

    ASSERT_EQ(snapshot.bids.size(), 2U);
    EXPECT_EQ(snapshot.bids[0].price, Price{73600});
    EXPECT_EQ(snapshot.bids[0].orders[0].order_id, OrderId{71});
    EXPECT_EQ(snapshot.bids[1].price, Price{73500});
    EXPECT_EQ(snapshot.bids[1].total_quantity, Quantity{12});
    ASSERT_EQ(snapshot.bids[1].orders.size(), 2U);
    EXPECT_EQ(snapshot.bids[1].orders[0].order_id, OrderId{70});
    EXPECT_EQ(snapshot.bids[1].orders[0].remaining_quantity, Quantity{7});
    EXPECT_EQ(snapshot.bids[1].orders[1].order_id, OrderId{74});
    EXPECT_EQ(snapshot.bids[1].orders[1].remaining_quantity, Quantity{5});
}

TEST(OrderBookTest, FormatsOrderBookSnapshotForHumans) {
    OrderBook book{InstrumentId{1001}};

    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{80}, Side::Buy, Price{73500},
                                                Quantity{10}, SequenceNumber{1})));
    EXPECT_TRUE(book.add_order(make_limit_order(OrderId{81}, Side::Sell, Price{73700},
                                                Quantity{6}, SequenceNumber{2})));
    EXPECT_TRUE(book.reduce_order(OrderId{80}, Quantity{3}));

    const std::string text = format_order_book(book.snapshot());

    EXPECT_NE(text.find("OrderBook instrument=1001"), std::string::npos);
    EXPECT_NE(text.find("ASK best-first"), std::string::npos);
    EXPECT_NE(text.find("73700 | total=6 | #81(rem=6,seq=2)"), std::string::npos);
    EXPECT_NE(text.find("----- spread -----"), std::string::npos);
    EXPECT_NE(text.find("BID best-first"), std::string::npos);
    EXPECT_NE(text.find("73500 | total=7 | #80(rem=7,seq=1)"), std::string::npos);
}

}  // namespace
