#include "domain/cancel_request.hpp"

namespace mini_ats::domain {

bool CancelRequest::has_valid_order_id() const noexcept {
    return order_id != 0;
}

bool CancelRequest::has_valid_instrument_id() const noexcept {
    return instrument_id != 0;
}

}  // namespace mini_ats::domain
