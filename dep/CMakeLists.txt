include(FetchContent)
find_package(Git REQUIRED)


find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS context)
target_include_directories(fibers SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_link_directories(fibers PUBLIC ${Boost_LIBRARY_DIRS})
target_link_libraries(fibers PUBLIC Boost::context)

