#pragma once

#include "marketdata/market_data.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mini_ats::marketdata {

enum class MarketDataPublishStatus {
    Ok,
    InvalidAddress,
    SocketCreateFailed,
    NotOpen,
    SendFailed,
};

struct MarketDataPublishResult {
    MarketDataPublishStatus status{MarketDataPublishStatus::Ok};
    std::size_t events_published{};
    std::size_t bytes_sent{};

    [[nodiscard]] bool ok() const noexcept;
};

class UdpMarketDataPublisher {
public:
    UdpMarketDataPublisher() noexcept = default;
    UdpMarketDataPublisher(const UdpMarketDataPublisher&) = delete;
    UdpMarketDataPublisher& operator=(const UdpMarketDataPublisher&) = delete;
    UdpMarketDataPublisher(UdpMarketDataPublisher&&) = delete;
    UdpMarketDataPublisher& operator=(UdpMarketDataPublisher&&) = delete;
    ~UdpMarketDataPublisher();

    [[nodiscard]] MarketDataPublishStatus open(std::string_view remote_address,
                                               std::uint16_t remote_port) noexcept;
    [[nodiscard]] MarketDataPublishResult publish(const MarketDataEvent& event) noexcept;
    [[nodiscard]] MarketDataPublishResult publish(
        const std::vector<MarketDataEvent>& events) noexcept;
    [[nodiscard]] bool is_open() const noexcept;
    void close() noexcept;

private:
    int socket_fd_{-1};
    std::string remote_address_{};
    std::uint16_t remote_port_{};
};

[[nodiscard]] const char* to_text(MarketDataPublishStatus status) noexcept;
[[nodiscard]] std::string format_trade_event(const TradeEvent& event);
[[nodiscard]] std::string format_book_update_event(const BookUpdateEvent& event);
[[nodiscard]] std::string format_market_data_event(const MarketDataEvent& event);

}  // namespace mini_ats::marketdata
