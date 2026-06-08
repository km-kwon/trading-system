#include "domain/cancel_request.hpp"
#include "domain/execution_report.hpp"
#include "domain/instrument.hpp"
#include "domain/trade.hpp"

#include <gtest/gtest.h>

namespace {

using mini_ats::domain::CancelRequest;
using mini_ats::domain::ExecutionReport;
using mini_ats::domain::ExecutionType;
using mini_ats::domain::InstrumentId;
using mini_ats::domain::InstrumentReference;
using mini_ats::domain::MarketSession;
using mini_ats::domain::OrderId;
using mini_ats::domain::OrderStatus;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::RejectReason;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::Trade;
using mini_ats::domain::TradeId;
using mini_ats::domain::default_instrument_reference;

TEST(DomainModelTest, TradeCalculatesIntegerNotional) {
    const Trade trade{
        .id = TradeId{1},
        .instrument_id = InstrumentId{1001},
        .resting_order_id = OrderId{10},
        .incoming_order_id = OrderId{20},
        .aggressor_side = Side::Buy,
        .price = Price{73500},
        .quantity = Quantity{3},
        .sequence = SequenceNumber{7},
    };

    EXPECT_TRUE(trade.has_valid_price());
    EXPECT_TRUE(trade.has_valid_quantity());
    EXPECT_EQ(trade.notional(), Price{220500});
}

TEST(DomainModelTest, ExecutionReportRepresentsPartialFill) {
    const ExecutionReport report{
        .order_id = OrderId{20},
        .instrument_id = InstrumentId{1001},
        .type = ExecutionType::Trade,
        .status = OrderStatus::PartiallyFilled,
        .filled_quantity = Quantity{3},
        .remaining_quantity = Quantity{7},
        .last_price = Price{73500},
        .last_quantity = Quantity{3},
        .reject_reason = RejectReason::None,
        .sequence = SequenceNumber{8},
    };

    EXPECT_TRUE(report.is_trade());
    EXPECT_FALSE(report.is_rejected());
    EXPECT_FALSE(report.is_terminal());
    EXPECT_TRUE(report.has_fill());
}

TEST(DomainModelTest, ExecutionReportRepresentsRejectedOrder) {
    const ExecutionReport report{
        .order_id = OrderId{21},
        .instrument_id = InstrumentId{1001},
        .type = ExecutionType::Rejected,
        .status = OrderStatus::Rejected,
        .filled_quantity = Quantity{0},
        .remaining_quantity = Quantity{0},
        .last_price = Price{0},
        .last_quantity = Quantity{0},
        .reject_reason = RejectReason::InvalidQuantity,
        .sequence = SequenceNumber{9},
    };

    EXPECT_FALSE(report.is_trade());
    EXPECT_TRUE(report.is_rejected());
    EXPECT_TRUE(report.is_terminal());
    EXPECT_FALSE(report.has_fill());
}

TEST(DomainModelTest, CancelRequestRequiresOrderAndInstrumentId) {
    const CancelRequest request{
        .order_id = OrderId{20},
        .instrument_id = InstrumentId{1001},
        .sequence = SequenceNumber{10},
    };

    EXPECT_TRUE(request.has_valid_order_id());
    EXPECT_TRUE(request.has_valid_instrument_id());

    const CancelRequest invalid_request{};

    EXPECT_FALSE(invalid_request.has_valid_order_id());
    EXPECT_FALSE(invalid_request.has_valid_instrument_id());
}

TEST(DomainModelTest, InstrumentReferenceValidatesTickSizePriceLimitsAndSession) {
    const InstrumentReference instrument{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = SequenceNumber{3},
    };

    EXPECT_TRUE(instrument.has_valid_instrument_id());
    EXPECT_TRUE(instrument.has_valid_tick_size());
    EXPECT_TRUE(instrument.has_valid_price_limits());
    EXPECT_TRUE(instrument.is_open());
    EXPECT_TRUE(instrument.price_is_on_tick(Price{73500}));
    EXPECT_FALSE(instrument.price_is_on_tick(Price{73502}));
    EXPECT_TRUE(instrument.price_is_within_limits(Price{73500}));
    EXPECT_FALSE(instrument.price_is_within_limits(Price{69000}));
    EXPECT_TRUE(instrument.accepts_limit_price(Price{73500}));
    EXPECT_FALSE(instrument.accepts_limit_price(Price{73502}));
    EXPECT_FALSE(instrument.accepts_limit_price(Price{81000}));
}

TEST(DomainModelTest, DefaultInstrumentReferenceKeepsLegacySingleInstrumentOpen) {
    const auto instrument = default_instrument_reference(InstrumentId{1001});

    EXPECT_EQ(instrument.id, InstrumentId{1001});
    EXPECT_TRUE(instrument.is_open());
    EXPECT_TRUE(instrument.accepts_limit_price(Price{1}));
    EXPECT_TRUE(instrument.accepts_limit_price(Price{73500}));
}

}  // namespace
