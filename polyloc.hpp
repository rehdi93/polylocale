#pragma once

#include <string>
#include <locale>
#include <cstdarg>
#include <iostream>

#include "boost/utility/string_view.hpp"

#ifndef SSIZE_MAX
using ssize_t = std::make_signed<size_t>::type;
#endif // !SSIZE_MAX


namespace red {

using boost::string_view;
using boost::wstring_view;
using boost::basic_string_view;

namespace polyloc {

struct write_result
{
    int64_t chars_written, chars_needed;
};

int do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list va);
    
auto do_printf_n(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list va) -> write_result;

}

}