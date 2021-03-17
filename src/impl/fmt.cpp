#include "printf_fmt.hpp"

#include <algorithm>
#include <iterator>
#include <locale>
#include <regex>
#include <bitset>

#include "fmtdefs.h"

using std::begin; using std::end;
using std::find;

using red::string_view;
using std::string;

static void assign_sv(string_view& sv, string_view::iterator begin_it, string_view::iterator end_it)
{
    auto* begin = std::addressof(*begin_it);
    auto dist = std::distance(begin_it, end_it);
    assert(dist > 0);
    sv = { begin, size_t(dist) };
}

namespace red::polyloc {

static constexpr bool isCharOneOf(char ch, string_view group) {
    return group.find(ch) != string::npos;
}

bool isfmtflag(char ch) noexcept
{
    return isCharOneOf(ch, FMT_FLAGS);
}

bool isfmttype(char ch) noexcept
{
    return isCharOneOf(ch, FMT_TYPE);
}

bool isfmtlength(char ch) noexcept
{
    return isCharOneOf(ch, FMT_LENGTHMOD);
}

bool isfmtchar(char ch)
{
    return isCharOneOf(ch, FMT_SPECIAL FMT_TYPE FMT_FLAGS FMT_LENGTHMOD) 
        || isdigit(ch, std::locale::classic());
}


bool fmt_separator::operator() (iterator& next, iterator end, red::string_view& token) {
    auto start = next;
    if (start == end)
        return false;

    if (*start == FMT_START)
    {
        if (std::next(start) == end)
            return false;

        if (*std::next(start) == FMT_START) {
            // escaped %
            //token.assign(1, FMT::Start);
            auto& ch = *std::next(start);
            token = { &ch, 1 };
            next += 2;
            return true;
        }
        else {
            // possible printf fmt
            // "%# +o| %#o" "%10.5d|:%10.5d"
            auto fmtend = std::find_if(next, end, isfmttype);
            next = fmtend != end ? fmtend + 1 : end;
            //token.assign(start, next);
            assign_sv(token, start, next);
            return true;
        }
    }
    else
    {
        next = find(start, end, FMT_START);
        //token.assign(start, next);
        assign_sv(token, start, next);
        return true;
    }
}


fmtspec_t parsefmt(string_view spec)
{
    using namespace std::literals;

    // format specifier regex
    // https://regex101.com/r/Imw6fT/2
    constexpr auto Pattern = "%"
        "([" FMT_FLAGS "]+)?" // flags
        "(" R"([0-9]+|\*)" ")?" // field width
        "(" R"(\.[0-9]+|\.\*)" ")?" // precision
        "(" "h|hh|l|ll|j|z|t|L|I32|I64" ")?" // length mod
        "([" FMT_TYPE "])"; // types

    enum match_groups : size_t {
        FullMatch,
        Flags, FieldWidth, Precision, LengthMod, Type
    };

    std::regex rx{ Pattern };
    rx.imbue(std::locale::classic());

    fmtspec_t fmtspec;
    fmtspec.fmt = spec;

    std::match_results<string_view::iterator> matches;
    if (std::regex_match(spec.begin(), spec.end(), matches, rx))
    {
        char typech = *matches[Type].first;
        const auto start = spec.data();

        // get type
        using ft = fmttype;
        using ff = fmtflag;

        auto typeis = [=](string_view chars) {
            return chars.find(typech) != string::npos;
        };

        if (typeis(FMT_CHAR)) { // chars
            fmtspec.type = ft::char_;
        }
        else if (typeis(FMT_STRING)) { // strings
            fmtspec.type = ft::string;
        }
        else if (typeis(FMT_INTEGER)) { 
            // integers
            if (typeis(FMT_SINT))
                fmtspec.type = ft::sint; // signed
            else if (typeis(FMT_UINT))
                fmtspec.type = ft::uint; // unsigned
        }
        else if (typeis(FMT_FLOATING_PT)) { // floating point
            fmtspec.type = ft::floatpt;
            fmtspec.uppercase = isupper(typech, std::locale::classic());
        }
        else if (typech == *FMT_POINTER) {
            fmtspec.type = ft::pointer;
        }

        // get repr
        if (typeis(FMT_SINT) || typech == FMT_UINT[0]) {
            fmtspec.style = repr::decimal;
        }

        switch (typech)
        {
        case 'd': case 'i': case 'u': // dec
            fmtspec.style = repr::decimal;
            break;
        case 'X': case 'x': // hex
            fmtspec.style = repr::hex;
            break;
        case 'f': case 'F': // float fixed
            fmtspec.style = repr::fixed;
            break;
        case 'e': case 'E': // float scientific
            fmtspec.style = repr::scientific;
            break;
        case 'g': case 'G': // float default
            fmtspec.style = repr::deffloat;
            break;
        case 'a': case 'A': // hexfloat
            fmtspec.style = repr::hexfloat;
            break;
        case 'C': case 'S': // special case for wide char and strings
            fmtspec.lengthmod = lenmod::wide;
            break;
        default:
            break;
        }

        if (matches[Flags].matched) {
            auto ptr = start + matches.position(Flags);
            auto flags_sv = string_view(ptr, matches.length(Flags));
            std::bitset<5> bits;

            for (auto f : flags_sv)
            {
                switch (f)
                {
                case '-': // left justify
                    bits[0] = true;
                    //fmtspec.flags |= (int)ff::left;
                    break;
                case '+': // show positive sign
                    bits[1] = true;
                    break;
                case ' ': // use a blank to prefix the output value if it is signed and positive
                    bits[2] = true;
                    break;
                case '#': // alt. form: ios::showbase for ints, ios::showpoint for floats
                    bits[3] = true;
                    break;
                case '0': // 0 fill
                    bits[4] = true;
                    break;
                default:
                    break;
                }
            }

            // ' ' has no effect if '+' is set
            if (bits[1] && bits[2]) {
                bits[2] = false;
            }

            fmtspec.flags = bits.to_ulong();
        }
        if (matches[LengthMod].matched) {
            auto ptr = start + matches.position(LengthMod);
            auto length = string_view(ptr, matches.length(LengthMod));

            if (length.empty()) {
                // do nothing...
            }
            else if (length == "h" || length == "hh") {
                // halfwidth, quarterwidth
                // not used, but keep track of it
                fmtspec.lengthmod = lenmod::narrow;
            }
            // are we 64-bit (unix style)
            else if (length == "l") {
                if (typeis("cs") || sizeof(long) == 8)
                    fmtspec.lengthmod = lenmod::wide;
            }
            else if (length == "ll") {
                fmtspec.lengthmod = lenmod::wide;
            }
            // are we 64-bit on intmax_t? (c99)
            else if (length == "j") {
                if (sizeof(intmax_t) == 8)
                    fmtspec.lengthmod = lenmod::wide;
            }
            // are we 64-bit (msft style)
            else if (length == "I64") {
                fmtspec.lengthmod = lenmod::wide;
            }
            else if (length == "I32") {
                if (sizeof(void*) == 8)
                    fmtspec.lengthmod = lenmod::wide;
            }
        }
        if (matches[FieldWidth].matched) {
            auto ptr = start + matches.position(FieldWidth);

            if (*ptr == FMT_VA) {
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

            if (*ptr == FMT_VA) {
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
