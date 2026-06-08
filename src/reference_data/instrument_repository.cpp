#include "reference_data/instrument_repository.hpp"

namespace mini_ats::reference_data {

bool InstrumentLoadResult::ok() const noexcept {
    return error == InstrumentLoadError::None && reference.has_value();
}

std::string_view instrument_reference_query() noexcept {
    return R"(SELECT instrument_id,
       symbol,
       tick_size,
       lower_price_limit,
       upper_price_limit,
       session,
       reference_version
FROM mini_ats.instruments
WHERE instrument_id = $1)";
}

std::optional<domain::MarketSession> parse_market_session(std::string_view session) noexcept {
    if (session == "OPEN") {
        return domain::MarketSession::Open;
    }

    if (session == "CLOSED") {
        return domain::MarketSession::Closed;
    }

    return std::nullopt;
}

InstrumentLoadResult map_instrument_record(const InstrumentRecord& record) {
    if (record.instrument_id == 0) {
        return InstrumentLoadResult{.error = InstrumentLoadError::InvalidInstrumentId};
    }

    if (record.symbol.empty()) {
        return InstrumentLoadResult{.error = InstrumentLoadError::EmptySymbol};
    }

    const auto session = parse_market_session(record.session);
    if (!session.has_value()) {
        return InstrumentLoadResult{.error = InstrumentLoadError::InvalidSession};
    }

    domain::InstrumentReference reference{
        .id = record.instrument_id,
        .tick_size = record.tick_size,
        .lower_price_limit = record.lower_price_limit,
        .upper_price_limit = record.upper_price_limit,
        .session = *session,
        .version = record.reference_version,
    };

    if (!reference.has_valid_tick_size()) {
        return InstrumentLoadResult{.error = InstrumentLoadError::InvalidTickSize};
    }

    if (!reference.has_valid_price_limits()) {
        return InstrumentLoadResult{.error = InstrumentLoadError::InvalidPriceLimits};
    }

    if (reference.version == 0) {
        return InstrumentLoadResult{.error = InstrumentLoadError::InvalidVersion};
    }

    return InstrumentLoadResult{
        .reference = reference,
        .error = InstrumentLoadError::None,
    };
}

}  // namespace mini_ats::reference_data
