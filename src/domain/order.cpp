#include "domain/order.hpp"

namespace mini_ats::domain {

bool Order::is_limit() const noexcept {
    return type == OrderType::Limit;
}

bool Order::is_market() const noexcept {
    return type == OrderType::Market;
}

bool Order::has_valid_quantity() const noexcept {
    return quantity > 0;
}

bool Order::has_valid_price() const noexcept {
    return is_market() || price > 0;
}

}  // namespace mini_ats::domain
