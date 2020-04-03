// polyimpl.h
#pragma once

#if __cplusplus >= 201703L
    #include <string_view>
#else
    #include "boost/utility/string_view.hpp"
#endif

#include <iosfwd>
#include "boost/iostreams/stream_buffer.hpp"
#include "boost/iostreams/device/array.hpp"


namespace red
{

#if __cplusplus >= 201703L
    using std::string_view;
    using std::wstring_view;
    using std::basic_string_view;
#else
    using boost::string_view;
    using boost::wstring_view;
    using boost::basic_string_view;
#endif

namespace io
{
    using namespace boost::iostreams;

    using array_read_buf = stream_buffer< array_source >;
    using array_buf = stream_buffer<array>;
}

    // string_view to long
    template <class StrView>
    inline long svtol(StrView sv, size_t* pos = 0, int base = 10)
    {
        using charT = typename StrView::value_type;

        charT* pend;
        auto pbeg = sv.data();
        long val;

        if constexpr (std::is_same_v<charT, char>) {
            val = strtol(pbeg, &pend, base);
        }
        else if constexpr (std::is_same_v<charT, wchar_t>) {
            val = wcstol(pbeg, &pend, base);
        }

        if (pbeg == pend) {
            throw std::invalid_argument("invalid svtol argument");
        }
        if (errno == ERANGE) {
            throw std::out_of_range("svtol argument out of range");
        }

        if (pos) {
            *pos = pend - pbeg;
        }

        return val;
    }

}