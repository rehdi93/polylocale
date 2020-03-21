﻿#include "printf.hpp"

#include <sstream>
#include <iomanip>
#include <cstddef>
#include <array>
#include <vector>
#include <cassert>
#include <cmath>
#include <functional>

#include "bitmask.hpp"

using std::ios;

// HELPERS
namespace
{

constexpr char FMT_START    = '%';
constexpr char FMT_ALL[]    = "%.*" "-+ #0" "hljztI" "CcudioXxEeFfGgAapSsn";
constexpr char FMT_FLAGS[]  = "-+ #0";
constexpr char FMT_TYPES[]  = "CcudioXxEeFfGgAapSsn";
constexpr char FMT_SIZES[]  = "hljztI";
// precision
constexpr char FMT_PRECISION = '.';
// value from VA
constexpr char FMT_FROM_VA = '*';

template <class T, std::size_t N>
constexpr std::size_t countof(const T(&array)[N]) noexcept { return N; }

}

using namespace bitmask::ops;
namespace bm = bitmask;

#ifdef __GNUC__
using va_list_ref = va_list;
#else
using va_list_ref = va_list&;
#endif

enum class arg_flags : unsigned short
{
    none,
    is_signed = 1 << 0,
    is_unsigned = 1 << 1,
    wide = 1 << 2,
    narrow = 1<<3,
    blankpos = 1 << 6,
    showpos = 1 << 7,

    altform = 1 << 9, // ios::showbase for ints, ios::showpoint for floats

    signfield = is_signed | is_unsigned,
    sizefield = wide|narrow,
    posfield = blankpos|showpos
};

enum class arg_type : unsigned char
{
    unknown = 0,
    char_t = 'c',
    int_t = 'i',
    float_t = 'g',
    pointer = 'p',
    string = 's',
    byteswriten = 'n'
};

struct stream_state
{
    ios::fmtflags flags;
    char fill;
    std::streamsize width, precision;
    std::ios& stream;

    stream_state(std::ios& ios)
        : flags(ios.flags()), fill(ios.fill()),
        width(ios.width()), precision(ios.precision())
        , stream(ios)
    {}

    ~stream_state() noexcept
    {
        restore(stream);
    }

    void restore(std::ios& io) const {
        io.flags(flags);
        io.fill(fill);
        io.width(width);
        io.precision(precision);
        //io.imbue(locale);
    }
};

struct fmtspec_t
{
    red::string_view fmt;
    red::string_view flags, length_mod;
    int field_width = -1, precision = -1;
    char conversion = '\030';


    bool valid() const
    {
        return conversion != '\030';
    }

    bool has_flag(char f) const {
        return flags.find(f) != std::string::npos;
    }

    static const int VAL_VA = -(int)'*', VAL_AUTO = -1;
};

static fmtspec_t parsefmt(const std::string& str, std::locale const& locale);

// holds info about the current spec being parsed
struct printf_arg
{
    
    // %[flags][width][.precision][size]type
    // worst case scenerio sizes:
    // 1 + 4  +  10 +  1 +  10  +   3  + 1 = 30
    // 10 is from INT_MAX
    printf_arg(const std::string& stor, std::locale const& locale, va_list_ref args)
    : fmtspec(parsefmt(stor, locale)), va(args)
    {
    }


    arg_flags aflags{};
    fmtspec_t fmtspec;
    va_list_ref va;

    // print value
    auto put(std::ostream& outs)
    {
        stream_state _ = outs;

        outs.fill(' ');

        setup(outs);

        switch (fmtspec.conversion)
        {
        case 'C': // wide char TODO
            aflags |= arg_flags::wide;
        case 'c': // char
            if (bm::has(aflags, arg_flags::wide)) {
                auto v = va_arg(va, wint_t);
                wchar_t cp[1] = { (wchar_t)v };
                put_str(outs, { cp, 1 });
            }
            else {
                auto v = va_arg(va, int);
                char cp[1] = { (char)v };
                put_str(outs, { cp, 1 });
            }
            break;

        case 'u': // Unsigned decimal integer
            outs << std::dec;
            goto common_uint;

        case 'd': // Signed decimal integer
        case 'i':
            outs << std::dec;
            aflags |= arg_flags::is_signed;

            if (bitmask::has(aflags, arg_flags::wide)) {
                put_int(outs, va_arg(va, int64_t));
            }
            else {
                put_int(outs, va_arg(va, int32_t));
            }
            break;

        case 'o': // Unsigned octal integer
            outs << std::oct;
            goto common_uint;

        case 'X': // Unsigned hexadecimal integer w/ uppercase letters
            outs.setf(ios::uppercase);
        case 'x': // Unsigned hexadecimal integer w/ lowercase letters
            outs << std::hex;
            goto common_uint;

        case 'E': // float, scientific notation
            outs.setf(ios::uppercase);
        case 'e':
            outs << std::scientific;
            put_fp(outs, va_arg(va, double));
            break;

        case 'F': // float, fixed notation
            outs.setf(ios::uppercase);
        case 'f':
            outs << std::fixed;
            put_fp(outs, va_arg(va, double));
            break;

        case 'G': // float, general notation
            outs.setf(ios::uppercase);
        case 'g':
            outs << std::defaultfloat;
            put_fp(outs, va_arg(va, double));
            break;

        case 'A': // hex float
            outs.setf(ios::uppercase);
        case 'a':
            outs << std::hexfloat;
            put_fp(outs, va_arg(va, double));
            break;

        case 'p': // pointer
            outs << std::hex << va_arg(va, void*);
            break;

        case 'S': // wide string TODO
            aflags |= arg_flags::wide;
            put_str(outs, va_arg(va, wchar_t*));
            break;
        case 's': // string
            put_str(outs, va_arg(va, char*));
            break;

        case 'n': // weird write-bytes specifier (not implemented)
            //value = va_arg(va, void*);
            break;

            // https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=vs-2019#argument-size-specification
            // integer args w/o size spec are treated as 32bit
        common_uint:
            aflags |= arg_flags::is_unsigned;

            if (bitmask::has(aflags, arg_flags::wide)) {
                put_int(outs, va_arg(va, uint64_t));
            }
            else {
                put_int(outs, va_arg(va, uint32_t));
            }
            break;
        default:
            // invalid, print fmt as-is minus %
            outs << fmtspec.fmt.substr(1);
            break;
        }
    }

private:
    void setup(std::ostream& os)
    {
        for (auto f : fmtspec.flags)
        {
            switch (f)
            {
            case '-': // left justify
                os << std::left;
                break;
            case '+': // show positive sign
                //bm::setm(aflags, arg_flags::showpos, arg_flags::posfield);
                os << std::showpos;
                break;
            case ' ': // Use a blank to prefix the output value if it is signed and positive, 
                bm::setm(aflags, arg_flags::blankpos, arg_flags::posfield);
                break;
            case '#':
                aflags |= arg_flags::altform;
                break;
            case '0':
                os.fill('0');
                break;
            default:
                break;
            }
        }

        // ' ' has no effect if '+' is set
        if (bm::has(os.flags(), ios::showpos))
        {
            aflags &= ~arg_flags::blankpos;
        }

        if (fmtspec.field_width == fmtspec.VAL_VA)
            fmtspec.field_width = va_arg(va, int);

        if (fmtspec.precision == fmtspec.VAL_VA)
            fmtspec.precision = va_arg(va, int);

        os.width(fmtspec.field_width > 0 ? fmtspec.field_width : 0);
        os.precision(fmtspec.precision > 0 ? fmtspec.precision : 0);

        if (!fmtspec.length_mod.empty())
        {
            if (fmtspec.length_mod == "h")
            {
                // halfwidth
                aflags |= arg_flags::narrow;
            }
            else if (fmtspec.length_mod == "hh")
            {
                // quarterwidth
            }
            // are we 64-bit (unix style)
            else if (fmtspec.length_mod == "l")
            {
                if (sizeof(long) == 8)
                    aflags |= arg_flags::wide;
            }
            else if (fmtspec.length_mod == "ll")
            {
                aflags |= arg_flags::wide;
            }
            // are we 64-bit on intmax_t? (c99)
            else if (fmtspec.length_mod == "j")
            {
                if (sizeof(size_t) == 8)
                    aflags |= arg_flags::wide;
            }
            // are we 64-bit on size_t or ptrdiff_t? (c99)
            else if (fmtspec.length_mod == "z" || fmtspec.length_mod == "t")
            {
                if (sizeof(ptrdiff_t) == 8)
                    aflags |= arg_flags::wide;
            }
            // are we 64-bit (msft style)
            else if (fmtspec.length_mod == "I64")
            {
                aflags |= arg_flags::wide;
            }
            else if (fmtspec.length_mod == "I32")
            {
                if (sizeof(void*) == 8)
                    aflags |= arg_flags::wide;
            }
        }
    }

    void put_fp(std::ostream& os, double number) const
    {
        if (bm::has(aflags, arg_flags::altform)) {
            os.setf(ios::showpoint);
        }

        const auto posfield = (aflags & arg_flags::posfield);
        if (posfield == arg_flags::blankpos && number >= 0)
            os.put(' ');

        if (fmtspec.precision == -1)
        {
            os.precision(6);
        }
        else if (fmtspec.precision == 0)
        {
            number = std::round(number);
        }

        os << number;
    }

    void put_str(std::ostream& os, red::wstring_view str) const {
        std::string tmp;

        auto& f = std::use_facet<std::ctype<wchar_t>>(os.getloc());
        tmp.append(str.size(), '\0');
        f.narrow(str.data(), str.data()+str.size(), '?', &tmp[0]);

        put_str(os, tmp);
    }

    void put_str(std::ostream& os, red::string_view str) const {
        if (fmtspec.precision > 0)
        {
            str = str.substr(0, fmtspec.precision);
        }

        os << str;
    }


    template<class I>
    using is_int = std::enable_if_t<std::is_integral<I>::value>;

    template<typename T, class = is_int<T>>
    void put_int(std::ostream& os, T val) const
    {
        const auto posfield = (aflags & arg_flags::posfield);
        if (bm::has(aflags, arg_flags::altform)) {
            os << std::showbase;
        }

        bool skip = false;
        if (fmtspec.precision <= 0 && val == 0)
        {
            if (bitmask::has_all(os.flags(), ios::oct | ios::showbase))
            {
                // for octal, if both the converted value and the precision are ​0​, single ​0​ is written.
                os << 0;
                skip = true;
            }
            else if (!bm::has(os.flags(), ios::showpos))
            {
                skip = true;
            }
        }

        if (posfield == arg_flags::blankpos && val >= 0 && fmtspec.field_width < 2)
            os.put(' ');

        if (skip)
            return;

        if (fmtspec.precision > 0)
        {
            auto remainingW = std::max(fmtspec.field_width - fmtspec.precision, 0);
            if (remainingW > 0)
            {
                char s = ' ';
                os.width(remainingW);
                os.fill(s);
                os << s;
            }

            os.width(fmtspec.precision);
            os.fill('0');
        }

        os << val;
    }

};



int red::polyloc::do_printf(string_view fmt, std::ostream& outs, const std::locale& loc, va_list va)
{
    if (fmt.empty())
        return 0;

    outs.imbue(loc);

    auto format = fmt;
    std::string spec;
    spec.reserve(30);

    for (size_t i;;)
    {
        // look for %
        i = format.find(FMT_START);
        
        if (i == string_view::npos)
        {
            // no more specs, we're done
            outs << format;
            return outs.tellp();
        }

        auto txtb4 = format.substr(0, i);

        if (!format[i + 1] || format[i + 1] == FMT_START)
        {
            // escaped % or null
            outs << txtb4 << format[i + 1];
            format.remove_prefix(txtb4.size()+2);
        }
        else
        {
            outs << txtb4;

            // possible fmtspec(s)
            // %[flags][width][.precision][size]type
            const auto end = format.find_first_of(FMT_TYPES, i + 1) + 1;
            auto part = format.substr(i, end - i);
            spec.assign(part);
            
            printf_arg arg{ spec, loc, va };
            arg.put(outs);

            i += spec.size();
            format.remove_prefix(i);
        }
    }
}

auto red::polyloc::do_printf(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list va) -> write_result
{
    write_result ret;

    std::ostringstream tmp;
    do_printf(format, tmp, loc, va);
    auto tmpstr = tmp.str();
    ret.chars_needed = tmpstr.size();

    if (ret.chars_needed <= max)
    {
        ret.chars_written = tmpstr.size();
        outs << tmpstr;
    }
    else if (max == 0)
    {
        ret.chars_written = 0;
    }
    else
    {
        auto to_be_writen = string_view(tmpstr).substr(0, max - 1);
        ret.chars_written = to_be_writen.size();
        outs << to_be_writen;
    }

    return ret;
}


fmtspec_t parsefmt(const std::string& str, std::locale const& locale)
{
    auto constexpr npos = std::string::npos;

    fmtspec_t fmtspec{ str };
    red::string_view spec = fmtspec.fmt;
    assert(spec[0] == FMT_START);

    size_t start=1, end=0;

    // Flags
    end = spec.find_first_not_of(FMT_FLAGS, start);
    if (end != npos)
    {
        fmtspec.flags = spec.substr(start, end - start);
    }
    else end = start;


    // field width
    if (spec[end] == FMT_FROM_VA)
    {
        fmtspec.field_width = fmtspec.VAL_VA;
        end++;
    }
    else if (std::isdigit(spec[end], locale))
    {
        try {
            size_t converted;
            fmtspec.field_width = std::stoi(str.substr(end), &converted);
            end += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }

    if (spec[end] == FMT_PRECISION)
    {
        end++;
        if (spec[end] == FMT_FROM_VA)
        {
            fmtspec.precision = fmtspec.VAL_VA;
            end++;
        }
        else if (std::isdigit(spec[end], locale)) try
        {
            size_t converted;
            fmtspec.precision = std::stoi(str.substr(end), &converted);
            end += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }

    // size overrides
    start = spec.find_first_of(FMT_SIZES, end);
    if (start != npos)
    {
        end = spec.find_first_not_of(FMT_SIZES, start);
        fmtspec.length_mod = red::string_view(str).substr(start, end-start);
    }

    // conversion spec
    start = spec.find_first_of(FMT_TYPES, end);
    if (start != npos)
        fmtspec.conversion = spec.at(start);
    else
        fmtspec.conversion = '\030';

    return fmtspec;
}
