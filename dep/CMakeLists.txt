include(FetchContent)
find_package(Git REQUIRED)



####################################################################################
# Deboost::Context
FetchContent_Declare(
    deboost_context
    GIT_REPOSITORY https://github.com/septag/deboost.context
    GIT_TAG        63476334fea6ccc4f2cc1de1d91257c3fe8b8fd3
)

FetchContent_GetProperties(deboost_context)
if(NOT deboost_context_POPULATED)
    FetchContent_Populate(deboost_context)

    add_subdirectory(${deboost_context_SOURCE_DIR} ${deboost_context_BINARY_DIR})
endif()

target_link_libraries(tamashii PUBLIC fcontext)



####################################################################################
# Preprocessor
FetchContent_Declare(
    boostorg_preprocessor
    GIT_REPOSITORY https://github.com/boostorg/preprocessor
    GIT_TAG        24abbdc9063116bdf97becab32bb6133a9a9407b
)

FetchContent_GetProperties(boostorg_preprocessor)
if(NOT boostorg_preprocessor_POPULATED)
    FetchContent_Populate(boostorg_preprocessor)

    add_subdirectory(${boostorg_preprocessor_SOURCE_DIR} ${boostorg_preprocessor_BINARY_DIR})
endif()

target_link_libraries(tamashii PUBLIC boost_preprocessor)



####################################################################################
# MoodyCamel::ConcurrentQueue
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

target_link_libraries(tamashii PUBLIC concurrentqueue)



####################################################################################
# Palanteer
set(PALANTEER_BUILD_VIEWER                  ON CACHE INTERNAL "")
set(PALANTEER_BUILD_CPP_EXAMPLE             OFF CACHE INTERNAL "")
set(PALANTEER_BUILD_PYTHON_INSTRUMENTATION  OFF CACHE INTERNAL "")
set(PALANTEER_BUILD_SERVER_SCRIPTING        OFF CACHE INTERNAL "")
FetchContent_Declare(
    palanteer
    GIT_REPOSITORY https://github.com/dfeneyrou/palanteer
    GIT_TAG        d49f35c648c9428703ebb1c8ca5a397b0c7e7c14
)

FetchContent_GetProperties(palanteer)
if(NOT palanteer_POPULATED)
    FetchContent_Populate(palanteer)

    add_subdirectory(${palanteer_SOURCE_DIR} ${palanteer_BINARY_DIR})
endif()

target_link_libraries(tamashii PUBLIC libpalanteer)
if (DISABLE_PALANTEER)
    target_compile_definitions(tamashii PUBLIC USE_PL=0 PL_VIRTUAL_THREADS=0)
else()
    target_compile_definitions(tamashii PUBLIC USE_PL=1 PL_VIRTUAL_THREADS=1)
endif()



####################################################################################
# SpdLog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.9.2
)

FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
    FetchContent_Populate(spdlog)
    add_subdirectory(${spdlog_SOURCE_DIR} ${spdlog_BINARY_DIR})
endif()

target_link_libraries(tamashii PUBLIC spdlog)



####################################################################################
# SG14 implementations
FetchContent_Declare(
    sg14
    GIT_REPOSITORY https://github.com/WG21-SG14/SG14
    GIT_TAG        5296fff6d507a962b5a81bf5f3731aeac0ea9391
)

FetchContent_GetProperties(sg14)
if(NOT sg14_POPULATED)
    FetchContent_Populate(sg14)
    add_subdirectory(${sg14_SOURCE_DIR} ${sg14_BINARY_DIR})
endif()

target_link_libraries(tamashii PUBLIC sg14)









