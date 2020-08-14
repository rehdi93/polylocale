#pragma once

#include "polyimpl.h"
#include <boost/tokenizer.hpp>

namespace red::polyloc
{
    bool isfmtflag(char ch, bool zero = true, bool space = true);
    bool isfmttype(char ch);
    bool isfmtsize(char ch);
    bool isfmtchar(char ch, bool digits = true);

    struct fmt_separator
    {
        using iterator = std::string::const_iterator;

        bool operator() (iterator& next, iterator end, std::string& token);
        void reset() {}

    private:
    };

    using fmt_tokenizer = boost::tokenizer<fmt_separator, fmt_separator::iterator>;

    // %[flags][width][.precision][size]type
    struct fmtspec_t
    {
        static constexpr int VAL_VA = -(int)'*', VAL_AUTO = -1;
        static constexpr char ILL_FORMED = '\030';

        // the full format specefier, maybe change to std::string?
        red::string_view fmt;

        red::string_view flags, length_mod;
        int field_width = -1, precision = -1;
        char conversion = ILL_FORMED;
    };

    fmtspec_t parsefmt(string_view fmt, std::locale const& locale);
    fmtspec_t parsefmt2(std::string const& spec);
}