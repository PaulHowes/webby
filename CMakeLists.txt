cmake_minimum_required(VERSION 2.8.12)
project(webby)

#
# Configures the compiler, regardless of the build type.
#
if("${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  add_definitions(-std=c++11
                  -stdlib=libc++
                  -Weverything
                  -Wno-c++98-compat
                  -Wno-padded
                  -Wno-documentation-unknown-command
                  -Wno-missing-prototypes
                  -Wno-missing-noreturn
                  -Wno-unreachable-code
                  -Wno-weak-vtables
                  -Wno-exit-time-destructors
                  -Wno-global-constructors
                  )
elseif("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
  add_definitions(-std=c++11
                  -Wall
                  -Wextra
                  -Wpedantic)
endif()

#
# If `git` is installed locally, perform an automatic update of submodules.
#
find_package(Git)
if(GIT_FOUND)
  message("Updating Git submodules with ${GIT_EXECUTABLE} ${GIT_VERSION_STRING}")
  execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive)
endif(GIT_FOUND)

#
# Use `make doc` to generate API documentation.
#
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/html
          DESTINATION doc)
endif(DOXYGEN_FOUND)

#
# Adds the include directory to the compiler's search path.
#
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/externals/asf/include
  ${CMAKE_CURRENT_SOURCE_DIR}/externals/asf/external/any/include
  ${CMAKE_CURRENT_SOURCE_DIR}/externals/net/include
  ${CMAKE_CURRENT_SOURCE_DIR}/externals/qlog/include
  ${CMAKE_CURRENT_SOURCE_DIR}/externals/mapped/include
  )

#
# Builds the test driver. This can be launched with either `make test` or `ctest`
#
enable_testing()
link_directories(${CMAKE_BINARY_DIR})
add_executable(webbyd ${CMAKE_CURRENT_SOURCE_DIR}/test/main.cpp)
