#include "domain/order.hpp"

#include <gtest/gtest.h>

#include <type_traits>

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

static_assert(std::is_integral_v<Price>);
static_assert(std::is_integral_v<Quantity>);

TEST(OrderTest, LimitOrderUsesIntegerPriceAndQuantity) {
    const Order order{
        .id = OrderId{1},
        .instrument_id = InstrumentId{1001},
        .side = Side::Buy,
        .type = OrderType::Limit,
        .time_in_force = TimeInForce::Day,
        .price = Price{73500},
        .quantity = Quantity{10},
        .sequence = SequenceNumber{1},
    };

    EXPECT_TRUE(order.is_limit());
    EXPECT_FALSE(order.is_market());
    EXPECT_TRUE(order.has_valid_quantity());
    EXPECT_TRUE(order.has_valid_price());
    EXPECT_EQ(order.price, Price{73500});
    EXPECT_EQ(order.quantity, Quantity{10});
}

}  // namespace
