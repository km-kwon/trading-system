#pragma once

#include "gateway/order_gateway.hpp"
#include "marketdata/market_data_publisher.hpp"

#include <iosfwd>
#include <cstddef>
#include <string_view>
#include <vector>

namespace mini_ats::gateway {

struct RecordedGatewayCommandResult {
    GatewayResponse response{};
    bool recorded{false};
};

struct PublishedGatewayCommandResult {
    GatewayResponse response{};
    bool recorded{false};
    std::vector<marketdata::MarketDataEvent> market_data_events{};
    marketdata::MarketDataPublishResult publish_result{};
};

[[nodiscard]] RecordedGatewayCommandResult handle_recorded_text_command(
    engine::MatchingEngine& engine,
    std::string_view command,
    std::ostream& accepted_input_log);
[[nodiscard]] PublishedGatewayCommandResult handle_published_text_command(
    engine::MatchingEngine& engine,
    std::string_view command,
    marketdata::UdpMarketDataPublisher& publisher,
    std::ostream* accepted_input_log = nullptr,
    std::size_t market_data_depth = 1);

}  // namespace mini_ats::gateway
