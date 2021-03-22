# tests
add_executable(tests tests.cpp)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(tests PRIVATE polylocale Catch2::Catch2)
target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(tests PRIVATE Boost::boost)

if(MSVC)
	target_compile_definitions(tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()