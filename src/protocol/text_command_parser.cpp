#include "protocol/text_command_parser.hpp"

#include <charconv>
#include <initializer_list>
#include <istream>
#include <map>
#include <sstream>
#include <string>

namespace mini_ats::protocol {

namespace {

using FieldMap = std::map<std::string, std::string>;

struct ParsedLine {
    std::string command{};
    FieldMap fields{};
    TextCommandParseError error{TextCommandParseError::None};
    std::string field{};
};

[[nodiscard]] bool is_blank(std::string_view text) noexcept {
    for (const char value : text) {
        if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
            return false;
        }
    }

    return true;
}

[[nodiscard]] ParsedLine parse_line(std::string_view line) {
    if (is_blank(line)) {
        return ParsedLine{.error = TextCommandParseError::EmptyLine};
    }

    std::istringstream input{std::string{line}};
    ParsedLine parsed{};
    input >> parsed.command;

    std::string token{};
    while (input >> token) {
        const auto delimiter = token.find('=');
        if (delimiter == std::string::npos || delimiter == 0 || delimiter + 1 >= token.size()) {
            return ParsedLine{
                .command = parsed.command,
                .fields = parsed.fields,
                .error = TextCommandParseError::MalformedToken,
                .field = token,
            };
        }

        parsed.fields[token.substr(0, delimiter)] = token.substr(delimiter + 1);
    }

    return parsed;
}

[[nodiscard]] const std::string* find_field(
    const FieldMap& fields,
    std::initializer_list<std::string_view> names) noexcept {
    for (const auto name : names) {
        const auto iterator = fields.find(std::string{name});
        if (iterator != fields.end()) {
            return &iterator->second;
        }
    }

    return nullptr;
}

template <typename Value>
[[nodiscard]] std::optional<Value> parse_integer(std::string_view text) noexcept {
    Value value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);

    if (error != std::errc{} || position != end) {
        return std::nullopt;
    }

    return value;
}

template <typename Value>
[[nodiscard]] std::optional<Value> parse_required_integer(
    const FieldMap& fields,
    std::initializer_list<std::string_view> names,
    TextCommandParseResult& result) {
    const auto* const value = find_field(fields, names);
    const std::string field_name{*names.begin()};

    if (value == nullptr) {
        result.error = TextCommandParseError::MissingField;
        result.field = field_name;
        return std::nullopt;
    }

    auto parsed = parse_integer<Value>(*value);
    if (!parsed.has_value()) {
        result.error = TextCommandParseError::InvalidNumber;
        result.field = field_name;
        return std::nullopt;
    }

    return parsed;
}

[[nodiscard]] std::optional<domain::Side> parse_side(std::string_view value) noexcept {
    if (value == "BUY") {
        return domain::Side::Buy;
    }

    if (value == "SELL") {
        return domain::Side::Sell;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<domain::OrderType> parse_order_type(std::string_view value) noexcept {
    if (value == "LIMIT") {
        return domain::OrderType::Limit;
    }

    if (value == "MARKET") {
        return domain::OrderType::Market;
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<domain::TimeInForce> parse_time_in_force(
    std::string_view value) noexcept {
    if (value == "DAY") {
        return domain::TimeInForce::Day;
    }

    if (value == "IOC") {
        return domain::TimeInForce::IOC;
    }

    if (value == "FOK") {
        return domain::TimeInForce::FOK;
    }

    return std::nullopt;
}

[[nodiscard]] TextCommandParseResult parse_submit(const FieldMap& fields) {
    TextCommandParseResult result{};

    const auto sequence = parse_required_integer<domain::SequenceNumber>(
        fields, {"seq", "input_sequence"}, result);
    if (!sequence.has_value()) {
        return result;
    }

    const auto reference_version = parse_required_integer<domain::SequenceNumber>(
        fields, {"ref", "reference_version"}, result);
    if (!reference_version.has_value()) {
        return result;
    }

    const auto order_id = parse_required_integer<domain::OrderId>(
        fields, {"order_id", "id"}, result);
    if (!order_id.has_value()) {
        return result;
    }

    const auto instrument_id = parse_required_integer<domain::InstrumentId>(
        fields, {"instrument_id", "instrument"}, result);
    if (!instrument_id.has_value()) {
        return result;
    }

    const auto quantity = parse_required_integer<domain::Quantity>(
        fields, {"quantity", "qty"}, result);
    if (!quantity.has_value()) {
        return result;
    }

    const auto* const side_value = find_field(fields, {"side"});
    if (side_value == nullptr) {
        return TextCommandParseResult{
            .error = TextCommandParseError::MissingField,
            .field = "side",
        };
    }

    const auto side = parse_side(*side_value);
    if (!side.has_value()) {
        return TextCommandParseResult{
            .error = TextCommandParseError::InvalidSide,
            .field = "side",
        };
    }

    const auto* const type_value = find_field(fields, {"type", "order_type"});
    if (type_value == nullptr) {
        return TextCommandParseResult{
            .error = TextCommandParseError::MissingField,
            .field = "type",
        };
    }

    const auto type = parse_order_type(*type_value);
    if (!type.has_value()) {
        return TextCommandParseResult{
            .error = TextCommandParseError::InvalidOrderType,
            .field = "type",
        };
    }

    const auto* const tif_value = find_field(fields, {"tif", "time_in_force"});
    if (tif_value == nullptr) {
        return TextCommandParseResult{
            .error = TextCommandParseError::MissingField,
            .field = "tif",
        };
    }

    const auto time_in_force = parse_time_in_force(*tif_value);
    if (!time_in_force.has_value()) {
        return TextCommandParseResult{
            .error = TextCommandParseError::InvalidTimeInForce,
            .field = "tif",
        };
    }

    domain::Price price{};
    if (*type == domain::OrderType::Limit) {
        const auto parsed_price = parse_required_integer<domain::Price>(
            fields, {"price"}, result);
        if (!parsed_price.has_value()) {
            return result;
        }
        price = *parsed_price;
    } else if (const auto* const price_value = find_field(fields, {"price"});
               price_value != nullptr) {
        const auto parsed_price = parse_integer<domain::Price>(*price_value);
        if (!parsed_price.has_value()) {
            return TextCommandParseResult{
                .error = TextCommandParseError::InvalidNumber,
                .field = "price",
            };
        }
        price = *parsed_price;
    }

    const domain::Order order{
        .id = *order_id,
        .instrument_id = *instrument_id,
        .side = *side,
        .type = *type,
        .time_in_force = *time_in_force,
        .price = price,
        .quantity = *quantity,
        .sequence = *sequence,
    };

    return TextCommandParseResult{
        .event = replay::ReplayEvent::submit_order(*sequence, *reference_version, order),
        .error = TextCommandParseError::None,
    };
}

[[nodiscard]] TextCommandParseResult parse_cancel(const FieldMap& fields) {
    TextCommandParseResult result{};

    const auto sequence = parse_required_integer<domain::SequenceNumber>(
        fields, {"seq", "input_sequence"}, result);
    if (!sequence.has_value()) {
        return result;
    }

    const auto reference_version = parse_required_integer<domain::SequenceNumber>(
        fields, {"ref", "reference_version"}, result);
    if (!reference_version.has_value()) {
        return result;
    }

    const auto order_id = parse_required_integer<domain::OrderId>(
        fields, {"order_id", "id"}, result);
    if (!order_id.has_value()) {
        return result;
    }

    const auto instrument_id = parse_required_integer<domain::InstrumentId>(
        fields, {"instrument_id", "instrument"}, result);
    if (!instrument_id.has_value()) {
        return result;
    }

    const domain::CancelRequest request{
        .order_id = *order_id,
        .instrument_id = *instrument_id,
        .sequence = *sequence,
    };

    return TextCommandParseResult{
        .event = replay::ReplayEvent::cancel_order(*sequence, *reference_version, request),
        .error = TextCommandParseError::None,
    };
}

}  // namespace

bool TextCommandParseResult::ok() const noexcept {
    return error == TextCommandParseError::None && event.has_value();
}

TextCommandParseResult parse_text_command(std::string_view line) {
    const auto parsed = parse_line(line);
    if (parsed.error != TextCommandParseError::None) {
        return TextCommandParseResult{
            .error = parsed.error,
            .field = parsed.field,
        };
    }

    if (parsed.command == "SUBMIT") {
        return parse_submit(parsed.fields);
    }

    if (parsed.command == "CANCEL") {
        return parse_cancel(parsed.fields);
    }

    return TextCommandParseResult{
        .error = TextCommandParseError::UnknownCommand,
        .field = parsed.command,
    };
}

}  // namespace mini_ats::protocol
