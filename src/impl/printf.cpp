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
#include <variant>

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
    blankpos = 1 << 0,
    altform = 1 << 1, 
};

using arg_variant = std::variant<char, wchar_t, char*, wchar_t*, void*, intmax_t, uintmax_t, long double>;

class arg_context
{
    fmtspec_t fmtspec;
    va_list* va;
    arg_flags aflags = arg_flags::none;

    void apply_flags(std::ostream& os)
    {
        for (auto f : fmtspec.flags)
        {
            switch (f)
            {
            case '-': // left justify
                os.setf(ios::left, ios::adjustfield);
                break;
            case '+': // show positive sign
                os.setf(ios::showpos);
                break;
            case ' ': // Use a blank to prefix the output value if it is signed and positive, 
                aflags |= arg_flags::blankpos;
                break;
            case '#': // alt. form: ios::showbase for ints, ios::showpoint for floats
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
    void apply_fwpr(std::ostream& os)
    {
        if (fmtspec.field_width == fmtspec.VAL_VA)
            fmtspec.field_width = va_arg(*va, int);

        if (fmtspec.precision == fmtspec.VAL_VA)
            fmtspec.precision = va_arg(*va, int);

        os.width(fmtspec.field_width > 0 ? fmtspec.field_width : 0);
        os.precision(fmtspec.precision > 0 ? fmtspec.precision : 0);
    }

    void apply_repr(std::ostream& os)
    {
        switch (fmtspec.conversion)
        {
        case 'd': case 'i': case 'u': // integers
            os << std::dec;
            break;
        case 'X':
            os.setf(ios::uppercase);
        case 'x':
            os << std::hex;
            break;
        case 'o':
            os << std::oct;
            break;

        case 'F': // float fixed
            os.setf(ios::uppercase);
        case 'f':
            os << std::fixed;
            break;
        case 'E': // float scientific
            os.setf(ios::uppercase);
        case 'e':
            os << std::scientific;
            break;
        case 'G': // float default
            os.setf(ios::uppercase);
        case 'g':
            os << std::defaultfloat;
            break;
        case 'A': // hexfloat
            os.setf(ios::uppercase);
        case 'a':
            os << std::hexfloat;
            break;
        }
    }

    arg_variant get_value()
    {
        using ssize_t = std::make_signed_t<size_t>;
        using uptrdiff_t = std::make_unsigned_t<ptrdiff_t>;

        // apply_fwpr() must be called before this
        assert(fmtspec.field_width != fmtspec.VAL_VA && fmtspec.precision != fmtspec.VAL_VA);

        arg_variant val;

        switch (fmtspec.conversion)
        {
        case 'C': // chars
        case 'c':
            if (fmtspec.length_mod == "l" || fmtspec.conversion == 'C') {
                auto arg = va_arg(*va, wint_t);
                val = (wchar_t)arg;
            }
            else {
                auto arg = va_arg(*va, int);
                val = (char)arg;
            }
            break;

        case 'S': // strings
        case 's':
            if (fmtspec.length_mod == "l" || fmtspec.conversion == 'S') {
                val = va_arg(*va, wchar_t*);
            }
            else {
                val = va_arg(*va, char*);
            }
            break;

        case 'd': // integers
        case 'i':
            if (fmtspec.length_mod == "h") {
                auto arg = va_arg(*va, signed char);
                val = intmax_t(arg);
            }
            else if (fmtspec.length_mod == "hh") {
                auto arg = va_arg(*va, short);
                val = intmax_t(arg);
            }
            else if (fmtspec.length_mod == "l") {
                auto arg = va_arg(*va, long);
                val = intmax_t(arg);
            }
            else if (fmtspec.length_mod == "ll") {
                auto arg = va_arg(*va, long long);
                val = intmax_t(arg);
            }
            else if (fmtspec.length_mod == "j" || fmtspec.length_mod == "I64") {
                val = va_arg(*va, intmax_t);
            }
            else if (fmtspec.length_mod == "z") {
                auto arg = va_arg(*va, ssize_t);
                val = intmax_t(arg);
            }
            else if (fmtspec.length_mod == "t") {
                auto arg = va_arg(*va, ptrdiff_t);
                val = intmax_t(arg);
            }
            else {
                auto arg = va_arg(*va, int);
                val = intmax_t(arg);
            }
            break;

        case 'u': // unsigned integers
        case 'o':
        case 'X': case 'x':
            if (fmtspec.length_mod == "h") {
                auto arg = va_arg(*va, unsigned char);
                val = uintmax_t(arg);
            }
            else if (fmtspec.length_mod == "hh") {
                auto arg = va_arg(*va, unsigned short);
                val = uintmax_t(arg);
            }
            else if (fmtspec.length_mod == "l") {
                auto arg = va_arg(*va, unsigned long);
                val = uintmax_t(arg);
            }
            else if (fmtspec.length_mod == "ll") {
                auto arg = va_arg(*va, unsigned long long);
                val = uintmax_t(arg);
            }
            else if (fmtspec.length_mod == "j" || fmtspec.length_mod == "I64") {
                val = va_arg(*va, uintmax_t);
            }
            else if (fmtspec.length_mod == "z") {
                auto arg = va_arg(*va, size_t);
                val = uintmax_t(arg);
            }
            else if (fmtspec.length_mod == "t") {
                auto arg = va_arg(*va, uptrdiff_t);
                val = uintmax_t(arg);
            }
            else {
                auto arg = va_arg(*va, unsigned);
                val = uintmax_t(arg);
            }
            break;
            
        case 'E': case 'e': // floating point
        case 'F': case 'f':
        case 'G': case 'g':
        case 'A': case 'a':
            if (fmtspec.length_mod == "L") {
                val = va_arg(*va, long double);
            }
            else {
                auto arg = va_arg(*va, double);
                val = (long double)arg;
            }
            break;

        case 'p':
            val = va_arg(*va, void*);
            break;

        case 'n':
        default:
            break;
        }

        return val;
    }

    void put_fp(long double number, std::ostream& os) const
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

    void put_str(red::wstring_view str, std::ostream& os) const {
        auto conv = wconverter(new cvt_t(os.getloc().name()));
        std::string tmp = conv.to_bytes(str.data(), str.data() + str.size());
        put_str(tmp, os);
    }

    void put_str(red::string_view str, std::ostream& os) const {
        if (fmtspec.precision > 0)
        {
            str = str.substr(0, fmtspec.precision);
        }

        os << str;
    }

    template<class I>
    using Integer = std::enable_if_t<std::is_integral<I>::value>;

    template<typename T, class = Integer<T>>
    void put_int(T val, std::ostream& os) const
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

    
public:
    arg_context(fmtspec_t fmts, va_list* pva) : fmtspec(fmts), va(pva)
    {}

    auto put(std::ostream& os)
    {
        if (!fmtspec.valid())
        {
            // invalid format
            red::string_view invalid = fmtspec.fmt;
            os << invalid.substr(1);
            return;
        }

        using std::get_if;
        ios_saver<char> _g_(os);

        apply_flags(os);
        apply_fwpr(os);
        apply_repr(os);
        auto value = get_value();

        if (auto ptr = get_if<char>(&value)) {
            put_str({ ptr, 1 }, os);
        }
        else if (auto ptr = get_if<wchar_t>(&value)) {
            put_str({ ptr, 1 }, os);
        }
        else if (auto ptr = get_if<char*>(&value)) {
            put_str(*ptr, os);
        }
        else if (auto ptr = get_if<wchar_t*>(&value)) {
            put_str(*ptr, os);
        }
        else if (auto ptr = get_if<void*>(&value)) {
            os << std::hex << ptr;
        }
        else if (auto ptr = get_if<intmax_t>(&value)) {
            put_int(*ptr, os);
        }
        else if (auto ptr = get_if<uintmax_t>(&value)) {
            put_int(*ptr, os);
        }
        else if (auto ptr = get_if<long double>(&value)) {
            put_fp(*ptr, os);
        }
    }

};

auto& operator<< (std::ostream& os, arg_context& ctx) {
    ctx.put(os);
    return os;
}

} // unnamed


int red::polyloc::do_printf(string_view format, std::ostream& outs, const std::locale& loc, va_list args)
{
    boost::io::ios_locale_saver _s_{ outs };
    outs.imbue(loc);
    return do_printf(format, outs, args);
}

int red::polyloc::do_printf(string_view fmt, std::ostream& outs, va_list args)
{
    if (fmt.empty())
        return 0;

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
            auto fmtspec = parsefmt(tok);
            arg_context ctx{ fmtspec, &va };
            outs << ctx;
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
    tmp.copyfmt(outs);
    ret.first = do_printf(format, tmp, va);

    if (max == 0)
    {
        ret.second = 0;
    }
    else if (ret.first < max)
    {
        ret.second = ret.first;
        outs << tmp.str();
    }
    else
    {
        auto str = tmp.str();
        auto to_be_writen = string_view(str).substr(0, max - 1);
        ret.second = to_be_writen.size();
        outs << to_be_writen;
    }

    return ret;
}

auto red::polyloc::do_printf(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list args) -> std::pair<int, int>
{
    boost::io::ios_locale_saver _s_{ outs };
    outs.imbue(loc);
    return do_printf(format, outs, max, args);;
}
