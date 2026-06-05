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
    src/domain/order.cpp
)

target_include_directories(mini_ats_domain
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(mini_ats_domain PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_domain PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(mini_ats
    src/main.cpp
)

target_link_libraries(mini_ats
    PRIVATE
        mini_ats_domain
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(mini_ats_unit_tests
    tests/unit/order_test.cpp
)

target_link_libraries(mini_ats_unit_tests
    PRIVATE
        mini_ats_domain
        GTest::gtest_main
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(mini_ats_unit_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

gtest_discover_tests(mini_ats_unit_tests)
