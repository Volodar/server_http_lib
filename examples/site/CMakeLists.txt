cmake_minimum_required(VERSION 3.10)
project(server_example_site)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-D_GNU_SOURCE -DASIO_HAS_STD_ALLOCATOR)

if (NOT MSVC AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
endif()

# Root to app libs used by targets below
set(SERVER_WEB_LIBS ${CMAKE_SOURCE_DIR}/../../src)

# Bring in http library (and its transitive deps)
include("${SERVER_WEB_LIBS}/cmake/http.cmake")

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${SERVER_WEB_LIBS}
)

file(GLOB_RECURSE SRC
  ${CMAKE_SOURCE_DIR}/src/*.cpp
  ${CMAKE_SOURCE_DIR}/src/*.h
)

add_executable(example_site ${SRC})
target_link_libraries(example_site PRIVATE http::http)
set_target_properties(example_site PROPERTIES CXX_STANDARD 20)
