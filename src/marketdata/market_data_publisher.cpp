#include "marketdata/market_data_publisher.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace mini_ats::marketdata {

namespace {

constexpr int invalid_fd = -1;

[[nodiscard]] const char* side_to_text(domain::Side side) noexcept {
    return side == domain::Side::Buy ? "BUY" : "SELL";
}

void close_fd(int& fd) noexcept {
    if (fd != invalid_fd) {
        ::close(fd);
        fd = invalid_fd;
    }
}

void append_optional_level(std::ostream& output,
                           std::string_view prefix,
                           const std::optional<MarketDataLevel>& level) {
    output << ' ' << prefix << "_price=";
    if (!level.has_value()) {
        output << "NONE"
               << ' ' << prefix << "_quantity=0";
        return;
    }

    output << level->price
           << ' ' << prefix << "_quantity=" << level->total_quantity;
}

void append_levels(std::ostream& output,
                   std::string_view prefix,
                   const std::vector<MarketDataLevel>& levels) {
    for (std::size_t index = 0; index < levels.size(); ++index) {
        output << ' ' << prefix << index << "_price=" << levels[index].price
               << ' ' << prefix << index << "_quantity=" << levels[index].total_quantity;
    }
}

[[nodiscard]] bool send_payload(int fd,
                                const sockaddr_in& remote_address,
                                std::string_view payload) noexcept {
    const auto sent = ::sendto(fd, payload.data(), payload.size(), 0,
                               reinterpret_cast<const sockaddr*>(&remote_address),
                               sizeof(remote_address));
    return sent >= 0 && static_cast<std::size_t>(sent) == payload.size();
}

[[nodiscard]] bool build_remote_address(std::string_view remote_address_text,
                                        std::uint16_t remote_port,
                                        sockaddr_in& remote_address) noexcept {
    remote_address = sockaddr_in{};
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    const auto parsed = ::inet_pton(AF_INET, std::string{remote_address_text}.c_str(),
                                    &remote_address.sin_addr);
    return parsed == 1;
}

}  // namespace

bool MarketDataPublishResult::ok() const noexcept {
    return status == MarketDataPublishStatus::Ok;
}

UdpMarketDataPublisher::~UdpMarketDataPublisher() {
    close();
}

MarketDataPublishStatus UdpMarketDataPublisher::open(std::string_view remote_address,
                                                     std::uint16_t remote_port) noexcept {
    close();

    sockaddr_in parsed_remote_address{};
    if (!build_remote_address(remote_address, remote_port, parsed_remote_address)) {
        return MarketDataPublishStatus::InvalidAddress;
    }

    socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ == invalid_fd) {
        return MarketDataPublishStatus::SocketCreateFailed;
    }

    remote_address_ = std::string{remote_address};
    remote_port_ = remote_port;
    return MarketDataPublishStatus::Ok;
}

MarketDataPublishResult UdpMarketDataPublisher::publish(
    const MarketDataEvent& event) noexcept {
    if (!is_open()) {
        return MarketDataPublishResult{.status = MarketDataPublishStatus::NotOpen};
    }

    sockaddr_in remote_address{};
    if (!build_remote_address(remote_address_, remote_port_, remote_address)) {
        return MarketDataPublishResult{.status = MarketDataPublishStatus::InvalidAddress};
    }

    const auto payload = format_market_data_event(event);
    if (!send_payload(socket_fd_, remote_address, payload)) {
        return MarketDataPublishResult{.status = MarketDataPublishStatus::SendFailed};
    }

    return MarketDataPublishResult{
        .status = MarketDataPublishStatus::Ok,
        .events_published = 1,
        .bytes_sent = payload.size(),
    };
}

MarketDataPublishResult UdpMarketDataPublisher::publish(
    const std::vector<MarketDataEvent>& events) noexcept {
    MarketDataPublishResult total{};
    for (const auto& event : events) {
        const auto result = publish(event);
        if (!result.ok()) {
            total.status = result.status;
            return total;
        }

        total.events_published += result.events_published;
        total.bytes_sent += result.bytes_sent;
    }

    return total;
}

bool UdpMarketDataPublisher::is_open() const noexcept {
    return socket_fd_ != invalid_fd;
}

void UdpMarketDataPublisher::close() noexcept {
    close_fd(socket_fd_);
    remote_address_.clear();
    remote_port_ = 0;
}

const char* to_text(MarketDataPublishStatus status) noexcept {
    switch (status) {
        case MarketDataPublishStatus::Ok:
            return "OK";
        case MarketDataPublishStatus::InvalidAddress:
            return "INVALID_ADDRESS";
        case MarketDataPublishStatus::SocketCreateFailed:
            return "SOCKET_CREATE_FAILED";
        case MarketDataPublishStatus::NotOpen:
            return "NOT_OPEN";
        case MarketDataPublishStatus::SendFailed:
            return "SEND_FAILED";
    }

    return "OK";
}

std::string format_trade_event(const TradeEvent& event) {
    std::ostringstream output;
    output << "TRADE"
           << " seq=" << event.sequence
           << " trade_id=" << event.trade_id
           << " instrument_id=" << event.instrument_id
           << " resting_order_id=" << event.resting_order_id
           << " incoming_order_id=" << event.incoming_order_id
           << " aggressor_side=" << side_to_text(event.aggressor_side)
           << " price=" << event.price
           << " quantity=" << event.quantity;
    return output.str();
}

std::string format_book_update_event(const BookUpdateEvent& event) {
    std::ostringstream output;
    output << "BOOK_UPDATE"
           << " seq=" << event.sequence
           << " instrument_id=" << event.instrument_id;

    append_optional_level(output, "best_bid", event.best_bid);
    append_optional_level(output, "best_ask", event.best_ask);

    output << " bids=" << event.bids.size()
           << " asks=" << event.asks.size();
    append_levels(output, "bid", event.bids);
    append_levels(output, "ask", event.asks);
    return output.str();
}

std::string format_market_data_event(const MarketDataEvent& event) {
    return std::visit([](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<Value, TradeEvent>) {
            return format_trade_event(value);
        } else {
            return format_book_update_event(value);
        }
    }, event);
}

}  // namespace mini_ats::marketdata
