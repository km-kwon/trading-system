#pragma once

#include "domain/types.hpp"

namespace mini_ats::domain {

struct ExecutionReport {
    OrderId order_id{};
    InstrumentId instrument_id{};
    ExecutionType type{ExecutionType::Accepted};
    OrderStatus status{OrderStatus::Accepted};
    Quantity filled_quantity{};
    Quantity remaining_quantity{};
    Price last_price{};
    Quantity last_quantity{};
    RejectReason reject_reason{RejectReason::None};
    SequenceNumber sequence{};

    [[nodiscard]] bool is_trade() const noexcept;
    [[nodiscard]] bool is_rejected() const noexcept;
    [[nodiscard]] bool is_terminal() const noexcept;
    [[nodiscard]] bool has_fill() const noexcept;
};

}  // namespace mini_ats::domain
