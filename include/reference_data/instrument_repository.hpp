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

enum class PostgresInstrumentLoadError {
    None,
    InvalidInstrumentId,
    CommandFailed,
    EmptyResult,
    MultipleRows,
    InvalidFieldCount,
    InvalidNumber,
    MappingFailed,
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

struct PostgresInstrumentRepositoryConfig {
    std::string database{"mini_ats"};
    std::string user{};
    std::string psql_path{"psql"};
};

struct PostgresInstrumentLoadResult {
    std::optional<domain::InstrumentReference> reference{};
    PostgresInstrumentLoadError error{PostgresInstrumentLoadError::None};
    InstrumentLoadError mapping_error{InstrumentLoadError::None};
    std::string detail{};

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] std::string_view instrument_reference_query() noexcept;
[[nodiscard]] std::string instrument_reference_psql_query(
    domain::InstrumentId instrument_id);
[[nodiscard]] std::string build_psql_instrument_command(
    const PostgresInstrumentRepositoryConfig& config,
    domain::InstrumentId instrument_id);
[[nodiscard]] std::optional<domain::MarketSession> parse_market_session(
    std::string_view session) noexcept;
[[nodiscard]] InstrumentLoadResult map_instrument_record(const InstrumentRecord& record);
[[nodiscard]] PostgresInstrumentLoadResult parse_psql_instrument_result(
    std::string_view output);
[[nodiscard]] PostgresInstrumentLoadResult load_instrument_reference_from_postgres(
    const PostgresInstrumentRepositoryConfig& config,
    domain::InstrumentId instrument_id);
[[nodiscard]] const char* to_text(InstrumentLoadError error) noexcept;
[[nodiscard]] const char* to_text(PostgresInstrumentLoadError error) noexcept;
[[nodiscard]] std::string format_instrument_reference(
    const domain::InstrumentReference& reference);

}  // namespace mini_ats::reference_data
