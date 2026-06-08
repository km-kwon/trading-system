#pragma once

#include "domain/types.hpp"

namespace mini_ats::domain {

struct Trade {
    TradeId id{};
    InstrumentId instrument_id{};
    OrderId resting_order_id{};
    OrderId incoming_order_id{};
    Side aggressor_side{Side::Buy};
    Price price{};
    Quantity quantity{};
    SequenceNumber sequence{};

    [[nodiscard]] bool has_valid_price() const noexcept;
    [[nodiscard]] bool has_valid_quantity() const noexcept;
    [[nodiscard]] Price notional() const noexcept;
};

}  // namespace mini_ats::domain
