#pragma once

#include "replay/replay_log.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace mini_ats::protocol {

enum class TextCommandParseError {
    None,
    EmptyLine,
    UnknownCommand,
    MalformedToken,
    MissingField,
    InvalidNumber,
    InvalidSide,
    InvalidOrderType,
    InvalidTimeInForce,
};

struct TextCommandParseResult {
    std::optional<replay::ReplayEvent> event{};
    TextCommandParseError error{TextCommandParseError::None};
    std::string field{};

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] TextCommandParseResult parse_text_command(std::string_view line);

}  // namespace mini_ats::protocol
