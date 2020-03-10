#include "polylocale.h"
#include "polyloc.hpp"

#include <locale>
#include <sstream>
#include <fstream>
#include <string>
#include <memory>
#include <algorithm>

#include "boost/iostreams/stream_buffer.hpp"
#include "boost/iostreams/device/array.hpp"
#include "boost/utility/string_view.hpp"

struct poly_impl
{
    std::locale loc;
    std::string name;

    poly_impl() : poly_impl(std::locale()) {}

    explicit poly_impl(const std::locale& lc)
        : loc(lc), name(lc.name()) {}
};

using poly_locale_ptr = std::unique_ptr<poly_locale>;


static auto make_polylocale(std::locale const& base) {
    auto plc = std::make_unique<poly_locale>();
    
    plc->impl = new poly_impl{ base };
    plc->name = plc->impl->name.c_str();
    
    return plc;
}

static auto make_polylocale(locale_t loc) {
    auto plc = std::make_unique<poly_locale>();
    plc->impl = new poly_impl(*loc->impl);
    plc->name = plc->impl->name.c_str();

    return plc;
}


static auto mask_to_cat(int mask) noexcept -> std::locale::category
{
    using Lc = std::locale;

    if (mask == LC_ALL_MASK)
        return Lc::all;
    else if ((LC_ALL_MASK & mask) == 0)
        return -1; // mask contains an unknown bit

    std::locale::category cat{0};

    if ((mask & LC_CTYPE_MASK) != 0)
        cat |= Lc::ctype;
    if ((mask & LC_NUMERIC_MASK) != 0)
        cat |= Lc::numeric;
    if ((mask & LC_TIME_MASK) != 0)
        cat |= Lc::time;
    if ((mask & LC_COLLATE_MASK) != 0)
        cat |= Lc::collate;
    if ((mask & LC_MONETARY_MASK) != 0)
        cat |= Lc::monetary;

    return cat;
}


// https://pubs.opengroup.org/onlinepubs/9699919799/functions/newlocale.html
locale_t newlocale(int category_mask, const char* localename, locale_t base)
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
            loc = std::locale(loc, localename, cats);

            *base->impl = poly_impl(loc);
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
void freelocale(locale_t loc) {
    if (loc)
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
        
        auto plc = make_polylocale(loc);
        return plc.release();
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

using array_read_buf = boost::iostreams::stream_buffer<boost::iostreams::array_source>;
using array_write_buf = boost::iostreams::stream_buffer<boost::iostreams::array_sink>;
using array_buf = boost::iostreams::stream_buffer<boost::iostreams::array>;


int printf_l(const char* fmt, locale_t locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    {
        std::ostringstream tmp;
        result = red::polyloc::do_printf(fmt, tmp, locale->impl->loc, va);
        auto str = tmp.str();
        std::cout << str;
    }
    va_end(va);
    return result;
}


int sprintf_l(char* buffer, const char* fmt, locale_t loc, ...)
{
    int result;
    va_list va;
    va_start(va, loc);
    {
        //result = vsnprintf_l(buffer, SIZE_MAX, fmt, loc, va);
        std::ostringstream tmp;
        red::polyloc::do_printf(fmt, tmp, loc->impl->loc, va);
        auto contents = tmp.str();
        result = contents.copy(buffer, contents.size());
    }
    va_end(va);
    return result;
}

int snprintf_l(char* buffer, size_t count, const char* format, locale_t locale, ...)
{
    int result;
    va_list va;
    va_start(va, locale);
    {
        result = vsnprintf_l(buffer, count, format, locale, va);
    }
    va_end(va);
    return result;
}

int vsnprintf_l(char* buffer, size_t count, const char* fmt, locale_t locT, va_list args)
{
    array_buf outputBuf (buffer, count);
    std::ostream outs(&outputBuf);

    auto result = red::polyloc::do_printf_n(fmt, outs, count, locT->impl->loc, args);
    return result.chars_needed;
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

int vfprintf_l(FILE* cfile, const char* fmt, locale_t locale, va_list args)
{
    int result;

    // TODO: FILE* to stream in other platforms
    // libstdc++ (stdio_filebuf) https://gcc.gnu.org/onlinedocs/gcc-9.2.0/libstdc++/api/a10659.html
    // libc++ (???) 

#if defined(_MSC_VER)
    auto outs = std::ofstream(cfile);
#elif defined(__GCC__)
    __gnu_cxx::stdio_filebuf<char> fbuf{ cfile, std::ios::out };
    auto outs = std::ofstream(&fbuf);
#else
    errno = ENOTSUP;
    result = -1;
#endif

    result = red::polyloc::do_printf(fmt, outs, locale->impl->loc, args);
    return result;
}
