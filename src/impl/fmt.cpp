#include "printf_fmt.hpp"
#include "boost/token_functions.hpp"

#include <algorithm>
#include <iterator>
#include <locale>


namespace
{

constexpr char FMT_START = '%';
constexpr char FMT_FLAGS[] = "-+#0 ";
constexpr char FMT_SIZES[] = "hljztI";
constexpr char FMT_TYPES[] = "CcudioXxEeFfGgAapSsn";
//constexpr char FMT_ALL[] = "%.*" "-+ #0" "hljztI" "CcudioXxEeFfGgAapSsn" /*"123456789"*/;

// precision
constexpr char FMT_PRECISION = '.';
// value from VA
constexpr char FMT_FROM_VA = '*';

template <class T, std::size_t N>
constexpr std::size_t countof(const T(&array)[N]) noexcept { return N; }

red::string_view ittosv(red::string_view::iterator b, red::string_view::iterator e) noexcept
{
    using std::addressof;
    auto pb = addressof(*b); auto pe = addressof(*e);
    return { pb, pe - pb };
}

} // unnamed namespace

using std::begin; using std::end;
using std::find;
using std::locale;

bool red::polyloc::isfmtflag(char ch, bool zero, bool space)
{
    auto end_ = end(FMT_TYPES);
    auto it = find(begin(FMT_TYPES), end_, ch);

    if (it != end_)
    {
        if (*it == '0')
            return zero;
        else if (*it == ' ')
            return space;
        else
            return true;
    }
    else return false;
}

bool red::polyloc::isfmttype(char ch)
{
    auto end_ = end(FMT_TYPES);
    return find(begin(FMT_TYPES), end_, ch) != end_;
}

bool red::polyloc::isfmtsize(char ch)
{
    auto end_ = end(FMT_SIZES);
    return find(begin(FMT_SIZES), end_, ch) != end_;
}

bool red::polyloc::isfmtchar(char ch, bool digits)
{
    return ch == FMT_PRECISION || ch == FMT_FROM_VA ||
        isfmtflag(ch, false) || isfmtsize(ch) || isfmttype(ch)
        || (digits && isdigit(ch, locale::classic()));
}


namespace red::polyloc {

const int fmtspec_t::VAL_VA = -(int)FMT_FROM_VA;

//template <typename FwdIterator, typename Token>
//bool operator() (FwdIterator& next, FwdIterator end, Token& token) {

bool printf_fmt_separator::operator() (iterator& next, iterator end, std::string& token) {
    using std::adjacent_find; using std::back_inserter;

    auto start = next;

    if (m_infmt)
    {
        // "%# +o| %#o" "%10.5d|:%10.5d"
        int offs = 1;
        next++;
        auto fmtend = adjacent_find(next, end, [&](char l, char r) mutable {
            if (isfmtchar(l) && isfmttype(r)) {
                offs++;
                return true;
            }
            if (isfmttype(l)) {
                return true;
            }
            return false;
        });

        next = std::min(fmtend + offs, end);
        token.assign(start, next);
        m_infmt = false;
        return true;
    }

    next = find(start, end, FMT_START);

    if (next == end) {
        token.assign(start, end);
        return false;
    }
    else if (std::next(next) == end) {
        // format ends with a single '%'
        next++;
        return false;
    }
    
    if (*std::next(next) == FMT_START)
    {
        // escaped %
        token += FMT_START;
        next += 2;
    }
    else
    {
        // possible printf fmt
        token.assign(start, next);
        m_infmt = true;
    }

    return true;
}

void printf_fmt_separator::reset()
{
    m_infmt = false;
}

fmtspec_t parsefmt(string_view spec, std::locale const& locale)
{
    fmtspec_t fmtspec{ spec };
    assert(spec[0] == FMT_START && spec.size() >= 2);

    auto beg = spec.begin() + 1;
    const auto end_ = end(spec);
    auto pos = beg;

    // Flags
    auto it = find_if_not(beg, end_, isfmtflag);
    if (it != end_)
    {
        fmtspec.flags = ittosv(beg, it);
        pos = it;
    }

    // field width
    if (*pos == FMT_FROM_VA)
    {
        fmtspec.field_width = -(int)FMT_FROM_VA;
        pos++;
    }
    else if (isdigit(*pos, locale)) try
    {
        size_t converted;
        fmtspec.field_width = svtol(spec.substr(pos-beg), &converted);
        pos += converted;
    }
    catch (const std::exception&)
    {
        // no conversion took place
    }

    // precision
    if (*pos == FMT_PRECISION)
    {
        pos++;
        if (*pos == FMT_FROM_VA)
        {
            fmtspec.precision = -(int)FMT_FROM_VA;
            pos++;
        }
        else if (isdigit(*pos, locale)) try
        {
            size_t converted;
            fmtspec.precision = svtol(spec.substr(pos - beg), &converted);
            pos += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }

    // size overrides
    it = find_if(pos, end_, isfmtsize);
    if (it != end_)
    {
        pos = it;
        if (*pos == 'I') {
            char sfix[] = "6432";
            it = find_first_of(pos, end_, begin(sfix), end(sfix), std::not_equal_to<>{});
        }
        else {
            it = find_if_not(pos, end_, isfmtsize);
        }

        fmtspec.length_mod = ittosv(pos, it);
    }

    // conversion spec
    it = find_if(pos, end_, isfmttype);
    if (it != end_)
        fmtspec.conversion = *it;

    return fmtspec;
}

} // red::polyloc
