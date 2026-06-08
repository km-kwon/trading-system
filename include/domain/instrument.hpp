#pragma once

#include "domain/types.hpp"

namespace mini_ats::domain {

struct InstrumentReference {
    InstrumentId id{};
    Price tick_size{1};
    Price lower_price_limit{1};
    Price upper_price_limit{};
    MarketSession session{MarketSession::Open};
    SequenceNumber version{1};

    [[nodiscard]] bool has_valid_instrument_id() const noexcept;
    [[nodiscard]] bool has_valid_tick_size() const noexcept;
    [[nodiscard]] bool has_valid_price_limits() const noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    [[nodiscard]] bool price_is_on_tick(Price price) const noexcept;
    [[nodiscard]] bool price_is_within_limits(Price price) const noexcept;
    [[nodiscard]] bool accepts_limit_price(Price price) const noexcept;
};

[[nodiscard]] InstrumentReference default_instrument_reference(InstrumentId instrument_id) noexcept;

}  // namespace mini_ats::domain
