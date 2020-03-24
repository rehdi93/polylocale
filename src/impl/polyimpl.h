// polyimpl.h
#pragma once

#if __cplusplus >= 201703L
#	include <string_view>
#else
#	include "boost/utility/string_view.hpp"
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


}