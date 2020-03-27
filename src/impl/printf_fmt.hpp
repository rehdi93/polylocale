#pragma once

#include "polyimpl.h"
#include "boost/tokenizer.hpp"

namespace red::polyloc
{
    bool isfmtflag(char ch, bool zero = true, bool space = true);
    bool isfmttype(char ch);
    bool isfmtsize(char ch);
    bool isfmtchar(char ch, bool digits = true);

    //template <class TokFunc=boost::char_separator<char>, class It=std::string::const_iterator, class Type=std::string>
    //struct tokenizer : boost::tokenizer<TokFunc, It, Type>
    //{
    //    using function = TokFunc;
    //    using boost::tokenizer<TokFunc, It, Type>::tokenizer;
    //};

    struct fmt_separator
    {
        using iterator = std::string::const_iterator;

        bool operator() (iterator& next, iterator end, std::string& token);
        void reset();

    private:
        bool m_infmt = false;
    };

    using fmt_tokenizer = boost::tokenizer<fmt_separator, fmt_separator::iterator>;

    // %[flags][width][.precision][size]type
    struct fmtspec_t
    {
        red::string_view fmt;
        red::string_view flags, length_mod;
        int field_width = -1, precision = -1;
        char conversion = '\030';

        static const int VAL_VA, VAL_AUTO = -1;
    };

    fmtspec_t parsefmt(string_view fmt, std::locale const& locale);
}