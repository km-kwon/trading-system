#include "reference_data/instrument_repository.hpp"

#include <gtest/gtest.h>

#include <string_view>

namespace {

using mini_ats::domain::InstrumentId;
using mini_ats::domain::MarketSession;
using mini_ats::domain::Price;
using mini_ats::domain::SequenceNumber;
using mini_ats::reference_data::InstrumentLoadError;
using mini_ats::reference_data::InstrumentRecord;
using mini_ats::reference_data::PostgresInstrumentLoadError;
using mini_ats::reference_data::PostgresInstrumentRepositoryConfig;
using mini_ats::reference_data::build_psql_instrument_command;
using mini_ats::reference_data::format_instrument_reference;
using mini_ats::reference_data::instrument_reference_query;
using mini_ats::reference_data::instrument_reference_psql_query;
using mini_ats::reference_data::load_instrument_reference_from_postgres;
using mini_ats::reference_data::map_instrument_record;
using mini_ats::reference_data::parse_psql_instrument_result;
using mini_ats::reference_data::parse_market_session;

InstrumentRecord make_valid_record() {
    return InstrumentRecord{
        .instrument_id = InstrumentId{1001},
        .symbol = "DEMO",
        .tick_size = Price{5},
        .lower_price_limit = Price{70000},
        .upper_price_limit = Price{80000},
        .session = "OPEN",
        .reference_version = SequenceNumber{7},
    };
}

TEST(ReferenceDataTest, ParsesMarketSessionFromPostgresValue) {
    ASSERT_TRUE(parse_market_session("OPEN").has_value());
    EXPECT_EQ(*parse_market_session("OPEN"), MarketSession::Open);

    ASSERT_TRUE(parse_market_session("CLOSED").has_value());
    EXPECT_EQ(*parse_market_session("CLOSED"), MarketSession::Closed);

    EXPECT_FALSE(parse_market_session("AUCTION").has_value());
}

TEST(ReferenceDataTest, MapsInstrumentRecordToInstrumentReference) {
    const auto result = map_instrument_record(make_valid_record());

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.reference.has_value());
    EXPECT_EQ(result.reference->id, InstrumentId{1001});
    EXPECT_EQ(result.reference->tick_size, Price{5});
    EXPECT_EQ(result.reference->lower_price_limit, Price{70000});
    EXPECT_EQ(result.reference->upper_price_limit, Price{80000});
    EXPECT_EQ(result.reference->session, MarketSession::Open);
    EXPECT_EQ(result.reference->version, SequenceNumber{7});
}

TEST(ReferenceDataTest, RejectsInvalidInstrumentRecordFields) {
    auto record = make_valid_record();
    record.tick_size = Price{0};
    EXPECT_EQ(map_instrument_record(record).error, InstrumentLoadError::InvalidTickSize);

    record = make_valid_record();
    record.upper_price_limit = Price{69000};
    EXPECT_EQ(map_instrument_record(record).error, InstrumentLoadError::InvalidPriceLimits);

    record = make_valid_record();
    record.session = "HALTED";
    EXPECT_EQ(map_instrument_record(record).error, InstrumentLoadError::InvalidSession);

    record = make_valid_record();
    record.reference_version = SequenceNumber{0};
    EXPECT_EQ(map_instrument_record(record).error, InstrumentLoadError::InvalidVersion);
}

TEST(ReferenceDataTest, ExposesParameterizedInstrumentReferenceQuery) {
    const std::string_view query = instrument_reference_query();

    EXPECT_NE(query.find("mini_ats.instruments"), std::string_view::npos);
    EXPECT_NE(query.find("instrument_id = $1"), std::string_view::npos);
    EXPECT_NE(query.find("reference_version"), std::string_view::npos);
}

TEST(ReferenceDataTest, BuildsPsqlInstrumentQueryAndCommand) {
    const auto query = instrument_reference_psql_query(InstrumentId{1001});

    EXPECT_NE(query.find("mini_ats.instruments"), std::string::npos);
    EXPECT_NE(query.find("instrument_id = 1001"), std::string::npos);
    EXPECT_EQ(query.find("$1"), std::string::npos);

    const auto command = build_psql_instrument_command(
        PostgresInstrumentRepositoryConfig{
            .database = "mini_ats",
            .user = "demo'user",
            .psql_path = "psql",
        },
        InstrumentId{1001});

    EXPECT_NE(command.find("'psql'"), std::string::npos);
    EXPECT_NE(command.find("-d 'mini_ats'"), std::string::npos);
    EXPECT_NE(command.find("-U 'demo'\\''user'"), std::string::npos);
    EXPECT_NE(command.find("-F '\t'"), std::string::npos);
    EXPECT_NE(command.find("2>/dev/null"), std::string::npos);
}

TEST(ReferenceDataTest, ParsesPsqlInstrumentResultRow) {
    const auto result = parse_psql_instrument_result(
        "1001\tDEMO\t5\t70000\t80000\tOPEN\t7\n");

    ASSERT_TRUE(result.ok());
    ASSERT_TRUE(result.reference.has_value());
    EXPECT_EQ(result.reference->id, InstrumentId{1001});
    EXPECT_EQ(result.reference->tick_size, Price{5});
    EXPECT_EQ(result.reference->version, SequenceNumber{7});
    EXPECT_EQ(format_instrument_reference(*result.reference),
              "INSTRUMENT instrument_id=1001 tick_size=5 lower_price_limit=70000 "
              "upper_price_limit=80000 session=OPEN reference_version=7");
}

TEST(ReferenceDataTest, RejectsMalformedPsqlInstrumentResults) {
    EXPECT_EQ(parse_psql_instrument_result("").error,
              PostgresInstrumentLoadError::EmptyResult);
    EXPECT_EQ(parse_psql_instrument_result(
                  "1001\tDEMO\t5\t70000\t80000\tOPEN\t7\n"
                  "1002\tALT\t5\t70000\t80000\tOPEN\t7\n")
                  .error,
              PostgresInstrumentLoadError::MultipleRows);
    EXPECT_EQ(parse_psql_instrument_result("1001\tDEMO\t5").error,
              PostgresInstrumentLoadError::InvalidFieldCount);
    EXPECT_EQ(parse_psql_instrument_result(
                  "1001\tDEMO\tnot-a-number\t70000\t80000\tOPEN\t7\n")
                  .error,
              PostgresInstrumentLoadError::InvalidNumber);
    const auto mapping_failed = parse_psql_instrument_result(
        "1001\tDEMO\t5\t70000\t80000\tHALTED\t7\n");
    EXPECT_EQ(mapping_failed.error, PostgresInstrumentLoadError::MappingFailed);
    EXPECT_EQ(mapping_failed.mapping_error, InstrumentLoadError::InvalidSession);
}

TEST(ReferenceDataTest, ReportsPostgresCommandFailureWithoutMutatingState) {
    EXPECT_EQ(load_instrument_reference_from_postgres(
                  PostgresInstrumentRepositoryConfig{}, InstrumentId{0})
                  .error,
              PostgresInstrumentLoadError::InvalidInstrumentId);

    const auto result = load_instrument_reference_from_postgres(
        PostgresInstrumentRepositoryConfig{
            .database = "mini_ats",
            .user = "",
            .psql_path = "definitely-missing-psql-binary",
        },
        InstrumentId{1001});

    EXPECT_EQ(result.error, PostgresInstrumentLoadError::CommandFailed);
}

}  // namespace
