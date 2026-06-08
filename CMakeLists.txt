cmake_minimum_required(VERSION 3.20)

project(
    MiniAtsMatchingSystem
    VERSION 0.1.0
    DESCRIPTION "Minimal ATS matching system portfolio project"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mini_ats_domain
    src/domain/cancel_request.cpp
    src/domain/execution_report.cpp
    src/domain/instrument.cpp
    src/domain/order.cpp
    src/domain/trade.cpp
)

target_include_directories(mini_ats_domain
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(mini_ats_domain PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_domain PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_engine
    src/engine/matching_engine.cpp
    src/engine/order_book.cpp
)

target_include_directories(mini_ats_engine
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_engine
    PUBLIC
        mini_ats_domain
)

target_compile_features(mini_ats_engine PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_reference_data
    src/reference_data/instrument_repository.cpp
)

target_include_directories(mini_ats_reference_data
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_reference_data
    PUBLIC
        mini_ats_domain
)

target_compile_features(mini_ats_reference_data PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_reference_data PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_replay
    src/replay/replay_log.cpp
)

target_include_directories(mini_ats_replay
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_replay
    PUBLIC
        mini_ats_engine
)

target_compile_features(mini_ats_replay PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_replay PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_protocol
    src/protocol/text_command_parser.cpp
)

target_include_directories(mini_ats_protocol
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_protocol
    PUBLIC
        mini_ats_replay
)

target_compile_features(mini_ats_protocol PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_protocol PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_replay_io
    src/replay/replay_log_io.cpp
)

target_include_directories(mini_ats_replay_io
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_replay_io
    PUBLIC
        mini_ats_protocol
)

target_compile_features(mini_ats_replay_io PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_replay_io PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_gateway
    src/gateway/gateway_recorder.cpp
    src/gateway/order_gateway.cpp
    src/gateway/tcp_order_server.cpp
)

target_include_directories(mini_ats_gateway
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_gateway
    PUBLIC
        mini_ats_protocol
        mini_ats_replay_io
)

target_compile_features(mini_ats_gateway PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_gateway PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(mini_ats_marketdata
    src/marketdata/market_data.cpp
    src/marketdata/market_data_publisher.cpp
)

target_include_directories(mini_ats_marketdata
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_marketdata
    PUBLIC
        mini_ats_engine
)

target_compile_features(mini_ats_marketdata PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_marketdata PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(mini_ats_gateway
    PUBLIC
        mini_ats_marketdata
)

add_library(mini_ats_stats
    src/stats/operational_stats.cpp
)

target_include_directories(mini_ats_stats
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_ats_stats
    PUBLIC
        mini_ats_engine
)

target_compile_features(mini_ats_stats PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_stats PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(mini_ats
    src/main.cpp
)

target_link_libraries(mini_ats
    PRIVATE
        mini_ats_engine
        mini_ats_gateway
        mini_ats_stats
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(mini_ats_unit_tests
    tests/unit/domain_model_test.cpp
    tests/unit/gateway_test.cpp
    tests/unit/market_data_test.cpp
    tests/unit/market_data_publisher_test.cpp
    tests/unit/matching_engine_test.cpp
    tests/unit/operational_stats_test.cpp
    tests/unit/order_test.cpp
    tests/unit/order_book_test.cpp
    tests/unit/reference_data_test.cpp
    tests/unit/replay_log_io_test.cpp
    tests/unit/replay_test.cpp
    tests/unit/tcp_order_server_test.cpp
    tests/unit/text_command_parser_test.cpp
)

target_link_libraries(mini_ats_unit_tests
    PRIVATE
        mini_ats_engine
        mini_ats_gateway
        mini_ats_marketdata
        mini_ats_reference_data
        mini_ats_replay
        mini_ats_replay_io
        mini_ats_stats
        mini_ats_protocol
        GTest::gtest_main
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_unit_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(mini_ats_unit_tests)
