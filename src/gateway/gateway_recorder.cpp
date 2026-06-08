#include "gateway/gateway_recorder.hpp"

#include "marketdata/market_data.hpp"
#include "replay/replay_log_io.hpp"

#include <ostream>
#include <utility>

namespace mini_ats::gateway {

namespace {

[[nodiscard]] std::vector<marketdata::MarketDataEvent> build_market_data_events(
    const replay::ReplayEvent& event,
    const replay::ReplayApplyResult& apply_result,
    const engine::OrderBookSnapshot& snapshot,
    std::size_t depth) {
    if (!apply_result.applied()) {
        return {};
    }

    if (event.type() == replay::ReplayCommandType::SubmitOrder) {
        const engine::SubmitOrderResult submit_result{
            .trades = apply_result.trades,
            .reports = apply_result.reports,
        };
        return marketdata::market_data_events_for(submit_result, snapshot, depth);
    }

    const engine::CancelOrderResult cancel_result{
        .reports = apply_result.reports,
    };
    return marketdata::market_data_events_for(cancel_result, snapshot, depth);
}

}  // namespace

RecordedGatewayCommandResult handle_recorded_text_command(
    engine::MatchingEngine& engine,
    std::string_view command,
    std::ostream& accepted_input_log) {
    const auto parse_result = protocol::parse_text_command(command);
    if (!parse_result.ok()) {
        return RecordedGatewayCommandResult{
            .response = make_parse_reject_response(parse_result),
            .recorded = false,
        };
    }

    const auto apply_result = replay::apply_replay_event(engine, *parse_result.event);
    auto response = make_engine_response(*parse_result.event, apply_result);
    if (!response.accepted()) {
        return RecordedGatewayCommandResult{
            .response = std::move(response),
            .recorded = false,
        };
    }

    accepted_input_log << replay::format_replay_event(*parse_result.event) << '\n';
    return RecordedGatewayCommandResult{
        .response = std::move(response),
        .recorded = accepted_input_log.good(),
    };
}

PublishedGatewayCommandResult handle_published_text_command(
    engine::MatchingEngine& engine,
    std::string_view command,
    marketdata::UdpMarketDataPublisher& publisher,
    std::ostream* accepted_input_log,
    std::size_t market_data_depth) {
    const auto parse_result = protocol::parse_text_command(command);
    if (!parse_result.ok()) {
        return PublishedGatewayCommandResult{
            .response = make_parse_reject_response(parse_result),
            .recorded = false,
            .publish_result = {},
        };
    }

    const auto apply_result = replay::apply_replay_event(engine, *parse_result.event);
    auto response = make_engine_response(*parse_result.event, apply_result);
    bool recorded = false;
    if (response.accepted() && accepted_input_log != nullptr) {
        *accepted_input_log << replay::format_replay_event(*parse_result.event) << '\n';
        recorded = accepted_input_log->good();
    }

    auto events = build_market_data_events(*parse_result.event, apply_result,
                                           engine.order_book().snapshot(),
                                           market_data_depth);
    auto publish_result = events.empty() ? marketdata::MarketDataPublishResult{}
                                         : publisher.publish(events);

    return PublishedGatewayCommandResult{
        .response = std::move(response),
        .recorded = recorded,
        .market_data_events = std::move(events),
        .publish_result = publish_result,
    };
}

}  // namespace mini_ats::gateway
