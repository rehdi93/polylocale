#pragma once

#include <string>
#include <locale>
#include <cstdarg>
#include <iostream>


namespace red
{

//std::string do_printf(std::string format, const std::locale& loc, va_list va);

std::ostream& do_printf(std::istream& format, std::ostream& outs, const std::locale& loc, va_list va);

int do_printf(std::iostream& st, const std::locale& loc, va_list va);

}