#pragma once
#include "store.hpp"
#include <string_view>

// Parse an ENTSO-E A44 XML response body into a DayPrices.
// date_local: the calendar date (YYYY-MM-DD, area-local) this request covers.
// Throws std::runtime_error on malformed input or missing PT15M period.
DayPrices parse_entsoe_xml(std::string_view xml, std::string_view date_local);
