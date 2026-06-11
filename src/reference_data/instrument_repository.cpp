#include "reference_data/instrument_repository.hpp"

#include <array>
#include <charconv>
#include <cstdio>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace mini_ats::reference_data {

namespace {

[[nodiscard]] std::string shell_quote(std::string_view value) {
    std::string quoted{"'"};
    for (const char character : value) {
        if (character == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(character);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

[[nodiscard]] std::string trim_trailing_line_endings(std::string_view value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.remove_suffix(1);
    }

    return std::string{value};
}

[[nodiscard]] std::vector<std::string_view> split_tab_fields(std::string_view line) {
    std::vector<std::string_view> fields{};
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string_view::npos) {
            fields.push_back(line.substr(start));
            break;
        }

        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }

    return fields;
}

template <typename Integer>
[[nodiscard]] std::optional<Integer> parse_integer(std::string_view text) noexcept {
    Integer value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || position != end) {
        return std::nullopt;
    }

    return value;
}

[[nodiscard]] PostgresInstrumentLoadResult parse_psql_instrument_row(
    std::string_view row) {
    const auto fields = split_tab_fields(row);
    if (fields.size() != 7U) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::InvalidFieldCount,
            .detail = "expected 7 fields",
        };
    }

    const auto instrument_id = parse_integer<domain::InstrumentId>(fields[0]);
    const auto tick_size = parse_integer<domain::Price>(fields[2]);
    const auto lower_price_limit = parse_integer<domain::Price>(fields[3]);
    const auto upper_price_limit = parse_integer<domain::Price>(fields[4]);
    const auto reference_version = parse_integer<domain::SequenceNumber>(fields[6]);
    if (!instrument_id.has_value() || !tick_size.has_value() ||
        !lower_price_limit.has_value() || !upper_price_limit.has_value() ||
        !reference_version.has_value()) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::InvalidNumber,
            .detail = "failed to parse numeric instrument field",
        };
    }

    const InstrumentRecord record{
        .instrument_id = *instrument_id,
        .symbol = std::string{fields[1]},
        .tick_size = *tick_size,
        .lower_price_limit = *lower_price_limit,
        .upper_price_limit = *upper_price_limit,
        .session = std::string{fields[5]},
        .reference_version = *reference_version,
    };
    const auto mapped = map_instrument_record(record);
    if (!mapped.ok()) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::MappingFailed,
            .mapping_error = mapped.error,
            .detail = to_text(mapped.error),
        };
    }

    return PostgresInstrumentLoadResult{
        .reference = mapped.reference,
        .error = PostgresInstrumentLoadError::None,
        .mapping_error = InstrumentLoadError::None,
    };
}

}  // namespace

bool InstrumentLoadResult::ok() const noexcept {
    return error == InstrumentLoadError::None && reference.has_value();
}

bool PostgresInstrumentLoadResult::ok() const noexcept {
    return error == PostgresInstrumentLoadError::None && reference.has_value();
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

std::string instrument_reference_psql_query(domain::InstrumentId instrument_id) {
    std::ostringstream output;
    output << "SELECT instrument_id, symbol, tick_size, lower_price_limit, "
           << "upper_price_limit, session, reference_version "
           << "FROM mini_ats.instruments "
           << "WHERE instrument_id = " << instrument_id;
    return output.str();
}

std::string build_psql_instrument_command(
    const PostgresInstrumentRepositoryConfig& config,
    domain::InstrumentId instrument_id) {
    std::ostringstream output;
    output << shell_quote(config.psql_path)
           << " -X -q -t -A -F " << shell_quote("\t")
           << " -v ON_ERROR_STOP=1";
    if (!config.user.empty()) {
        output << " -U " << shell_quote(config.user);
    }
    output << " -d " << shell_quote(config.database)
           << " -c " << shell_quote(instrument_reference_psql_query(instrument_id))
           << " 2>/dev/null";
    return output.str();
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

PostgresInstrumentLoadResult parse_psql_instrument_result(std::string_view output) {
    const std::string trimmed = trim_trailing_line_endings(output);
    if (trimmed.empty()) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::EmptyResult,
            .detail = "instrument not found",
        };
    }

    if (trimmed.find('\n') != std::string::npos) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::MultipleRows,
            .detail = "expected exactly one instrument row",
        };
    }

    return parse_psql_instrument_row(trimmed);
}

PostgresInstrumentLoadResult load_instrument_reference_from_postgres(
    const PostgresInstrumentRepositoryConfig& config,
    domain::InstrumentId instrument_id) {
    if (instrument_id == 0) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::InvalidInstrumentId,
            .detail = "instrument_id must be positive",
        };
    }

    const std::string command = build_psql_instrument_command(config, instrument_id);
    FILE* pipe = ::popen(command.c_str(), "r");
    if (pipe == nullptr) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::CommandFailed,
            .detail = "failed to start psql",
        };
    }

    std::string output{};
    std::array<char, 4096> buffer{};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    const int status = ::pclose(pipe);
    if (status != 0) {
        return PostgresInstrumentLoadResult{
            .error = PostgresInstrumentLoadError::CommandFailed,
            .detail = "psql command failed",
        };
    }

    return parse_psql_instrument_result(output);
}

const char* to_text(InstrumentLoadError error) noexcept {
    switch (error) {
        case InstrumentLoadError::None:
            return "NONE";
        case InstrumentLoadError::InvalidInstrumentId:
            return "INVALID_INSTRUMENT_ID";
        case InstrumentLoadError::EmptySymbol:
            return "EMPTY_SYMBOL";
        case InstrumentLoadError::InvalidTickSize:
            return "INVALID_TICK_SIZE";
        case InstrumentLoadError::InvalidPriceLimits:
            return "INVALID_PRICE_LIMITS";
        case InstrumentLoadError::InvalidSession:
            return "INVALID_SESSION";
        case InstrumentLoadError::InvalidVersion:
            return "INVALID_VERSION";
    }

    return "NONE";
}

const char* to_text(PostgresInstrumentLoadError error) noexcept {
    switch (error) {
        case PostgresInstrumentLoadError::None:
            return "NONE";
        case PostgresInstrumentLoadError::InvalidInstrumentId:
            return "INVALID_INSTRUMENT_ID";
        case PostgresInstrumentLoadError::CommandFailed:
            return "COMMAND_FAILED";
        case PostgresInstrumentLoadError::EmptyResult:
            return "EMPTY_RESULT";
        case PostgresInstrumentLoadError::MultipleRows:
            return "MULTIPLE_ROWS";
        case PostgresInstrumentLoadError::InvalidFieldCount:
            return "INVALID_FIELD_COUNT";
        case PostgresInstrumentLoadError::InvalidNumber:
            return "INVALID_NUMBER";
        case PostgresInstrumentLoadError::MappingFailed:
            return "MAPPING_FAILED";
    }

    return "NONE";
}

std::string format_instrument_reference(const domain::InstrumentReference& reference) {
    std::ostringstream output;
    output << "INSTRUMENT"
           << " instrument_id=" << reference.id
           << " tick_size=" << reference.tick_size
           << " lower_price_limit=" << reference.lower_price_limit
           << " upper_price_limit=" << reference.upper_price_limit
           << " session=" << (reference.session == domain::MarketSession::Open ? "OPEN"
                                                                                : "CLOSED")
           << " reference_version=" << reference.version;
    return output.str();
}

}  // namespace mini_ats::reference_data
