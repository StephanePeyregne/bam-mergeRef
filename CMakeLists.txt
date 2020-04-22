cmake_minimum_required(VERSION 3.1)

project(bam-mergeRef)

set(CMAKE_CXX_STANDARD 11)

set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS} -O3 -mcx16 -g")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -O3 -mcx16 -g")

# The following folder will be included
include_directories("${PROJECT_SOURCE_DIR}")

# Add external projects
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

# libhandlegraph (full build using its cmake config)
ExternalProject_Add(bamtools
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/bamtools"
  CMAKE_ARGS "${CMAKE_ARGS};-DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>")
ExternalProject_Get_property(bamtools INSTALL_DIR)
set(bamtools_INCLUDE "${INSTALL_DIR}/include")
set(bamtools_LIB "${INSTALL_DIR}/lib")

#add_subdirectory(
#  bamtools EXCLUDE_FROM_ALL)
#set(bamtools_INCLUDE bamtools/src/api)

add_executable(bam-mergeRef
  main.cpp)
target_link_libraries(bam-mergeRef
  "${bamtools_LIB}/libbamtools.a"
  popt
  z)
target_include_directories(bam-mergeRef PUBLIC
  "${bamtools_INCLUDE}/bamtools")
add_dependencies(bam-mergeRef bamtools)

if (BUILD_STATIC)
  set(CMAKE_EXE_LINKER_FLAGS "-static")
endif()
