#include "printf.hpp"

#include <sstream>
#include <iomanip>
#include <cstddef>
#include <array>
#include <vector>
#include <cassert>
#include <cmath>
#include <functional>

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
namespace bm = bitmask;

enum class arg_flags : unsigned short
{
    none,
    is_signed = 1 << 0,
    is_unsigned = 1 << 1,
    wide = 1 << 2,
    blankpos = 1 << 4,
    showpos = 1 << 5,

    altform = 1 << 9, // ios::showbase for ints, ios::showpoint for floats

    signfield = is_signed | is_unsigned,
    sizefield = wide,
    posfield = blankpos|showpos
};

enum class arg_type : unsigned char
{
    unknown = 0,
    char_t = 'c',
    int_t = 'i',
    float_t = 'g',
    pointer = 'p',
    string = 's',
    byteswriten = 'n'
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
        void set(long double d) { floatpt = d; }
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
    int fieldw = -1, precision = -1; // auto
    std::locale const& locale;

    std::string const& fmtspec;
    values_u value;

    
    // sets arg_flags::wide if sizeof(T)==8
    template<typename T>
    constexpr void set_sizem() 
    {
        if (sizeof(T)==8)
        {
            flags |= arg_flags::wide;
        }
    }


    // print value
    auto& put(std::ostream& os) const
    {
        stream_state state = os;

        const auto signfield = (flags & arg_flags::signfield);
        const auto widthfield = (flags & arg_flags::sizefield);

        os.flags(iosflags);
        os.width(fieldw != -1 ? fieldw : 0);
        os.precision(precision != -1 ? precision : 0);
        os.fill(fill);

        switch (type)
        {
        case arg_type::char_t:
            os << value.character;
            break;
        case arg_type::int_t:
        {
            if (widthfield == arg_flags::wide)
            {
                if (signfield == arg_flags::is_signed) {
                    put_int(os, int64_t(value.integer));
                }
                else {
                    put_int(os, uint64_t(value.unsigned_integer));
                }
            }
            else
            {
                if (signfield == arg_flags::is_signed) {
                    put_int(os, int32_t(value.integer));
                }
                else {
                    put_int(os, uint32_t(value.unsigned_integer));
                }
            }
        }
            break;
        case arg_type::float_t:
            put_fp(os, value.floatpt);
            break;
        case arg_type::pointer:
            os << value.pointer;
            break;
        case arg_type::string:
            put_str(os, value.string);
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

private:
    void put_fp(std::ostream& os, double number) const
    {
        if (bm::has(flags, arg_flags::altform)) {
            os << std::showpoint;
        }

        const auto posfield = (flags & arg_flags::posfield);
        if (posfield == arg_flags::blankpos && number >= 0)
            os.put(' ');

        if (precision == -1)
        {
            os.precision(6);
        }
        else if (precision == 0)
        {
            number = std::round(number);
        }

        os << number;
    }

    void put_str(std::ostream& os, red::wstring_view str) const {
        std::string tmp;

        auto& f = std::use_facet<std::ctype<wchar_t>>(os.getloc());
        tmp.append(str.size(), '\0');
        f.narrow(str.data(), str.data()+str.size(), '?', &tmp[0]);

        put_str(os, tmp);
    }

    void put_str(std::ostream& os, red::string_view str) const {
        if (precision > 0)
        {
            str = str.substr(0, precision);
        }

        os << str;
    }

    template<class I>
    using is_int = std::enable_if_t<std::is_integral<I>::value>;

    template<typename T, class = is_int<T>>
    void put_int(std::ostream& os, T val) const
    {
        const auto posfield = (flags & arg_flags::posfield);

        bool skip = false;
        if (precision <= 0 && val == 0)
        {
            if (bitmask::has_all(iosflags, ios::oct | ios::showbase))
            {
                // for octal, if both the converted value and the precision are ​0​, single ​0​ is written.
                os << 0;
                skip = true;
            }
            else if (posfield != arg_flags::showpos)
            {
                skip = true;
            }
        }

        if (posfield == arg_flags::blankpos && val >= 0 && fieldw < 2)
            os.put(' ');

        if (skip)
            return;

        if (precision > 0)
        {
            auto remainingW = std::max(fieldw - precision, 0);
            if (remainingW > 0)
            {
                char s = ' ';
                os.width(remainingW);
                os.fill(s);
                os << s;
            }

            os.width(precision);
            os.fill('0');
        }

        os << val;
    }

};

auto& operator << (std::ostream& os, const printf_arg& arg) {
    return arg.put(os);
}

#ifdef __GNUC__
using va_list_ref = va_list;
#else
using va_list_ref = va_list&;
#endif



static auto parsefmt(printf_arg& info, va_list_ref va) -> size_t;

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

auto red::polyloc::do_printf(string_view format, std::ostream& outs, size_t max, const std::locale& loc, va_list va) -> write_result
{
    write_result ret;

    std::ostringstream tmp;
    do_printf(format, tmp, loc, va);
    auto tmpstr = tmp.str();
    ret.chars_needed = tmpstr.size();

    if (ret.chars_needed <= max)
    {
        ret.chars_written = tmpstr.size();
        outs << tmpstr;
    }
    else if (max == 0)
    {
        ret.chars_written = 0;
    }
    else
    {
        auto to_be_writen = string_view(tmpstr).substr(0, max - 1);
        ret.chars_written = to_be_writen.size();
        outs << to_be_writen;
    }

    return ret;
}

static size_t parseflags(printf_arg& info)
{
    red::string_view spec = info.fmtspec;

    assert(spec[0] == FMT_START);
    size_t i = 0, p = 1;

    while (i = spec.find_first_of(FMT_FLAGS) != std::string::npos)
    {
        switch (spec[i])
        {
        case '-': // left justify
            info.iosflags |= ios::left;
            break;
        case '+': // show positive sign
            bm::setm(info.flags, arg_flags::showpos, arg_flags::posfield);
            break;
        case ' ': // Use a blank to prefix the output value if it is signed and positive, 
            bm::setm(info.flags, arg_flags::blankpos, arg_flags::posfield);
            break;
        case '#':
            info.flags |= arg_flags::altform;
            break;
        case '0':
            if (i == 1)
                info.fill = '0';
            else {
                // not a flag, but part of the width spec
                goto after;
            }
            break;
        default:
            goto after;
        }

        spec.remove_prefix(i);
        p += i;
    }

after:
    if ((info.flags & arg_flags::posfield) == arg_flags::showpos)
    {
        info.iosflags |= ios::showpos;
    }
    
    return p;
}

size_t parsefmt(printf_arg& info, va_list_ref va)
{
    auto constexpr npos = std::string::npos;
    const auto& locale = info.locale;

    auto i = parseflags(info);

    // get field width (optional)
    if (info.fmtspec[i] == FMT_FROM_VA)
    {
        info.fieldw = va_arg(va, int);
        i++;
    }
    else if (std::isdigit(info.fmtspec[i], locale))
    {
        size_t converted;
        try {
            info.fieldw = std::stoi(info.fmtspec.substr(i), &converted);
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
            info.precision = va_arg(va, int);
            i++;
        }
        else if (std::isdigit(info.fmtspec[i], locale)) try
        {
            size_t converted;
            info.precision = std::stoi(info.fmtspec.substr(i), &converted);
            i += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
    }

    // handle size overrides (optional)
    // this decides between int32/int64, float/double
    bool auto_size = false;
    auto prev = i;
    i = info.fmtspec.find_first_of(FMT_SIZES, i);
    if (i != npos)
    {
        switch (info.fmtspec[i++])
        {
        case 'h': // halfwidth
            /*info.flags = arg_flags::narrow;*/
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
        auto_size = true;
    }

    // handle type, extract properties and value
    prev = i;
    i = info.fmtspec.find_first_of(FMT_TYPES, i);
    if (i != npos)
    {
        switch (info.fmtspec[i])
        {
        case 'C': // wide char TODO
            info.flags |= arg_flags::wide;
        case 'c': // char
            info.type = arg_type::char_t;
#ifdef __GNUC__
            {
                // special needs code for a special needs compiler
                int ass = va_arg(va, int);
                info.value = ass;
            }
#else
            info.value = va_arg(va, char);
#endif
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
            //info.iosflags |= ios::hexfloat;
            info.iosflags |= (ios::fixed | ios::scientific);
            info.type = arg_type::float_t;

            goto common_float;
        case 'p': // pointer
            info.iosflags |= ios::hex;
            info.type = arg_type::pointer;
            info.value = va_arg(va, void*);
            break;
        case 'S': // wide string TODO
            info.flags |= arg_flags::wide;
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

            if (bm::has(info.flags, arg_flags::altform)) {
                info.iosflags |= ios::showbase;
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

            if (bm::has(info.flags, arg_flags::altform)) {
                info.iosflags |= ios::showbase;
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
            if (auto_size) {
                info.flags |= arg_flags::wide;
            }
            if (bm::has(info.flags, arg_flags::altform)) {
                info.iosflags |= ios::showpoint;
            }

            auto a = va_arg(va, double);
            info.flags |= a >= 0 ? arg_flags::is_signed : arg_flags::is_unsigned;
            info.value = a;
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
