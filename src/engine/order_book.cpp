#include "engine/order_book.hpp"

#include <iterator>
#include <sstream>

namespace mini_ats::engine {

domain::OrderId BookOrder::id() const noexcept {
    return order.id;
}

domain::Side BookOrder::side() const noexcept {
    return order.side;
}

domain::Price BookOrder::price() const noexcept {
    return order.price;
}

domain::SequenceNumber BookOrder::sequence() const noexcept {
    return order.sequence;
}

OrderBook::OrderBook(domain::InstrumentId instrument_id) : instrument_id_{instrument_id} {}

domain::InstrumentId OrderBook::instrument_id() const noexcept {
    return instrument_id_;
}

bool OrderBook::empty() const noexcept {
    return locations_.empty();
}

std::size_t OrderBook::order_count() const noexcept {
    return locations_.size();
}

bool OrderBook::contains_order(domain::OrderId order_id) const {
    return locations_.contains(order_id);
}

bool OrderBook::add_order(const domain::Order& order) {
    return add_order(order, order.quantity);
}

bool OrderBook::add_order(const domain::Order& order, domain::Quantity remaining_quantity) {
    if (!can_rest(order) || contains_order(order.id)) {
        return false;
    }

    if (remaining_quantity <= 0 || remaining_quantity > order.quantity) {
        return false;
    }

    BookOrder book_order{
        .order = order,
        .remaining_quantity = remaining_quantity,
    };

    if (order.side == domain::Side::Buy) {
        auto& level = bids_[order.price];
        level.orders.push_back(book_order);
        level.total_quantity += remaining_quantity;
        locations_.emplace(order.id,
                           OrderLocation{order.side, order.price, std::prev(level.orders.end())});
        return true;
    }

    auto& level = asks_[order.price];
    level.orders.push_back(book_order);
    level.total_quantity += remaining_quantity;
    locations_.emplace(order.id,
                       OrderLocation{order.side, order.price, std::prev(level.orders.end())});
    return true;
}

bool OrderBook::reduce_order(domain::OrderId order_id, domain::Quantity executed_quantity) {
    if (executed_quantity <= 0) {
        return false;
    }

    auto location = locations_.find(order_id);
    if (location == locations_.end()) {
        return false;
    }

    auto& book_order = *location->second.iterator;
    if (executed_quantity > book_order.remaining_quantity) {
        return false;
    }

    const auto side = location->second.side;
    const auto price = location->second.price;
    const bool fully_filled = executed_quantity == book_order.remaining_quantity;

    if (side == domain::Side::Buy) {
        auto level = bids_.find(price);
        level->second.total_quantity -= executed_quantity;
        if (fully_filled) {
            level->second.orders.erase(location->second.iterator);
            if (level->second.orders.empty()) {
                bids_.erase(level);
            }
            locations_.erase(location);
            return true;
        }
    } else {
        auto level = asks_.find(price);
        level->second.total_quantity -= executed_quantity;
        if (fully_filled) {
            level->second.orders.erase(location->second.iterator);
            if (level->second.orders.empty()) {
                asks_.erase(level);
            }
            locations_.erase(location);
            return true;
        }
    }

    book_order.remaining_quantity -= executed_quantity;
    return true;
}

bool OrderBook::cancel_order(const domain::CancelRequest& request) {
    if (!request.has_valid_order_id() || request.instrument_id != instrument_id_) {
        return false;
    }

    auto location = locations_.find(request.order_id);
    if (location == locations_.end()) {
        return false;
    }

    erase_location(location);
    return true;
}

const BookOrder* OrderBook::find_order(domain::OrderId order_id) const {
    auto location = locations_.find(order_id);
    if (location == locations_.end()) {
        return nullptr;
    }

    return &(*location->second.iterator);
}

const BookOrder* OrderBook::best_bid_order() const {
    if (bids_.empty() || bids_.begin()->second.orders.empty()) {
        return nullptr;
    }

    return &bids_.begin()->second.orders.front();
}

const BookOrder* OrderBook::best_ask_order() const {
    if (asks_.empty() || asks_.begin()->second.orders.empty()) {
        return nullptr;
    }

    return &asks_.begin()->second.orders.front();
}

std::optional<domain::Price> OrderBook::best_bid_price() const {
    if (bids_.empty()) {
        return std::nullopt;
    }

    return bids_.begin()->first;
}

std::optional<domain::Price> OrderBook::best_ask_price() const {
    if (asks_.empty()) {
        return std::nullopt;
    }

    return asks_.begin()->first;
}

domain::Quantity OrderBook::total_quantity_at(domain::Side side, domain::Price price) const {
    if (side == domain::Side::Buy) {
        auto level = bids_.find(price);
        return level == bids_.end() ? domain::Quantity{0} : level->second.total_quantity;
    }

    auto level = asks_.find(price);
    return level == asks_.end() ? domain::Quantity{0} : level->second.total_quantity;
}

std::size_t OrderBook::price_level_count(domain::Side side) const noexcept {
    return side == domain::Side::Buy ? bids_.size() : asks_.size();
}

OrderBookSnapshot OrderBook::snapshot() const {
    OrderBookSnapshot snapshot{
        .instrument_id = instrument_id_,
    };

    for (const auto& [price, level] : asks_) {
        PriceLevelSnapshot price_level{
            .side = domain::Side::Sell,
            .price = price,
            .total_quantity = level.total_quantity,
        };

        for (const auto& order : level.orders) {
            price_level.orders.push_back(BookOrderSnapshot{
                .order_id = order.id(),
                .remaining_quantity = order.remaining_quantity,
                .sequence = order.sequence(),
            });
        }

        snapshot.asks.push_back(price_level);
    }

    for (const auto& [price, level] : bids_) {
        PriceLevelSnapshot price_level{
            .side = domain::Side::Buy,
            .price = price,
            .total_quantity = level.total_quantity,
        };

        for (const auto& order : level.orders) {
            price_level.orders.push_back(BookOrderSnapshot{
                .order_id = order.id(),
                .remaining_quantity = order.remaining_quantity,
                .sequence = order.sequence(),
            });
        }

        snapshot.bids.push_back(price_level);
    }

    return snapshot;
}

bool OrderBook::can_rest(const domain::Order& order) const {
    return order.id != 0 && order.instrument_id == instrument_id_ && order.is_limit() &&
           order.time_in_force == domain::TimeInForce::Day && order.has_valid_price() &&
           order.has_valid_quantity();
}

void OrderBook::erase_location(
    std::unordered_map<domain::OrderId, OrderLocation>::iterator location) {
    const auto side = location->second.side;
    const auto price = location->second.price;
    const auto remaining_quantity = location->second.iterator->remaining_quantity;

    if (side == domain::Side::Buy) {
        auto level = bids_.find(price);
        level->second.total_quantity -= remaining_quantity;
        level->second.orders.erase(location->second.iterator);
        if (level->second.orders.empty()) {
            bids_.erase(level);
        }
    } else {
        auto level = asks_.find(price);
        level->second.total_quantity -= remaining_quantity;
        level->second.orders.erase(location->second.iterator);
        if (level->second.orders.empty()) {
            asks_.erase(level);
        }
    }

    locations_.erase(location);
}

std::string format_order_book(const OrderBookSnapshot& snapshot) {
    std::ostringstream output;
    output << "OrderBook instrument=" << snapshot.instrument_id << '\n';
    output << "ASK best-first\n";

    if (snapshot.asks.empty()) {
        output << "  (empty)\n";
    } else {
        for (const auto& level : snapshot.asks) {
            output << "  " << level.price << " | total=" << level.total_quantity << " | ";

            for (std::size_t index = 0; index < level.orders.size(); ++index) {
                const auto& order = level.orders[index];
                if (index > 0) {
                    output << " -> ";
                }
                output << "#" << order.order_id << "(rem=" << order.remaining_quantity
                       << ",seq=" << order.sequence << ")";
            }

            output << '\n';
        }
    }

    output << "----- spread -----\n";
    output << "BID best-first\n";

    if (snapshot.bids.empty()) {
        output << "  (empty)\n";
    } else {
        for (const auto& level : snapshot.bids) {
            output << "  " << level.price << " | total=" << level.total_quantity << " | ";

            for (std::size_t index = 0; index < level.orders.size(); ++index) {
                const auto& order = level.orders[index];
                if (index > 0) {
                    output << " -> ";
                }
                output << "#" << order.order_id << "(rem=" << order.remaining_quantity
                       << ",seq=" << order.sequence << ")";
            }

            output << '\n';
        }
    }

    return output.str();
}

}  // namespace mini_ats::engine
