cmake_minimum_required(VERSION 3.20)
project(EFL_Probe VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(YYTK_TYPES_CPP
    ../../vendor/YYToolkit/include/YYToolkit/YYTK_Shared_Types.cpp
)

add_library(EFL_Probe SHARED probe_runtime.cpp ${YYTK_TYPES_CPP})

target_include_directories(EFL_Probe PRIVATE
    ../../include
    ../../vendor/Aurie/include
    ../../vendor/YYToolkit/include
)

target_compile_definitions(EFL_Probe PRIVATE
    EFL_PROBE_BUILD
)
