#include "domain/trade.hpp"

namespace mini_ats::domain {

bool Trade::has_valid_price() const noexcept {
    return price > 0;
}

bool Trade::has_valid_quantity() const noexcept {
    return quantity > 0;
}

Price Trade::notional() const noexcept {
    return price * quantity;
}

}  // namespace mini_ats::domain
