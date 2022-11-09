cmake_minimum_required(VERSION 3.24)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions($<$<CONFIG:Debug>:MP_DEBUG>)

SET(INCLUDE_FILES
        include/fixpool.hpp)

SET(MEMPOOL_NAME mempool)
add_library(${MEMPOOL_NAME} INTERFACE
        ${INCLUDE_FILES})

SET(COMPILER_OPTIONS "-Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-padded -Wno-reserved-identifier -Wno-poison-system-directories")

set_source_files_properties(
        ${INCLUDE_FILES}
        PROPERTIES
        COMPILE_FLAGS "${COMPILER_OPTIONS}")
