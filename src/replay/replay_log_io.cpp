#include "replay/replay_log_io.hpp"

#include <fstream>
#include <ostream>
#include <sstream>
#include <string_view>

namespace mini_ats::replay {

namespace {

[[nodiscard]] bool is_blank_or_comment(std::string_view line) noexcept {
    const auto first = line.find_first_not_of(" \t\r\n");
    return first == std::string_view::npos || line[first] == '#';
}

[[nodiscard]] const char* side_to_text(domain::Side side) noexcept {
    return side == domain::Side::Buy ? "BUY" : "SELL";
}

[[nodiscard]] const char* order_type_to_text(domain::OrderType type) noexcept {
    return type == domain::OrderType::Limit ? "LIMIT" : "MARKET";
}

[[nodiscard]] const char* time_in_force_to_text(domain::TimeInForce time_in_force) noexcept {
    switch (time_in_force) {
        case domain::TimeInForce::Day:
            return "DAY";
        case domain::TimeInForce::IOC:
            return "IOC";
        case domain::TimeInForce::FOK:
            return "FOK";
    }

    return "DAY";
}

}  // namespace

bool ReplayLogReadResult::ok() const noexcept {
    return error == ReplayLogIoError::None && !failure.has_value();
}

std::string format_replay_event(const ReplayEvent& event) {
    std::ostringstream output;

    if (event.type() == ReplayCommandType::SubmitOrder) {
        const auto& order = std::get<domain::Order>(event.command);
        output << "SUBMIT"
               << " seq=" << event.input_sequence << " ref=" << event.reference_version
               << " order_id=" << order.id << " instrument_id=" << order.instrument_id
               << " side=" << side_to_text(order.side)
               << " type=" << order_type_to_text(order.type)
               << " tif=" << time_in_force_to_text(order.time_in_force);

        if (order.is_limit() || order.price != 0) {
            output << " price=" << order.price;
        }

        output << " quantity=" << order.quantity;
        return output.str();
    }

    const auto& request = std::get<domain::CancelRequest>(event.command);
    output << "CANCEL"
           << " seq=" << event.input_sequence << " ref=" << event.reference_version
           << " order_id=" << request.order_id << " instrument_id=" << request.instrument_id;
    return output.str();
}

ReplayLogReadResult read_replay_log(std::istream& input) {
    ReplayLogReadResult result{};
    std::string line{};
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;

        if (is_blank_or_comment(line)) {
            continue;
        }

        const auto parsed = protocol::parse_text_command(line);
        if (!parsed.ok()) {
            result.error = ReplayLogIoError::ParseError;
            result.failure = ReplayLogParseFailure{
                .line_number = line_number,
                .parse_error = parsed.error,
                .field = parsed.field,
                .line = line,
            };
            return result;
        }

        result.events.push_back(*parsed.event);
    }

    return result;
}

ReplayLogReadResult read_replay_log_file(const std::filesystem::path& path) {
    std::ifstream input{path};
    if (!input.is_open()) {
        return ReplayLogReadResult{.error = ReplayLogIoError::OpenFailed};
    }

    return read_replay_log(input);
}

bool write_replay_log(std::ostream& output, const std::vector<ReplayEvent>& events) {
    for (const auto& event : events) {
        output << format_replay_event(event) << '\n';
        if (!output.good()) {
            return false;
        }
    }

    return output.good();
}

bool write_replay_log_file(const std::filesystem::path& path,
                           const std::vector<ReplayEvent>& events) {
    std::ofstream output{path};
    if (!output.is_open()) {
        return false;
    }

    return write_replay_log(output, events);
}

}  // namespace mini_ats::replay
