#pragma once

#include <cstdint>

namespace mini_ats::domain {

using OrderId = std::uint64_t;
using InstrumentId = std::uint32_t;
using Price = std::int64_t;
using Quantity = std::int64_t;
using SequenceNumber = std::uint64_t;

enum class Side {
    Buy,
    Sell,
};

enum class OrderType {
    Limit,
    Market,
};

enum class TimeInForce {
    Day,
    IOC,
    FOK,
};

}  // namespace mini_ats::domain
