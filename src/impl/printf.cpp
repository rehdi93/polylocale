// disable deprication warning about wstring_convert
/*
  I could use a 3rd-party lib like Boost.Locale, but those usualy accept only a subset of locale
  names like "en_US", "pt_BR.UTF-8"... OR they don't know the fact that MSVC now supports
  utf8 locales https://github.com/MicrosoftDocs/cpp-docs/issues/1469
*/

// MSVC
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "printf.hpp"
#include "printf_fmt.hpp"
#include "bitmask.hpp"
#include <boost/io/ios_state.hpp>

#include <sstream>
#include <iomanip>
#include <codecvt>
#include <algorithm>
#include <optional>
#include <cstddef>
#include <cmath>
#include <cassert>


using std::ios;
using red::polyloc::fmtspec_t;
namespace bm = bitmask;
using namespace std::literals;
using namespace bitmask::ops;

// HELPERS
namespace
{

struct va_deleter
{
    using pointer = va_list*;

    void operator () (pointer va) const {
        va_end(*va);
    }
};

// utility wrapper to adapt locale-bound facets for wstring/wbuffer convert
template<class Facet>
struct facet_adapter : Facet
{
    using Facet::Facet; // inherit constructors
    ~facet_adapter() = default;
};

using cvt_t = facet_adapter<std::codecvt_byname<wchar_t, char, std::mbstate_t>>;
using wconverter = std::wstring_convert<cvt_t>;

template<class Ch, class Tr = std::char_traits<Ch>>
struct ios_saver
{
    using state_type = std::basic_ios<Ch, Tr>;

    explicit ios_saver(state_type& s) : m_wpfsaver(s), m_fillsaver(s)
    {}

    void restore() {
        m_wpfsaver.restore();
        m_fillsaver.restore();
    }

private:
    boost::io::ios_base_all_saver m_wpfsaver;
    boost::io::basic_ios_fill_saver<Ch, Tr> m_fillsaver;
};

enum class arg_flags : unsigned short
{
    none,
    wide = 1 << 2,
    narrow = 1<<3,
    blankpos = 1 << 6,

    altform = 1 << 9, // ios::showbase for ints, ios::showpoint for floats

    sizefield = wide|narrow
};


struct arg_printer
{
    arg_printer(fmtspec_t fmts, std::ostream& os_, va_list* pva)
    : fmtspec(fmts), os(os_), va(pva)
    {
    }


    std::ostream& os;
    va_list* va;
    fmtspec_t fmtspec;
    mutable std::optional<wconverter> conv;
    arg_flags aflags{};

    // print value
    auto put()
    {
        ios_saver<char> _g_(os);

        apply_flags();
        apply_fwpr();
        apply_len();

        switch (fmtspec.conversion)
        {
        case 'C': // wide char
            aflags |= arg_flags::wide;
        case 'c': // char
            if (bm::has(aflags, arg_flags::wide)) {
                auto v = va_arg(*va, wint_t);
                wchar_t cp[1] = { (wchar_t)v };
                put_str({ cp, 1 });
            }
            else {
                auto v = va_arg(*va, int);
                char cp[1] = { (char)v };
                put_str({ cp, 1 });
            }
            break;

        case 'u': // Unsigned decimal integer
            os << std::dec;
            goto common_uint;

        case 'd': // Signed decimal integer
        case 'i':
            os << std::dec;

            if (bitmask::has(aflags, arg_flags::wide)) {
                put_int(va_arg(*va, int64_t));
            }
            else {
                put_int(va_arg(*va, int32_t));
            }
            break;

        case 'o': // Unsigned octal integer
            os << std::oct;
            goto common_uint;

        case 'X': // Unsigned hexadecimal integer w/ uppercase letters
            os.setf(ios::uppercase);
        case 'x': // Unsigned hexadecimal integer w/ lowercase letters
            os << std::hex;
            goto common_uint;

        case 'E': // float, scientific notation
            os.setf(ios::uppercase);
        case 'e':
            os << std::scientific;
            put_fp(va_arg(*va, double));
            break;

        case 'F': // float, fixed notation
            os.setf(ios::uppercase);
        case 'f':
            os << std::fixed;
            put_fp(va_arg(*va, double));
            break;

        case 'G': // float, general notation
            os.setf(ios::uppercase);
        case 'g':
            os << std::defaultfloat;
            put_fp(va_arg(*va, double));
            break;

        case 'A': // hex float
            os.setf(ios::uppercase);
        case 'a':
            os << std::hexfloat;
            put_fp(va_arg(*va, double));
            break;

        case 'p': // pointer
            os << std::hex << va_arg(*va, void*);
            break;

        case 'S': // wide string
            aflags |= arg_flags::wide;
        case 's': // string
            if (bm::has(aflags, arg_flags::wide)) {
                put_str(va_arg(*va, wchar_t*));
            }
            else {
                put_str(va_arg(*va, char*));
            }
            break;

        case 'n': // weird write-bytes specifier (not implemented)
        default:
            // invalid, print fmt as-is minus %
            os << fmtspec.fmt.substr(1);
            break;

            // https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=vs-2019#argument-size-specification
            // integer args w/o size spec are treated as 32bit
        common_uint:
            if (bitmask::has(aflags, arg_flags::wide)) {
                auto num = va_arg(*va, uint64_t);
                put_int(num);
            }
            else {
                auto num = va_arg(*va, uint32_t);
                put_int(num);
            }
            break;
        }
    }

private:

    void put_fp(double number) const
    {
        if (bm::has(aflags, arg_flags::altform)) {
            os.setf(ios::showpoint);
        }

        if (bm::has(aflags, arg_flags::blankpos) && number >= 0)
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

    void put_str(red::wstring_view str) const {
        if (!conv) {
            conv.emplace(new cvt_t(os.getloc().name()));
        }
        std::string tmp = conv->to_bytes(str.data(), str.data() + str.size());
        put_str(tmp);
    }

    void put_str(red::string_view str) const {
        if (fmtspec.precision > 0)
        {
            str = str.substr(0, fmtspec.precision);
        }

        os << str;
    }

    template<class I>
    using is_int = std::enable_if_t<std::is_integral<I>::value>;

    template<typename T, class = is_int<T>>
    void put_int(T val) const
    {
        if (bm::has(aflags, arg_flags::altform)) {
            os << std::showbase;
        }

        if (bm::has(aflags, arg_flags::blankpos) && val >= 0 && fmtspec.field_width < 2)
            os.put(' ');

        if (fmtspec.precision >= 0 && val == 0)
        {
            if (bitmask::has_all(os.flags(), ios::oct | ios::showbase))
            {
                // for octal, if both the converted value and the precision are ​0​, single ​0​ is written.
                os << 0;
                return;
            }
            else if (!bm::has(os.flags(), ios::showpos))
            {
                return;
            }
        }

        if (fmtspec.precision > 0)
        {
            auto remainingW = fmtspec.field_width - fmtspec.precision;
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

    void apply_flags()
    {
        for (auto f : fmtspec.flags)
        {
            switch (f)
            {
            case '-': // left justify
                os << std::left;
                break;
            case '+': // show positive sign
                os << std::showpos;
                break;
            case ' ': // Use a blank to prefix the output value if it is signed and positive, 
                aflags |= arg_flags::blankpos;
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
    }

    // field width, precision
    void apply_fwpr()
    {
        if (fmtspec.field_width == fmtspec.VAL_VA)
            fmtspec.field_width = va_arg(*va, int);

        if (fmtspec.precision == fmtspec.VAL_VA)
            fmtspec.precision = va_arg(*va, int);

        os.width(fmtspec.field_width > 0 ? fmtspec.field_width : 0);
        os.precision(fmtspec.precision > 0 ? fmtspec.precision : 0);
    }

    void apply_len() noexcept
    {
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
                if (sizeof(long) == 8 || fmtspec.conversion == 'c' || fmtspec.conversion == 's')
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
};

} // unnamed


int red::polyloc::do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list args)
{
    auto oldloc = outs.imbue(loc);
    int result = do_printf(format, outs, args);
    outs.imbue(oldloc);
    return result;
}

int red::polyloc::do_printf(string_view fmt, std::ostream& outs, va_list args)
{
    if (fmt.empty())
        return 0;

    auto loc = outs.getloc();

#ifdef __GNUC__
    va_list va;
    va_copy(va, args);
    auto _g_ = std::unique_ptr<va_list, va_deleter>(&va);
#else
    auto va = args;
#endif // __GNUC__

    auto format = std::string(fmt);
    fmt_tokenizer toknz{ format };

    for (auto& tok : toknz)
    {
        if (tok.size() >= 2 && tok[0] == '%')
        {
            auto fmtspec = parsefmt(tok, loc);
            arg_printer pfarg{ fmtspec, outs, &va };
            pfarg.put();
        }
        else
        {
            outs << tok;
        }
    }

    return outs.tellp();
}


auto red::polyloc::do_printf(string_view format, std::ostream& outs, size_t max, va_list va) -> std::pair<int, int>
{
    std::pair<int, int> ret;

    std::ostringstream tmp;
    do_printf(format, tmp, va);
    auto tmpstr = tmp.str();
    ret.first = tmpstr.size();

    if (max == 0)
    {
        ret.second = 0;
    }
    else if (ret.first < max)
    {
        ret.second = ret.first;
        outs << tmpstr;
    }
    else
    {
        auto to_be_writen = string_view(tmpstr).substr(0, max - 1);
        ret.second = to_be_writen.size();
        outs << to_be_writen;
    }

    return ret;
}

auto red::polyloc::do_printf(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list args) -> std::pair<int, int>
{
    auto oldloc = outs.imbue(loc);
    auto result = do_printf(format, outs, max, args);
    outs.imbue(oldloc);
    return result;
}
