#pragma once

#include "domain/cancel_request.hpp"
#include "domain/order.hpp"
#include "domain/types.hpp"

#include <cstddef>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace mini_ats::engine {

struct BookOrder {
    domain::Order order{};
    domain::Quantity remaining_quantity{};

    [[nodiscard]] domain::OrderId id() const noexcept;
    [[nodiscard]] domain::Side side() const noexcept;
    [[nodiscard]] domain::Price price() const noexcept;
    [[nodiscard]] domain::SequenceNumber sequence() const noexcept;
};

struct BookOrderSnapshot {
    domain::OrderId order_id{};
    domain::Quantity remaining_quantity{};
    domain::SequenceNumber sequence{};
};

struct PriceLevelSnapshot {
    domain::Side side{};
    domain::Price price{};
    domain::Quantity total_quantity{};
    std::vector<BookOrderSnapshot> orders{};
};

struct OrderBookSnapshot {
    domain::InstrumentId instrument_id{};
    std::vector<PriceLevelSnapshot> asks{};
    std::vector<PriceLevelSnapshot> bids{};
};

class OrderBook {
public:
    explicit OrderBook(domain::InstrumentId instrument_id);

    [[nodiscard]] domain::InstrumentId instrument_id() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t order_count() const noexcept;
    [[nodiscard]] bool contains_order(domain::OrderId order_id) const;

    [[nodiscard]] bool add_order(const domain::Order& order);
    [[nodiscard]] bool add_order(const domain::Order& order,
                                 domain::Quantity remaining_quantity);
    [[nodiscard]] bool reduce_order(domain::OrderId order_id, domain::Quantity executed_quantity);
    [[nodiscard]] bool cancel_order(const domain::CancelRequest& request);

    [[nodiscard]] const BookOrder* find_order(domain::OrderId order_id) const;
    [[nodiscard]] const BookOrder* best_bid_order() const;
    [[nodiscard]] const BookOrder* best_ask_order() const;

    [[nodiscard]] std::optional<domain::Price> best_bid_price() const;
    [[nodiscard]] std::optional<domain::Price> best_ask_price() const;
    [[nodiscard]] domain::Quantity total_quantity_at(domain::Side side,
                                                     domain::Price price) const;
    [[nodiscard]] std::size_t price_level_count(domain::Side side) const noexcept;
    [[nodiscard]] OrderBookSnapshot snapshot() const;

private:
    struct PriceLevel {
        std::list<BookOrder> orders{};
        domain::Quantity total_quantity{};
    };

    struct OrderLocation {
        domain::Side side{};
        domain::Price price{};
        std::list<BookOrder>::iterator iterator{};
    };

    using BidLevels = std::map<domain::Price, PriceLevel, std::greater<domain::Price>>;
    using AskLevels = std::map<domain::Price, PriceLevel, std::less<domain::Price>>;

    [[nodiscard]] bool can_rest(const domain::Order& order) const;
    void erase_location(std::unordered_map<domain::OrderId, OrderLocation>::iterator location);

    domain::InstrumentId instrument_id_{};
    BidLevels bids_{};
    AskLevels asks_{};
    std::unordered_map<domain::OrderId, OrderLocation> locations_{};
};

[[nodiscard]] std::string format_order_book(const OrderBookSnapshot& snapshot);

}  // namespace mini_ats::engine
