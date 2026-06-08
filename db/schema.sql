CREATE SCHEMA IF NOT EXISTS mini_ats;

CREATE TABLE IF NOT EXISTS mini_ats.instruments (
    instrument_id INTEGER PRIMARY KEY CHECK (instrument_id > 0),
    symbol TEXT NOT NULL UNIQUE CHECK (length(symbol) > 0),
    tick_size BIGINT NOT NULL CHECK (tick_size > 0),
    lower_price_limit BIGINT NOT NULL CHECK (lower_price_limit > 0),
    upper_price_limit BIGINT NOT NULL,
    session TEXT NOT NULL CHECK (session IN ('OPEN', 'CLOSED')),
    reference_version BIGINT NOT NULL CHECK (reference_version > 0),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    CHECK (upper_price_limit >= lower_price_limit)
);
