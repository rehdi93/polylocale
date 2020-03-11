// polyimpl.h

#if __cplusplus >= 201703L
#	include <string_view>
#else
#	include "boost/utility/string_view.hpp"
#endif

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

}