/*
  Disable deprication warning about wstring_convert.

  I could use a 3rd-party lib like Boost.Locale, but those usualy accept only a subset of locale
  names like "en_US", "pt_BR.UTF-8"... OR they don't know the fact that MSVC now supports
  utf8 locales https://github.com/MicrosoftDocs/cpp-docs/issues/1469
*/
// MSVC
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING

#include "printf.hpp"
#include "printf_fmt.hpp"
#include "fmtdefs.h"
#include "boost/io/ios_state.hpp"
#include "boost/iostreams/filter/counter.hpp"
#include "boost/iostreams/filtering_stream.hpp"

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
using polyloc::fmtspec_t;
using namespace std::literals;

// HELPERS
namespace
{

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


using arg_variant = std::variant<char, wchar_t, char*, wchar_t*, void*, int64_t, uint64_t, long double>;


void apply_flags(std::ostream& os, fmtspec_t& fmts)
{
    using namespace polyloc;

    auto oldflags = os.flags(fmts.iosflags);

    if ((fmts.moreflags & moreflags::zerofill) != 0) {
        os.fill('0');
    }
    if ((fmts.moreflags & moreflags::altform) != 0) {
        if (fmts.type == fmttype::sint || fmts.type == fmttype::uint) {
            os.setf(ios::showbase);
        }
        else if (fmts.type == fmttype::floatpt) {
            os.setf(ios::showpoint);
        }
    }
    // blankpos requires the value

    if (oldflags != fmts.iosflags) {
        fmts.iosflags = os.flags();
    }
}

// field width, precision
void apply_fwpr(std::ostream& os, fmtspec_t& fmts, va_list* pva)
{
    if (fmts.field_width == fmts.VAL_VA)
        fmts.field_width = va_arg(*pva, int);
    
    if (fmts.precision == fmts.VAL_VA)
        fmts.precision = va_arg(*pva, int);

    if (fmts.field_width > fmts.VAL_AUTO)
        os.width(fmts.field_width);

    if (fmts.precision > fmts.VAL_AUTO)
        os.precision(fmts.precision);
}

auto extract_value(fmtspec_t const& fmts, va_list* pva)
{
    using ft = polyloc::fmttype;
    using mf = polyloc::moreflags::Enum;
    arg_variant val;

    bool wideflag = (fmts.moreflags & mf::wide) != 0;

    switch (fmts.type)
    {
    case ft::char_:
        if (wideflag) {
            auto arg = va_arg(*pva, wint_t);
            val = (wchar_t)arg;
        }
        else {
            auto arg = va_arg(*pva, int); // intentional
            val = (char)arg;
        }
        break;
    case ft::string:
        if (wideflag) {
            val = va_arg(*pva, wchar_t*);
        }
        else {
            val = va_arg(*pva, char*);
        }
        break;
    case ft::sint:
        if (wideflag) {
            val = va_arg(*pva, int64_t);
        }
        else {
            val = (int64_t)va_arg(*pva, int32_t);
        }
        break;
    case ft::uint:
        if (wideflag) {
            val = va_arg(*pva, uint64_t);
        }
        else {
            val = (uint64_t)va_arg(*pva, uint32_t);
        }
        break;
    case ft::floatpt:
        if (wideflag) {
            val = va_arg(*pva, long double);
        }
        else {
            auto arg = va_arg(*pva, double);
            val = (long double)arg;
        }
        break;
    case ft::pointer:
        val = va_arg(*pva, void*);
        break;
    default:
        break;
    }

    return val;
}

void put_fp(long double number, fmtspec_t const& fmts, std::ostream& os)
{
    using mf = polyloc::moreflags::Enum;

    if ((fmts.moreflags & mf::blankpos) != 0 && number >= 0)
        os.put(' ');

    if (fmts.precision == 0)
    {
        number = std::round(number);
    }

    os << number;
}

void put_str(red::string_view str, fmtspec_t const& fmts, std::ostream& os) {
    if (fmts.precision > 0)
        os << str.substr(0, fmts.precision);
    else
        os << str;
}

void put_str(red::wstring_view str, fmtspec_t const& fmts, std::ostream& os, wconverter* wconv) {
    assert(wconv && "wconv is null!");
    std::string tmp = wconv->to_bytes(str.data(), str.data() + str.size());
    put_str(tmp, fmts, os);
}

template<typename T>
void put_int(T val, fmtspec_t const& fmts, std::ostream& os)
{
    using mf = red::polyloc::moreflags::Enum;

    if ((fmts.moreflags & mf::blankpos) != 0 && val >= 0 && fmts.field_width < 2)
        os.put(' ');

    if (fmts.precision >= 0 && val == 0)
    {
        auto special = ios::oct | ios::showbase;
        if ((os.flags() & special) == special)
        {
            os << 0;
            return;
        }
        else if ((os.flags() & ios::showpos) == 0)
        {
            return;
        }
    }

    if (fmts.precision > 0)
    {
        // precision in this case is the minimum number of digits to write
        auto padding = fmts.field_width - fmts.precision;
        if (padding > 0)
        {
            os.width(padding);
            os << ' ';
        }

        os.width(fmts.precision);
        os.fill('0');
    }

    os << val;
}

void put(std::ostream& os, arg_variant const& value, fmtspec_t const& fmts, wconverter* wconv) {
    using std::get_if;

    if (auto ptr = get_if<char>(&value)) {
        put_str({ ptr, 1 }, fmts, os);
    }
    else if (auto ptr = get_if<wchar_t>(&value)) {
        put_str({ ptr, 1 }, fmts, os, wconv);
    }
    else if (auto ptr = get_if<char*>(&value)) {
        put_str(*ptr, fmts, os);
    }
    else if (auto ptr = get_if<wchar_t*>(&value)) {
        put_str(*ptr, fmts, os, wconv);
    }
    else if (auto ptr = get_if<void*>(&value)) {
        os << std::hex << ptr;
    }
    else if (auto ptr = get_if<int64_t>(&value)) {
        put_int(*ptr, fmts, os);
    }
    else if (auto ptr = get_if<uint64_t>(&value)) {
        put_int(*ptr, fmts, os);
    }
    else if (auto ptr = get_if<long double>(&value)) {
        put_fp(*ptr, fmts, os);
    }
}

} // unnamed


int polyloc::do_printf(red::string_view format, std::ostream& outs, const std::locale& loc, va_list args)
{
    boost::io::ios_locale_saver _s_{ outs };
    outs.imbue(loc);
    return do_printf(format, outs, args);
}

int polyloc::do_printf(red::string_view fmt, std::ostream& stream, va_list args)
{
    using boost::iostreams::filtering_ostream;
    using boost::iostreams::counter;

    if (fmt.empty())
        return 0;

#ifdef __GNUC__
    struct Guard
    {
        va_list *pva;

        ~Guard() {
            va_end(*pva);
        }
    };

    va_list va;
    va_copy(va, args);
    auto* pva = &va;
    auto _vaptrguard_ = Guard{ pva };
#else
    auto* pva = &args;
#endif // __GNUC__

    filtering_ostream os;
    os.push(counter());
    os.push(stream);
    os.copyfmt(stream);

    std::unique_ptr<wconverter> wconv;

    auto fmtcopy = std::string(fmt);
    fmt_tokenizer toknz{ fmtcopy };

    for (auto& tok : toknz)
    {
        if (tok.size() >= 2 && tok[0] == FMT_START)
        {
            auto fmtspec = parsefmt(tok);
            if (!fmtspec) {
                // invalid format
                os << fmtspec.fmt.substr(1);
            }
            else {
                ios_saver<char> _g_(os);

                // prepare
                apply_fwpr(os, fmtspec, pva);
                apply_flags(os, fmtspec);

                auto value = extract_value(fmtspec, pva);
                ;
                if (!wconv && value.index() == 1 || value.index() == 3) {
                    wconv.reset(new wconverter(new cvt_t(os.getloc().name())));
                }

                put(os, value, fmtspec, wconv.get());
            }
        }
        else
        {
            os << tok;
        }
    }

    return os.component<counter>(0)->characters();
}


auto polyloc::do_printf(red::string_view format, std::ostream& outs, size_t max, va_list va) -> std::pair<int, int>
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
        auto to_be_writen = red::string_view(str).substr(0, max - 1);
        ret.second = to_be_writen.size();
        outs << to_be_writen;
    }

    return ret;
}

auto polyloc::do_printf(red::string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list args) -> std::pair<int, int>
{
    boost::io::ios_locale_saver _s_{ outs };
    outs.imbue(loc);
    return do_printf(format, outs, max, args);;
}
