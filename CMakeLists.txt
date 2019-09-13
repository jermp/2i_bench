cmake_minimum_required(VERSION 2.8)
project(DS2I)

configure_file(
  ${DS2I_SOURCE_DIR}/include/ds2i/ds2i_config.hpp.in
  ${DS2I_SOURCE_DIR}/include/ds2i/ds2i_config.hpp
  ESCAPE_QUOTES)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
MESSAGE( STATUS "CMAKE_BUILD_TYPE: " ${CMAKE_BUILD_TYPE} )

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

if (UNIX)

   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb")
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-missing-braces")

   if (USE_SANITIZERS)
     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
   endif ()

endif()

find_package(Boost COMPONENTS iostreams unit_test_framework filesystem system log log_setup date_time chrono REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

include_directories(${DS2I_SOURCE_DIR}/include/ds2i)
include_directories(${DS2I_SOURCE_DIR}/include/dint)

add_subdirectory(external)

include_directories(${DS2I_SOURCE_DIR}/external
        ${DS2I_SOURCE_DIR}/include
        )

add_subdirectory(src)
add_subdirectory(benchmarks)

enable_testing()
add_subdirectory(test)
