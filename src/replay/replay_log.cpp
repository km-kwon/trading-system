#include "replay/replay_log.hpp"

#include <algorithm>
#include <utility>

namespace mini_ats::replay {

ReplayEvent ReplayEvent::submit_order(domain::SequenceNumber input_sequence,
                                      domain::SequenceNumber reference_version,
                                      const domain::Order& order) {
    return ReplayEvent{
        .input_sequence = input_sequence,
        .reference_version = reference_version,
        .command = order,
    };
}

ReplayEvent ReplayEvent::cancel_order(domain::SequenceNumber input_sequence,
                                      domain::SequenceNumber reference_version,
                                      const domain::CancelRequest& request) {
    return ReplayEvent{
        .input_sequence = input_sequence,
        .reference_version = reference_version,
        .command = request,
    };
}

ReplayCommandType ReplayEvent::type() const noexcept {
    return command.index() == 0 ? ReplayCommandType::SubmitOrder : ReplayCommandType::CancelOrder;
}

domain::InstrumentId ReplayEvent::instrument_id() const noexcept {
    if (type() == ReplayCommandType::SubmitOrder) {
        return std::get<domain::Order>(command).instrument_id;
    }

    return std::get<domain::CancelRequest>(command).instrument_id;
}

domain::SequenceNumber ReplayEvent::command_sequence() const noexcept {
    if (type() == ReplayCommandType::SubmitOrder) {
        return std::get<domain::Order>(command).sequence;
    }

    return std::get<domain::CancelRequest>(command).sequence;
}

bool ReplayEvent::has_valid_input_sequence() const noexcept {
    return input_sequence != 0;
}

bool ReplayEvent::has_valid_reference_version() const noexcept {
    return reference_version != 0;
}

bool ReplayEvent::has_matching_command_sequence() const noexcept {
    return input_sequence == command_sequence();
}

bool ReplayApplyResult::applied() const noexcept {
    return status == ReplayEventStatus::Applied;
}

bool ReplayRunResult::ok() const noexcept {
    return std::all_of(steps.begin(), steps.end(), [](const ReplayApplyResult& step) {
        return step.applied();
    });
}

std::size_t ReplayRunResult::applied_count() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(steps.begin(), steps.end(), [](const ReplayApplyResult& step) {
            return step.applied();
        }));
}

ReplayApplyResult apply_replay_event(engine::MatchingEngine& engine, const ReplayEvent& event) {
    if (!event.has_valid_input_sequence()) {
        return ReplayApplyResult{.status = ReplayEventStatus::InvalidInputSequence};
    }

    if (!event.has_valid_reference_version()) {
        return ReplayApplyResult{.status = ReplayEventStatus::InvalidReferenceVersion};
    }

    if (!event.has_matching_command_sequence()) {
        return ReplayApplyResult{.status = ReplayEventStatus::CommandSequenceMismatch};
    }

    if (event.reference_version != engine.instrument().version) {
        return ReplayApplyResult{.status = ReplayEventStatus::ReferenceVersionMismatch};
    }

    if (event.type() == ReplayCommandType::SubmitOrder) {
        const auto result = engine.submit_order(std::get<domain::Order>(event.command));
        return ReplayApplyResult{
            .status = ReplayEventStatus::Applied,
            .trades = result.trades,
            .reports = result.reports,
        };
    }

    const auto result = engine.cancel_order(std::get<domain::CancelRequest>(event.command));
    return ReplayApplyResult{
        .status = ReplayEventStatus::Applied,
        .reports = result.reports,
    };
}

ReplayRunResult replay_events(engine::MatchingEngine& engine,
                              const std::vector<ReplayEvent>& events) {
    ReplayRunResult result{};

    for (const auto& event : events) {
        auto step = apply_replay_event(engine, event);
        result.trades.insert(result.trades.end(), step.trades.begin(), step.trades.end());
        result.reports.insert(result.reports.end(), step.reports.begin(), step.reports.end());
        const bool applied = step.applied();
        result.steps.push_back(std::move(step));

        if (!applied) {
            break;
        }
    }

    return result;
}

}  // namespace mini_ats::replay
