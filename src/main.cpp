#include "domain/order.hpp"
#include "gateway/gateway_recorder.hpp"
#include "gateway/order_gateway.hpp"
#include "gateway/tcp_order_server.hpp"
#include "engine/matching_engine.hpp"
#include "engine/order_book.hpp"
#include "marketdata/market_data_publisher.hpp"
#include "stats/operational_stats.hpp"

#include <charconv>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <istream>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] mini_ats::domain::InstrumentReference gateway_demo_instrument() noexcept {
    return mini_ats::domain::InstrumentReference{
        .id = mini_ats::domain::InstrumentId{1001},
        .tick_size = mini_ats::domain::Price{5},
        .lower_price_limit = mini_ats::domain::Price{70000},
        .upper_price_limit = mini_ats::domain::Price{80000},
        .session = mini_ats::domain::MarketSession::Open,
        .version = mini_ats::domain::SequenceNumber{7},
    };
}

[[nodiscard]] bool is_blank_or_comment(std::string_view line) noexcept {
    const auto first = line.find_first_not_of(" \t\r\n");
    return first == std::string_view::npos || line[first] == '#';
}

[[nodiscard]] std::optional<std::uint16_t> parse_port(std::string_view text) noexcept {
    std::uint32_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || position != end || value > 65535) {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>(value);
}

int run_gateway(std::istream& input,
                std::ostream& output,
                std::ostream* accepted_input_log,
                std::ostream* stats_output) {
    mini_ats::engine::MatchingEngine engine{gateway_demo_instrument()};
    mini_ats::stats::OperationalStatistics stats{};

    std::string line{};
    while (std::getline(input, line)) {
        if (is_blank_or_comment(line)) {
            continue;
        }

        const auto started_at = std::chrono::steady_clock::now();
        const auto response = accepted_input_log == nullptr
                                  ? mini_ats::gateway::handle_text_command(engine, line)
                                  : mini_ats::gateway::handle_recorded_text_command(
                                        engine, line, *accepted_input_log)
                                        .response;
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started_at);
        if (stats_output != nullptr) {
            stats.record_command_result(response.accepted(), response.trades, elapsed);
        }

        output << mini_ats::gateway::format_gateway_response(response) << '\n';
    }

    output.flush();
    if (stats_output != nullptr) {
        *stats_output << mini_ats::stats::format_operational_statistics(stats.snapshot())
                      << '\n';
    }

    const bool log_ok = accepted_input_log == nullptr || accepted_input_log->good();
    const bool stats_ok = stats_output == nullptr || stats_output->good();
    return output.good() && log_ok && stats_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int run_tcp_gateway(std::uint16_t port,
                    std::ostream* accepted_input_log,
                    mini_ats::marketdata::UdpMarketDataPublisher* market_data_publisher) {
    mini_ats::engine::MatchingEngine engine{gateway_demo_instrument()};
    mini_ats::gateway::TcpOrderServer server{engine, accepted_input_log,
                                             market_data_publisher};

    const auto listen_status = server.listen("127.0.0.1", port);
    if (listen_status != mini_ats::gateway::TcpOrderServerStatus::Ok) {
        std::cerr << "failed to start TCP gateway: "
                  << mini_ats::gateway::to_text(listen_status) << '\n';
        return EXIT_FAILURE;
    }

    std::cerr << "Mini ATS TCP gateway listening on 127.0.0.1:" << server.port() << '\n';
    if (market_data_publisher != nullptr) {
        std::cerr << "Mini ATS market data publisher enabled" << '\n';
    }
    const auto result = server.serve();
    if (!result.ok()) {
        std::cerr << "TCP gateway stopped: " << mini_ats::gateway::to_text(result.status)
                  << " clients=" << result.clients_served
                  << " commands=" << result.commands_processed << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void print_usage(std::ostream& output, std::string_view program_name) {
    output << "Usage:\n"
           << "  " << program_name << "              Run deterministic matching demo\n"
           << "  " << program_name << " --gateway [--record-log <path>] [--stats]\n"
           << "                         Read text commands from stdin\n"
           << "                         Append accepted input commands and/or print stats\n"
           << "  " << program_name
           << " --tcp --port <port> [--record-log <path>]\n"
           << "                         [--market-data <addr> <port>]\n"
           << "                         Serve TCP orders and optionally publish market data\n"
           << "  " << program_name << " --help       Show this help\n";
}

int run_demo() {
    using mini_ats::domain::InstrumentId;
    using mini_ats::domain::Order;
    using mini_ats::domain::OrderId;
    using mini_ats::domain::OrderType;
    using mini_ats::domain::Price;
    using mini_ats::domain::Quantity;
    using mini_ats::domain::SequenceNumber;
    using mini_ats::domain::Side;
    using mini_ats::domain::TimeInForce;
    using mini_ats::engine::MatchingEngine;
    using mini_ats::engine::format_order_book;

    MatchingEngine engine{InstrumentId{1001}};

    const auto resting_sell_1 = engine.submit_order(Order{
        .id = OrderId{10},
        .instrument_id = InstrumentId{1001},
        .side = Side::Sell,
        .type = OrderType::Limit,
        .time_in_force = TimeInForce::Day,
        .price = Price{73700},
        .quantity = Quantity{3},
        .sequence = SequenceNumber{1},
    });
    if (resting_sell_1.reports.empty() || resting_sell_1.reports[0].is_rejected()) {
        std::cerr << "failed to submit resting sell order 10" << '\n';
        return EXIT_FAILURE;
    }

    const auto resting_sell_2 = engine.submit_order(Order{
        .id = OrderId{11},
        .instrument_id = InstrumentId{1001},
        .side = Side::Sell,
        .type = OrderType::Limit,
        .time_in_force = TimeInForce::Day,
        .price = Price{73800},
        .quantity = Quantity{4},
        .sequence = SequenceNumber{2},
    });
    if (resting_sell_2.reports.empty() || resting_sell_2.reports[0].is_rejected()) {
        std::cerr << "failed to submit resting sell order 11" << '\n';
        return EXIT_FAILURE;
    }

    const auto incoming_buy = engine.submit_order(Order{
        .id = OrderId{20},
        .instrument_id = InstrumentId{1001},
        .side = Side::Buy,
        .type = OrderType::Limit,
        .time_in_force = TimeInForce::Day,
        .price = Price{73800},
        .quantity = Quantity{10},
        .sequence = SequenceNumber{3},
    });
    if (incoming_buy.reports.empty()) {
        std::cerr << "failed to submit incoming buy order 20" << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Mini ATS Matching System demo\n";
    std::cout << "Input\n";
    std::cout << "  resting sell #10 price=73700 qty=3\n";
    std::cout << "  resting sell #11 price=73800 qty=4\n";
    std::cout << "  incoming buy #20 price=73800 qty=10\n\n";

    std::cout << "Trades\n";
    if (incoming_buy.trades.empty()) {
        std::cout << "  (none)\n";
    } else {
        for (const auto& trade : incoming_buy.trades) {
            std::cout << "  trade#" << trade.id << " resting#" << trade.resting_order_id
                      << " incoming#" << trade.incoming_order_id << " price=" << trade.price
                      << " qty=" << trade.quantity << '\n';
        }
    }

    std::cout << "\nFinal book\n";
    std::cout << format_order_book(engine.order_book().snapshot());
    return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string_view program_name = argc > 0 ? argv[0] : "mini_ats";

    if (argc == 1) {
        return run_demo();
    }

    const std::string_view mode{argv[1]};
    if (mode == "--gateway") {
        std::ofstream accepted_input_log{};
        std::ostream* accepted_input_log_ptr = nullptr;
        bool stats_enabled = false;

        int index = 2;
        while (index < argc) {
            const std::string_view option{argv[index]};
            if (option == "--record-log") {
                if (index + 1 >= argc || accepted_input_log_ptr != nullptr) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                accepted_input_log.open(argv[index + 1], std::ios::app);
                if (!accepted_input_log.is_open()) {
                    std::cerr << "failed to open replay log: " << argv[index + 1] << '\n';
                    return EXIT_FAILURE;
                }

                accepted_input_log_ptr = &accepted_input_log;
                index += 2;
                continue;
            }

            if (option == "--stats") {
                stats_enabled = true;
                ++index;
                continue;
            }

            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        return run_gateway(std::cin, std::cout, accepted_input_log_ptr,
                           stats_enabled ? &std::cerr : nullptr);
    }

    if (mode == "--tcp") {
        if (argc < 4) {
            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        if (std::string_view{argv[2]} != "--port") {
            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        const auto port = parse_port(argv[3]);
        if (!port.has_value()) {
            std::cerr << "invalid TCP port: " << argv[3] << '\n';
            return EXIT_FAILURE;
        }

        std::ofstream accepted_input_log{};
        std::ostream* accepted_input_log_ptr = nullptr;
        mini_ats::marketdata::UdpMarketDataPublisher market_data_publisher{};
        mini_ats::marketdata::UdpMarketDataPublisher* market_data_publisher_ptr = nullptr;

        int index = 4;
        while (index < argc) {
            const std::string_view option{argv[index]};
            if (option == "--record-log") {
                if (index + 1 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                accepted_input_log.open(argv[index + 1], std::ios::app);
                if (!accepted_input_log.is_open()) {
                    std::cerr << "failed to open replay log: " << argv[index + 1] << '\n';
                    return EXIT_FAILURE;
                }
                accepted_input_log_ptr = &accepted_input_log;
                index += 2;
                continue;
            }

            if (option == "--market-data") {
                if (index + 2 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                const auto market_data_port = parse_port(argv[index + 2]);
                if (!market_data_port.has_value()) {
                    std::cerr << "invalid market data port: " << argv[index + 2] << '\n';
                    return EXIT_FAILURE;
                }

                const auto open_status = market_data_publisher.open(argv[index + 1],
                                                                    *market_data_port);
                if (open_status != mini_ats::marketdata::MarketDataPublishStatus::Ok) {
                    std::cerr << "failed to open market data publisher: "
                              << mini_ats::marketdata::to_text(open_status) << '\n';
                    return EXIT_FAILURE;
                }

                market_data_publisher_ptr = &market_data_publisher;
                index += 3;
                continue;
            }

            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        return run_tcp_gateway(*port, accepted_input_log_ptr, market_data_publisher_ptr);
    }

    if (mode == "--help" || mode == "-h") {
        print_usage(std::cout, program_name);
        return EXIT_SUCCESS;
    }

    print_usage(std::cerr, program_name);
    return EXIT_FAILURE;
}
