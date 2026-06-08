#pragma once

#include "protocol/text_command_parser.hpp"
#include "replay/replay_log.hpp"

#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace mini_ats::replay {

enum class ReplayLogIoError {
    None,
    OpenFailed,
    ParseError,
    WriteFailed,
};

struct ReplayLogParseFailure {
    std::size_t line_number{};
    protocol::TextCommandParseError parse_error{protocol::TextCommandParseError::None};
    std::string field{};
    std::string line{};
};

struct ReplayLogReadResult {
    std::vector<ReplayEvent> events{};
    ReplayLogIoError error{ReplayLogIoError::None};
    std::optional<ReplayLogParseFailure> failure{};

    [[nodiscard]] bool ok() const noexcept;
};

[[nodiscard]] std::string format_replay_event(const ReplayEvent& event);
[[nodiscard]] ReplayLogReadResult read_replay_log(std::istream& input);
[[nodiscard]] ReplayLogReadResult read_replay_log_file(const std::filesystem::path& path);
[[nodiscard]] bool write_replay_log(std::ostream& output, const std::vector<ReplayEvent>& events);
[[nodiscard]] bool write_replay_log_file(const std::filesystem::path& path,
                                         const std::vector<ReplayEvent>& events);

}  // namespace mini_ats::replay
