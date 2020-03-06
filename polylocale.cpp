#include "polylocale.h"
#include "polyprintf.h"

#include <locale>
#include <sstream>
#include <fstream>
#include <string>
#include <memory>
#include <algorithm>

#include "boost/iostreams/stream_buffer.hpp"
#include "boost/iostreams/device/array.hpp"

struct poly_impl
{
    std::locale loc;
    std::string name;
};


static auto make_polylocale(std::locale const& base) {
    auto plc = std::make_unique<poly_locale>();
    
    plc->impl = new poly_impl{ base, base.name() };
    plc->name = plc->impl->name.c_str();
    
    return plc;
}

static auto make_polylocale(locale_t loc) {
    auto plc = std::make_unique<poly_locale>();
    plc->impl = new poly_impl(*loc->impl);
    plc->name = plc->impl->name.c_str();

    return plc;
}

static auto getloc(locale_t lc) -> std::locale&
{
    return lc->impl->loc;
}


locale_t newlocale(int category_mask, const char* localename, locale_t baseloc)
{
    try
    {
        auto base = std::locale::classic();
        if (baseloc)
            base = baseloc->impl->loc;

        auto loc = std::locale(base, localename, category_mask);
        auto plc = make_polylocale(loc);

        return plc.release();
    }
    catch(const std::bad_alloc&)
    {
        errno = ENOMEM;
        return nullptr;
    }
    catch(const std::runtime_error&)
    {
        errno = ENOENT;
        return nullptr;
    }
}

void freelocale(locale_t loc) { 
    delete loc->impl;
    delete loc;
}

locale_t duplocale(locale_t loc)
{
    try
    {
        if (loc == LC_GLOBAL_LOCALE) {
            auto plc = make_polylocale(std::locale());
            return plc.release();
        }
        
        auto* dup = new poly_locale(*loc);
        return dup;
    }
    catch(const std::bad_alloc&)
    {
        errno = ENOMEM;
        return nullptr;
    }
}

locale_t uselocale(locale_t)
{
    // not implemented
    errno = ENOTSUP;
    return nullptr;
}

// ---

double strtod_l(const char* str, char** endptr, locale_t locale)
{
    std::istringstream ss{str};
    ss.imbue(locale->impl->loc);

    double num;
    ss >> num;

    if (endptr)
    {
        if (ss.good()) {
            std::streamoff pos = ss.tellg();
            auto* end = str + pos;
            *endptr = const_cast<char*>(end);
        } else {
            *endptr = const_cast<char*>(str);
        }
    }

    return num;
}

using array_read_buf    = boost::iostreams::stream_buffer<boost::iostreams::array_source>;
using array_write_buf   = boost::iostreams::stream_buffer<boost::iostreams::array_sink>;
using array_buf         = boost::iostreams::stream_buffer<boost::iostreams::array>;

int printf_l(const char* fmt, locale_t locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);

    array_read_buf inputBuf(fmt, strlen(fmt));
    //auto fmts = std::istream(&inputBuf);
    std::istream fmts{ &inputBuf };
    std::ostringstream tmp;
    
    red::do_printf(fmts, tmp, locale->impl->loc, va);

    if (tmp.fail())
        result = -1;
    else {
        result = tmp.tellp();
        std::cout << tmp.rdbuf();
    }

    va_end(va);
    return result;
}

int fprintf_l(FILE* fs, const char* format, locale_t locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    {
        result = vfprintf_l(fs, format, locale, va);
    }
    va_end(va);
    return result;
}

int sprintf_l(char* buffer, const char* fmt, locale_t loc, ...)
{
    int result;
    va_list va;
    va_start(va, loc);

    result = vsnprintf_l(buffer, SIZE_MAX, fmt, loc, va);

    va_end(va);
    return result;
}

int snprintf_l(char* buffer, size_t count, const char* format, locale_t locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    
    result = vsnprintf_l(buffer, count, format, locale, va);
    
    va_end(va);
    return result;
}

int vsnprintf_l(char* buffer, size_t count, const char* fmt, locale_t locT, va_list args)
{
    array_buf outputBuf (buffer, count);
    array_read_buf inputBuf (fmt, strlen(fmt));

    std::ostream outs(&outputBuf);
    std::istream fmts(&inputBuf);
    int result = red::do_printf(fmts, outs, locT->impl->loc, args).tellp();
    return result;
}

int vfprintf_l(FILE* cfile, const char* fmt, locale_t locale, va_list args)
{
    int result;
    array_read_buf fmtbuf (fmt, strlen(fmt));
    std::istream fmts(&fmtbuf);

#ifdef _MSC_VER
    auto outs = std::ofstream(cfile);
#else
    errno = ENOTSUP;
    result = -1;
#endif

    red::do_printf(fmts, outs, locale->impl->loc, args);

    result = outs.tellp();
    return result;
}

// reference:
// https://github.com/mltframework/mlt/issues/214#issuecomment-285895964
// https://code.woboq.org/userspace/glibc/locale/locale.h.html#144
// https://docs.microsoft.com/en-us/cpp/c-runtime-library/locale?view=vs-2019
// https://github.com/nothings/stb
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/locale.h.html