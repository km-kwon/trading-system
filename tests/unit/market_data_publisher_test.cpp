#include "marketdata/market_data_publisher.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <optional>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace {

using mini_ats::domain::InstrumentId;
using mini_ats::domain::OrderId;
using mini_ats::domain::Price;
using mini_ats::domain::Quantity;
using mini_ats::domain::SequenceNumber;
using mini_ats::domain::Side;
using mini_ats::domain::TradeId;
using mini_ats::marketdata::BookUpdateEvent;
using mini_ats::marketdata::MarketDataLevel;
using mini_ats::marketdata::MarketDataPublishStatus;
using mini_ats::marketdata::TradeEvent;
using mini_ats::marketdata::UdpMarketDataPublisher;
using mini_ats::marketdata::format_book_update_event;
using mini_ats::marketdata::format_market_data_event;
using mini_ats::marketdata::format_trade_event;

class SocketFd {
public:
    explicit SocketFd(int fd) noexcept : fd_{fd} {}
    SocketFd(const SocketFd&) = delete;
    SocketFd& operator=(const SocketFd&) = delete;
    ~SocketFd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

private:
    int fd_{-1};
};

[[nodiscard]] std::uint16_t bound_port(int fd) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }

    return ntohs(address.sin_port);
}

[[nodiscard]] bool bind_receiver(int fd) {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(0);
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
        return false;
    }

    return ::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
}

[[nodiscard]] bool set_receive_timeout(int fd) {
    timeval timeout{};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
}

[[nodiscard]] std::string receive_payload(int fd) {
    std::array<char, 4096> buffer{};
    while (true) {
        const auto received = ::recv(fd, buffer.data(), buffer.size(), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {};
        }

        return std::string{buffer.data(), static_cast<std::size_t>(received)};
    }
}

}  // namespace

TEST(MarketDataPublisherTest, FormatsTradeEventAsCanonicalTextPayload) {
    const TradeEvent event{
        .trade_id = TradeId{1},
        .instrument_id = InstrumentId{1001},
        .resting_order_id = OrderId{20},
        .incoming_order_id = OrderId{21},
        .aggressor_side = Side::Buy,
        .price = Price{73700},
        .quantity = Quantity{3},
        .sequence = SequenceNumber{2},
    };

    EXPECT_EQ(format_trade_event(event),
              "TRADE seq=2 trade_id=1 instrument_id=1001 resting_order_id=20 "
              "incoming_order_id=21 aggressor_side=BUY price=73700 quantity=3");
    EXPECT_EQ(format_market_data_event(event), format_trade_event(event));
}

TEST(MarketDataPublisherTest, FormatsBookUpdateEventAsCanonicalTextPayload) {
    const BookUpdateEvent event{
        .instrument_id = InstrumentId{1001},
        .sequence = SequenceNumber{4},
        .best_bid = MarketDataLevel{
            .side = Side::Buy,
            .price = Price{73700},
            .total_quantity = Quantity{2},
        },
        .best_ask = std::nullopt,
        .bids = {
            MarketDataLevel{
                .side = Side::Buy,
                .price = Price{73700},
                .total_quantity = Quantity{2},
            },
        },
        .asks = {},
    };

    EXPECT_EQ(format_book_update_event(event),
              "BOOK_UPDATE seq=4 instrument_id=1001 best_bid_price=73700 "
              "best_bid_quantity=2 best_ask_price=NONE best_ask_quantity=0 "
              "bids=1 asks=0 bid0_price=73700 bid0_quantity=2");
    EXPECT_EQ(format_market_data_event(event), format_book_update_event(event));
}

TEST(MarketDataPublisherTest, PublishRequiresOpenSocket) {
    UdpMarketDataPublisher publisher{};
    const auto result = publisher.publish(TradeEvent{
        .trade_id = TradeId{1},
        .instrument_id = InstrumentId{1001},
        .price = Price{73700},
        .quantity = Quantity{1},
        .sequence = SequenceNumber{1},
    });

    EXPECT_EQ(result.status, MarketDataPublishStatus::NotOpen);
    EXPECT_FALSE(result.ok());
}

TEST(MarketDataPublisherTest, ReportsInvalidRemoteAddress) {
    UdpMarketDataPublisher publisher{};

    EXPECT_EQ(publisher.open("not-an-ip", 9001), MarketDataPublishStatus::InvalidAddress);
    EXPECT_FALSE(publisher.is_open());
}

TEST(MarketDataPublisherTest, PublishesSingleDatagramOverLoopback) {
    SocketFd receiver{::socket(AF_INET, SOCK_DGRAM, 0)};
    if (receiver.get() < 0) {
        GTEST_SKIP() << "UDP receiver socket unavailable";
    }

    if (!bind_receiver(receiver.get())) {
        GTEST_SKIP() << "UDP receiver bind unavailable";
    }
    ASSERT_TRUE(set_receive_timeout(receiver.get()));

    const auto port = bound_port(receiver.get());
    ASSERT_NE(port, 0);

    UdpMarketDataPublisher publisher{};
    const auto open_status = publisher.open("127.0.0.1", port);
    if (open_status == MarketDataPublishStatus::SocketCreateFailed) {
        GTEST_SKIP() << "UDP publisher socket unavailable";
    }
    ASSERT_EQ(open_status, MarketDataPublishStatus::Ok);

    const TradeEvent event{
        .trade_id = TradeId{7},
        .instrument_id = InstrumentId{1001},
        .resting_order_id = OrderId{50},
        .incoming_order_id = OrderId{51},
        .aggressor_side = Side::Sell,
        .price = Price{73500},
        .quantity = Quantity{4},
        .sequence = SequenceNumber{9},
    };

    const auto publish_result = publisher.publish(event);
    ASSERT_TRUE(publish_result.ok());
    EXPECT_EQ(publish_result.events_published, 1U);
    EXPECT_EQ(publish_result.bytes_sent, format_trade_event(event).size());

    EXPECT_EQ(receive_payload(receiver.get()), format_trade_event(event));
}
