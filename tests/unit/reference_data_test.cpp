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
using mini_ats::reference_data::instrument_reference_query;
using mini_ats::reference_data::map_instrument_record;
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

}  // namespace
