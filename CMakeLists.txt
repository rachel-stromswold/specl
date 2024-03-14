cmake_minimum_required(VERSION 3.3.2)
project(specl)

set(SPECL_LIB specl)

#set names for applications
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(TEST_EXE specl_test)
    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS "-Og -Wall")
    set(CMAKE_C_FLAGS "-Og -Wall")
    add_compile_definitions(DEBUG_INFO=1)
else()
    set(CMAKE_CXX_FLAGS "-O3")
endif()

add_library(${SPECL_LIB} src/read.c)

if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    add_executable( ${TEST_EXE} src/tests.cpp )
    target_link_libraries(${TEST_EXE} ${SPECL_LIB})
    target_include_directories( ${TEST_EXE} PRIVATE "/usr/include/doctest" )
endif()

if (NOT EXCLUDE_SCRIPTS)
    message(STATUS "copying scripts")
    configure_file(tests/test.geom test.geom COPYONLY)
endif()
