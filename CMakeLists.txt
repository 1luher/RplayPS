# SPDX-License-Identifier: AGPL-3.0-only
cmake_minimum_required(VERSION 3.24)
project(RplayPS LANGUAGES C CXX)

if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "RplayPS builds only on Windows with MSVC")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(RPLAY_DEFS UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _WIN32_WINNT=0x0A00)
set(RPLAY_OPTS /W4 /permissive- /utf-8 /MP)

add_subdirectory(third_party)

# ---------------------------------------------------------------- core (no Windows APIs)
file(GLOB RPLAY_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/core/*.cpp")
add_library(rplay_core STATIC ${RPLAY_CORE_SOURCES})
target_include_directories(rplay_core PUBLIC "${CMAKE_SOURCE_DIR}/src" "${CMAKE_SOURCE_DIR}/third_party")
target_compile_options(rplay_core PRIVATE ${RPLAY_OPTS})
target_compile_definitions(rplay_core PUBLIC ${RPLAY_DEFS})

# ---------------------------------------------------------------- tests
enable_testing()
file(GLOB RPLAY_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(rplay_tests ${RPLAY_TEST_SOURCES})
target_link_libraries(rplay_tests PRIVATE rplay_core)
target_compile_options(rplay_tests PRIVATE ${RPLAY_OPTS})
add_test(NAME rplay_tests COMMAND rplay_tests)
