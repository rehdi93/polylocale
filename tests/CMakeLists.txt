# tests
find_package(Catch2 CONFIG REQUIRED)
find_package(Boost 1.70 REQUIRED)

add_executable(tester tests.cpp)
target_link_libraries(tester PRIVATE polylocale Catch2::Catch2 Boost::boost)
target_include_directories(tester PRIVATE ${CMAKE_SOURCE_DIR}/src)