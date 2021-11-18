include(FetchContent)
find_package(Git REQUIRED)


find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS context)
target_include_directories(fibers SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_link_directories(fibers PUBLIC ${Boost_LIBRARY_DIRS})
target_link_libraries(fibers PUBLIC Boost::context)


FetchContent_Declare(
    moodycamel
    GIT_REPOSITORY https://github.com/cameron314/concurrentqueue
    GIT_TAG        feb72d0ed6eb96e7eb6d9d840e51b1cd32c27b0d
)

FetchContent_GetProperties(moodycamel)
if(NOT moodycamel_POPULATED)
    FetchContent_Populate(moodycamel)

    add_subdirectory(${moodycamel_SOURCE_DIR} ${moodycamel_BINARY_DIR})
endif()

target_link_libraries(fibers PUBLIC concurrentqueue )

