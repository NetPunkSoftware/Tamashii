set(LIB_SOURCES
    container/spmc_queue.hpp
    core/fiber.hpp
    core/fiber.cpp
    core/fiber_base.hpp
    core/fiber_base.cpp
    core/detail/fiber.hpp
    ext/channel.hpp
    ext/executor.hpp
    ext/executor.cpp
    ext/fiber_scmp.hpp
    ext/thread_scmp.hpp
    local/fiber_local.hpp
    local/thread_local.hpp
    pool/fiber_pool.hpp
    pool/fiber_pool.cpp
    synchronization/barrier.hpp
    synchronization/barrier.cpp
    synchronization/condition_variable.hpp
    synchronization/condition_variable.cpp
    synchronization/counter.hpp
    synchronization/counter.cpp
    synchronization/event.hpp
    synchronization/event.cpp
    synchronization/mutex.hpp
    synchronization/mutex.cpp
    synchronization/one_way_barrier.hpp
    synchronization/one_way_barrier.cpp
    synchronization/spinbarrier.hpp
    synchronization/spinbarrier.cpp
    synchronization/spinlock.hpp
    utils/badge.hpp)

# LIBRARY
add_library(tamashii STATIC ${LIB_SOURCES})
target_compile_features(tamashii PUBLIC cxx_std_20)
target_include_directories(tamashii PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(tamashii PUBLIC Threads::Threads)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_definitions(tamashii PUBLIC BOOST_ASIO_DISABLE_CONCEPTS)
    target_link_libraries(tamashii PUBLIC m)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_definitions(tamashii PUBLIC _UNICODE UNICODE)
    target_compile_options(tamashii PUBLIC /MP /Zc:__cplusplus)
endif()

# EXECUTABLE
add_executable(tamashii_test main.cpp)
target_compile_features(tamashii_test PUBLIC cxx_std_20)
target_link_libraries(tamashii_test PRIVATE tamashii)
