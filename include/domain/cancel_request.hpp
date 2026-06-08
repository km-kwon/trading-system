#pragma once

#include "domain/types.hpp"

namespace mini_ats::domain {

struct CancelRequest {
    OrderId order_id{};
    InstrumentId instrument_id{};
    SequenceNumber sequence{};

    [[nodiscard]] bool has_valid_order_id() const noexcept;
    [[nodiscard]] bool has_valid_instrument_id() const noexcept;
};

}  // namespace mini_ats::domain
