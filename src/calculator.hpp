#pragma once
#include "config.hpp"
#include "store.hpp"

// Build a CachedDay from raw prices: runs the pricing formula for every
// provider and pre-generates all JSON response strings.
CachedDay make_cached_day(const DayPrices& prices, const AreaConfig& area);
