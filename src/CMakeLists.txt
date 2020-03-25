# src
add_library(polylocale 
	polylocale.cpp polylocale.h
	impl/printf.cpp impl/printf.hpp)
target_compile_features(polylocale PUBLIC cxx_std_17)

configure_file(config.h.in config.h)

target_include_directories(polylocale PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

# HACK
if(Boost_FOUND AND NOT Boost_iostreams_FOUND)
	target_include_directories(polylocale PRIVATE ${Boost_INCLUDE_DIRS})
	target_link_libraries(polylocale PRIVATE ${Boost_LIBRARIES})
else()
	target_link_libraries(polylocale PRIVATE Boost::iostreams Boost::headers)
endif()
