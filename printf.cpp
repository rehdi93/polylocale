#include "polyloc.hpp"

#include <sstream>
#include <iomanip>
#include <cstddef>
#include <array>
#include <vector>
#include <cassert>


using std::ios;

// HELPERS
namespace
{

constexpr auto STREAM_MAX = std::numeric_limits<std::streamsize>::max();

constexpr char FMT_START    = '%';
constexpr char FMT_ALL[]    = "%.*" "-+ #0" "hljztI" "CcudioXxEeFfGgAapSsn";
constexpr char FMT_CHARS[]  = "%.*" "-+#0"  "hljztI" "CcudioXxEeFfGgAapSsn";
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

// holds info about the current spec being parsed
struct printf_arg
{
    enum arg_t
    {
        t_char = 1,
        t_integer,
        t_floating_pt,
        t_pointer,
        t_string,
        t_byteswriten = -1,
        t_invalid = 0
    };

    enum arg_sizem
    {
        m_none,
        narrow,
        wide
    };

    enum arg_sign
    {
        s_none,
        s_signed,
        s_unsigned
    };
    
    // %[flags][width][.precision][size]type
    // worst case scenerio sizes:
    // 1 + 4  +  10 +  1 +  10  +   3  + 1 = 30
    // 10 is from INT_MAX
    explicit printf_arg(const std::string& stor)
        : fmtspec(stor)
    {}

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


    arg_t type = t_invalid;
    arg_sizem sizem = m_none;
    arg_sign sign = s_none;
    bool blankpfx = false;
    std::string const& fmtspec;
    values_u value;

    void setprops(arg_t t, arg_sign s, arg_sizem m) {
        sizem = m;
        type = t;
        sign = s;
    }
    void setprops(arg_t t, arg_sign s) {
        type = t;
        sign = s;
    }
    void setprops(arg_t t, bool signd) {
        type = t;
        sign = signd ? s_signed : s_unsigned;
    }

    template<typename T>
    void blankpos(std::ostream& os, T val) const
    {
        if (blankpfx && val >= 0) {
            os << ' ';
        }
        os << val;
    }

    // sets printf_arg::sizem to wide if sizeof(T)==8, otherwise it's set to narrow
    template<typename T>
    constexpr void set_sizem() {
        sizem = sizeof(T) == 8 ? wide : narrow;
    }

    // print value
    auto& put(std::ostream& os) const
    {
        switch (this->type)
        {
        case t_char:
            if (sizem == wide)
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
        case t_integer:
            if (this->sizem == wide)
            {
                if (this->sign == s_signed) {
                    blankpos(os, int64_t(value.integer));
                }
                else
                    os << uint64_t(value.unsigned_integer);
            }
            else
            {
                if (this->sign == s_signed) {
                    blankpos(os, int32_t(value.integer));
                }
                else
                    os << uint32_t(value.unsigned_integer);
            }

            break;
        case t_floating_pt:
            if (this->sizem == narrow) {
                blankpos(os, float(value.floatpt));
            }
            else {
                blankpos(os, value.floatpt);
            }
            break;
        case t_pointer:
            os << value.pointer;
            break;
        case t_string:
            if (sizem == wide) {
                auto& f = std::use_facet<std::ctype<wchar_t>>(os.getloc());
                red::wstring_view view = value.wstring;
                std::string tmp(view.size(), '\0');
                f.narrow(view.data(), view.data() + view.size(), '?', &tmp[0]);

                os << tmp;
            } else {
                os << value.string;
            }
            break;
        case t_byteswriten:
            // not implemented
        default:
            // invalid, print fmt as-is
            os << fmtspec;
            break;
        }

        return os;
    }

    // print the state of this
    auto& putdbg(std::ostream& os) const
    {
        const auto fl = os.flags();
        os << "state of printf_arg @ "<<std::hex<<std::showbase<<this;
        os.flags(fl);

        os << std::dec;
        os << "\n\t" "fmt = " << fmtspec;
        os << "\n\t" "type = ";
        switch (type)
        {
        case t_char:
            os << "char";
            os << " (" << value.character << ")";
            break;
        case t_integer:
            os << "integer";
            os << " (" << value.integer << ")";
            break;
        case t_floating_pt:
            os << "floating point";
            os << " (" << value.floatpt << ")";
            break;
        case t_pointer:
            os << "pointer";
            os << " (" << std::hex << value.pointer << ")";
            break;
        case t_string:
            os << "string";
            os << " (" << value.string << ")";
            break;
        case t_byteswriten:
            os << "byteswriten";
            os << " (" << std::hex << value.pointer << ")";
            break;
        case t_invalid:
            os << "invalid (NA)";
            break;
        default:
            os << "???";
            break;
        }

        os << "\n\t" "sign = ";
        switch (sign)
        {
        case s_signed: os << "signed"; break;
        case s_unsigned: os << "unsigned"; break;
        default: os << "???"; break;
        }

        os << "\n\t" "sizem = ";
        switch (sizem)
        {
        case m_none: os << "none"; break;
        case narrow: os << "narrow"; break;
        case wide: os << "wide"; break;
        default: os << "???"; break;
        }

        os << "\n\t" "blankpfx = "<<std::boolalpha<<blankpfx;

        os.flags(fl);
        return os;
    }
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


auto& operator << (std::ostream& os, const printf_arg& arg) {
    return arg.put(os);
}

static auto parsefmt(std::ostream& outs, printf_arg& info, va_list& va) -> size_t;

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
            stream_state state = outs;

            const auto end = format.find_first_of(FMT_TYPES, i + 1) + 1;
            spec.assign(format.data() + i, end - i);

            // %[flags][width][.precision][size]type
            printf_arg info{ spec };
            parsefmt(outs, info, va);
            outs << info;

            i += spec.size();
            format.remove_prefix(i);
        }
    }
}


size_t parsefmt(std::ostream& outs, printf_arg& info, va_list& va)
{
    auto constexpr npos = std::string::npos;
    const auto locale = outs.getloc();
    
    size_t i = 0, prev = 0;

    i = info.fmtspec.find_first_of(FMT_FLAGS, i);
    if (i != npos)
    {
        switch (info.fmtspec[i])
        {
        case '-': // left justify
            outs << std::left;
            break;
        case '+': // show positive sign
            outs.setf(ios::showpos);
            break;
        case ' ': // Use a blank to prefix the output value if it is signed and positive, ignored if '+' is also set
            if ((outs.flags() & ios::showpos) != 0) {
                info.blankpfx = true;
            }
            // ...
            break;
        case '#':
            outs.setf(ios::showbase);
            break;
        case '0': // zero fill
            outs.fill('0');
            break;
        }
        i++;
    }
    else i = prev;

    // get field width (optional)
    if (info.fmtspec[i] == FMT_FROM_VA)
    {
        auto fw = va_arg(va, uint32_t);
        outs.width(fw);
        i++;
    }
    else if (std::isdigit(info.fmtspec[i], locale))
    {
        int fw; size_t converted;
        try {
            fw = std::stoi(info.fmtspec.substr(i), &converted);
            outs.width(fw);
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
            auto pr = va_arg(va, uint32_t);
            outs.precision(pr);
            i++;
        }
        else if (std::isdigit(info.fmtspec[i], locale)) try
        {
            int pr; size_t converted;
            pr = std::stoi(info.fmtspec.substr(i), &converted);
            outs.precision(pr);
            i += converted;
        }
        catch (const std::exception&) {
            // no conversion took place
        }
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
            info.sizem = info.narrow;
            if (info.fmtspec[i] == 'h')
                i++; // QUARTERWIDTH
            break;
        case 'l':  // are we 64-bit (unix style)
            if (info.fmtspec[i] == 'l') {
                info.sizem = info.wide;
                i++;
            }
            else {
                info.set_sizem<long>();
            }
        case 'j': // are we 64-bit on intmax_t? (c99)
            info.set_sizem<size_t>();
            break;
        case 'z': // are we 64-bit on size_t or ptrdiff_t? (c99)
        case 't':
            info.set_sizem<ptrdiff_t>();
            break;
        case 'I': // are we 64-bit (msft style)
            if (info.fmtspec[i] == '6' && info.fmtspec[i+1] == '4') {
                info.sizem = info.wide;
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
        info.sizem = info.m_none;
    }

    // handle type, extract properties and value
    prev = i;
    i = info.fmtspec.find_first_of(FMT_TYPES, i);
    if (i != npos)
    {
        switch (info.fmtspec[i])
        {
        case 'C': // wide char TODO
            info.sizem = info.wide;
            info.type = info.t_char;
            info.value = va_arg(va, wchar_t);
            break;
        case 'c': // char
            info.type = info.t_char;
            info.value = va_arg(va, char);
            break;
        case 'u': // Unsigned decimal integer
            outs << std::dec;
            info.setprops(info.t_integer, info.s_unsigned);

            goto common_uint;
        case 'd': // Signed decimal integer
        case 'i':
            outs << std::dec;
            info.setprops(info.t_integer, info.s_signed);

            goto common_int;
        case 'o': // Unsigned octal integer
            outs << std::oct;
            info.setprops(info.t_integer, info.s_unsigned);

            goto common_uint;
        case 'X': // Unsigned hexadecimal integer w/ uppercase letters
            outs.setf(ios::uppercase);
        case 'x': // Unsigned hexadecimal integer w/ lowercase letters
            outs << std::hex;
            info.setprops(info.t_integer, info.s_unsigned);
            
            goto common_uint;
        case 'E': // float, scientific notation
            outs.setf(ios::uppercase);
        case 'e': {
            outs << std::scientific;
            goto common_float;
        } break;
        case 'F': // float, fixed notation
            outs.setf(ios::uppercase);
        case 'f': {
            outs << std::fixed;
            goto common_float;
        } break;
        case 'G': // float, general notation
            outs.setf(ios::uppercase);
        case 'g': {
            outs << std::defaultfloat;
            goto common_float;
        } break;
        case 'A': // hex float
        case 'a': {
            outs << std::hexfloat;
            goto common_float;
        } break;
        case 'p': // pointer
            outs << std::hex;
            info.type = info.t_pointer;
            info.value = va_arg(va, void*);
            break;
        case 'S': // wide string TODO
            info.sizem = info.wide;
            info.type = info.t_string;
            info.value = va_arg(va, wchar_t*);
            break;
        case 's': // string
            info.type = info.t_string;
            info.value = va_arg(va, char*);
            break;
        case 'n': // weird write-bytes specifier (not implemented)
            info.value = va_arg(va, void*);
            info.type = info.t_byteswriten;
            break;

        // https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=vs-2019#argument-size-specification
        // integer args w/o size spec are treated as 32bit
        common_int:
            if (info.sizem == info.m_none) {
                info.sizem = info.narrow;
            }

            if (info.sizem == info.wide) {
                info.value = va_arg(va, int64_t);
            }
            else {
                info.value = va_arg(va, int32_t);
            }
            break;
        common_uint:
            if (info.sizem == info.m_none) {
                info.sizem = info.narrow;
            }

            if (info.sizem == info.wide) {
                info.value = va_arg(va, uint64_t);
            }
            else {
                info.value = va_arg(va, uint32_t);
            }
            break;
        // floating point types w/o size spec are treated as 64bit
        common_float:
            if (info.sizem == info.m_none) {
                info.sizem = info.wide;
            }

            if (info.sizem == info.wide) {
                auto a = va_arg(va, double);
                info.setprops(info.t_floating_pt, a >= 0);
                info.value = a;
            }
            else {
                auto a = va_arg(va, float);
                info.setprops(info.t_floating_pt, a >= 0);
                info.value = a;
            }

            break;
        }

        return i + 1;
    }
    else
    {
        // couldn't find type specifier
        fail: info.setprops(info.t_invalid, info.s_signed, info.m_none);
        return npos;
    }
}
