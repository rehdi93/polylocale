#include "polyprintf.h"

#include <sstream>
#include <iomanip>
#include <cstddef>
#include <array>
#include <vector>
#include <cassert>

using std::ios;

// HELPERS


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

    enum arg_t_w
    {
        m_none,
        narrow,
        wide
    };

    enum arg_t_sign
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
        void set(intmax_t i) { integer = i; }
        void set(uintmax_t i) { unsigned_integer = i; }
        void set(long double d) { floatpt = d; }
        void set(void* p) { pointer = p; }
        // ----

        template<typename T>
        values_u(T value) {
            set(value);
        }

        values_u() = default;
    };


    arg_t type = t_invalid;
    arg_t_w mod = m_none;
    arg_t_sign sign = s_none;
    bool blankpfx = false;
    std::string const& fmtspec;
    values_u value;

    void setprops(arg_t t, arg_t_sign s, arg_t_w m) {
        mod = m;
        type = t;
        sign = s;
    }
    void setprops(arg_t t, arg_t_sign s) {
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

    // print value
    auto& put(std::ostream& os) const
    {
        switch (this->type)
        {
        case t_char:
            os << this->value.character;
            break;
        case t_integer:
            if (this->mod == narrow)
            {
                if (this->sign == s_signed) {
                    blankpos(os, int32_t(value.integer));
                }
                else
                    os << uint32_t(value.unsigned_integer);
            }
            else if (this->mod == wide)
            {
                if (this->sign == s_signed) {
                    blankpos(os, int64_t(value.integer));
                }
                else
                    os << uint64_t(value.unsigned_integer);
            }
            break;
        case t_floating_pt:
            if (this->mod == wide) {
                blankpos(os, value.floatpt);
            }
            else {
                blankpos(os, double(value.floatpt));
            }
            break;
        case t_pointer:
            os << value.pointer;
            break;
        case t_string:
            os << value.string;
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

        os << "\n\t" "mod = ";
        switch (mod)
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


// ctype that treats '%' as whitespace
struct pf_ctype : std::ctype<char>
{
private:
    static mask* make_table()
    {
        static std::vector<mask> table{ classic_table(), classic_table() + table_size };
        table['%'] |= space;

        return &table[0];
    }

protected:

public:
    pf_ctype() : ctype(make_table()) {}
};

struct stream_state
{
    ios::fmtflags flags;
    char fill;
    std::streamsize width, precision;
    //std::locale locale;

    stream_state(std::ios const& ios)
        : flags(ios.flags()), fill(ios.fill()),
        width(ios.width()), precision(ios.precision())
        //,locale(ios.getloc())
    {}

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

template<class R1, class R2>
auto find_first_of(R1&& a, R2&& b) {
    using namespace std;
    return std::find_first_of(begin(a), end(a), begin(b), end(b));
}

static void parsefmt(std::ostream& outs, printf_arg& info, va_list& va);

std::ostream& red::do_printf(std::istream& fmtss, std::ostream& outs, const std::locale& locale, va_list va)
{
    if (fmtss.eof())
        return outs;

    outs.imbue(locale);
    fmtss.imbue(locale);
    auto locale_sep //= std::locale{ locale, new pf_ctype };
                    = std::locale(std::locale::classic(), new pf_ctype);

    std::string spec; spec.reserve(30);

    for (std::string text; std::getline(fmtss, text, '%');)
    {
        // found %
        stream_state ss_state = outs;
        outs << text;

        // %[flags][width][.precision][size]type
        printf_arg info{ spec };
        
        if (fmtss.eof()) {
            // end
        }
        else if (fmtss.peek() == '%') {
            // escaped %% char
            outs << fmtss.get();
        }
        else {
            fmtss.imbue(locale_sep);
            fmtss >> spec;
            fmtss.imbue(locale);

            parsefmt(outs, info, va);

            outs << info;
        }

        ss_state.restore(outs);
    }

    return outs;
}

void parsefmt(std::ostream& outs, printf_arg& info, va_list& va)
{
    const char chFlags[] = "-+ #0";
    const char chSizes[] = "hljztI";
    const char chType[] = "CcudioXxEeFfGgAapSsn";
    auto constexpr npos = std::string::npos;
    const auto locale = outs.getloc();
    
    size_t i = 0;
    auto prev = i;

    i = info.fmtspec.find_first_of(chFlags, i);
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
            if ((outs.flags() & ios::showpos) != ios::showpos) {
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
    if (info.fmtspec[i] == '*')
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
    if (info.fmtspec[i] == '.')
    {
        i++;
        if (info.fmtspec[i] == '*')
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
    // this decides between int32/int64, double/long double
    prev = i;
    i = info.fmtspec.find_first_of(chSizes, i);
    if (i != npos)
    {
        switch (info.fmtspec[i++])
        {
        case 'h': // halfwidth
            info.mod = info.narrow;
            if (info.fmtspec[i] == 'h')
                i++; // QUARTERWIDTH
            break;
        case 'l':  // are we 64-bit (unix style)
            if (info.fmtspec[i] == 'l') {
                info.mod = info.wide;
                i++;
            }
            else {
                info.mod = sizeof(long) == 8 ? info.wide : info.narrow;
            }
        case 'j': // are we 64-bit on intmax_t? (c99)
            info.mod = sizeof(size_t) == 8 ? info.wide : info.narrow;
            break;
        case 'z': // are we 64-bit on size_t or ptrdiff_t? (c99)
        case 't':
            info.mod = sizeof(ptrdiff_t) == 8 ? info.wide : info.narrow;
            break;
        case 'I': // are we 64-bit (msft style)
            if (info.fmtspec[i] == '6' && info.fmtspec[i+1] == '4') {
                info.mod = info.wide;
                i++;
            }
            else if (info.fmtspec[i] == '3' && info.fmtspec[i+1] == '2') {
                info.mod = sizeof(void*) == 8 ? info.wide : info.narrow;
                i++;
            }
            break;
        }
    }
    else {
        i = prev;
        info.mod = info.m_none;
    }

    // handle type, extract properties and value
    prev = i;
    i = info.fmtspec.find_first_of(chType, i);
    if (i != npos)
    {
        switch (info.fmtspec[i])
        {
        case 'C': // wide char TODO
            info.mod = info.wide;
            info.type = info.t_char;
            info.value = va_arg(va, wchar_t);
            break;
        case 'c': // char
            info.type = info.t_char;
            info.value.set(va_arg(va, char));
            break;
        case 'u': // Unsigned decimal integer
            outs << std::dec;
            info.setprops(info.t_integer, info.s_unsigned);
            info.value.set(va_arg(va, uintmax_t));
            break;
        case 'd': // Signed decimal integer
        case 'i':
            outs << std::dec;
            info.setprops(info.t_integer, info.s_signed);
            info.value.set(va_arg(va, intmax_t));
            break;
        case 'o': // Unsigned octal integer
            outs << std::oct;
            info.setprops(info.t_integer, info.s_unsigned);
            info.value.set(va_arg(va, uintmax_t));
            break;
        case 'X': // Unsigned hexadecimal integer w/ uppercase letters
            outs.setf(ios::uppercase);
        case 'x': // Unsigned hexadecimal integer w/ lowercase letters
            outs << std::hex;
            info.setprops(info.t_integer, info.s_unsigned);
            info.value.set(va_arg(va, uintmax_t));
            break;
        case 'E': // float, scientific notation
            outs.setf(ios::uppercase);
        case 'e': {
            outs << std::scientific;
            auto a = va_arg(va, long double);
            info.setprops(info.t_floating_pt, a >= 0);
            info.value.set(a);
        } break;
        case 'F': // float, fixed notation
            outs.setf(ios::uppercase);
        case 'f': {
            outs << std::fixed;
            auto a = va_arg(va, long double);
            info.setprops(info.t_floating_pt, a >= 0);
            info.value = a;
        } break;
        case 'G': // float, general notation
            outs.setf(ios::uppercase);
        case 'g': {
            outs << std::defaultfloat;
            auto a = va_arg(va, long double);
            info.setprops(info.t_floating_pt, a >= 0);
            info.value.set(a);
        } break;
        case 'A': // hex float
        case 'a': {
            outs << std::hexfloat;
            auto a = va_arg(va, long double);
            info.setprops(info.t_floating_pt, a >= 0);
            info.value.set(a);
        } break;
        case 'p': // pointer
            outs << std::hex;
            info.type = info.t_pointer;
            info.value.set(va_arg(va, void*));
            break;
        case 'S': // wide string TODO
            info.mod = info.wide;
            info.type = info.t_string;
            info.value = va_arg(va, wchar_t*);
            break;
        case 's': // string
            info.type = info.t_string;
            info.value.set(va_arg(va, char*));
            break;
        case 'n': // weird write-bytes specifier (not implemented)
            info.value.set(va_arg(va, void*));
            info.type = info.t_byteswriten;
            break;
        default: // unknown
            goto fail;
        }
        
        // https://docs.microsoft.com/en-us/cpp/c-runtime-library/format-specification-syntax-printf-and-wprintf-functions?view=vs-2019#argument-size-specification
        if (info.type != info.t_invalid && info.mod == info.m_none)
        {
            switch (info.type)
            {
            case info.t_char:
            case info.t_integer:
                info.mod = info.narrow;
                break;
            case info.t_floating_pt:
                info.mod = info.wide;
                break;
            }
        }
    }
    else
    {
        // couldn't find type specifier
        fail: info.setprops(info.t_invalid, info.s_signed, info.m_none);
    }
}
