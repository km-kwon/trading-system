#pragma once

#include "engine/matching_engine.hpp"
#include "marketdata/market_data_publisher.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string_view>

namespace mini_ats::gateway {

enum class TcpOrderServerStatus {
    Ok,
    InvalidAddress,
    SocketCreateFailed,
    SetSocketOptionFailed,
    BindFailed,
    ListenFailed,
    NotListening,
    AcceptFailed,
    ReceiveFailed,
    SendFailed,
    RecordFailed,
    PublishFailed,
};

struct TcpOrderServerRunResult {
    TcpOrderServerStatus status{TcpOrderServerStatus::Ok};
    std::size_t clients_served{};
    std::size_t commands_processed{};

    [[nodiscard]] bool ok() const noexcept;
};

class TcpOrderServer {
public:
    explicit TcpOrderServer(engine::MatchingEngine& engine,
                            std::ostream* accepted_input_log = nullptr,
                            marketdata::UdpMarketDataPublisher* market_data_publisher = nullptr,
                            std::size_t market_data_depth = 1) noexcept;
    TcpOrderServer(const TcpOrderServer&) = delete;
    TcpOrderServer& operator=(const TcpOrderServer&) = delete;
    TcpOrderServer(TcpOrderServer&&) = delete;
    TcpOrderServer& operator=(TcpOrderServer&&) = delete;
    ~TcpOrderServer();

    [[nodiscard]] TcpOrderServerStatus listen(std::string_view bind_address,
                                              std::uint16_t port,
                                              int backlog = 16) noexcept;
    [[nodiscard]] TcpOrderServerRunResult serve(std::size_t max_clients = 0);
    [[nodiscard]] std::uint16_t port() const noexcept;
    [[nodiscard]] bool listening() const noexcept;
    void close() noexcept;

private:
    engine::MatchingEngine& engine_;
    std::ostream* accepted_input_log_{};
    marketdata::UdpMarketDataPublisher* market_data_publisher_{};
    std::size_t market_data_depth_{1};
    int listen_fd_{-1};
};

[[nodiscard]] const char* to_text(TcpOrderServerStatus status) noexcept;

}  // namespace mini_ats::gateway
