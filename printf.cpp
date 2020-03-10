#include "polyloc.hpp"

#include <sstream>
#include <iomanip>
#include <cstddef>
#include <array>
#include <vector>
#include <cassert>

#include "bitmask.hpp"

using std::ios;

// HELPERS
namespace
{

constexpr auto STREAM_MAX = std::numeric_limits<std::streamsize>::max();

constexpr char FMT_START    = '%';
constexpr char FMT_ALL[]    = "%.*" "-+ #0" "hljztI" "CcudioXxEeFfGgAapSsn";
constexpr char FMT_FLAGS[]  = "-+ #0";
constexpr char FMT_TYPES[]  = "CcudioXxEeFfGgAapSsn";
constexpr char FMT_SIZES[]  = "hljztI";
// precision
constexpr char FMT_PRECISION = '.';
// value from VA
constexpr char FMT_FROM_VA = '*';

template <class T, std::size_t N>
constexpr std::size_t countof(const T(&array)[N]) noexcept { return N; }

}


using namespace bitmask::ops;

enum class arg_flags : short
{
    none,
    is_signed = 1 << 0,
    is_unsigned = 1 << 1,
    wide = 1 << 2,
    narrow = 1 << 3,
    size_unknown = 1 << 4,
    blankpos = 1 << 5,
    auto_precision = 1 << 6,

    signfield = is_signed | is_unsigned,
    sizefield = wide | narrow
};

enum class arg_type : unsigned char
{
    char_t = 'c',
    int_t = 'i',
    float_t = 'g',
    pointer = 'p',
    string = 's',
    byteswriten = 'n',
    unknown = 0
};

struct stream_state
{
    ios::fmtflags flags;
    char fill;
    std::streamsize width, precision;
    std::ios& stream;

    stream_state(std::ios& ios)
        : flags(ios.flags()), fill(ios.fill()),
        width(ios.width()), precision(ios.precision())
        , stream(ios)
    {}

    ~stream_state() noexcept
    {
        restore(stream);
    }

    void restore(std::ios& io) const {
        io.flags(flags);
        io.fill(fill);
        io.width(width);
        io.precision(precision);
        //io.imbue(locale);
    }
};

// holds info about the current spec being parsed
struct printf_arg
{
    
    // %[flags][width][.precision][size]type
    // worst case scenerio sizes:
    // 1 + 4  +  10 +  1 +  10  +   3  + 1 = 30
    // 10 is from INT_MAX
    explicit printf_arg(const std::string& stor, const std::locale& loc)
        : fmtspec(stor), locale(loc)
    {
    }

    union values_u {
        char character;
        char* string;
        wchar_t wchar;
        wchar_t* wstring;
        void* pointer;

        intmax_t integer;
        uintmax_t unsigned_integer;

        long double floatpt;

        // ----
        void set(char c) { character = c; }
        void set(char* s) { string = s; }
        void set(wchar_t c) { wchar = c; }
        void set(wchar_t* s) { wstring = s; }
        void set(int64_t i) { integer = i; }
        void set(int32_t i) { integer = i; }
        void set(uint64_t i) { unsigned_integer = i; }
        void set(uint32_t i) { unsigned_integer = i; }
        void set(double d) { floatpt = d; }
        void set(void* p) { pointer = p; }
        // ----


        template<typename T>
        values_u(T value) {
            set(value);
        }

        values_u() = default;
    };


    arg_type type{};
    arg_flags flags{};
    ios::fmtflags iosflags{};
    char fill = ' ';
    int fieldw = 0, precision = 0;
    std::locale locale;

    std::string const& fmtspec;
    values_u value;

    
    // sets arg_flags::wide if sizeof(T)==8, otherwise sets arg_flags::narrow
    template<typename T>
    constexpr void set_sizem() {
        auto sizem = sizeof(T) == 8 ? arg_flags::wide : arg_flags::narrow;
        flags |= sizem;
    }

    // print value
    auto& put(std::ostream& os) const
    {
        stream_state state = os;

        os.width(fieldw);
        os.precision(precision);
        os.fill(fill);
        os.flags(iosflags);

        switch (type)
        {
        case arg_type::char_t:
            if (bitmask::has(flags, arg_flags::wide))
            {
                auto& f = std::use_facet<std::ctype<wchar_t>>(os.getloc());
                auto c = f.narrow(value.wchar);
                os << c;
            }
            else
            {
                os << value.character;
            }
            break;
        case arg_type::int_t:
        {
            bool skip = false;
            bool doblankpos = bitmask::has_all(flags, arg_flags::blankpos | arg_flags::is_signed);

            if (precision==0 && value.integer == 0)
            {
                if (bitmask::has_all(iosflags, ios::oct | ios::showbase))
                {
                    // for octal, if both the converted value and the precision are ​0​, single ​0​ is written.
                    os << 0;
                    skip = true;
                }
                else if (bitmask::has(flags, arg_flags::is_signed))
                {
                    skip = true;
                }
            }

            if (doblankpos)
                os.put(' ');

            if (skip)
            {
                break;
            }

            if (bitmask::has(flags, arg_flags::wide))
            {
                if (bitmask::has(flags, arg_flags::is_signed)) {

                    os << int64_t(value.integer);
                }
                else {
                    os << uint64_t(value.unsigned_integer);
                }
            }
            else
            {
                if (bitmask::has(flags, arg_flags::is_signed)) {
                    os << int32_t(value.integer);
                }
                else {
                    os << uint32_t(value.unsigned_integer);
                }
            }
        }
            break;
        case arg_type::float_t:
            if (bitmask::has_all(flags, arg_flags::blankpos | arg_flags::is_signed))
                os.put(' ');

            if (bitmask::has(flags, arg_flags::narrow)) {
                os << float(value.floatpt);
            }
            else {
                os << double(value.floatpt);
            }
            break;
        case arg_type::pointer:
            os << value.pointer;
            break;
        case arg_type::string:
            if (bitmask::has(flags, arg_flags::wide)) {
                auto& f = std::use_facet<std::ctype<wchar_t>>(os.getloc());
                red::wstring_view view = value.wstring;
                std::string tmp(view.size(), '\0');
                f.narrow(view.data(), view.data() + view.size(), '?', &tmp[0]);

                os << tmp;
            } else {
                os << value.string;
            }
            break;
        case arg_type::byteswriten:
            // not implemented
        default:
            // invalid, print fmt as-is minus %
            auto v = red::string_view(fmtspec).substr(1);
            os << v;
            break;
        }

        return os;
    }

};





auto& operator << (std::ostream& os, const printf_arg& arg) {
    return arg.put(os);
}

static auto parsefmt(printf_arg& info, va_list& va) -> size_t;

int red::polyloc::do_printf(string_view fmt, std::ostream& outs, const std::locale& loc, va_list va)
{
    if (fmt.empty())
        return 0;

    outs.imbue(loc);

    auto format = fmt;
    std::string spec;
    spec.reserve(30);

    for (size_t i;;)
    {
        // look for %
        i = format.find(FMT_START);
        
        if (i == string_view::npos)
        {
            // no more specs, we're done
            outs << format;
            return outs.tellp();
        }

        auto txtb4 = format.substr(0, i);

        if (!format[i + 1] || format[i + 1] == FMT_START)
        {
            // escaped % or null
            outs << txtb4 << format[i + 1];
            format.remove_prefix(txtb4.size()+2);
        }
        else
        {
            outs << txtb4;

            // possible fmtspec(s)
            const auto end = format.find_first_of(FMT_TYPES, i + 1) + 1;
            spec.assign(format.data() + i, end - i);

            // %[flags][width][.precision][size]type
            printf_arg info{ spec, loc };
            parsefmt(info, va);
            outs << info;

            i += spec.size();
            format.remove_prefix(i);
        }
    }
}

auto red::polyloc::do_printf_n(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list va) -> write_result
{
    write_result ret;
    std::ostringstream tmp;
    ret.chars_needed = do_printf(format, tmp, loc, va);
    auto tmpstr = tmp.str();

    if (ret.chars_needed <= max)
    {
        outs << tmpstr;
        ret.chars_written = tmpstr.size();
    }
    else
    {
        auto i = max > 0 ? max - 1 : 0;
        auto to_be_writen = string_view(tmpstr).substr(0, i);

        ret.chars_needed = tmpstr.size();
        ret.chars_written = to_be_writen.size();

        outs << to_be_writen;
    }

    return ret;
}


size_t parsefmt(printf_arg& info, va_list& va)
{
    auto constexpr npos = std::string::npos;
    const auto locale = info.locale;
    
    size_t i = 0, prev = 0;

    assert(info.fmtspec[i] == FMT_START);

    i = info.fmtspec.find_first_of(FMT_FLAGS, i);
    if (i != npos)
    {
        switch (info.fmtspec[i])
        {
        case '-': // left justify
            info.iosflags |= ios::left;
            break;
        case '+': // show positive sign
            info.iosflags |= ios::showpos;
            break;
        case ' ': // Use a blank to prefix the output value if it is signed and positive, ignored if '+' is also set
            if (!bitmask::has(info.iosflags, ios::showpos)) {
                info.flags |= arg_flags::blankpos;
            }
            // ...
            break;
        case '#':
            info.iosflags |= ios::showbase;
            break;
        case '0': // zero fill
            if (i == 1)
                info.fill = '0';
            else
                i = 0;
            
            break;
        }
    }
    else i = prev;

    i++;


    // get field width (optional)
    if (info.fmtspec[i] == FMT_FROM_VA)
    {
        auto fw = va_arg(va, int);
        info.fieldw = fw;
        i++;
    }
    else if (std::isdigit(info.fmtspec[i], locale))
    {
        int fw; size_t converted;
        try {
            fw = std::stoi(info.fmtspec.substr(i), &converted);
            info.fieldw = fw;
            i += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }

    // get precision (optional)
    if (info.fmtspec[i] == FMT_PRECISION)
    {
        i++;
        if (info.fmtspec[i] == FMT_FROM_VA)
        {
            auto pr = va_arg(va, int);
            info.precision = pr;
            i++;
        }
        else if (std::isdigit(info.fmtspec[i], locale)) try
        {
            int pr; size_t converted;
            pr = std::stoi(info.fmtspec.substr(i), &converted);
            info.precision = pr;
            i += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }
    else
    {
        info.flags |= arg_flags::auto_precision;
    }

    // handle size overrides (optional)
    // this decides between int32/int64, float/double
    prev = i;
    i = info.fmtspec.find_first_of(FMT_SIZES, i);
    if (i != npos)
    {
        switch (info.fmtspec[i++])
        {
        case 'h': // halfwidth
            info.flags = arg_flags::narrow;
            if (info.fmtspec[i] == 'h')
                i++; // QUARTERWIDTH
            break;
        case 'l':  // are we 64-bit (unix style)
            if (info.fmtspec[i] == 'l') {
                info.flags |= arg_flags::wide;
                i++;
            }
            else {
                info.set_sizem<long>();
            }
            break;
        case 'j': // are we 64-bit on intmax_t? (c99)
            info.set_sizem<size_t>();
            break;
        case 'z': // are we 64-bit on size_t or ptrdiff_t? (c99)
        case 't':
            info.set_sizem<ptrdiff_t>();
            break;
        case 'I': // are we 64-bit (msft style)
            if (info.fmtspec[i] == '6' && info.fmtspec[i+1] == '4') {
                info.flags |= arg_flags::wide;
                i++;
            }
            else if (info.fmtspec[i] == '3' && info.fmtspec[i+1] == '2') {
                info.set_sizem<void*>();
                i++;
            }
            break;
        }
    }
    else {
        i = prev;
        info.flags |= arg_flags::size_unknown;
    }

    // handle type, extract properties and value
    prev = i;
    i = info.fmtspec.find_first_of(FMT_TYPES, i);
    if (i != npos)
    {
        switch (info.fmtspec[i])
        {
        case 'C': // wide char TODO
            info.type = arg_type::char_t;
            bitmask::setm(info.flags, arg_flags::wide, arg_flags::sizefield);
            info.value = va_arg(va, wchar_t);
            break;
        case 'c': // char
            info.type = arg_type::char_t;
            bitmask::setm(info.flags, arg_flags::narrow, arg_flags::sizefield);
            info.value = va_arg(va, char);
            break;
        case 'u': // Unsigned decimal integer
            info.iosflags |= ios::dec;
            info.type = arg_type::int_t;
            goto common_uint;
        case 'd': // Signed decimal integer
        case 'i':
            info.iosflags |= ios::dec;
            info.type = arg_type::int_t;

            goto common_int;
        case 'o': // Unsigned octal integer
            info.type = arg_type::int_t;
            info.iosflags |= ios::oct;

            goto common_uint;
        case 'X': // Unsigned hexadecimal integer w/ uppercase letters
            info.iosflags |= ios::uppercase;
        case 'x': // Unsigned hexadecimal integer w/ lowercase letters
            info.iosflags |= ios::hex;
            info.type = arg_type::int_t;
            
            goto common_uint;
        case 'E': // float, scientific notation
            info.iosflags |= ios::uppercase;
        case 'e':
            info.iosflags |= ios::scientific;
            info.type = arg_type::float_t;
            goto common_float;
        case 'F': // float, fixed notation
            info.iosflags |= ios::uppercase;
        case 'f':
            info.iosflags |= ios::fixed;
            info.type = arg_type::float_t;

            goto common_float;
        case 'G': // float, general notation
            info.iosflags |= ios::uppercase;
        case 'g':
            info.iosflags &= ~ios::floatfield;
            info.type = arg_type::float_t;

            goto common_float;
        case 'A': // hex float
            info.iosflags |= ios::uppercase;
        case 'a':
            info.iosflags |= ios::hexfloat;
            info.type = arg_type::float_t;

            goto common_float;
        case 'p': // pointer
            info.iosflags |= ios::hex;
            info.type = arg_type::pointer;
            info.value = va_arg(va, void*);
            break;
        case 'S': // wide string TODO
            info.flags |= arg_flags::wide;
            info.type = arg_type::string;
            info.value = va_arg(va, wchar_t*);
            break;
        case 's': // string
            info.type = arg_type::string;
            info.value = va_arg(va, char*);
            break;
        case 'n': // weird write-bytes specifier (not implemented)
            info.value = va_arg(va, void*);
            info.type = arg_type::byteswriten;
            break;

        // https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=vs-2019#argument-size-specification
        // integer args w/o size spec are treated as 32bit
        common_int:
            info.flags |= arg_flags::is_signed;

            if (bitmask::has(info.flags, arg_flags::size_unknown)) {
                info.flags |= arg_flags::narrow;
            }

            if (bitmask::has(info.flags, arg_flags::wide)) {
                info.value = va_arg(va, int64_t);
            }
            else {
                info.value = va_arg(va, int32_t);
            }
            break;
        common_uint:
            info.flags |= arg_flags::is_unsigned;

            if (bitmask::has(info.flags, arg_flags::size_unknown)) {
                info.flags |= arg_flags::narrow;
            }

            if (bitmask::has(info.flags, arg_flags::wide)) {
                info.value = va_arg(va, uint64_t);
            }
            else {
                info.value = va_arg(va, uint32_t);
            }
            break;
        // floating point types w/o size spec are treated as 64bit
        common_float:
            if (bitmask::has(info.flags, arg_flags::size_unknown)) {
                info.flags |= arg_flags::wide;
            }

            if (bitmask::has(info.flags, arg_flags::wide)) {
                auto a = va_arg(va, double);
                info.flags |= a >= 0 ? arg_flags::is_signed : arg_flags::is_unsigned;
                info.value = a;
            }
            else {
                auto a = va_arg(va, float);
                info.flags |= a >= 0 ? arg_flags::is_signed : arg_flags::is_unsigned;
                info.value = a;
            }

            break;
        }

        return i + 1;
    }
    else
    {
        // couldn't find type specifier
        info.type = arg_type::unknown;
        info.flags = arg_flags::none;
        info.iosflags = ios::fmtflags{};
        return npos;
    }
}
