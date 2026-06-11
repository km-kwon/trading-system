#include "benchmark/deterministic_benchmark.hpp"
#include "domain/order.hpp"
#include "gateway/gateway_recorder.hpp"
#include "gateway/order_gateway.hpp"
#include "gateway/tcp_order_server.hpp"
#include "engine/matching_engine.hpp"
#include "engine/order_book.hpp"
#include "marketdata/market_data_publisher.hpp"
#include "reference_data/instrument_repository.hpp"
#include "stats/operational_stats.hpp"

#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <istream>
#include <limits>
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

[[nodiscard]] std::optional<std::size_t> parse_size(std::string_view text) noexcept {
    std::uint64_t value{};
    const char* const begin = text.data();
    const char* const end = text.data() + text.size();
    const auto [position, error] = std::from_chars(begin, end, value);
    if (error != std::errc{} || position != end) {
        return std::nullopt;
    }

    return static_cast<std::size_t>(value);
}

[[nodiscard]] std::optional<mini_ats::domain::InstrumentId> parse_instrument_id(
    std::string_view text) noexcept {
    const auto parsed = parse_size(text);
    if (!parsed.has_value() || *parsed > static_cast<std::size_t>(
                                      std::numeric_limits<mini_ats::domain::InstrumentId>::max())) {
        return std::nullopt;
    }

    return static_cast<mini_ats::domain::InstrumentId>(*parsed);
}

[[nodiscard]] std::string default_database_name() {
    if (const char* value = std::getenv("MINI_ATS_DB_NAME"); value != nullptr && *value != '\0') {
        return value;
    }

    return "mini_ats";
}

[[nodiscard]] std::string default_database_user() {
    if (const char* value = std::getenv("MINI_ATS_DB_USER"); value != nullptr && *value != '\0') {
        return value;
    }

    if (const char* value = std::getenv("USER"); value != nullptr && *value != '\0') {
        return value;
    }

    return {};
}

struct ReferenceDataOptions {
    bool load_reference_data{false};
    bool postgres_option_seen{false};
    std::optional<mini_ats::domain::InstrumentId> instrument_id{};
    mini_ats::reference_data::PostgresInstrumentRepositoryConfig config{
        .database = default_database_name(),
        .user = default_database_user(),
        .psql_path = "psql",
    };
};

enum class CliOptionParseResult {
    Handled,
    NotHandled,
    Error,
};

[[nodiscard]] CliOptionParseResult parse_reference_data_option(
    std::string_view option,
    int argc,
    char* argv[],
    int& index,
    ReferenceDataOptions& options,
    std::ostream& error) {
    if (option == "--load-reference-data") {
        options.load_reference_data = true;
        ++index;
        return CliOptionParseResult::Handled;
    }

    if (option == "--instrument-id") {
        if (index + 1 >= argc) {
            error << "missing value for --instrument-id" << '\n';
            return CliOptionParseResult::Error;
        }

        options.instrument_id = parse_instrument_id(argv[index + 1]);
        if (!options.instrument_id.has_value() || *options.instrument_id == 0) {
            error << "invalid instrument id: " << argv[index + 1] << '\n';
            return CliOptionParseResult::Error;
        }

        index += 2;
        return CliOptionParseResult::Handled;
    }

    if (option == "--db-name") {
        if (index + 1 >= argc) {
            error << "missing value for --db-name" << '\n';
            return CliOptionParseResult::Error;
        }

        options.config.database = argv[index + 1];
        options.postgres_option_seen = true;
        index += 2;
        return CliOptionParseResult::Handled;
    }

    if (option == "--db-user") {
        if (index + 1 >= argc) {
            error << "missing value for --db-user" << '\n';
            return CliOptionParseResult::Error;
        }

        options.config.user = argv[index + 1];
        options.postgres_option_seen = true;
        index += 2;
        return CliOptionParseResult::Handled;
    }

    if (option == "--psql") {
        if (index + 1 >= argc) {
            error << "missing value for --psql" << '\n';
            return CliOptionParseResult::Error;
        }

        options.config.psql_path = argv[index + 1];
        options.postgres_option_seen = true;
        index += 2;
        return CliOptionParseResult::Handled;
    }

    return CliOptionParseResult::NotHandled;
}

void print_instrument_load_error(
    std::ostream& error,
    std::string_view prefix,
    const mini_ats::reference_data::PostgresInstrumentLoadResult& result) {
    error << prefix << mini_ats::reference_data::to_text(result.error);
    if (result.mapping_error != mini_ats::reference_data::InstrumentLoadError::None) {
        error << " mapping=" << mini_ats::reference_data::to_text(result.mapping_error);
    }
    if (!result.detail.empty()) {
        error << " detail=" << result.detail;
    }
    error << '\n';
}

[[nodiscard]] std::optional<mini_ats::domain::InstrumentReference> resolve_gateway_instrument(
    const ReferenceDataOptions& options,
    std::ostream& error) {
    if (!options.load_reference_data) {
        if (options.instrument_id.has_value() || options.postgres_option_seen) {
            error << "reference data options require --load-reference-data" << '\n';
            return std::nullopt;
        }

        return gateway_demo_instrument();
    }

    if (!options.instrument_id.has_value()) {
        error << "--load-reference-data requires --instrument-id <id>" << '\n';
        return std::nullopt;
    }

    const auto result = mini_ats::reference_data::load_instrument_reference_from_postgres(
        options.config, *options.instrument_id);
    if (!result.ok()) {
        print_instrument_load_error(error, "failed to load reference data: ", result);
        return std::nullopt;
    }

    return result.reference;
}

int run_gateway(const mini_ats::domain::InstrumentReference& instrument,
                std::istream& input,
                std::ostream& output,
                std::ostream* accepted_input_log,
                std::ostream* stats_output) {
    mini_ats::engine::MatchingEngine engine{instrument};
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

int run_tcp_gateway(const mini_ats::domain::InstrumentReference& instrument,
                    std::uint16_t port,
                    std::ostream* accepted_input_log,
                    mini_ats::marketdata::UdpMarketDataPublisher* market_data_publisher,
                    std::ostream* stats_output) {
    mini_ats::engine::MatchingEngine engine{instrument};
    mini_ats::stats::OperationalStatistics stats{};
    auto* stats_ptr = stats_output == nullptr ? nullptr : &stats;
    mini_ats::gateway::TcpOrderServer server{engine, accepted_input_log,
                                             market_data_publisher, 1, stats_ptr};

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
    if (stats_output != nullptr) {
        std::cerr << "Mini ATS TCP stats enabled" << '\n';
    }
    const auto result = server.serve();
    if (stats_output != nullptr) {
        *stats_output << mini_ats::stats::format_operational_statistics(stats.snapshot())
                      << '\n';
    }
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
           << "                         [--load-reference-data --instrument-id <id>]\n"
           << "                         [--db-name <name>] [--db-user <user>] [--psql <path>]\n"
           << "                         Read text commands from stdin\n"
           << "                         Append accepted input commands and/or print stats\n"
           << "  " << program_name
           << " --tcp --port <port> [--record-log <path>]\n"
           << "                         [--market-data <addr> <port>] [--stats]\n"
           << "                         [--load-reference-data --instrument-id <id>]\n"
           << "                         [--db-name <name>] [--db-user <user>] [--psql <path>]\n"
           << "                         Serve TCP orders and optionally publish market data\n"
           << "  " << program_name << " --benchmark [--iterations <n>] [--output <path>]\n"
           << "                         Run deterministic gateway benchmark scenario\n"
           << "                         Append result payload when --output is set\n"
           << "  " << program_name
           << " --load-instrument --instrument-id <id>\n"
           << "                         [--db-name <name>] [--db-user <user>] [--psql <path>]\n"
           << "                         Load one instrument reference from PostgreSQL via psql\n"
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
        ReferenceDataOptions reference_data_options{};

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

            const auto reference_data_option = parse_reference_data_option(
                option, argc, argv, index, reference_data_options, std::cerr);
            if (reference_data_option == CliOptionParseResult::Handled) {
                continue;
            }
            if (reference_data_option == CliOptionParseResult::Error) {
                return EXIT_FAILURE;
            }

            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        const auto instrument = resolve_gateway_instrument(reference_data_options, std::cerr);
        if (!instrument.has_value()) {
            return EXIT_FAILURE;
        }

        return run_gateway(*instrument, std::cin, std::cout, accepted_input_log_ptr,
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
        bool stats_enabled = false;
        ReferenceDataOptions reference_data_options{};

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

            if (option == "--stats") {
                stats_enabled = true;
                ++index;
                continue;
            }

            const auto reference_data_option = parse_reference_data_option(
                option, argc, argv, index, reference_data_options, std::cerr);
            if (reference_data_option == CliOptionParseResult::Handled) {
                continue;
            }
            if (reference_data_option == CliOptionParseResult::Error) {
                return EXIT_FAILURE;
            }

            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        const auto instrument = resolve_gateway_instrument(reference_data_options, std::cerr);
        if (!instrument.has_value()) {
            return EXIT_FAILURE;
        }

        return run_tcp_gateway(*instrument, *port, accepted_input_log_ptr, market_data_publisher_ptr,
                               stats_enabled ? &std::cerr : nullptr);
    }

    if (mode == "--benchmark") {
        std::size_t iterations = 1000;
        std::optional<std::string> output_path{};
        int index = 2;
        while (index < argc) {
            const std::string_view option{argv[index]};
            if (option == "--iterations") {
                if (index + 1 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                const auto parsed_iterations = parse_size(argv[index + 1]);
                if (!parsed_iterations.has_value() || *parsed_iterations == 0) {
                    std::cerr << "invalid benchmark iterations: " << argv[index + 1] << '\n';
                    return EXIT_FAILURE;
                }

                iterations = *parsed_iterations;
                index += 2;
                continue;
            }

            if (option == "--output") {
                if (index + 1 >= argc || output_path.has_value()) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                output_path = argv[index + 1];
                index += 2;
                continue;
            }

            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        std::ofstream benchmark_output{};
        if (output_path.has_value()) {
            benchmark_output.open(*output_path, std::ios::app);
            if (!benchmark_output.is_open()) {
                std::cerr << "failed to open benchmark output: " << *output_path << '\n';
                return EXIT_FAILURE;
            }
        }

        const auto result = mini_ats::benchmark::run_deterministic_gateway_benchmark(iterations);
        const auto formatted = mini_ats::benchmark::format_deterministic_benchmark_result(result);
        std::cout << formatted << '\n';
        if (benchmark_output.is_open()) {
            benchmark_output << formatted << '\n';
        }

        const bool benchmark_output_ok = !benchmark_output.is_open() || benchmark_output.good();
        return std::cout.good() && benchmark_output_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (mode == "--load-instrument") {
        std::optional<mini_ats::domain::InstrumentId> instrument_id{};
        mini_ats::reference_data::PostgresInstrumentRepositoryConfig config{
            .database = default_database_name(),
            .user = default_database_user(),
            .psql_path = "psql",
        };

        int index = 2;
        while (index < argc) {
            const std::string_view option{argv[index]};
            if (option == "--instrument-id") {
                if (index + 1 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                instrument_id = parse_instrument_id(argv[index + 1]);
                if (!instrument_id.has_value() || *instrument_id == 0) {
                    std::cerr << "invalid instrument id: " << argv[index + 1] << '\n';
                    return EXIT_FAILURE;
                }

                index += 2;
                continue;
            }

            if (option == "--db-name") {
                if (index + 1 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                config.database = argv[index + 1];
                index += 2;
                continue;
            }

            if (option == "--db-user") {
                if (index + 1 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                config.user = argv[index + 1];
                index += 2;
                continue;
            }

            if (option == "--psql") {
                if (index + 1 >= argc) {
                    print_usage(std::cerr, program_name);
                    return EXIT_FAILURE;
                }

                config.psql_path = argv[index + 1];
                index += 2;
                continue;
            }

            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        if (!instrument_id.has_value()) {
            print_usage(std::cerr, program_name);
            return EXIT_FAILURE;
        }

        const auto result = mini_ats::reference_data::load_instrument_reference_from_postgres(
            config, *instrument_id);
        if (!result.ok()) {
            print_instrument_load_error(std::cerr, "failed to load instrument: ", result);
            return EXIT_FAILURE;
        }

        std::cout << mini_ats::reference_data::format_instrument_reference(*result.reference)
                  << '\n';
        return std::cout.good() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    if (mode == "--help" || mode == "-h") {
        print_usage(std::cout, program_name);
        return EXIT_SUCCESS;
    }

    print_usage(std::cerr, program_name);
    return EXIT_FAILURE;
}
