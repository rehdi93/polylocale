#pragma once

#include <string>
#include <locale>
#include <cstdarg>
#include <iostream>

#include "boost/utility/string_view.hpp"

namespace red {

using string_view = boost::string_view;

namespace polyloc {

int do_printf(std::istream& format, std::ostream& outs, const std::locale& loc, va_list va);
int do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list va);


}

}