#pragma once

#include <cstdint>

namespace mini_ats::domain {

using OrderId = std::uint64_t;
using TradeId = std::uint64_t;
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

enum class MarketSession {
    Open,
    Closed,
};

enum class OrderStatus {
    Accepted,
    PartiallyFilled,
    Filled,
    Canceled,
    Rejected,
};

enum class ExecutionType {
    Accepted,
    Trade,
    Canceled,
    Rejected,
};

enum class RejectReason {
    None,
    InvalidOrderId,
    UnknownInstrument,
    InvalidPrice,
    InvalidQuantity,
    DuplicateOrderId,
    OrderNotFound,
    WouldNotExecute,
    MarketClosed,
    InternalError,
};

}  // namespace mini_ats::domain
