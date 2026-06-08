#include "gateway/gateway_recorder.hpp"
#include "gateway/order_gateway.hpp"
#include "marketdata/market_data_publisher.hpp"
#include "replay/replay_log_io.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <variant>

namespace {

using mini_ats::domain::InstrumentId;
using mini_ats::domain::InstrumentReference;
using mini_ats::domain::MarketSession;
using mini_ats::domain::OrderId;
using mini_ats::domain::OrderStatus;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::RejectReason;
using mini_ats::domain::SequenceNumber;
using mini_ats::engine::MatchingEngine;
using mini_ats::gateway::GatewayRejectReason;
using mini_ats::gateway::GatewayRequest;
using mini_ats::gateway::GatewayResponseStatus;
using mini_ats::gateway::format_gateway_response;
using mini_ats::gateway::handle_published_text_command;
using mini_ats::gateway::handle_recorded_text_command;
using mini_ats::gateway::handle_gateway_request;
using mini_ats::gateway::handle_text_command;
using mini_ats::marketdata::BookUpdateEvent;
using mini_ats::marketdata::MarketDataEventType;
using mini_ats::marketdata::MarketDataPublishStatus;
using mini_ats::marketdata::TradeEvent;
using mini_ats::marketdata::UdpMarketDataPublisher;
using mini_ats::marketdata::event_type;
using mini_ats::protocol::TextCommandParseError;
using mini_ats::replay::ReplayCommandType;
using mini_ats::replay::read_replay_log;

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

TEST(GatewayTest, AcceptsValidSubmitCommandAndReturnsEngineReports) {
    MatchingEngine engine{make_instrument()};

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10");

    EXPECT_TRUE(response.accepted());
    EXPECT_EQ(response.status, GatewayResponseStatus::Accepted);
    EXPECT_EQ(response.reject_reason, GatewayRejectReason::None);
    ASSERT_TRUE(response.command_type.has_value());
    EXPECT_EQ(*response.command_type, ReplayCommandType::SubmitOrder);
    EXPECT_EQ(response.input_sequence, SequenceNumber{1});
    ASSERT_EQ(response.reports.size(), 1U);
    EXPECT_EQ(response.reports[0].status, OrderStatus::Accepted);
    EXPECT_EQ(engine.order_book().total_quantity_at(mini_ats::domain::Side::Buy, Price{73500}),
              Quantity{10});
}

TEST(GatewayTest, RejectsMalformedTextCommandBeforeMutatingEngine) {
    MatchingEngine engine{make_instrument()};

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY quantity=10");

    EXPECT_TRUE(response.rejected());
    EXPECT_EQ(response.status, GatewayResponseStatus::Rejected);
    EXPECT_EQ(response.reject_reason, GatewayRejectReason::ParseError);
    EXPECT_EQ(response.detail, "MISSING_FIELD:price");
    EXPECT_FALSE(response.command_type.has_value());
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(GatewayTest, RejectsReplayValidationErrorBeforeMutatingEngine) {
    MatchingEngine engine{make_instrument(SequenceNumber{7})};

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=6 order_id=20 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10");

    EXPECT_TRUE(response.rejected());
    EXPECT_EQ(response.reject_reason, GatewayRejectReason::ReplayValidationError);
    EXPECT_EQ(response.detail, "REFERENCE_VERSION_MISMATCH");
    ASSERT_TRUE(response.command_type.has_value());
    EXPECT_EQ(*response.command_type, ReplayCommandType::SubmitOrder);
    EXPECT_EQ(response.input_sequence, SequenceNumber{1});
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(GatewayTest, ConvertsEngineRejectToGatewayRejectResponse) {
    MatchingEngine engine{make_instrument()};

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=30 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73502 quantity=10");

    EXPECT_TRUE(response.rejected());
    EXPECT_EQ(response.reject_reason, GatewayRejectReason::EngineRejected);
    ASSERT_TRUE(response.command_type.has_value());
    EXPECT_EQ(*response.command_type, ReplayCommandType::SubmitOrder);
    ASSERT_EQ(response.reports.size(), 1U);
    EXPECT_EQ(response.reports[0].reject_reason, RejectReason::InvalidPrice);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(GatewayTest, AcceptsCancelCommandAfterRestingOrder) {
    MatchingEngine engine{make_instrument()};

    const auto submit_response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=40 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10");
    ASSERT_TRUE(submit_response.accepted());

    const auto cancel_response = handle_text_command(
        engine,
        "CANCEL seq=2 ref=7 order_id=40 instrument_id=1001");

    EXPECT_TRUE(cancel_response.accepted());
    ASSERT_TRUE(cancel_response.command_type.has_value());
    EXPECT_EQ(*cancel_response.command_type, ReplayCommandType::CancelOrder);
    EXPECT_EQ(cancel_response.input_sequence, SequenceNumber{2});
    ASSERT_EQ(cancel_response.reports.size(), 1U);
    EXPECT_EQ(cancel_response.reports[0].status, OrderStatus::Canceled);
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(GatewayTest, HandlesGatewayRequestWrapper) {
    MatchingEngine engine{make_instrument()};
    const GatewayRequest request{
        .text =
            "SUBMIT seq=1 ref=7 order_id=50 instrument_id=1001 side=SELL type=LIMIT "
            "tif=DAY price=73700 quantity=3",
    };

    const auto response = handle_gateway_request(engine, request);

    EXPECT_TRUE(response.accepted());
    EXPECT_EQ(response.input_sequence, SequenceNumber{1});
    ASSERT_TRUE(response.command_type.has_value());
    EXPECT_EQ(*response.command_type, ReplayCommandType::SubmitOrder);
}

TEST(GatewayTest, ConvertsEmptyLineToParseRejectText) {
    MatchingEngine engine{make_instrument()};

    const auto response = handle_text_command(engine, "");

    EXPECT_TRUE(response.rejected());
    EXPECT_EQ(response.reject_reason, GatewayRejectReason::ParseError);
    EXPECT_EQ(response.detail, "EMPTY_LINE");
    EXPECT_EQ(mini_ats::gateway::to_text(TextCommandParseError::EmptyLine),
              std::string{"EMPTY_LINE"});
}

TEST(GatewayTest, FormatsAcceptedGatewayResponseAsCanonicalText) {
    MatchingEngine engine{make_instrument()};

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10");

    EXPECT_EQ(format_gateway_response(response),
              "ACCEPTED reason=NONE seq=1 command=SUBMIT detail=ACCEPTED trades=0 "
              "reports=1 report0_order_id=10 report0_instrument_id=1001 "
              "report0_type=ACCEPTED report0_status=ACCEPTED report0_filled_quantity=0 "
              "report0_remaining_quantity=10 report0_last_price=0 report0_last_quantity=0 "
              "report0_reject_reason=NONE report0_sequence=1");
}

TEST(GatewayTest, FormatsParseRejectGatewayResponseAsCanonicalText) {
    MatchingEngine engine{make_instrument()};

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=10 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY quantity=10");

    EXPECT_EQ(format_gateway_response(response),
              "REJECTED reason=PARSE_ERROR seq=0 command=NONE detail=MISSING_FIELD:price "
              "trades=0 reports=0");
}

TEST(GatewayTest, FormatsTradeResponseWithTradeAndReportDetails) {
    MatchingEngine engine{make_instrument()};

    const auto resting_response = handle_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=60 instrument_id=1001 side=SELL type=LIMIT "
        "tif=DAY price=73700 quantity=3");
    ASSERT_TRUE(resting_response.accepted());

    const auto response = handle_text_command(
        engine,
        "SUBMIT seq=2 ref=7 order_id=61 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73700 quantity=3");

    EXPECT_EQ(format_gateway_response(response),
              "ACCEPTED reason=NONE seq=2 command=SUBMIT detail=ACCEPTED trades=1 "
              "reports=2 trade0_id=1 trade0_instrument_id=1001 "
              "trade0_resting_order_id=60 trade0_incoming_order_id=61 "
              "trade0_aggressor_side=BUY trade0_price=73700 trade0_quantity=3 "
              "trade0_sequence=2 report0_order_id=60 report0_instrument_id=1001 "
              "report0_type=TRADE report0_status=FILLED report0_filled_quantity=3 "
              "report0_remaining_quantity=0 report0_last_price=73700 "
              "report0_last_quantity=3 report0_reject_reason=NONE report0_sequence=3 "
              "report1_order_id=61 report1_instrument_id=1001 report1_type=TRADE "
              "report1_status=FILLED report1_filled_quantity=3 "
              "report1_remaining_quantity=0 report1_last_price=73700 "
              "report1_last_quantity=3 report1_reject_reason=NONE report1_sequence=4");
}

TEST(GatewayTest, RecordsAcceptedCommandsAsCanonicalReplayLogLines) {
    MatchingEngine engine{make_instrument()};
    std::ostringstream accepted_input_log{};

    const auto submit_result = handle_recorded_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=70 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10",
        accepted_input_log);
    ASSERT_TRUE(submit_result.response.accepted());
    EXPECT_TRUE(submit_result.recorded);

    const auto cancel_result = handle_recorded_text_command(
        engine,
        "CANCEL seq=2 ref=7 order_id=70 instrument_id=1001",
        accepted_input_log);
    ASSERT_TRUE(cancel_result.response.accepted());
    EXPECT_TRUE(cancel_result.recorded);

    EXPECT_EQ(accepted_input_log.str(),
              "SUBMIT seq=1 ref=7 order_id=70 instrument_id=1001 side=BUY type=LIMIT "
              "tif=DAY price=73500 quantity=10\n"
              "CANCEL seq=2 ref=7 order_id=70 instrument_id=1001\n");
}

TEST(GatewayTest, DoesNotRecordRejectedCommands) {
    MatchingEngine engine{make_instrument()};
    std::ostringstream accepted_input_log{};

    const auto parse_reject = handle_recorded_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=80 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY quantity=10",
        accepted_input_log);
    EXPECT_TRUE(parse_reject.response.rejected());
    EXPECT_FALSE(parse_reject.recorded);

    const auto replay_reject = handle_recorded_text_command(
        engine,
        "SUBMIT seq=1 ref=6 order_id=81 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10",
        accepted_input_log);
    EXPECT_TRUE(replay_reject.response.rejected());
    EXPECT_FALSE(replay_reject.recorded);

    const auto engine_reject = handle_recorded_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=82 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73502 quantity=10",
        accepted_input_log);
    EXPECT_TRUE(engine_reject.response.rejected());
    EXPECT_FALSE(engine_reject.recorded);

    EXPECT_TRUE(accepted_input_log.str().empty());
    EXPECT_TRUE(engine.order_book().empty());
}

TEST(GatewayTest, RecordedAcceptedLogCanReplaySameState) {
    MatchingEngine gateway_engine{make_instrument()};
    std::ostringstream accepted_input_log{};

    const auto resting_result = handle_recorded_text_command(
        gateway_engine,
        "SUBMIT seq=1 ref=7 order_id=90 instrument_id=1001 side=SELL type=LIMIT "
        "tif=DAY price=73700 quantity=3",
        accepted_input_log);
    ASSERT_TRUE(resting_result.response.accepted());

    const auto incoming_result = handle_recorded_text_command(
        gateway_engine,
        "SUBMIT seq=2 ref=7 order_id=91 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73700 quantity=5",
        accepted_input_log);
    ASSERT_TRUE(incoming_result.response.accepted());

    std::istringstream replay_input{accepted_input_log.str()};
    const auto read_result = read_replay_log(replay_input);
    ASSERT_TRUE(read_result.ok());

    MatchingEngine replay_engine{make_instrument()};
    const auto replay_result = mini_ats::replay::replay_events(replay_engine,
                                                              read_result.events);

    ASSERT_TRUE(replay_result.ok());
    EXPECT_EQ(replay_engine.order_book().total_quantity_at(mini_ats::domain::Side::Buy,
                                                           Price{73700}),
              Quantity{2});
    EXPECT_EQ(replay_engine.order_book().snapshot().asks.size(), 0U);
}

TEST(GatewayTest, PublishedCommandBuildsMarketDataEventsFromEngineResult) {
    MatchingEngine engine{make_instrument()};
    UdpMarketDataPublisher publisher{};

    const auto resting_result = handle_published_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=100 instrument_id=1001 side=SELL type=LIMIT "
        "tif=DAY price=73700 quantity=3",
        publisher);

    ASSERT_TRUE(resting_result.response.accepted());
    ASSERT_EQ(resting_result.market_data_events.size(), 1U);
    EXPECT_EQ(event_type(resting_result.market_data_events[0]),
              MarketDataEventType::BookUpdate);
    EXPECT_EQ(resting_result.publish_result.status, MarketDataPublishStatus::NotOpen);

    const auto trade_result = handle_published_text_command(
        engine,
        "SUBMIT seq=2 ref=7 order_id=101 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73700 quantity=5",
        publisher);

    ASSERT_TRUE(trade_result.response.accepted());
    ASSERT_EQ(trade_result.market_data_events.size(), 2U);
    EXPECT_EQ(event_type(trade_result.market_data_events[0]), MarketDataEventType::Trade);
    EXPECT_EQ(event_type(trade_result.market_data_events[1]),
              MarketDataEventType::BookUpdate);

    const auto& trade = std::get<TradeEvent>(trade_result.market_data_events[0]);
    EXPECT_EQ(trade.resting_order_id, OrderId{100});
    EXPECT_EQ(trade.incoming_order_id, OrderId{101});
    EXPECT_EQ(trade.quantity, Quantity{3});

    const auto& book_update = std::get<BookUpdateEvent>(trade_result.market_data_events[1]);
    ASSERT_TRUE(book_update.has_best_bid());
    EXPECT_EQ(book_update.best_bid->price, Price{73700});
    EXPECT_EQ(book_update.best_bid->total_quantity, Quantity{2});
    EXPECT_EQ(trade_result.publish_result.status, MarketDataPublishStatus::NotOpen);
}

TEST(GatewayTest, PublishedCommandRecordsAcceptedInputAndSkipsRejectedMarketData) {
    MatchingEngine engine{make_instrument()};
    UdpMarketDataPublisher publisher{};
    std::ostringstream accepted_input_log{};

    const auto accepted_result = handle_published_text_command(
        engine,
        "SUBMIT seq=1 ref=7 order_id=110 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73500 quantity=10",
        publisher,
        &accepted_input_log);

    ASSERT_TRUE(accepted_result.response.accepted());
    EXPECT_TRUE(accepted_result.recorded);
    EXPECT_FALSE(accepted_result.market_data_events.empty());
    EXPECT_NE(accepted_input_log.str().find("order_id=110"), std::string::npos);

    const auto rejected_result = handle_published_text_command(
        engine,
        "SUBMIT seq=2 ref=7 order_id=111 instrument_id=1001 side=BUY type=LIMIT "
        "tif=DAY price=73502 quantity=10",
        publisher,
        &accepted_input_log);

    EXPECT_TRUE(rejected_result.response.rejected());
    EXPECT_FALSE(rejected_result.recorded);
    EXPECT_TRUE(rejected_result.market_data_events.empty());
    EXPECT_TRUE(rejected_result.publish_result.ok());
    EXPECT_EQ(accepted_input_log.str().find("order_id=111"), std::string::npos);
}

}  // namespace
