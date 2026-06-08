#include "marketdata/market_data.hpp"

#include <algorithm>

namespace mini_ats::marketdata {

namespace {

[[nodiscard]] MarketDataLevel to_level(
    const engine::PriceLevelSnapshot& level) noexcept {
    return MarketDataLevel{
        .side = level.side,
        .price = level.price,
        .total_quantity = level.total_quantity,
    };
}

[[nodiscard]] std::vector<MarketDataLevel> to_levels(
    const std::vector<engine::PriceLevelSnapshot>& levels,
    std::size_t depth) {
    std::vector<MarketDataLevel> result{};
    const auto count = std::min(depth, levels.size());
    result.reserve(count);

    for (std::size_t index = 0; index < count; ++index) {
        result.push_back(to_level(levels[index]));
    }

    return result;
}

[[nodiscard]] domain::SequenceNumber max_report_sequence(
    const std::vector<domain::ExecutionReport>& reports) noexcept {
    domain::SequenceNumber sequence{};
    for (const auto& report : reports) {
        sequence = std::max(sequence, report.sequence);
    }

    return sequence;
}

[[nodiscard]] bool has_non_rejected_report(
    const std::vector<domain::ExecutionReport>& reports) noexcept {
    return std::any_of(reports.begin(), reports.end(), [](const auto& report) {
        return !report.is_rejected();
    });
}

[[nodiscard]] bool submit_changed_book(const engine::SubmitOrderResult& result) noexcept {
    if (!result.trades.empty()) {
        return true;
    }

    return std::any_of(result.reports.begin(), result.reports.end(), [](const auto& report) {
        return report.type == domain::ExecutionType::Accepted &&
               report.status == domain::OrderStatus::Accepted &&
               report.remaining_quantity > 0;
    });
}

[[nodiscard]] bool cancel_changed_book(const engine::CancelOrderResult& result) noexcept {
    return std::any_of(result.reports.begin(), result.reports.end(), [](const auto& report) {
        return report.type == domain::ExecutionType::Canceled && !report.is_rejected();
    });
}

}  // namespace

bool MarketDataLevel::empty() const noexcept {
    return total_quantity == 0;
}

bool BookUpdateEvent::has_best_bid() const noexcept {
    return best_bid.has_value();
}

bool BookUpdateEvent::has_best_ask() const noexcept {
    return best_ask.has_value();
}

MarketDataEventType event_type(const MarketDataEvent& event) noexcept {
    return std::holds_alternative<TradeEvent>(event) ? MarketDataEventType::Trade
                                                     : MarketDataEventType::BookUpdate;
}

TradeEvent to_trade_event(const domain::Trade& trade) noexcept {
    return TradeEvent{
        .trade_id = trade.id,
        .instrument_id = trade.instrument_id,
        .resting_order_id = trade.resting_order_id,
        .incoming_order_id = trade.incoming_order_id,
        .aggressor_side = trade.aggressor_side,
        .price = trade.price,
        .quantity = trade.quantity,
        .sequence = trade.sequence,
    };
}

BookUpdateEvent to_book_update_event(const engine::OrderBookSnapshot& snapshot,
                                     domain::SequenceNumber sequence,
                                     std::size_t depth) {
    BookUpdateEvent event{
        .instrument_id = snapshot.instrument_id,
        .sequence = sequence,
        .bids = to_levels(snapshot.bids, depth),
        .asks = to_levels(snapshot.asks, depth),
    };

    if (!event.bids.empty()) {
        event.best_bid = event.bids.front();
    }

    if (!event.asks.empty()) {
        event.best_ask = event.asks.front();
    }

    return event;
}

std::vector<MarketDataEvent> market_data_events_for(
    const engine::SubmitOrderResult& result,
    const engine::OrderBookSnapshot& snapshot,
    std::size_t depth) {
    std::vector<MarketDataEvent> events{};
    events.reserve(result.trades.size() + 1);

    for (const auto& trade : result.trades) {
        events.push_back(to_trade_event(trade));
    }

    if (has_non_rejected_report(result.reports) && submit_changed_book(result)) {
        events.push_back(to_book_update_event(snapshot, max_report_sequence(result.reports),
                                             depth));
    }

    return events;
}

std::vector<MarketDataEvent> market_data_events_for(
    const engine::CancelOrderResult& result,
    const engine::OrderBookSnapshot& snapshot,
    std::size_t depth) {
    if (!has_non_rejected_report(result.reports) || !cancel_changed_book(result)) {
        return {};
    }

    return {
        to_book_update_event(snapshot, max_report_sequence(result.reports), depth),
    };
}

}  // namespace mini_ats::marketdata
