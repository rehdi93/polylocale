#include "printf_fmt.hpp"

#include <algorithm>
#include <iterator>
#include <locale>
#include <regex>

#include "fmtdefs.h"

using std::string_view; using std::string;

template<class Char>
static constexpr bool isCharIn(Char ch, std::basic_string_view<Char> group) {
    return group.find(ch) != string::npos;
}
template<class Char, size_t N>
static constexpr bool isCharIn(Char ch, const Char(&lit)[N]) {
    return isCharIn(ch, std::basic_string_view<Char>(lit, N));
}

// string_view to long
template <class StrView>
static long svtol(StrView sv, size_t* pos = 0, int base = 10)
{
    using charT = typename StrView::value_type;

    charT* pend;
    auto pbeg = sv.data();
    long val;

    if constexpr (std::is_same_v<charT, char>) {
        val = strtol(pbeg, &pend, base);
    }
    else {
        val = wcstol(pbeg, &pend, base);
    }

    if (pbeg == pend) {
        throw std::invalid_argument("invalid svtol argument");
    }
    if (errno == ERANGE) {
        throw std::out_of_range("svtol argument out of range");
    }

    if (pos) {
        *pos = pend - pbeg;
    }

    return val;
}

template<class Iterator, typename char_t>
static void assign_sv(Iterator b, Iterator e, std::basic_string_view<char_t>& t)
{
    t = { std::addressof(*b), static_cast<size_t>(std::distance(b, e)) };
}

namespace polyloc {

bool fmt_separator::operator() (iterator& next, iterator end, token_t& token) {
    auto start = next;
    if (start == end)
        return false;

    if (*start == FMT_START)
    {
        if (std::next(start) == end)
            return false;

        if (*std::next(start) == FMT_START) {
            // escaped %
            token = "%";
            next += 2;
            return true;
        }
        else {
            // possible printf fmt
            // "%# +o| %#o" "%10.5d|:%10.5d"
            // %[flags][width][.precision][size]type
            auto typeCh = find_if(next, end, [](char ch) { return isCharIn(ch, FMT_TYPE); });
            next = typeCh != end ? typeCh + 1 : end;
            assign_sv(start, next, token);
            return true;
        }
    }
    else
    {
        next = find(start, end, FMT_START);
        assign_sv(start, next, token);
        return true;
    }
}

fmtspec_t parsefmt(string_view spec)
{
    using namespace std::literals;

    // format specifier regex
    // https://regex101.com/r/Imw6fT/3
    constexpr char Pattern[] = "%"
        "([" FMT_FLAGS "]+)?" // flags
        "(" R"([0-9]+|\*)" ")?" // field width
        "(" R"(\.[0-9]+|\.\*)" ")?" // precision
        "(h|hh|l|ll|j|z|t|L|I32|I64)?" // length mod
        "([" FMT_TYPE "])"; // types

    enum mgroups {
        FullMatch,
        Flags, FieldWidth, Precision, LengthMod, Type
    };

    std::regex rx;
    rx.imbue(std::locale::classic()); // must be called b4 assigning pattern
    rx.assign(Pattern);

    fmtspec_t fmtspec;
    fmtspec.fmt = spec;

    std::match_results<string_view::iterator> matches;
    if (std::regex_match(spec.begin(), spec.end(), matches, rx))
    {
        char typech = *matches[Type].first;
        const auto start = spec.data();

        // get type
        using ft = fmttype;
        using iof = std::ios_base;
        using mf = moreflags::Enum;

        auto isType = [=](string_view chars) {
            return chars.find(typech) != string::npos;
        };

        if (isType(FMT_CHAR)) { // chars
            fmtspec.type = ft::char_;
        }
        else if (isType(FMT_STRING)) { // strings
            fmtspec.type = ft::string;
        }
        else if (isType(FMT_SINT)) { // integers
            fmtspec.type = ft::sint;
        }
        else if (isType(FMT_UINT)) {
            fmtspec.type = ft::uint;
        }
        else if (isType(FMT_FLOATING_PT)) { // floating point
            fmtspec.type = ft::floatpt;
        }
        else if (isType(FMT_POINTER)) {
            fmtspec.type = ft::pointer;
        }

        if (isupper(typech, std::locale::classic())) {
            fmtspec.iosflags |= iof::uppercase;
        }

        // get repr
        switch (typech)
        {
        case 'd': case 'i': case 'u': // dec
            fmtspec.iosflags |= iof::dec;
            break;
        case 'X': case 'x': // hex
            fmtspec.iosflags |= iof::hex;
            break;
        case 'f': case 'F': // float fixed
            fmtspec.iosflags |= iof::fixed;
            break;
        case 'e': case 'E': // float scientific
            fmtspec.iosflags |= iof::scientific;
            break;
        case 'g': case 'G': // float default
            fmtspec.iosflags &= ~iof::floatfield;
            break;
        case 'a': case 'A': // hexfloat
            fmtspec.iosflags |= iof::floatfield;
            break;
        case 'o': // octal
            fmtspec.iosflags |= iof::oct;
            break;
        case 'C': case 'S': // special case for wide char and strings
            fmtspec.moreflags |= mf::wide;
            break;
        default:
            break;
        }

        if (matches[Flags].matched) {
            auto ptr = start + matches.position(Flags);
            auto flags_sv = string_view(ptr, matches.length(Flags));

            for (auto f : flags_sv)
            {
                switch (f)
                {
                case '-': // left justify
                    fmtspec.iosflags |= iof::left;
                    break;
                case '+': // show positive sign
                    fmtspec.iosflags |= iof::showpos;
                    break;
                case ' ': // use a blank to prefix the output value if it is signed and positive
                    fmtspec.moreflags |= mf::blankpos;
                    break;
                case '#': // alt. form: ios::showbase for ints, ios::showpoint for floats
                    fmtspec.moreflags |= mf::altform;
                    break;
                case '0': // 0 fill
                    fmtspec.moreflags |= mf::zerofill;
                    break;
                default:
                    break;
                }
            }

            // ' ' has no effect if '+' is set
            if ((fmtspec.iosflags & iof::showpos) != 0) {
                fmtspec.moreflags &= ~mf::blankpos;
            }
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
                fmtspec.moreflags |= mf::narrow;
            }
            // are we 64-bit (unix style)
            else if (length == "l") {
                if (isType("cs") || sizeof(long) == 8)
                    fmtspec.moreflags |= mf::wide;
            }
            else if (length == "ll") {
                fmtspec.moreflags |= mf::wide;
            }
            // are we 64-bit on intmax_t? (c99)
            else if (length == "j") {
                if (sizeof(intmax_t) == 8)
                    fmtspec.moreflags |= mf::wide;
            }
            // are we 64-bit (msft style)
            else if (length == "I64") {
                fmtspec.moreflags |= mf::wide;
            }
            else if (length == "I32") {
                if (sizeof(void*) == 8)
                    fmtspec.moreflags |= mf::wide;
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
            auto ptr = start + matches.position(Precision) + 1; /* skip '.' */

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

    return fmtspec;
    }

} // polyloc
