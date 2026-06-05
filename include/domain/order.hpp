#pragma once

#include "domain/types.hpp"

namespace mini_ats::domain {

struct Order {
    OrderId id{};
    InstrumentId instrument_id{};
    Side side{Side::Buy};
    OrderType type{OrderType::Limit};
    TimeInForce time_in_force{TimeInForce::Day};
    Price price{};
    Quantity quantity{};
    SequenceNumber sequence{};

    [[nodiscard]] bool is_limit() const noexcept;
    [[nodiscard]] bool is_market() const noexcept;
    [[nodiscard]] bool has_valid_quantity() const noexcept;
    [[nodiscard]] bool has_valid_price() const noexcept;
};

}  // namespace mini_ats::domain
