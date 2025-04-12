cmake_minimum_required(VERSION 3.10)

# find_package(Boost 1.74.0 REQUIRED COMPONENTS program_options)

include(ExternalProject)

set(source_dir "${CMAKE_CURRENT_BINARY_DIR}/googletest-src")
set(build_dir "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")
set(install_dir "${CMAKE_CURRENT_BINARY_DIR}")

EXTERNALPROJECT_ADD(
  googletest
  GIT_REPOSITORY    https://github.com/google/googletest.git
  GIT_TAG           v1.14.0
  PREFIX            ${install_dir}
  SOURCE_DIR        ${source_dir}
  BINARY_DIR        ${build_dir}
  CMAKE_ARGS        -D BUILD_SHARED_LIBS=ON
  INSTALL_COMMAND   ""
  UPDATE_COMMAND    ""
)

include_directories(${source_dir}/googletest/include)
include_directories(${source_dir}/googlemock/include)
link_directories(${build_dir}/lib)
