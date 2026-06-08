#include "domain/instrument.hpp"

#include <limits>

namespace mini_ats::domain {

bool InstrumentReference::has_valid_instrument_id() const noexcept {
    return id != 0;
}

bool InstrumentReference::has_valid_tick_size() const noexcept {
    return tick_size > 0;
}

bool InstrumentReference::has_valid_price_limits() const noexcept {
    return lower_price_limit > 0 && upper_price_limit >= lower_price_limit;
}

bool InstrumentReference::is_open() const noexcept {
    return session == MarketSession::Open;
}

bool InstrumentReference::price_is_on_tick(Price price) const noexcept {
    return has_valid_tick_size() && price > 0 && price % tick_size == 0;
}

bool InstrumentReference::price_is_within_limits(Price price) const noexcept {
    return has_valid_price_limits() && price >= lower_price_limit && price <= upper_price_limit;
}

bool InstrumentReference::accepts_limit_price(Price price) const noexcept {
    return price_is_on_tick(price) && price_is_within_limits(price);
}

InstrumentReference default_instrument_reference(InstrumentId instrument_id) noexcept {
    return InstrumentReference{
        .id = instrument_id,
        .tick_size = Price{1},
        .lower_price_limit = Price{1},
        .upper_price_limit = std::numeric_limits<Price>::max(),
        .session = MarketSession::Open,
        .version = SequenceNumber{1},
    };
}

}  // namespace mini_ats::domain
