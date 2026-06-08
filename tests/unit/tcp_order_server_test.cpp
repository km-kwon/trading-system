#include "gateway/tcp_order_server.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

using mini_ats::domain::InstrumentId;
using mini_ats::domain::InstrumentReference;
using mini_ats::domain::MarketSession;
using mini_ats::domain::Price;
using mini_ats::domain::SequenceNumber;
using mini_ats::engine::MatchingEngine;
using mini_ats::gateway::TcpOrderServer;
using mini_ats::gateway::TcpOrderServerRunResult;
using mini_ats::gateway::TcpOrderServerStatus;

InstrumentReference make_instrument() {
    return InstrumentReference{
        .id = InstrumentId{1001},
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = MarketSession::Open,
        .version = SequenceNumber{7},
    };
}

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

[[nodiscard]] int connect_to_server(std::uint16_t port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1) {
        ::close(fd);
        return -1;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        ::close(fd);
        return -1;
    }

    return fd;
}

[[nodiscard]] bool send_all(int fd, std::string_view text) {
    std::size_t sent = 0;
    while (sent < text.size()) {
        const auto result = ::send(fd, text.data() + sent, text.size() - sent, 0);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (result == 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

[[nodiscard]] bool send_lines(int fd, const std::vector<std::string>& lines) {
    for (const auto& line : lines) {
        if (!send_all(fd, line) || !send_all(fd, "\n")) {
            return false;
        }
    }

    return ::shutdown(fd, SHUT_WR) == 0;
}

[[nodiscard]] std::string read_response(int fd) {
    std::string response{};
    std::array<char, 4096> buffer{};
    while (true) {
        const auto received = ::recv(fd, buffer.data(), buffer.size(), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return {};
        }

        if (received == 0) {
            break;
        }

        response.append(buffer.data(), static_cast<std::size_t>(received));
    }

    return response;
}

}  // namespace

TEST(TcpOrderServerTest, ProcessesLineDelimitedCommandsOverLoopback) {
    MatchingEngine engine{make_instrument()};
    std::ostringstream accepted_input_log{};
    TcpOrderServer server{engine, &accepted_input_log};
    const auto listen_status = server.listen("127.0.0.1", 0);
    if (listen_status != TcpOrderServerStatus::Ok) {
        GTEST_SKIP() << "loopback socket unavailable: "
                     << mini_ats::gateway::to_text(listen_status);
    }
    ASSERT_NE(server.port(), 0);

    SocketFd socket_fd{connect_to_server(server.port())};
    if (socket_fd.get() < 0) {
        GTEST_SKIP() << "loopback client connect unavailable";
    }

    ASSERT_TRUE(send_lines(
        socket_fd.get(),
        {
            "SUBMIT seq=1 ref=7 order_id=200 instrument_id=1001 side=SELL type=LIMIT "
            "tif=DAY price=73700 quantity=3",
            "SUBMIT seq=2 ref=7 order_id=201 instrument_id=1001 side=BUY type=LIMIT "
            "tif=DAY price=73702 quantity=1",
            "SUBMIT seq=3 ref=7 order_id=202 instrument_id=1001 side=BUY type=LIMIT "
            "tif=DAY price=73700 quantity=5",
        }));

    TcpOrderServerRunResult run_result{};
    std::thread server_thread{[&server, &run_result] {
        run_result = server.serve(1);
    }};
    const auto response = read_response(socket_fd.get());
    server_thread.join();

    EXPECT_TRUE(run_result.ok());
    EXPECT_EQ(run_result.clients_served, 1U);
    EXPECT_EQ(run_result.commands_processed, 3U);
    EXPECT_NE(response.find("ACCEPTED reason=NONE seq=1 command=SUBMIT"), std::string::npos);
    EXPECT_NE(response.find("REJECTED reason=ENGINE_REJECTED seq=2 command=SUBMIT"),
              std::string::npos);
    EXPECT_NE(response.find("ACCEPTED reason=NONE seq=3 command=SUBMIT"), std::string::npos);
    EXPECT_EQ(accepted_input_log.str(),
              "SUBMIT seq=1 ref=7 order_id=200 instrument_id=1001 side=SELL type=LIMIT "
              "tif=DAY price=73700 quantity=3\n"
              "SUBMIT seq=3 ref=7 order_id=202 instrument_id=1001 side=BUY type=LIMIT "
              "tif=DAY price=73700 quantity=5\n");
}

TEST(TcpOrderServerTest, ReportsInvalidBindAddress) {
    MatchingEngine engine{make_instrument()};
    TcpOrderServer server{engine};

    EXPECT_EQ(server.listen("not-an-ip", 0), TcpOrderServerStatus::InvalidAddress);
    EXPECT_FALSE(server.listening());
}
