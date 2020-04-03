#pragma once

#include <locale>
#include <cstdarg>
#include <iosfwd>

#include "polyimpl.h"


namespace red { namespace polyloc {

struct write_result
{
    int chars_written, chars_needed;
};

int do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list va);
    
auto do_printf(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list va) -> write_result;

}}