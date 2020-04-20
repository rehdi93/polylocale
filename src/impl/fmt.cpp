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

} // unnamed namespace

using std::begin; using std::end;
using std::find;

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
        isfmtflag(ch, digits) || isfmtsize(ch) || isfmttype(ch)
        || (digits && isdigit(ch, std::locale::classic()));
}


namespace red::polyloc {

const int fmtspec_t::VAL_VA = -(int)FMT_FROM_VA;

bool fmt_separator::operator() (iterator& next, iterator end, std::string& token) {
    using std::adjacent_find;

    auto start = next;
    if (start == end)
        return false;
    
    if (*start == FMT_START)
    {
        if (std::next(start) == end)
            return false;

        if (*std::next(start) == FMT_START) {
            // escaped %
            token.assign(1, FMT_START);
            next += 2;
            return true;
        }
        else {
            // possible printf fmt
            m_infmt = true;
        }
    }

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
                offs = 2;
                return true;
            }
            return false;
        });

        offs = std::min(distance(fmtend, end), offs);
        next = fmtend + offs;

        token.assign(start, next);
        m_infmt = false;
    }
    else
    {
        next = find(start, end, FMT_START);
        token.assign(start, next);
    }

    return true;
}

void fmt_separator::reset()
{
    m_infmt = false;
}


fmtspec_t parsefmt(red::string_view spec, std::locale const& locale)
{
    constexpr auto npos = red::string_view::npos;
    fmtspec_t fmtspec{ spec };
    assert(spec.size() >= 2 && spec[0] == FMT_START);

    size_t pos = 1;

    // Flags
    auto i = spec.find_first_not_of(FMT_FLAGS, 1);
    if (i != npos)
    {
        fmtspec.flags = spec.substr(1, i-1);
        pos = i;
    }

    // field width
    if (spec[pos] == FMT_FROM_VA)
    {
        fmtspec.field_width = -(int)FMT_FROM_VA;
        pos++;
    }
    else if (isdigit(spec[pos], locale)) try
    {
        size_t converted;
        fmtspec.field_width = svtol(spec.substr(pos), &converted);
        pos += converted;
    }
    catch (const std::exception&) {
        // no conversion took place
    }

    // precision
    if (spec[pos] == FMT_PRECISION)
    {
        pos++;
        if (spec[pos] == FMT_FROM_VA)
        {
            fmtspec.precision = -(int)FMT_FROM_VA;
            pos++;
        }
        else if (isdigit(spec[pos], locale)) try
        {
            size_t converted;
            fmtspec.precision = svtol(spec.substr(pos), &converted);
            pos += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }
    
    // size overrides
    i = spec.find_first_of(FMT_SIZES, pos);
    if (i != npos)
    {
        pos = i;
        if (spec[pos] == 'I') {
            i = spec.find_first_not_of("6432", pos);
        }
        else {
            i = spec.find_first_not_of(FMT_SIZES, pos);
        }
        
        fmtspec.length_mod = spec.substr(pos, i - pos);
        pos = i;
    }

    // conversion spec
    i = spec.find_first_of(FMT_TYPES, pos);
    if (i != npos)
        fmtspec.conversion = spec.at(i);

    return fmtspec;
}

} // red::polyloc
