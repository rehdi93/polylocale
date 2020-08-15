#include "printf_fmt.hpp"

#include <algorithm>
#include <iterator>
#include <functional>
#include <locale>
#include <regex>


namespace
{

#define CH_FLAGS "-+#0 "
#define CH_TYPES "CcudioXxEeFfGgAapSsn"

constexpr char FMT_START = '%';
constexpr char FMT_LENGTHMOD[] = "hljztLI";
#define RX_LENGTHMOD "h|hh|l|ll|j|z|t|L|I32|I64"
constexpr char FMT_FLAGS[] = CH_FLAGS;
constexpr char FMT_TYPES[] = CH_TYPES;

// precision
constexpr char FMT_PRECISION = '.';
// value from VA
constexpr char FMT_FROM_VA = '*';

// format specifier regex
// https://regex101.com/r/Imw6fT/1
constexpr auto FMT_PATTERN = "%"
    "([" CH_FLAGS "]+)?"
    "(" R"([0-9]+|\*)" ")?" // field width
    "(" R"(\.[0-9]+|\.\*)" ")?" // precision
    "(" RX_LENGTHMOD ")?"
    "([" CH_TYPES "])";


#undef CH_FLAGS
#undef CH_TYPES
#undef RX_LENGTHMOD

} // unnamed namespace

using std::begin; using std::end;
using std::find;

namespace red::polyloc {

bool isfmtflag(char ch, bool zero, bool space)
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

bool isfmttype(char ch)
{
    auto const e = end(FMT_TYPES);
    return find(begin(FMT_TYPES), e, ch) != e;
}

bool isfmtlength(char ch)
{
    auto const e = end(FMT_LENGTHMOD);
    return find(begin(FMT_LENGTHMOD), e, ch) != e;
}

bool isfmtchar(char ch, bool digits)
{
    return ch == FMT_PRECISION || ch == FMT_FROM_VA ||
        isfmtflag(ch, digits) || isfmtlength(ch) || isfmttype(ch)
        || (digits && isdigit(ch, std::locale::classic()));
}


bool fmt_separator::operator() (iterator& next, iterator end, std::string& token) {
    auto start = next;
    bool in_fmt = false;

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
            in_fmt = true;
        }
    }

    if (in_fmt)
    {
        // "%# +o| %#o" "%10.5d|:%10.5d"
        ptrdiff_t offs = 1;
        next++;
        auto fmtend = std::adjacent_find(next, end, [&](char l, char r) mutable {
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
    }
    else
    {
        next = find(start, end, FMT_START);
        token.assign(start, next);
    }

    return true;
}


fmtspec_t parsefmt(std::string const& spec)
{
    using red::string_view;

    const std::regex rx{ FMT_PATTERN };
    enum match_groups : size_t {
        FullMatch,
        Flags, FieldWidth, Precision, LengthMod, Type
    };
    fmtspec_t fmtspec;
    fmtspec.fmt = spec;

    std::smatch matches;
    if (std::regex_match(spec, matches, rx))
    {
        fmtspec.conversion = *matches[Type].first;
        //const auto start = std::addressof(*matches[FullMatch].first);

        const auto start = spec.data();

        if (matches[Flags].matched) {
            auto ptr = start + matches.position(Flags);
            fmtspec.flags = string_view(ptr, matches.length(Flags));
        }
        if (matches[LengthMod].matched) {
            auto ptr = start + matches.position(LengthMod);
            fmtspec.length_mod = string_view(ptr, matches.length(LengthMod));
        }

        if (matches[FieldWidth].matched) {
            auto ptr = start + matches.position(FieldWidth);

            if (*ptr == FMT_FROM_VA) {
                fmtspec.field_width = fmtspec.VAL_VA;
            }
            else try {
                auto num = string_view(ptr, matches.length(FieldWidth));
                fmtspec.field_width = svtol(num);
            }
            catch (const std::exception&) {
                // no conversion took place
            }
        }
        if (matches[Precision].matched) {
            auto ptr = start + matches.position(Precision) + 1 /* skip '.' */;

            if (*ptr == FMT_FROM_VA) {
                fmtspec.precision = fmtspec.VAL_VA;
            }
            else try {
                auto num = string_view(ptr, matches.length(Precision));
                fmtspec.precision = svtol(num);
            }
            catch (const std::exception&) {
                // no conversion took place
            }
        }

    }

    return fmtspec;
}

} // red::polyloc
