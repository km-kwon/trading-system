#include "protocol/text_command_parser.hpp"

#include <gtest/gtest.h>

#include <variant>
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
using mini_ats::protocol::TextCommandParseError;
using mini_ats::protocol::parse_text_command;
using mini_ats::replay::ReplayCommandType;
using mini_ats::replay::ReplayEvent;
using mini_ats::replay::replay_events;

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

TEST(TextCommandParserTest, ParsesLimitSubmitCommand) {
    const auto result = parse_text_command(
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.event.has_value());
    EXPECT_EQ(result.event->type(), ReplayCommandType::SubmitOrder);
    EXPECT_EQ(result.event->input_sequence, SequenceNumber{1});
    EXPECT_EQ(result.event->reference_version, SequenceNumber{7});

    const auto& order = std::get<Order>(result.event->command);
    EXPECT_EQ(order.id, OrderId{10});
    EXPECT_EQ(order.instrument_id, InstrumentId{1001});
    EXPECT_EQ(order.side, Side::Buy);
    EXPECT_EQ(order.type, OrderType::Limit);
    EXPECT_EQ(order.time_in_force, TimeInForce::Day);
    EXPECT_EQ(order.price, Price{73500});
    EXPECT_EQ(order.quantity, Quantity{10});
    EXPECT_EQ(order.sequence, SequenceNumber{1});
}

TEST(TextCommandParserTest, ParsesMarketSubmitCommandWithAliases) {
    const auto result = parse_text_command(
        "SUBMIT input_sequence=2 reference_version=7 id=11 instrument=1001 side=SELL "
        "order_type=MARKET time_in_force=IOC qty=5");

    ASSERT_TRUE(result.ok());
    const auto& order = std::get<Order>(result.event->command);
    EXPECT_EQ(order.id, OrderId{11});
    EXPECT_EQ(order.side, Side::Sell);
    EXPECT_EQ(order.type, OrderType::Market);
    EXPECT_EQ(order.time_in_force, TimeInForce::IOC);
    EXPECT_EQ(order.price, Price{0});
    EXPECT_EQ(order.quantity, Quantity{5});
}

TEST(TextCommandParserTest, ParsesCancelCommand) {
    const auto result =
        parse_text_command("CANCEL seq=3 ref=7 order_id=10 instrument_id=1001");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.event.has_value());
    EXPECT_EQ(result.event->type(), ReplayCommandType::CancelOrder);
    EXPECT_EQ(result.event->input_sequence, SequenceNumber{3});

    const auto& request = std::get<CancelRequest>(result.event->command);
    EXPECT_EQ(request.order_id, OrderId{10});
    EXPECT_EQ(request.instrument_id, InstrumentId{1001});
    EXPECT_EQ(request.sequence, SequenceNumber{3});
}

TEST(TextCommandParserTest, ReportsMalformedAndUnknownInput) {
    EXPECT_EQ(parse_text_command("").error, TextCommandParseError::EmptyLine);
    EXPECT_EQ(parse_text_command("REPLACE seq=1").error, TextCommandParseError::UnknownCommand);
    EXPECT_EQ(parse_text_command("CANCEL seq=1 ref=7 order_id=10 instrument_id").error,
              TextCommandParseError::MalformedToken);
}

TEST(TextCommandParserTest, ReportsMissingAndInvalidFields) {
    auto result = parse_text_command(
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY quantity=10");
    EXPECT_EQ(result.error, TextCommandParseError::MissingField);
    EXPECT_EQ(result.field, "price");

    result = parse_text_command(
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BID type=LIMIT "
        "tif=DAY price=73500 quantity=10");
    EXPECT_EQ(result.error, TextCommandParseError::InvalidSide);

    result = parse_text_command(
        "SUBMIT seq=abc ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10");
    EXPECT_EQ(result.error, TextCommandParseError::InvalidNumber);
    EXPECT_EQ(result.field, "seq");
}

TEST(TextCommandParserTest, ParsedCommandsCanDriveReplay) {
    const std::vector<ReplayEvent> events{
        *parse_text_command(
             "SUBMIT seq=1 ref=7 order_id=20 instrument_id=1001 side=SELL type=LIMIT "
             "tif=DAY price=73700 quantity=3")
             .event,
        *parse_text_command(
             "SUBMIT seq=2 ref=7 order_id=21 instrument_id=1001 side=BUY type=LIMIT "
             "tif=DAY price=73700 quantity=5")
             .event,
    };

    MatchingEngine engine{make_instrument()};
    const auto result = replay_events(engine, events);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.trades.size(), 1U);
    EXPECT_EQ(result.trades[0].price, Price{73700});
    EXPECT_EQ(result.trades[0].quantity, Quantity{3});
    ASSERT_EQ(engine.order_book().snapshot().bids.size(), 1U);
    EXPECT_EQ(engine.order_book().snapshot().bids[0].total_quantity, Quantity{2});
    ASSERT_EQ(result.reports.size(), 3U);
    EXPECT_EQ(result.reports.back().status, OrderStatus::PartiallyFilled);
}

}  // namespace
