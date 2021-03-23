#pragma once

#include <string_view>
#include <boost/tokenizer.hpp>

namespace polyloc
{
    struct fmt_separator
    {
        using token_type = std::string_view;
        using iterator = std::string_view::const_iterator;

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

    namespace moreflags { enum Enum
    {
        altform =   0b00001,
        zerofill =  0b00010,
        blankpos =  0b00100,
        narrow =    0b01000,
        wide =      0b10000
    };}

    // %[flags][width][.precision][size]type
    struct fmtspec_t
    {
        enum special_values { VAL_VA = -2, VAL_AUTO };
        
        // the full format specefier
        std::string_view fmt;
        
        // values
        int field_width = VAL_AUTO, precision = VAL_AUTO;
        long iosflags = 0, moreflags = 0;
        fmttype type = fmttype::error;

        explicit operator bool() const {
            return type != fmttype::error;
        }
    };

    fmtspec_t parsefmt(std::string_view spec);
}

// assign_or_plus_equal specialization for string_view
// https://github.com/boostorg/tokenizer/issues/15
template<>
struct boost::tokenizer_detail::assign_or_plus_equal<std::random_access_iterator_tag>
{
    template<class Iterator, typename char_t>
    static void assign(Iterator b, Iterator e, std::basic_string_view<char_t>& t)
    {
        t = { std::addressof(*b), static_cast<size_t>(std::distance(b, e)) };
    }
};