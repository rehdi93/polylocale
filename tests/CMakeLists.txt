# tests
add_executable(tester tests.cpp)

find_package(Catch2 CONFIG REQUIRED)
target_link_libraries(tester PRIVATE polylocale Catch2::Catch2)
target_include_directories(tester PRIVATE ${CMAKE_SOURCE_DIR}/src)

# HACK
if(Boost_FOUND AND NOT Boost_iostreams_FOUND)
	target_include_directories(tester PUBLIC ${Boost_INCLUDE_DIRS})
	target_link_libraries(tester PUBLIC ${Boost_LIBRARIES})
else()
	target_link_libraries(tester PRIVATE Boost::boost)
endif()

