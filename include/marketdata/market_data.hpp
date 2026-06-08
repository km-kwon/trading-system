#pragma once

#include "domain/trade.hpp"
#include "domain/types.hpp"
#include "engine/matching_engine.hpp"
#include "engine/order_book.hpp"

#include <cstddef>
#include <optional>
#include <variant>
#include <vector>

namespace mini_ats::marketdata {

enum class MarketDataEventType {
    Trade,
    BookUpdate,
};

struct MarketDataLevel {
    domain::Side side{};
    domain::Price price{};
    domain::Quantity total_quantity{};

    [[nodiscard]] bool empty() const noexcept;
};

struct TradeEvent {
    domain::TradeId trade_id{};
    domain::InstrumentId instrument_id{};
    domain::OrderId resting_order_id{};
    domain::OrderId incoming_order_id{};
    domain::Side aggressor_side{domain::Side::Buy};
    domain::Price price{};
    domain::Quantity quantity{};
    domain::SequenceNumber sequence{};
};

struct BookUpdateEvent {
    domain::InstrumentId instrument_id{};
    domain::SequenceNumber sequence{};
    std::optional<MarketDataLevel> best_bid{};
    std::optional<MarketDataLevel> best_ask{};
    std::vector<MarketDataLevel> bids{};
    std::vector<MarketDataLevel> asks{};

    [[nodiscard]] bool has_best_bid() const noexcept;
    [[nodiscard]] bool has_best_ask() const noexcept;
};

using MarketDataEvent = std::variant<TradeEvent, BookUpdateEvent>;

[[nodiscard]] MarketDataEventType event_type(const MarketDataEvent& event) noexcept;
[[nodiscard]] TradeEvent to_trade_event(const domain::Trade& trade) noexcept;
[[nodiscard]] BookUpdateEvent to_book_update_event(
    const engine::OrderBookSnapshot& snapshot,
    domain::SequenceNumber sequence,
    std::size_t depth = 1);
[[nodiscard]] std::vector<MarketDataEvent> market_data_events_for(
    const engine::SubmitOrderResult& result,
    const engine::OrderBookSnapshot& snapshot,
    std::size_t depth = 1);
[[nodiscard]] std::vector<MarketDataEvent> market_data_events_for(
    const engine::CancelOrderResult& result,
    const engine::OrderBookSnapshot& snapshot,
    std::size_t depth = 1);

}  // namespace mini_ats::marketdata
