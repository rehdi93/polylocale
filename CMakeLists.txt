# CMakeList.txt : CMake project for polylocale
#
cmake_minimum_required (VERSION 3.8)

project ("polylocale" VERSION 0.0.1)

if(MSVC)
	add_compile_options(-Zc:__cplusplus)
endif()

option(ENABLE_TESTING "Enable Test Builds" ON)
option(POLYLOC_UNDECORATED "Define names w/o poly_* prefix (#define newlocale poly_newlocale)")

# TODO: if the C library conforms to X/Open, default to no undecorated names

find_package(Boost 1.70 REQUIRED COMPONENTS iostreams)

add_subdirectory(src)

if(ENABLE_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()