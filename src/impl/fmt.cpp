#include "printf_fmt.hpp"

#include <algorithm>
#include <iterator>
#include <locale>
#include <regex>

#include "fmtdefs.h"

using std::begin; using std::end;
using std::find;

namespace red::polyloc {

bool isfmtflag(char ch) noexcept
{
    auto const e = end(FMT::Flags);
    return find(begin(FMT::Flags), e, ch) != e;
}

bool isfmttype(char ch) noexcept
{
    auto const e = end(FMT::Types);
    return find(begin(FMT::Types), e, ch) != e;
}

bool isfmtlength(char ch) noexcept
{
    auto const e = end(FMT::Length);
    return find(begin(FMT::Length), e, ch) != e;
}

bool isfmtchar(char ch)
{
    return ch==FMT::Precision || ch==FMT::FromVa || ch==FMT::Start ||
        isfmtflag(ch) || isfmtlength(ch) || isfmttype(ch)
        || isdigit(ch, std::locale::classic());
}


bool fmt_separator::operator() (iterator& next, iterator end, std::string& token) {
    auto start = next;
    if (start == end)
        return false;
    
    if (*start == FMT::Start)
    {
        if (std::next(start) == end)
            return false;

        if (*std::next(start) == FMT::Start) {
            // escaped %
            token.assign(1, FMT::Start);
            next += 2;
            return true;
        }
        else {
            // possible printf fmt
            // "%# +o| %#o" "%10.5d|:%10.5d"
            auto fmtend = std::find_if(next, end, isfmttype);
            next = fmtend != end ? fmtend + 1 : end;
            token.assign(start, next);
            return true;
        }
    }
    else
    {
        next = find(start, end, FMT::Start);
        token.assign(start, next);
        return true;
    }
}


fmtspec_t parsefmt(std::string const& spec)
{
    using red::string_view;

    const std::regex rx{ FMT::Pattern };
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

        const auto start = spec.c_str();

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

            if (*ptr == FMT::FromVa) {
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

            if (*ptr == FMT::FromVa) {
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
    else
    {
        fmtspec.error = true;
    }

    return fmtspec;
}

} // red::polyloc
