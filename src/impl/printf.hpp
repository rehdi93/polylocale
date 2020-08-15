#pragma once

#include <locale>
#include <cstdarg>
#include <iosfwd>
#include <utility>

#include "polyimpl.h"


namespace red { namespace polyloc {

int do_printf(string_view format, std::ostream& outs, va_list args);

int do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list va);

// Prints at most 'max' chars of the contents of 'args' into 'outs' according to the format string.
// returns a pair containing:
//  - num. chars needed to write the full contents of args into ostream.
//  - num. chars written to ostream. In the range [0, max).
auto do_printf(string_view format, std::ostream& outs, size_t max, va_list args)->std::pair<int, int>;

auto do_printf(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list va)->std::pair<int, int>;

}} // red::polyloc