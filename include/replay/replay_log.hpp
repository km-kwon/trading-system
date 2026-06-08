#pragma once

#include "domain/cancel_request.hpp"
#include "domain/execution_report.hpp"
#include "domain/order.hpp"
#include "domain/trade.hpp"
#include "engine/matching_engine.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace mini_ats::replay {

enum class ReplayCommandType {
    SubmitOrder,
    CancelOrder,
};

enum class ReplayEventStatus {
    Applied,
    InvalidInputSequence,
    InvalidReferenceVersion,
    CommandSequenceMismatch,
    ReferenceVersionMismatch,
};

using ReplayCommand = std::variant<domain::Order, domain::CancelRequest>;

struct ReplayEvent {
    domain::SequenceNumber input_sequence{};
    domain::SequenceNumber reference_version{};
    ReplayCommand command{domain::Order{}};

    [[nodiscard]] static ReplayEvent submit_order(
        domain::SequenceNumber input_sequence,
        domain::SequenceNumber reference_version,
        const domain::Order& order);
    [[nodiscard]] static ReplayEvent cancel_order(
        domain::SequenceNumber input_sequence,
        domain::SequenceNumber reference_version,
        const domain::CancelRequest& request);

    [[nodiscard]] ReplayCommandType type() const noexcept;
    [[nodiscard]] domain::InstrumentId instrument_id() const noexcept;
    [[nodiscard]] domain::SequenceNumber command_sequence() const noexcept;
    [[nodiscard]] bool has_valid_input_sequence() const noexcept;
    [[nodiscard]] bool has_valid_reference_version() const noexcept;
    [[nodiscard]] bool has_matching_command_sequence() const noexcept;
};

struct ReplayApplyResult {
    ReplayEventStatus status{ReplayEventStatus::Applied};
    std::vector<domain::Trade> trades{};
    std::vector<domain::ExecutionReport> reports{};

    [[nodiscard]] bool applied() const noexcept;
};

struct ReplayRunResult {
    std::vector<ReplayApplyResult> steps{};
    std::vector<domain::Trade> trades{};
    std::vector<domain::ExecutionReport> reports{};

    [[nodiscard]] bool ok() const noexcept;
    [[nodiscard]] std::size_t applied_count() const noexcept;
};

[[nodiscard]] ReplayApplyResult apply_replay_event(engine::MatchingEngine& engine,
                                                   const ReplayEvent& event);
[[nodiscard]] ReplayRunResult replay_events(engine::MatchingEngine& engine,
                                            const std::vector<ReplayEvent>& events);

}  // namespace mini_ats::replay
