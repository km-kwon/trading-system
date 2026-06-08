#include "engine/matching_engine.hpp"

#include <algorithm>

namespace mini_ats::engine {

namespace {

domain::Side opposite_side(domain::Side side) noexcept {
    return side == domain::Side::Buy ? domain::Side::Sell : domain::Side::Buy;
}

}  // namespace

MatchingEngine::MatchingEngine(domain::InstrumentId instrument_id)
    : MatchingEngine{domain::default_instrument_reference(instrument_id)} {}

MatchingEngine::MatchingEngine(const domain::InstrumentReference& instrument)
    : order_book_{instrument.id}, instrument_{instrument} {}

domain::InstrumentId MatchingEngine::instrument_id() const noexcept {
    return order_book_.instrument_id();
}

const domain::InstrumentReference& MatchingEngine::instrument() const noexcept {
    return instrument_;
}

const OrderBook& MatchingEngine::order_book() const noexcept {
    return order_book_;
}

SubmitOrderResult MatchingEngine::submit_order(const domain::Order& order) {
    SubmitOrderResult result{};

    const auto reject_reason = reject_reason_for(order);
    if (reject_reason != domain::RejectReason::None) {
        result.reports.push_back(reject_report(order, reject_reason));
        return result;
    }

    if (accepted_order_ids_.contains(order.id)) {
        result.reports.push_back(reject_report(order, domain::RejectReason::DuplicateOrderId));
        return result;
    }

    accepted_order_ids_.insert(order.id);

    if (requires_full_execution(order) && executable_quantity(order) < order.quantity) {
        result.reports.push_back(reject_report(order, domain::RejectReason::WouldNotExecute));
        return result;
    }

    domain::Quantity incoming_remaining = order.quantity;
    domain::Quantity incoming_filled = 0;

    while (incoming_remaining > 0) {
        const BookOrder* resting_pointer = best_opposite_order(order.side);
        if (resting_pointer == nullptr || !crosses(order, *resting_pointer)) {
            break;
        }

        const BookOrder resting = *resting_pointer;
        const domain::Quantity executed_quantity =
            std::min(incoming_remaining, resting.remaining_quantity);
        const domain::Price executed_price = resting.price();

        result.trades.push_back(domain::Trade{
            .id = next_trade_id_++,
            .instrument_id = order.instrument_id,
            .resting_order_id = resting.id(),
            .incoming_order_id = order.id,
            .aggressor_side = order.side,
            .price = executed_price,
            .quantity = executed_quantity,
            .sequence = next_event_sequence_++,
        });

        incoming_remaining -= executed_quantity;
        incoming_filled += executed_quantity;

        const domain::Quantity resting_remaining =
            resting.remaining_quantity - executed_quantity;
        const auto resting_status =
            resting_remaining == 0 ? domain::OrderStatus::Filled
                                   : domain::OrderStatus::PartiallyFilled;

        result.reports.push_back(make_report(resting.id(), resting.order.instrument_id,
                                             domain::ExecutionType::Trade, resting_status,
                                             executed_quantity, resting_remaining, executed_price,
                                             executed_quantity, domain::RejectReason::None));

        const auto incoming_status = status_for_remaining(incoming_filled, incoming_remaining);
        result.reports.push_back(make_report(order.id, order.instrument_id,
                                             domain::ExecutionType::Trade, incoming_status,
                                             incoming_filled, incoming_remaining, executed_price,
                                             executed_quantity, domain::RejectReason::None));

        if (!order_book_.reduce_order(resting.id(), executed_quantity)) {
            result.reports.push_back(reject_report(order, domain::RejectReason::InternalError));
            return result;
        }
    }

    if (incoming_remaining > 0) {
        if (can_rest(order)) {
            if (!order_book_.add_order(order, incoming_remaining)) {
                result.reports.push_back(reject_report(order, domain::RejectReason::InternalError));
                return result;
            }

            if (incoming_filled == 0) {
                result.reports.push_back(make_report(order.id, order.instrument_id,
                                                     domain::ExecutionType::Accepted,
                                                     domain::OrderStatus::Accepted,
                                                     domain::Quantity{0}, incoming_remaining,
                                                     domain::Price{0}, domain::Quantity{0},
                                                     domain::RejectReason::None));
            }
        } else {
            result.reports.push_back(make_report(order.id, order.instrument_id,
                                                 domain::ExecutionType::Canceled,
                                                 domain::OrderStatus::Canceled,
                                                 incoming_filled, domain::Quantity{0},
                                                 domain::Price{0}, domain::Quantity{0},
                                                 domain::RejectReason::None));
        }
    }

    return result;
}

CancelOrderResult MatchingEngine::cancel_order(const domain::CancelRequest& request) {
    CancelOrderResult result{};

    const auto reject_reason = reject_reason_for(request);
    if (reject_reason != domain::RejectReason::None) {
        result.reports.push_back(make_report(request.order_id, request.instrument_id,
                                             domain::ExecutionType::Rejected,
                                             domain::OrderStatus::Rejected, domain::Quantity{0},
                                             domain::Quantity{0}, domain::Price{0},
                                             domain::Quantity{0}, reject_reason));
        return result;
    }

    const BookOrder* resting_order = order_book_.find_order(request.order_id);
    if (resting_order == nullptr) {
        result.reports.push_back(make_report(request.order_id, request.instrument_id,
                                             domain::ExecutionType::Rejected,
                                             domain::OrderStatus::Rejected, domain::Quantity{0},
                                             domain::Quantity{0}, domain::Price{0},
                                             domain::Quantity{0},
                                             domain::RejectReason::OrderNotFound));
        return result;
    }

    const domain::Quantity remaining_quantity = resting_order->remaining_quantity;
    const domain::Quantity filled_quantity = resting_order->order.quantity - remaining_quantity;

    if (!order_book_.cancel_order(request)) {
        result.reports.push_back(make_report(request.order_id, request.instrument_id,
                                             domain::ExecutionType::Rejected,
                                             domain::OrderStatus::Rejected, domain::Quantity{0},
                                             domain::Quantity{0}, domain::Price{0},
                                             domain::Quantity{0},
                                             domain::RejectReason::InternalError));
        return result;
    }

    result.reports.push_back(make_report(request.order_id, request.instrument_id,
                                         domain::ExecutionType::Canceled,
                                         domain::OrderStatus::Canceled, filled_quantity,
                                         domain::Quantity{0}, domain::Price{0},
                                         domain::Quantity{0}, domain::RejectReason::None));
    return result;
}

bool MatchingEngine::can_accept(const domain::Order& order) const {
    return reject_reason_for(order) == domain::RejectReason::None;
}

domain::RejectReason MatchingEngine::reject_reason_for(const domain::Order& order) const {
    if (!instrument_.has_valid_instrument_id() || order.instrument_id != instrument_id()) {
        return domain::RejectReason::UnknownInstrument;
    }

    if (!instrument_.is_open()) {
        return domain::RejectReason::MarketClosed;
    }

    if (!order.has_valid_quantity()) {
        return domain::RejectReason::InvalidQuantity;
    }

    if (!order.is_limit() && !order.is_market()) {
        return domain::RejectReason::WouldNotExecute;
    }

    if (!order.has_valid_price()) {
        return domain::RejectReason::InvalidPrice;
    }

    if (order.is_limit() && !instrument_.accepts_limit_price(order.price)) {
        return domain::RejectReason::InvalidPrice;
    }

    if (order.id == 0) {
        return domain::RejectReason::InvalidOrderId;
    }

    return domain::RejectReason::None;
}

domain::RejectReason MatchingEngine::reject_reason_for(
    const domain::CancelRequest& request) const {
    if (!request.has_valid_order_id()) {
        return domain::RejectReason::InvalidOrderId;
    }

    if (request.instrument_id != instrument_id()) {
        return domain::RejectReason::UnknownInstrument;
    }

    return domain::RejectReason::None;
}

bool MatchingEngine::can_rest(const domain::Order& order) const noexcept {
    return order.is_limit() && order.time_in_force == domain::TimeInForce::Day;
}

bool MatchingEngine::requires_full_execution(const domain::Order& order) const noexcept {
    return order.time_in_force == domain::TimeInForce::FOK;
}

bool MatchingEngine::crosses(const domain::Order& incoming, const BookOrder& resting) const
    noexcept {
    return crosses_price(incoming, resting.price());
}

bool MatchingEngine::crosses_price(const domain::Order& incoming,
                                   domain::Price resting_price) const noexcept {
    if (incoming.is_market()) {
        return true;
    }

    if (incoming.side == domain::Side::Buy) {
        return incoming.price >= resting_price;
    }

    return incoming.price <= resting_price;
}

const BookOrder* MatchingEngine::best_opposite_order(domain::Side incoming_side) const {
    return opposite_side(incoming_side) == domain::Side::Buy ? order_book_.best_bid_order()
                                                            : order_book_.best_ask_order();
}

domain::Quantity MatchingEngine::executable_quantity(const domain::Order& order) const {
    domain::Quantity total_quantity = 0;
    const auto snapshot = order_book_.snapshot();
    const auto& levels = order.side == domain::Side::Buy ? snapshot.asks : snapshot.bids;

    for (const auto& level : levels) {
        if (!crosses_price(order, level.price)) {
            break;
        }

        total_quantity += level.total_quantity;
        if (total_quantity >= order.quantity) {
            return total_quantity;
        }
    }

    return total_quantity;
}

domain::OrderStatus MatchingEngine::status_for_remaining(
    domain::Quantity filled_quantity,
    domain::Quantity remaining_quantity) const noexcept {
    if (remaining_quantity == 0) {
        return domain::OrderStatus::Filled;
    }

    return filled_quantity == 0 ? domain::OrderStatus::Accepted
                                : domain::OrderStatus::PartiallyFilled;
}

domain::ExecutionReport MatchingEngine::make_report(
    domain::OrderId order_id,
    domain::InstrumentId instrument_id,
    domain::ExecutionType type,
    domain::OrderStatus status,
    domain::Quantity filled_quantity,
    domain::Quantity remaining_quantity,
    domain::Price last_price,
    domain::Quantity last_quantity,
    domain::RejectReason reject_reason) {
    return domain::ExecutionReport{
        .order_id = order_id,
        .instrument_id = instrument_id,
        .type = type,
        .status = status,
        .filled_quantity = filled_quantity,
        .remaining_quantity = remaining_quantity,
        .last_price = last_price,
        .last_quantity = last_quantity,
        .reject_reason = reject_reason,
        .sequence = next_event_sequence_++,
    };
}

domain::ExecutionReport MatchingEngine::reject_report(const domain::Order& order,
                                                      domain::RejectReason reason) {
    return make_report(order.id, order.instrument_id, domain::ExecutionType::Rejected,
                       domain::OrderStatus::Rejected, domain::Quantity{0}, domain::Quantity{0},
                       domain::Price{0}, domain::Quantity{0}, reason);
}

}  // namespace mini_ats::engine
