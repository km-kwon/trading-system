#pragma once

#include "domain/execution_report.hpp"
#include "domain/trade.hpp"
#include "engine/matching_engine.hpp"
#include "protocol/text_command_parser.hpp"
#include "replay/replay_log.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mini_ats::gateway {

enum class GatewayResponseStatus {
    Accepted,
    Rejected,
};

enum class GatewayRejectReason {
    None,
    ParseError,
    ReplayValidationError,
    EngineRejected,
};

struct GatewayRequest {
    std::string text{};
};

struct GatewayResponse {
    GatewayResponseStatus status{GatewayResponseStatus::Accepted};
    GatewayRejectReason reject_reason{GatewayRejectReason::None};
    std::optional<replay::ReplayCommandType> command_type{};
    domain::SequenceNumber input_sequence{};
    std::string detail{};
    std::vector<domain::Trade> trades{};
    std::vector<domain::ExecutionReport> reports{};

    [[nodiscard]] bool accepted() const noexcept;
    [[nodiscard]] bool rejected() const noexcept;
};

[[nodiscard]] const char* to_text(GatewayResponseStatus status) noexcept;
[[nodiscard]] const char* to_text(GatewayRejectReason reason) noexcept;
[[nodiscard]] const char* to_text(protocol::TextCommandParseError error) noexcept;
[[nodiscard]] const char* to_text(replay::ReplayCommandType type) noexcept;
[[nodiscard]] const char* to_text(replay::ReplayEventStatus status) noexcept;
[[nodiscard]] std::string format_gateway_response(const GatewayResponse& response);

[[nodiscard]] GatewayResponse make_parse_reject_response(
    const protocol::TextCommandParseResult& parse_result);
[[nodiscard]] GatewayResponse make_replay_reject_response(
    const replay::ReplayEvent& event,
    replay::ReplayEventStatus status);
[[nodiscard]] GatewayResponse make_engine_response(
    const replay::ReplayEvent& event,
    const replay::ReplayApplyResult& apply_result);

[[nodiscard]] GatewayResponse handle_text_command(engine::MatchingEngine& engine,
                                                  std::string_view command);
[[nodiscard]] GatewayResponse handle_gateway_request(engine::MatchingEngine& engine,
                                                     const GatewayRequest& request);

}  // namespace mini_ats::gateway
