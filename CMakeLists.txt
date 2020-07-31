cmake_minimum_required(VERSION 3.6)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Release/Debug")
endif()

project(ubuild VERSION 0.2.0 LANGUAGES CXX C)

message(STATUS "yay = $ENV{YAY}")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")
endif()

set(TEAPORT_SRC_DIR "${PROJECT_SOURCE_DIR}/teas")
function(add_tea name)
    if("${name}" STREQUAL "")
        message(FATAL_ERROR "add_tea requires name argument to be not empty")
    endif()
    set(dir ${CMAKE_CURRENT_LIST_DIR}/teas/${name})
    if(NOT EXISTS ${dir})
        message(FATAL_ERROR "dir not existS: ${dir}")
    endif()
    file(GLOB_RECURSE cpp ${dir}/*.cpp ${dir}/*.c)
    file(GLOB_RECURSE hpp ${dir}/*.hpp ${dir}/*.h)
    if("${cpp}" STREQUAL "")
        message(STATUS "interface library ${name}")
        add_library(${name} INTERFACE)
        target_include_directories(${name} INTERFACE ${dir})
    else()
        message(STATUS "static library ${name}")

        add_library(${name} STATIC ${cpp} ${hpp})
        target_include_directories(${name} PUBLIC ${dir})

    endif()

    list(APPEND TEA_LIBS ${name})
    set(TEA_LIBS ${TEA_LIBS} PARENT_SCOPE)
endfunction()

add_tea(iostream)
add_tea(sqlite)

add_library(sqlite3 ALIAS sqlite)

add_subdirectory(teas/teaport_utils teaport_utils)
add_subdirectory(teas/subprocess subprocess)


file(GLOB_RECURSE src_files src/cpp/*.cpp)



add_executable(buildhl ${src_files})

target_include_directories(buildhl PUBLIC
    src/cpp
    ${CMAKE_CURRENT_BINARY_DIR}/include
)
if(MSVC)
    target_compile_options(buildhl PUBLIC -Zc:__cplusplus)
endif()

target_link_libraries(buildhl PUBLIC
    teaport_utils subprocess iostream
)

target_compile_definitions(buildhl PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_link_libraries(buildhl PUBLIC stdc++fs)
    if(NOT WIN32)
        target_link_libraries(buildhl PUBLIC pthread dl)
    endif()
endif()


install(TARGETS buildhl DESTINATION bin)

