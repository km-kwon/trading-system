#include "replay/replay_log_io.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

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
using mini_ats::protocol::TextCommandParseError;
using mini_ats::replay::ReplayCommandType;
using mini_ats::replay::ReplayEvent;
using mini_ats::replay::ReplayLogIoError;
using mini_ats::replay::format_replay_event;
using mini_ats::replay::read_replay_log;
using mini_ats::replay::read_replay_log_file;
using mini_ats::replay::replay_events;
using mini_ats::replay::write_replay_log;
using mini_ats::replay::write_replay_log_file;

Order make_limit_order(OrderId id,
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

std::vector<ReplayEvent> make_events() {
    return {
        ReplayEvent::submit_order(
            SequenceNumber{1}, SequenceNumber{7},
            make_limit_order(OrderId{10}, Side::Sell, Price{73700}, Quantity{3},
                             SequenceNumber{1})),
        ReplayEvent::submit_order(
            SequenceNumber{2}, SequenceNumber{7},
            make_limit_order(OrderId{11}, Side::Buy, Price{73700}, Quantity{5},
                             SequenceNumber{2})),
        ReplayEvent::cancel_order(
            SequenceNumber{3}, SequenceNumber{7},
            CancelRequest{
                .order_id = OrderId{11},
                .instrument_id = InstrumentId{1001},
                .sequence = SequenceNumber{3},
            }),
    };
}

TEST(ReplayLogIoTest, ReadsTextLogFromStreamSkippingBlankLinesAndComments) {
    std::istringstream input{
        "# demo replay log\n"
        "\n"
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY "
        "price=73700 quantity=3\n"
        "  # indented comment\n"
        "CANCEL seq=2 ref=7 order_id=10 instrument_id=1001\n"};

    const auto result = read_replay_log(input);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.events.size(), 2U);
    EXPECT_EQ(result.events[0].type(), ReplayCommandType::SubmitOrder);
    EXPECT_EQ(result.events[1].type(), ReplayCommandType::CancelOrder);
    EXPECT_EQ(result.events[1].input_sequence, SequenceNumber{2});
}

TEST(ReplayLogIoTest, ReportsParseFailureWithLineNumberAndOriginalLine) {
    std::istringstream input{
        "# ok\n"
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT tif=DAY "
        "price=73700 quantity=3\n"
        "CANCEL seq=bad ref=7 order_id=10 instrument_id=1001\n"};

    const auto result = read_replay_log(input);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ReplayLogIoError::ParseError);
    ASSERT_TRUE(result.failure.has_value());
    EXPECT_EQ(result.failure->line_number, 3U);
    EXPECT_EQ(result.failure->parse_error, TextCommandParseError::InvalidNumber);
    EXPECT_EQ(result.failure->field, "seq");
    EXPECT_EQ(result.events.size(), 1U);
    EXPECT_NE(result.failure->line.find("seq=bad"), std::string::npos);
}

TEST(ReplayLogIoTest, FormatsReplayEventsAsCanonicalTextCommands) {
    const auto events = make_events();

    EXPECT_EQ(format_replay_event(events[0]),
              "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=SELL type=LIMIT "
              "tif=DAY price=73700 quantity=3");
    EXPECT_EQ(format_replay_event(events[2]),
              "CANCEL seq=3 ref=7 order_id=11 instrument_id=1001");
}

TEST(ReplayLogIoTest, WritesAndReadsReplayLogFromStream) {
    const auto events = make_events();
    std::ostringstream output{};

    ASSERT_TRUE(write_replay_log(output, events));

    std::istringstream input{output.str()};
    const auto result = read_replay_log(input);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.events.size(), events.size());
    EXPECT_EQ(format_replay_event(result.events[0]), format_replay_event(events[0]));
    EXPECT_EQ(format_replay_event(result.events[2]), format_replay_event(events[2]));
}

TEST(ReplayLogIoTest, WritesAndReadsReplayLogFile) {
    const auto events = make_events();
    const auto path = std::filesystem::temp_directory_path() / "mini_ats_replay_log_io_test.txt";

    ASSERT_TRUE(write_replay_log_file(path, events));
    const auto result = read_replay_log_file(path);
    std::filesystem::remove(path);

    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.events.size(), events.size());
    EXPECT_EQ(format_replay_event(result.events[1]), format_replay_event(events[1]));
}

TEST(ReplayLogIoTest, ReadEventsCanDriveReplay) {
    std::istringstream input{
        "SUBMIT seq=1 ref=7 order_id=20 instrument_id=1001 side=SELL type=LIMIT tif=DAY "
        "price=73700 quantity=3\n"
        "SUBMIT seq=2 ref=7 order_id=21 instrument_id=1001 side=BUY type=LIMIT tif=DAY "
        "price=73700 quantity=5\n"};

    const auto read_result = read_replay_log(input);
    ASSERT_TRUE(read_result.ok());

    MatchingEngine engine{make_instrument()};
    const auto replay_result = replay_events(engine, read_result.events);

    ASSERT_TRUE(replay_result.ok());
    ASSERT_EQ(replay_result.trades.size(), 1U);
    EXPECT_EQ(replay_result.trades[0].quantity, Quantity{3});
    ASSERT_EQ(engine.order_book().snapshot().bids.size(), 1U);
    EXPECT_EQ(engine.order_book().snapshot().bids[0].total_quantity, Quantity{2});
}

}  // namespace
