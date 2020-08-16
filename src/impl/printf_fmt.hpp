#pragma once

#include "polyimpl.h"
#include <boost/tokenizer.hpp>

namespace red::polyloc
{
    bool isfmtflag(char ch);
    bool isfmttype(char ch);
    bool isfmtlength(char ch);
    bool isfmtchar(char ch);

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

        // the full format specefier, maybe change to std::string?
        red::string_view fmt;

        red::string_view flags, length_mod;
        int field_width = VAL_AUTO, precision = VAL_AUTO;
        char conversion = '\030';

        bool valid() const noexcept;

    private:
        constexpr bool isValidNum(int n) const {
            return n == VAL_AUTO || n == VAL_VA || n >= 0;
        }
    };

    fmtspec_t parsefmt(std::string const& spec);
}