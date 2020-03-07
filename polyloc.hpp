#pragma once

#include <string>
#include <locale>
#include <cstdarg>
#include <iostream>


namespace red { namespace polyloc {

int do_printf(std::istream& format, std::ostream& outs, const std::locale& loc, va_list va);


}}