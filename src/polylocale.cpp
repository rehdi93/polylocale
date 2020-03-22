#include <locale>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <string>
#include <memory>
#include <algorithm>

#include "polylocale.h"
#include "impl/printf.hpp"
#include "impl/polyimpl.h"

#include "boost/iostreams/stream_buffer.hpp"
#include "boost/iostreams/device/array.hpp"

#ifdef __GNUC__
#include <ext/stdio_filebuf.h>
#endif

#define POLY_LC_ALL_MASK (LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK | LC_MONETARY_MASK)

struct poly_impl
{
    std::locale loc;
    std::string name;

    poly_impl() : poly_impl(std::locale()) {}

    explicit poly_impl(const std::locale& lc)
        : loc(lc), name(lc.name()) {}
};

using poly_locale_ptr = std::unique_ptr<poly_locale, decltype(poly_freelocale)*>;


static auto make_polylocale(std::locale const& base) {
    auto plc = std::make_unique<poly_locale>();
    
    plc->impl = new poly_impl{ base };
    plc->name = plc->impl->name.c_str();
    
    return plc;
}

static auto make_polylocale(poly_locale_s loc) {
    auto plc = std::make_unique<poly_locale>();
    plc->impl = new poly_impl(*loc->impl);
    plc->name = plc->impl->name.c_str();

    return plc;
}



static auto mask_to_cat(int mask) noexcept -> std::locale::category
{
    using Lc = std::locale;

    if (mask == POLY_ALL_MASK)
        return Lc::all;
    else if ((POLY_ALL_MASK & mask) == 0)
        return -1; // mask contains an unknown bit

    std::locale::category cat{0};

    if ((mask & POLY_CTYPE_MASK) != 0)
        cat |= Lc::ctype;
    if ((mask & POLY_NUMERIC_MASK) != 0)
        cat |= Lc::numeric;
    if ((mask & POLY_TIME_MASK) != 0)
        cat |= Lc::time;
    if ((mask & POLY_COLLATE_MASK) != 0)
        cat |= Lc::collate;
    if ((mask & POLY_MONETARY_MASK) != 0)
        cat |= Lc::monetary;

    return cat;
}

extern "C" {

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/newlocale.html
poly_locale_s poly_newlocale(int category_mask, const char* localename, poly_locale_s base)
{
    try
    {
        auto const cats = mask_to_cat(category_mask);
        if (cats == -1) {
            // locale data is not available for one of the bits in mask
            errno = EINVAL;
            return nullptr;
        }

        if (base)
        {
            auto& loc = base->impl->loc;

            *base->impl = poly_impl(std::locale(loc, localename, cats));
            base->name = base->impl->name.c_str();

            return base;
        }
        else
        {
            auto lc = std::locale({}, localename, cats);
            auto plc = make_polylocale(lc);

            return plc.release();
        }
        
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

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/freelocale.html
void poly_freelocale(poly_locale_s loc) {
    if (loc)
        delete loc->impl;

    delete loc;
}

poly_locale_s poly_duplocale(poly_locale_s loc)
{
    try
    {
        if (loc == POLY_GLOBAL_LOCALE) {
            auto plc = make_polylocale(std::locale());
            return plc.release();
        }
        
        auto plc = make_polylocale(loc);
        return plc.release();
    }
    catch(const std::bad_alloc&)
    {
        errno = ENOMEM;
        return nullptr;
    }
}

// ---

double poly_strtod_l(const char* str, char** endptr, poly_locale_s locale)
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

using array_read_buf = boost::iostreams::stream_buffer<boost::iostreams::array_source>;
using array_buf = boost::iostreams::stream_buffer<boost::iostreams::array>;


int poly_printf_l(const char* fmt, poly_locale_s locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    {
        result = poly_vprintf_l(fmt, locale, va);
    }
    va_end(va);
    return result;
}

int poly_vprintf_l(const char* fmt, poly_locale_s locale, va_list args)
{
    std::ostringstream tmp;
    red::polyloc::do_printf(fmt, tmp, locale->impl->loc, args);
    auto contents = tmp.str();
    auto result = (int)contents.size();
    std::cout << contents;
    return result;
}


int poly_sprintf_l(char* buffer, const char* fmt, poly_locale_s loc, ...)
{
    int result;
    va_list va;
    va_start(va, loc);
    {
        result = poly_vsprintf_l(buffer, fmt, loc, va);
    }
    va_end(va);
    return result;
}

int poly_vsprintf_l(char* buffer, const char* fmt, poly_locale_s loc, va_list args)
{
    std::ostringstream tmp;
    red::polyloc::do_printf(fmt, tmp, loc->impl->loc, args);
    auto contents = tmp.str();
    int result = contents.copy(buffer, contents.size());
    return result;
}

int poly_snprintf_l(char* buffer, size_t count, const char* format, poly_locale_s locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    {
        result = poly_vsnprintf_l(buffer, count, format, locale, va);
    }
    va_end(va);
    return result;
}

int poly_vsnprintf_l(char* buffer, size_t count, const char* fmt, poly_locale_s locT, va_list args)
{
    array_buf outputBuf (buffer, count);
    std::ostream outs(&outputBuf);

    auto result = red::polyloc::do_printf(fmt, outs, count, locT->impl->loc, args);
    return result.chars_needed;
}


int poly_fprintf_l(FILE* fs, const char* format, poly_locale_s locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    {
        result = poly_vfprintf_l(fs, format, locale, va);
    }
    va_end(va);
    return result;
}

int poly_vfprintf_l(FILE* cfile, const char* fmt, poly_locale_s locale, va_list args)
{
    int result;

    // convert FILE* to ostream
#if defined(_MSC_VER)
    auto outs = std::ofstream(cfile);
    result = red::polyloc::do_printf(fmt, outs, locale->impl->loc, args);
#elif defined(__GNUC__)
    __gnu_cxx::stdio_filebuf<char> fbuf{ cfile, std::ios::out };
    std::ostream outs{ &fbuf };
    result = red::polyloc::do_printf(fmt, outs, locale->impl->loc, args);
#else
    // Fallback
    std::ostringstream outs;
    red::polyloc::do_printf(fmt, outs, locale->impl->loc, args);
    auto contents = outs.str();
    result = std::fputs(contents.c_str(), cfile);
    if (result != EOF) {
        result = (int)contents.size();
    }
#endif

    return result;
}

} // extern C
