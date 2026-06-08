#pragma once

#include "domain/cancel_request.hpp"
#include "domain/execution_report.hpp"
#include "domain/instrument.hpp"
#include "domain/order.hpp"
#include "domain/trade.hpp"
#include "engine/order_book.hpp"

#include <unordered_set>
#include <vector>

namespace mini_ats::engine {

struct SubmitOrderResult {
    std::vector<domain::Trade> trades{};
    std::vector<domain::ExecutionReport> reports{};
};

struct CancelOrderResult {
    std::vector<domain::ExecutionReport> reports{};
};

class MatchingEngine {
public:
    explicit MatchingEngine(domain::InstrumentId instrument_id);
    explicit MatchingEngine(const domain::InstrumentReference& instrument);

    [[nodiscard]] domain::InstrumentId instrument_id() const noexcept;
    [[nodiscard]] const domain::InstrumentReference& instrument() const noexcept;
    [[nodiscard]] const OrderBook& order_book() const noexcept;

    [[nodiscard]] SubmitOrderResult submit_order(const domain::Order& order);
    [[nodiscard]] CancelOrderResult cancel_order(const domain::CancelRequest& request);

private:
    [[nodiscard]] bool can_accept(const domain::Order& order) const;
    [[nodiscard]] domain::RejectReason reject_reason_for(const domain::Order& order) const;
    [[nodiscard]] domain::RejectReason reject_reason_for(
        const domain::CancelRequest& request) const;
    [[nodiscard]] bool can_rest(const domain::Order& order) const noexcept;
    [[nodiscard]] bool requires_full_execution(const domain::Order& order) const noexcept;
    [[nodiscard]] bool crosses(const domain::Order& incoming,
                               const BookOrder& resting) const noexcept;
    [[nodiscard]] bool crosses_price(const domain::Order& incoming,
                                     domain::Price resting_price) const noexcept;
    [[nodiscard]] const BookOrder* best_opposite_order(domain::Side incoming_side) const;
    [[nodiscard]] domain::Quantity executable_quantity(const domain::Order& order) const;
    [[nodiscard]] domain::OrderStatus status_for_remaining(
        domain::Quantity filled_quantity,
        domain::Quantity remaining_quantity) const noexcept;

    [[nodiscard]] domain::ExecutionReport make_report(
        domain::OrderId order_id,
        domain::InstrumentId instrument_id,
        domain::ExecutionType type,
        domain::OrderStatus status,
        domain::Quantity filled_quantity,
        domain::Quantity remaining_quantity,
        domain::Price last_price,
        domain::Quantity last_quantity,
        domain::RejectReason reject_reason);

    [[nodiscard]] domain::ExecutionReport reject_report(const domain::Order& order,
                                                        domain::RejectReason reason);

    OrderBook order_book_;
    domain::InstrumentReference instrument_;
    std::unordered_set<domain::OrderId> accepted_order_ids_{};
    domain::TradeId next_trade_id_{1};
    domain::SequenceNumber next_event_sequence_{1};
};

}  // namespace mini_ats::engine
