#include "gateway/tcp_order_server.hpp"

#include "gateway/gateway_recorder.hpp"
#include "gateway/order_gateway.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace mini_ats::gateway {

namespace {

constexpr int invalid_fd = -1;

struct ClientProcessResult {
    TcpOrderServerStatus status{TcpOrderServerStatus::Ok};
    std::size_t commands_processed{};
};

[[nodiscard]] bool is_blank_or_comment(std::string_view line) noexcept {
    const auto first = line.find_first_not_of(" \t\r\n");
    return first == std::string_view::npos || line[first] == '#';
}

void close_fd(int& fd) noexcept {
    if (fd != invalid_fd) {
        ::close(fd);
        fd = invalid_fd;
    }
}

[[nodiscard]] bool send_all(int fd, std::string_view text) noexcept {
    std::size_t sent = 0;
    while (sent < text.size()) {
#ifdef MSG_NOSIGNAL
        const auto result = ::send(fd, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
#else
        const auto result = ::send(fd, text.data() + sent, text.size() - sent, 0);
#endif
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

void trim_line_ending(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

[[nodiscard]] TcpOrderServerStatus process_command_line(engine::MatchingEngine& engine,
                                                        std::ostream* accepted_input_log,
                                                        marketdata::UdpMarketDataPublisher*
                                                            market_data_publisher,
                                                        std::size_t market_data_depth,
                                                        int client_fd,
                                                        std::string line) {
    trim_line_ending(line);
    if (is_blank_or_comment(line)) {
        return TcpOrderServerStatus::Ok;
    }

    bool recorded = true;
    marketdata::MarketDataPublishResult publish_result{};
    GatewayResponse response{};
    if (market_data_publisher != nullptr) {
        const auto result = handle_published_text_command(engine, line, *market_data_publisher,
                                                          accepted_input_log,
                                                          market_data_depth);
        response = result.response;
        recorded = result.recorded;
        publish_result = result.publish_result;
    } else if (accepted_input_log == nullptr) {
        response = handle_text_command(engine, line);
    } else {
        const auto result = handle_recorded_text_command(engine, line, *accepted_input_log);
        response = result.response;
        recorded = result.recorded;
    }

    auto output = format_gateway_response(response);
    output.push_back('\n');
    if (!send_all(client_fd, output)) {
        return TcpOrderServerStatus::SendFailed;
    }

    if (accepted_input_log != nullptr && response.accepted() && !recorded) {
        return TcpOrderServerStatus::RecordFailed;
    }

    if (!publish_result.ok()) {
        return TcpOrderServerStatus::PublishFailed;
    }

    return TcpOrderServerStatus::Ok;
}

[[nodiscard]] ClientProcessResult process_client(engine::MatchingEngine& engine,
                                                 std::ostream* accepted_input_log,
                                                 marketdata::UdpMarketDataPublisher*
                                                     market_data_publisher,
                                                 std::size_t market_data_depth,
                                                 int client_fd) {
    ClientProcessResult result{};
    std::string pending{};
    std::array<char, 4096> buffer{};

    while (true) {
        const auto received = ::recv(client_fd, buffer.data(), buffer.size(), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            result.status = TcpOrderServerStatus::ReceiveFailed;
            return result;
        }

        if (received == 0) {
            break;
        }

        pending.append(buffer.data(), static_cast<std::size_t>(received));
        std::size_t newline = pending.find('\n');
        while (newline != std::string::npos) {
            std::string line = pending.substr(0, newline);
            pending.erase(0, newline + 1);

            const auto status = process_command_line(engine, accepted_input_log,
                                                     market_data_publisher, market_data_depth,
                                                     client_fd, std::move(line));
            if (status != TcpOrderServerStatus::Ok) {
                result.status = status;
                return result;
            }
            ++result.commands_processed;
            newline = pending.find('\n');
        }
    }

    if (!pending.empty()) {
        const auto status = process_command_line(engine, accepted_input_log,
                                                 market_data_publisher, market_data_depth,
                                                 client_fd, std::move(pending));
        if (status != TcpOrderServerStatus::Ok) {
            result.status = status;
            return result;
        }
        ++result.commands_processed;
    }

    return result;
}

}  // namespace

bool TcpOrderServerRunResult::ok() const noexcept {
    return status == TcpOrderServerStatus::Ok;
}

TcpOrderServer::TcpOrderServer(
    engine::MatchingEngine& engine,
    std::ostream* accepted_input_log,
    marketdata::UdpMarketDataPublisher* market_data_publisher,
    std::size_t market_data_depth) noexcept
    : engine_{engine},
      accepted_input_log_{accepted_input_log},
      market_data_publisher_{market_data_publisher},
      market_data_depth_{market_data_depth} {}

TcpOrderServer::~TcpOrderServer() {
    close();
}

TcpOrderServerStatus TcpOrderServer::listen(std::string_view bind_address,
                                            std::uint16_t listen_port,
                                            int backlog) noexcept {
    close();

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(listen_port);
    const auto parsed = ::inet_pton(AF_INET, std::string{bind_address}.c_str(),
                                    &address.sin_addr);
    if (parsed != 1) {
        return TcpOrderServerStatus::InvalidAddress;
    }

    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ == invalid_fd) {
        return TcpOrderServerStatus::SocketCreateFailed;
    }

    const int reuse_address = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_address,
                     sizeof(reuse_address)) != 0) {
        close();
        return TcpOrderServerStatus::SetSocketOptionFailed;
    }

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close();
        return TcpOrderServerStatus::BindFailed;
    }

    if (::listen(listen_fd_, backlog) != 0) {
        close();
        return TcpOrderServerStatus::ListenFailed;
    }

    return TcpOrderServerStatus::Ok;
}

TcpOrderServerRunResult TcpOrderServer::serve(std::size_t max_clients) {
    if (!listening()) {
        return TcpOrderServerRunResult{.status = TcpOrderServerStatus::NotListening};
    }

    TcpOrderServerRunResult result{};
    while (max_clients == 0 || result.clients_served < max_clients) {
        int client_fd = invalid_fd;
        while (client_fd == invalid_fd) {
            client_fd = ::accept(listen_fd_, nullptr, nullptr);
            if (client_fd == invalid_fd && errno != EINTR) {
                result.status = TcpOrderServerStatus::AcceptFailed;
                return result;
            }
        }

        auto client_result = process_client(engine_, accepted_input_log_, market_data_publisher_,
                                            market_data_depth_, client_fd);
        close_fd(client_fd);
        ++result.clients_served;
        result.commands_processed += client_result.commands_processed;
        if (client_result.status != TcpOrderServerStatus::Ok) {
            result.status = client_result.status;
            return result;
        }
    }

    return result;
}

std::uint16_t TcpOrderServer::port() const noexcept {
    if (!listening()) {
        return 0;
    }

    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return 0;
    }

    return ntohs(address.sin_port);
}

bool TcpOrderServer::listening() const noexcept {
    return listen_fd_ != invalid_fd;
}

void TcpOrderServer::close() noexcept {
    close_fd(listen_fd_);
}

const char* to_text(TcpOrderServerStatus status) noexcept {
    switch (status) {
        case TcpOrderServerStatus::Ok:
            return "OK";
        case TcpOrderServerStatus::InvalidAddress:
            return "INVALID_ADDRESS";
        case TcpOrderServerStatus::SocketCreateFailed:
            return "SOCKET_CREATE_FAILED";
        case TcpOrderServerStatus::SetSocketOptionFailed:
            return "SET_SOCKET_OPTION_FAILED";
        case TcpOrderServerStatus::BindFailed:
            return "BIND_FAILED";
        case TcpOrderServerStatus::ListenFailed:
            return "LISTEN_FAILED";
        case TcpOrderServerStatus::NotListening:
            return "NOT_LISTENING";
        case TcpOrderServerStatus::AcceptFailed:
            return "ACCEPT_FAILED";
        case TcpOrderServerStatus::ReceiveFailed:
            return "RECEIVE_FAILED";
        case TcpOrderServerStatus::SendFailed:
            return "SEND_FAILED";
        case TcpOrderServerStatus::RecordFailed:
            return "RECORD_FAILED";
        case TcpOrderServerStatus::PublishFailed:
            return "PUBLISH_FAILED";
    }

    return "OK";
}

}  // namespace mini_ats::gateway
