# CMakeList.txt : CMake project for polylocale, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("polylocale")

if(MSVC)
	add_compile_options(-Zc:__cplusplus)
endif()

# Add source to this project's executable.
add_library(polylocale polylocale.cpp polylocale.h "printf.cpp" "polyloc.hpp")

# TODO: Add tests and install targets if needed.
find_package(Boost 1.70 REQUIRED COMPONENTS iostreams)
find_package(Catch2 CONFIG REQUIRED)

target_link_libraries(polylocale PRIVATE Boost::iostreams Boost::boost)

add_executable(tester test.cpp)
target_link_libraries(tester PRIVATE polylocale Catch2::Catch2)