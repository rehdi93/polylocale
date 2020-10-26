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

    // %[flags][width][.precision][size]type
    struct fmtspec_t
    {
        static constexpr int VAL_VA = -(int)'*', VAL_AUTO = -1;

        // the full format specefier
        red::string_view fmt;

        red::string_view flags, length_mod;
        int field_width = VAL_AUTO, precision = VAL_AUTO;
        char conversion = '\030';

        bool error = false;
    };

    fmtspec_t parsefmt(red::string_view spec);
}