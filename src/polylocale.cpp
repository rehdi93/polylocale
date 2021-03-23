#include <locale>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <string>
#include <memory>
#include <algorithm>

#include "polylocale.h"
#include "impl/printf.hpp"
#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/device/array.hpp>

#ifdef __GNUC__
#include <ext/stdio_filebuf.h>
#endif


struct poly_locale
{
    std::locale loc;
    std::string name;

    poly_locale(std::locale const& locale) : loc(locale), name(locale.name())
    {}
};

namespace // threadlocal locale
{
    thread_local poly_locale_t tl_locale = nullptr;
}


static auto cpploc(poly_locale_t ploc) -> std::locale
{
    if (ploc == POLY_GLOBAL_LOCALE)
        return {};
    if (!ploc) {
        throw std::invalid_argument("locale_t is null!");
    }

    return ploc->loc;
}

static auto mask_to_cat(int mask) noexcept -> std::locale::category
{
    using Lc = std::locale;

    if (mask == POLY_ALL_MASK)
        return Lc::all;
    else if ((POLY_ALL_MASK & mask) == 0)
        throw std::invalid_argument("mask contains a bit that does not correspond to a valid category");

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

// helpers
template<typename Char>
class unbound_sink
{
public:
    using char_type = Char;
    using category = boost::iostreams::sink_tag;

    explicit unbound_sink(char_type* begin) : cur_(begin)
    {}

    std::streamsize write(const char_type* s, std::streamsize n)
    {
        cur_ = std::copy_n(s, n, cur_);
        return n;
    }

private:
    char_type* cur_;
};

using unsafe_buf = boost::iostreams::stream_buffer<unbound_sink<char>>;
using array_buf = boost::iostreams::stream_buffer<boost::iostreams::array>;


extern "C" {

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/newlocale.html
poly_locale_t poly_newlocale(int category_mask, const char* localename, poly_locale_t base)
{
    try
    {
        auto const cats = mask_to_cat(category_mask);

        if (base)
        {
            auto baseloc = cpploc(base);
            auto newloc = std::locale(baseloc, localename, cats);
            *base = poly_locale{newloc};
            return base;
        }
        else
        {
            auto lc = std::locale({}, localename, cats);
            return new poly_locale{ lc };
        }
    }
    catch (const std::bad_alloc&)
    {
        errno = ENOMEM;
        return nullptr;
    }
    catch (const std::runtime_error&)
    {
        errno = ENOENT;
        return nullptr;
    }
    catch (const std::invalid_argument&)
    {
        errno = EINVAL;
        return nullptr;
    }
}

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/freelocale.html
void poly_freelocale(poly_locale_t loc) { delete loc; }

poly_locale_t poly_duplocale(poly_locale_t loc)
{
    try
    {
        if (loc == POLY_GLOBAL_LOCALE) {
            return new poly_locale(std::locale());
        }
        else 
            return new poly_locale(*loc);
    }
    catch(const std::bad_alloc&)
    {
        errno = ENOMEM;
        return nullptr;
    }
}

poly_locale_t poly_uselocale(poly_locale_t nloc)
{
    if (!nloc)
    {
        return tl_locale != nullptr ? tl_locale : POLY_GLOBAL_LOCALE;
    }

    if (nloc == POLY_GLOBAL_LOCALE)
    {
        delete tl_locale;
        tl_locale = nullptr;
        return POLY_GLOBAL_LOCALE;
    }

    tl_locale = new poly_locale{ *nloc };
    return tl_locale;
}


double poly_strtod_l(const char* str, char** endptr, poly_locale_t ploc)
{
    std::istringstream ss{str};
    ss.imbue(cpploc(ploc));

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


int poly_printf_l(const char* fmt, poly_locale_t locale, ...)
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

int poly_vprintf_l(const char* fmt, poly_locale_t locale, va_list args)
{
    try
    {
        return polyloc::do_printf(fmt, std::cout, cpploc(locale), args);
    }
    catch (std::exception&)
    {
        return -1;
    }
}


int poly_sprintf_l(char* buffer, const char* fmt, poly_locale_t loc, ...)
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

int poly_vsprintf_l(char* buffer, const char* fmt, poly_locale_t loc, va_list args)
{
    try
    {
        unsafe_buf sink{ buffer };
        std::ostream os(&sink);
        auto result = polyloc::do_printf(fmt, os, cpploc(loc), args);
        os.put('\0');
        return result;
    }
    catch (std::exception&)
    {
        return -1;
    }
}

int poly_snprintf_l(char* buffer, size_t count, const char* format, poly_locale_t locale, ...)
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

int poly_vsnprintf_l(char* buffer, size_t count, const char* fmt, poly_locale_t ploc, va_list args)
{
    try
    {
        array_buf outputBuf(buffer, count);
        std::ostream outs(&outputBuf);
        auto result = polyloc::do_printf(fmt, outs, count, cpploc(ploc), args);
        outs.put('\0');
        return result.first;
    }
    catch (std::exception&)
    {
        return -1;
    }
}


int poly_fprintf_l(FILE* fs, const char* format, poly_locale_t locale, ...)
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

int poly_vfprintf_l(FILE* cfile, const char* fmt, poly_locale_t loc, va_list args)
{
    try
    {
        int result;

        // convert FILE* to ostream
#if defined(_MSC_VER)
        auto outs = std::ofstream(cfile);
        result = polyloc::do_printf(fmt, outs, cpploc(loc), args);
#elif defined(__GNUC__)
        __gnu_cxx::stdio_filebuf<char> fbuf{ cfile, std::ios::out };
        std::ostream outs{ &fbuf };
        result = polyloc::do_printf(fmt, outs, cpploc(loc), args);
#else
    // Fallback
        std::ostringstream outs;
        polyloc::do_printf(fmt, outs, cpploc(loc), args);
        auto contents = outs.str();
        result = std::fputs(contents.c_str(), cfile);
        if (result != EOF) {
            result = (int)contents.size();
        }
#endif

        return result;
    }
    catch (std::exception&)
    {
        return -1;
    }
}

const char* polyloc_getname(poly_locale_t l)
{
    return l->name.c_str();
}

} // extern C
