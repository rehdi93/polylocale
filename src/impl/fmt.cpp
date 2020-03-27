#include "printf_fmt.hpp"

#include <algorithm>
#include <iterator>
#include <functional>
#include <locale>


namespace
{

constexpr char FMT_START   = '%';
constexpr char FMT_FLAGS[] = "-+#0 ";
constexpr char FMT_SIZES[] = "hljztI";
constexpr char FMT_TYPES[] = "CcudioXxEeFfGgAapSsn";
//constexpr char FMT_ALL[] = "%.*" "-+ #0" "hljztI" "CcudioXxEeFfGgAapSsn" /*"123456789"*/;

// precision
constexpr char FMT_PRECISION = '.';
// value from VA
constexpr char FMT_FROM_VA = '*';


red::string_view ittosv(red::string_view::iterator b, red::string_view::iterator e) noexcept
{
    auto pb = std::addressof(*b); auto pe = std::addressof(*e);
    return { pb, size_t(pe - pb) };
}

} // unnamed namespace

using std::begin; using std::end;
using std::find;
using std::locale;

bool red::polyloc::isfmtflag(char ch, bool zero, bool space)
{
    auto end_ = end(FMT_FLAGS);
    auto it = find(begin(FMT_FLAGS), end_, ch);

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

bool fmt_separator::operator() (iterator& next, iterator end, std::string& token) {
    using std::adjacent_find;

    auto start = next;
    if (start == end)
        return false;

    if (m_infmt)
    {
        // "%# +o| %#o" "%10.5d|:%10.5d"
        ptrdiff_t offs = 1;
        next++;
        auto fmtend = adjacent_find(next, end, [&](char l, char r) mutable {
            if (isfmttype(l)) {
                return true;
            }
            if (isfmtchar(l) && isfmttype(r)) {
                offs++;
                return true;
            }
            return false;
        });

        offs = std::min(distance(fmtend, end), offs);
        next = fmtend + offs;

        token.assign(start, next);
        m_infmt = false;
        return true;
    }

    next = find(start, end, FMT_START);
    token.assign(start, next);

    if (distance(next, end) >= 2)
    {
        if (*std::next(next) == FMT_START) {
            // escaped %
            token += FMT_START;
            next += 2;
        }
        else {
            // possible printf fmt
            m_infmt = true;
        }
    }
    else if (next != end && std::next(next)==end)
    {
        next++;
    }

    return true;
}

void fmt_separator::reset()
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
    auto it = find_if_not(beg, end_, [](char ch) { return isfmtflag(ch); });
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
        fmtspec.field_width = svtol(spec.substr(pos-beg+1), &converted);
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
            fmtspec.precision = svtol(spec.substr(pos - beg + 1), &converted);
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
