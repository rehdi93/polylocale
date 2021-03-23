#pragma once

#include "polyimpl.h"
#include <boost/tokenizer.hpp>

namespace polyloc
{
    struct fmt_separator
    {
        using token_type = std::string;
        using iterator = std::string::const_iterator;

        bool operator() (iterator& next, iterator end, token_type& token);
        void reset() {}

    private:
    };

    using fmt_tokenizer = boost::tokenizer<fmt_separator, fmt_separator::iterator, fmt_separator::token_type>;

    enum class fmttype
    {
        error=-1,
        char_,
        string,
        sint,
        uint,
        floatpt,
        pointer
    };

    namespace moreflags {
        enum Enum
        {
            altform =   0b00001,
            zerofill =  0b00010,
            blankpos =  0b00100,
            narrow =    0b01000,
            wide =      0b10000
        };
    }

    // %[flags][width][.precision][size]type
    struct fmtspec_t
    {
        static const int VAL_VA = -2, VAL_AUTO = -1;
        
        // the full format specefier
        red::string_view fmt;
        
        // values
        int field_width = VAL_AUTO, precision = VAL_AUTO;
        long iosflags = 0, moreflags = 0;
        fmttype type = fmttype::error;

        explicit operator bool() const {
            return type != fmttype::error;
        }
    };

    fmtspec_t parsefmt(red::string_view spec);
}
