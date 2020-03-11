# CMakeList.txt : CMake project for polylocale
#
cmake_minimum_required (VERSION 3.8)

project ("polylocale" VERSION 0.0.1)

if(MSVC)
	add_compile_options(-Zc:__cplusplus)
endif()

option(ENABLE_TESTING "Enable Test Builds" ON)

add_subdirectory(src)

if(ENABLE_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()