#pragma once

#include "domain/instrument.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace mini_ats::reference_data {

enum class InstrumentLoadError {
    None,
    InvalidInstrumentId,
    EmptySymbol,
    InvalidTickSize,
    InvalidPriceLimits,
    InvalidSession,
    InvalidVersion,
};

struct InstrumentRecord {
    domain::InstrumentId instrument_id{};
    std::string symbol{};
    domain::Price tick_size{};
    domain::Price lower_price_limit{};
    domain::Price upper_price_limit{};
    std::string session{};
    domain::SequenceNumber reference_version{};
};

struct InstrumentLoadResult {
    std::optional<domain::InstrumentReference> reference{};
    InstrumentLoadError error{InstrumentLoadError::None};

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] std::string_view instrument_reference_query() noexcept;
[[nodiscard]] std::optional<domain::MarketSession> parse_market_session(
    std::string_view session) noexcept;
[[nodiscard]] InstrumentLoadResult map_instrument_record(const InstrumentRecord& record);

}  // namespace mini_ats::reference_data
