#include "domain/execution_report.hpp"

namespace mini_ats::domain {

bool ExecutionReport::is_trade() const noexcept {
    return type == ExecutionType::Trade;
}

bool ExecutionReport::is_rejected() const noexcept {
    return type == ExecutionType::Rejected || status == OrderStatus::Rejected;
}

bool ExecutionReport::is_terminal() const noexcept {
    return status == OrderStatus::Filled || status == OrderStatus::Canceled ||
           status == OrderStatus::Rejected;
}

bool ExecutionReport::has_fill() const noexcept {
    return filled_quantity > 0 || last_quantity > 0;
}

}  // namespace mini_ats::domain
