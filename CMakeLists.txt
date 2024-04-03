cmake_minimum_required(VERSION 3.5.0)
project(speclang VERSION 0.3.0 DESCRIPTION "A C library for reading complicated configuration settings")
set(CMAKE_DEBUG_POSTFIX d)
set(SPCL_LIB speclang)

#set names for applications
if(CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")

    set(CMAKE_BUILD_TYPE Debug)
    set(CMAKE_CXX_FLAGS "-Og -Wall -Wextra -Wpedantic")
    set(CMAKE_C_FLAGS "-Og -Wall")
    add_compile_definitions(SPCL_DEBUG_LVL=1)
else()
    set(CMAKE_CXX_FLAGS "-O3")
endif()

#shamelessly stolen from https://dominikberner.ch/cmake-library-setup
add_library(${SPCL_LIB})
#add_library(${SPCL_LIB}::${SPCL_LIB} ALIAS ${SPCL_LIB})
set_target_properties(${SPCL_LIB} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR} VISIBILITY_INLINES_HIDDEN TRUE)
target_sources(${SPCL_LIB} PRIVATE src/read.c)
target_include_directories(
    ${SPCL_LIB}
    PRIVATE src
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
#generate_export_header(${SPCL_LIB} EXPORT_FILE_NAME export/speclang.h)
#target_include_directories(
#    ${SPCL_LIB}
#    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/export
#    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
#)

#install the library
include(GNUInstallDirs)
message(STATUS "found installation directories: ${CMAKE_INSTALL_LIBDIR}, ${CMAKE_INSTALL_BINDIR}, and ${CMAKE_INSTALL_INCLUDEDIR}")
install(
    TARGETS ${SPCL_LIB} EXPORT speclang_target
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#write cmake package information
include(CMakePackageConfigHelpers)
write_basic_package_version_file("SpeclangConfigVersion.cmake" VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/speclang_config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/SpeclangConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/speclang
)
install(
    EXPORT speclang_target
    FILE speclang_target.cmake
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/cmake/speclang
)

#now we make unit tests
if(BUILD_TESTS MATCHES "y")
    set(TEST_EXE spcl_test)
    add_compile_definitions(TEST_GEOM_NAME="${CMAKE_SOURCE_DIR}/tests/test.geom")
    add_compile_definitions(TEST_BENCH_NAME="${CMAKE_SOURCE_DIR}/tests/benchmark.spcl")
    add_compile_definitions(TEST_ASSERT_NAME="${CMAKE_SOURCE_DIR}/tests/assertions.spcl")
    add_executable( ${TEST_EXE} src/tests.cpp )
    target_link_libraries(${TEST_EXE} ${SPCL_LIB})
    target_include_directories( ${TEST_EXE} PRIVATE "/usr/include/doctest" )
endif()
