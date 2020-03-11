# src
# public lib
add_library(polylocale polylocale.cpp polylocale.h)

add_library(polyimpl STATIC impl/printf.cpp impl/printf.hpp)
configure_file(impl/polyimpl.in.h impl/polyimpl.h)
target_include_directories(polyimpl PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

find_package(Boost 1.70 REQUIRED COMPONENTS iostreams)

target_link_libraries(polyimpl PUBLIC Boost::iostreams Boost::boost)
target_link_libraries(polylocale PRIVATE polyimpl)
