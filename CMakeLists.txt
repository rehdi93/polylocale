# CMakeList.txt : CMake project for polylocale
#
cmake_minimum_required (VERSION 3.12.4)

project ("polylocale" VERSION 0.1.5)

if(MSVC)
	add_compile_options(-Zc:__cplusplus)
endif()

option(ENABLE_TESTING "Enable Test Builds" ON)
option(POLYLOC_UNDECORATED "Define names w/o poly_* prefix (#define newlocale poly_newlocale)")

find_package(Boost 1.70 REQUIRED COMPONENTS iostreams)

add_subdirectory(src)

if(ENABLE_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()