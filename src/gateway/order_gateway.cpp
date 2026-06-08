#include "gateway/order_gateway.hpp"

#include <algorithm>
#include <cstddef>
#include <ostream>
#include <sstream>

namespace mini_ats::gateway {

namespace {

[[nodiscard]] bool has_rejected_report(
    const std::vector<domain::ExecutionReport>& reports) noexcept {
    return std::any_of(reports.begin(), reports.end(), [](const auto& report) {
        return report.is_rejected();
    });
}

[[nodiscard]] std::string build_parse_detail(
    const protocol::TextCommandParseResult& parse_result) {
    std::string detail{to_text(parse_result.error)};
    if (!parse_result.field.empty()) {
        detail += ":";
        detail += parse_result.field;
    }
    return detail;
}

[[nodiscard]] std::string_view detail_or_none(const std::string& detail) noexcept {
    return detail.empty() ? std::string_view{"NONE"} : std::string_view{detail};
}

[[nodiscard]] const char* command_type_to_text(
    std::optional<replay::ReplayCommandType> type) noexcept {
    return type.has_value() ? mini_ats::gateway::to_text(*type) : "NONE";
}

[[nodiscard]] const char* to_text(domain::Side side) noexcept {
    return side == domain::Side::Buy ? "BUY" : "SELL";
}

[[nodiscard]] const char* to_text(domain::ExecutionType type) noexcept {
    switch (type) {
        case domain::ExecutionType::Accepted:
            return "ACCEPTED";
        case domain::ExecutionType::Trade:
            return "TRADE";
        case domain::ExecutionType::Canceled:
            return "CANCELED";
        case domain::ExecutionType::Rejected:
            return "REJECTED";
    }

    return "ACCEPTED";
}

[[nodiscard]] const char* to_text(domain::OrderStatus status) noexcept {
    switch (status) {
        case domain::OrderStatus::Accepted:
            return "ACCEPTED";
        case domain::OrderStatus::PartiallyFilled:
            return "PARTIALLY_FILLED";
        case domain::OrderStatus::Filled:
            return "FILLED";
        case domain::OrderStatus::Canceled:
            return "CANCELED";
        case domain::OrderStatus::Rejected:
            return "REJECTED";
    }

    return "ACCEPTED";
}

[[nodiscard]] const char* to_text(domain::RejectReason reason) noexcept {
    switch (reason) {
        case domain::RejectReason::None:
            return "NONE";
        case domain::RejectReason::InvalidOrderId:
            return "INVALID_ORDER_ID";
        case domain::RejectReason::UnknownInstrument:
            return "UNKNOWN_INSTRUMENT";
        case domain::RejectReason::InvalidPrice:
            return "INVALID_PRICE";
        case domain::RejectReason::InvalidQuantity:
            return "INVALID_QUANTITY";
        case domain::RejectReason::DuplicateOrderId:
            return "DUPLICATE_ORDER_ID";
        case domain::RejectReason::OrderNotFound:
            return "ORDER_NOT_FOUND";
        case domain::RejectReason::WouldNotExecute:
            return "WOULD_NOT_EXECUTE";
        case domain::RejectReason::MarketClosed:
            return "MARKET_CLOSED";
        case domain::RejectReason::InternalError:
            return "INTERNAL_ERROR";
    }

    return "NONE";
}

void append_trade(std::ostream& output, const domain::Trade& trade, std::size_t index) {
    output << " trade" << index << "_id=" << trade.id
           << " trade" << index << "_instrument_id=" << trade.instrument_id
           << " trade" << index << "_resting_order_id=" << trade.resting_order_id
           << " trade" << index << "_incoming_order_id=" << trade.incoming_order_id
           << " trade" << index << "_aggressor_side=" << to_text(trade.aggressor_side)
           << " trade" << index << "_price=" << trade.price
           << " trade" << index << "_quantity=" << trade.quantity
           << " trade" << index << "_sequence=" << trade.sequence;
}

void append_report(std::ostream& output,
                   const domain::ExecutionReport& report,
                   std::size_t index) {
    output << " report" << index << "_order_id=" << report.order_id
           << " report" << index << "_instrument_id=" << report.instrument_id
           << " report" << index << "_type=" << to_text(report.type)
           << " report" << index << "_status=" << to_text(report.status)
           << " report" << index << "_filled_quantity=" << report.filled_quantity
           << " report" << index << "_remaining_quantity=" << report.remaining_quantity
           << " report" << index << "_last_price=" << report.last_price
           << " report" << index << "_last_quantity=" << report.last_quantity
           << " report" << index << "_reject_reason=" << to_text(report.reject_reason)
           << " report" << index << "_sequence=" << report.sequence;
}

}  // namespace

bool GatewayResponse::accepted() const noexcept {
    return status == GatewayResponseStatus::Accepted;
}

bool GatewayResponse::rejected() const noexcept {
    return status == GatewayResponseStatus::Rejected;
}

const char* to_text(GatewayResponseStatus status) noexcept {
    return status == GatewayResponseStatus::Accepted ? "ACCEPTED" : "REJECTED";
}

const char* to_text(GatewayRejectReason reason) noexcept {
    switch (reason) {
        case GatewayRejectReason::None:
            return "NONE";
        case GatewayRejectReason::ParseError:
            return "PARSE_ERROR";
        case GatewayRejectReason::ReplayValidationError:
            return "REPLAY_VALIDATION_ERROR";
        case GatewayRejectReason::EngineRejected:
            return "ENGINE_REJECTED";
    }

    return "NONE";
}

const char* to_text(protocol::TextCommandParseError error) noexcept {
    switch (error) {
        case protocol::TextCommandParseError::None:
            return "NONE";
        case protocol::TextCommandParseError::EmptyLine:
            return "EMPTY_LINE";
        case protocol::TextCommandParseError::UnknownCommand:
            return "UNKNOWN_COMMAND";
        case protocol::TextCommandParseError::MalformedToken:
            return "MALFORMED_TOKEN";
        case protocol::TextCommandParseError::MissingField:
            return "MISSING_FIELD";
        case protocol::TextCommandParseError::InvalidNumber:
            return "INVALID_NUMBER";
        case protocol::TextCommandParseError::InvalidSide:
            return "INVALID_SIDE";
        case protocol::TextCommandParseError::InvalidOrderType:
            return "INVALID_ORDER_TYPE";
        case protocol::TextCommandParseError::InvalidTimeInForce:
            return "INVALID_TIME_IN_FORCE";
    }

    return "NONE";
}

const char* to_text(replay::ReplayCommandType type) noexcept {
    switch (type) {
        case replay::ReplayCommandType::SubmitOrder:
            return "SUBMIT";
        case replay::ReplayCommandType::CancelOrder:
            return "CANCEL";
    }

    return "SUBMIT";
}

const char* to_text(replay::ReplayEventStatus status) noexcept {
    switch (status) {
        case replay::ReplayEventStatus::Applied:
            return "APPLIED";
        case replay::ReplayEventStatus::InvalidInputSequence:
            return "INVALID_INPUT_SEQUENCE";
        case replay::ReplayEventStatus::InvalidReferenceVersion:
            return "INVALID_REFERENCE_VERSION";
        case replay::ReplayEventStatus::CommandSequenceMismatch:
            return "COMMAND_SEQUENCE_MISMATCH";
        case replay::ReplayEventStatus::ReferenceVersionMismatch:
            return "REFERENCE_VERSION_MISMATCH";
    }

    return "APPLIED";
}

std::string format_gateway_response(const GatewayResponse& response) {
    std::ostringstream output;
    output << to_text(response.status)
           << " reason=" << to_text(response.reject_reason)
           << " seq=" << response.input_sequence
           << " command=" << command_type_to_text(response.command_type)
           << " detail=" << detail_or_none(response.detail)
           << " trades=" << response.trades.size()
           << " reports=" << response.reports.size();

    for (std::size_t index = 0; index < response.trades.size(); ++index) {
        append_trade(output, response.trades[index], index);
    }

    for (std::size_t index = 0; index < response.reports.size(); ++index) {
        append_report(output, response.reports[index], index);
    }

    return output.str();
}

GatewayResponse make_parse_reject_response(
    const protocol::TextCommandParseResult& parse_result) {
    return GatewayResponse{
        .status = GatewayResponseStatus::Rejected,
        .reject_reason = GatewayRejectReason::ParseError,
        .detail = build_parse_detail(parse_result),
    };
}

GatewayResponse make_replay_reject_response(
    const replay::ReplayEvent& event,
    replay::ReplayEventStatus status) {
    return GatewayResponse{
        .status = GatewayResponseStatus::Rejected,
        .reject_reason = GatewayRejectReason::ReplayValidationError,
        .command_type = event.type(),
        .input_sequence = event.input_sequence,
        .detail = to_text(status),
    };
}

GatewayResponse make_engine_response(
    const replay::ReplayEvent& event,
    const replay::ReplayApplyResult& apply_result) {
    if (!apply_result.applied()) {
        return make_replay_reject_response(event, apply_result.status);
    }

    const bool rejected = has_rejected_report(apply_result.reports);
    return GatewayResponse{
        .status = rejected ? GatewayResponseStatus::Rejected : GatewayResponseStatus::Accepted,
        .reject_reason = rejected ? GatewayRejectReason::EngineRejected : GatewayRejectReason::None,
        .command_type = event.type(),
        .input_sequence = event.input_sequence,
        .detail = rejected ? to_text(GatewayRejectReason::EngineRejected)
                           : to_text(GatewayResponseStatus::Accepted),
        .trades = apply_result.trades,
        .reports = apply_result.reports,
    };
}

GatewayResponse handle_text_command(engine::MatchingEngine& engine, std::string_view command) {
    const auto parse_result = protocol::parse_text_command(command);
    if (!parse_result.ok()) {
        return make_parse_reject_response(parse_result);
    }

    const auto apply_result = replay::apply_replay_event(engine, *parse_result.event);
    return make_engine_response(*parse_result.event, apply_result);
}

GatewayResponse handle_gateway_request(engine::MatchingEngine& engine,
                                       const GatewayRequest& request) {
    return handle_text_command(engine, request.text);
}

}  // namespace mini_ats::gateway
