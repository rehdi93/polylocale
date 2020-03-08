#pragma once

#include <string>
#include <locale>
#include <cstdarg>
#include <iostream>

#include "boost/utility/string_view.hpp"

namespace red {

using string_view = boost::string_view;
using wstring_view = boost::wstring_view;

namespace polyloc {

int do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list va);


}

}