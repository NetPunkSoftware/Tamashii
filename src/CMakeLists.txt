set(LIB_SOURCES
    container/spmc_queue.hpp
    core/fiber.hpp
    core/fiber.cpp
    pool/fiber_pool.hpp
    pool/fiber_pool.cpp)

# LIBRARY
add_library(fibers STATIC ${LIB_SOURCES})
target_compile_features(fibers PUBLIC cxx_std_20)
target_include_directories(fibers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_definitions(fibers PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
    target_link_libraries(fibers PUBLIC m)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(fibers PUBLIC _UNICODE UNICODE)
    target_compile_options(fibers PUBLIC /MP /Zc:__cplusplus)
endif()

# EXECUTABLE
add_executable(fibers_test main.cpp)
target_link_libraries(fibers_test PRIVATE fibers)
