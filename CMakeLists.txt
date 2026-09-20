cmake_minimum_required(VERSION 3.20)
project(huina_dump LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(huina_dump SHARED
    src/app/dllmain.cpp
    src/app/session.cpp
    src/core/log.cpp
    src/core/mem.cpp
    src/core/sig.cpp
    src/dump/interfaces.cpp
    src/dump/netvars.cpp
    src/dump/offsets.cpp
    src/emit/report.cpp
)

target_include_directories(huina_dump PRIVATE src)

target_link_libraries(huina_dump PRIVATE psapi)

set_target_properties(huina_dump PROPERTIES
    OUTPUT_NAME "huina_dump"
    SUFFIX ".dll"
)

if(MSVC)
    target_compile_options(huina_dump PRIVATE
        /EHa
        $<$<NOT:$<CONFIG:Debug>>:/O2>
        /W4
        /utf-8
    )
    target_compile_definitions(huina_dump PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
