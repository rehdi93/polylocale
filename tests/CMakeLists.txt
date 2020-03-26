# tests
add_executable(tester tests.cpp)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(tester PRIVATE polylocale Catch2::Catch2)
target_include_directories(tester PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(tester PRIVATE Boost::boost)
