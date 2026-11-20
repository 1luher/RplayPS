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

# ---------------------------------------------------------------- PS5 link (chiaki-ng core)
file(GLOB RPLAY_PS5_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/ps5/*.cpp")
add_library(rplay_ps5 STATIC ${RPLAY_PS5_SOURCES})
target_link_libraries(rplay_ps5 PUBLIC rplay_core chiaki_core crypt32 shell32 ole32 iphlpapi ws2_32)
target_compile_options(rplay_ps5 PRIVATE ${RPLAY_OPTS})

add_executable(rplayctl "${CMAKE_SOURCE_DIR}/tools/rplayctl.cpp")
target_link_libraries(rplayctl PRIVATE rplay_ps5)
target_compile_options(rplayctl PRIVATE ${RPLAY_OPTS})

# ---------------------------------------------------------------- Windows input + sounds
file(GLOB RPLAY_WIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/win/*.cpp")
add_library(rplay_win STATIC ${RPLAY_WIN_SOURCES})
target_link_libraries(rplay_win PUBLIC rplay_core avrt winmm)
target_compile_options(rplay_win PRIVATE ${RPLAY_OPTS})

add_executable(inputcheck WIN32 "${CMAKE_SOURCE_DIR}/tools/inputcheck.cpp")
target_link_libraries(inputcheck PRIVATE rplay_win)
target_compile_options(inputcheck PRIVATE ${RPLAY_OPTS})

# ---------------------------------------------------------------- tests
enable_testing()
file(GLOB RPLAY_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(rplay_tests ${RPLAY_TEST_SOURCES})
target_link_libraries(rplay_tests PRIVATE rplay_core rplay_ps5)
target_compile_options(rplay_tests PRIVATE ${RPLAY_OPTS})
target_compile_definitions(rplay_tests PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
add_test(NAME rplay_tests COMMAND rplay_tests)
