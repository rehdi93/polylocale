#pragma once

#include "polyimpl.h"
#include <boost/tokenizer.hpp>

namespace red::polyloc
{
    bool isfmtflag(char ch) noexcept;
    bool isfmttype(char ch) noexcept;
    bool isfmtlength(char ch) noexcept;
    bool isfmtchar(char ch);

    struct fmt_separator
    {
        using iterator = red::string_view::iterator;

        bool operator() (iterator& next, iterator end, red::string_view& token);
        void reset() {}

    private:
    };

    using fmt_tokenizer = boost::tokenizer<fmt_separator, fmt_separator::iterator, red::string_view>;

    enum class fmtflag
    {
        left =     0b00001,
        showpos =  0b00010,
        blankpos = 0b00100,
        altform =  0b01000,
        zerofill = 0b10000,
    };

    enum class repr
    {
        none,
        decimal,

        hex,
        oct,

        fixed,
        scientific,
        deffloat,
        hexfloat,
        
        reprcount
    };

    enum class lenmod
    {
        narrow,
        wide
    };

    enum class fmttype
    {
        char_,
        string,
        sint,
        uint,
        floatpt,
        pointer
    };

    // %[flags][width][.precision][size]type
    struct fmtspec_t
    {
        static const int VAL_VA = -2, VAL_AUTO = -1;
        
        bool error = false;

        // the full format specefier
        red::string_view fmt;
        // views into fmt
        //red::string_view flags, length_mod;
        
        // values
        int field_width = VAL_AUTO, precision = VAL_AUTO;
        //char conversion = '\030'; // type

        unsigned long flags;
        lenmod lengthmod{};
        repr style;
        fmttype type;
        bool uppercase{};
    };

    fmtspec_t parsefmt(red::string_view spec);
}