INSERT INTO mini_ats.instruments (
    instrument_id,
    symbol,
    tick_size,
    lower_price_limit,
    upper_price_limit,
    session,
    reference_version
) VALUES (
    1001,
    'DEMO',
    5,
    70000,
    80000,
    'OPEN',
    1
) ON CONFLICT (instrument_id) DO UPDATE SET
    symbol = EXCLUDED.symbol,
    tick_size = EXCLUDED.tick_size,
    lower_price_limit = EXCLUDED.lower_price_limit,
    upper_price_limit = EXCLUDED.upper_price_limit,
    session = EXCLUDED.session,
    reference_version = EXCLUDED.reference_version,
    updated_at = now();
