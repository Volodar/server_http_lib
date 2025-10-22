cmake_minimum_required(VERSION 3.10)
project(server_web_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-D_GNU_SOURCE -DASIO_HAS_STD_ALLOCATOR)

if (NOT MSVC AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
endif()

# Root to app libs used by targets below
set(SERVER_WEB_LIBS ${CMAKE_SOURCE_DIR}/../src)

# Bring in http library (and its transitive deps)
include("${SERVER_WEB_LIBS}/cmake/http.cmake")

include_directories(
  ${CMAKE_SOURCE_DIR}
  ${SERVER_WEB_LIBS}
)

# -------- Unit tests --------
file(GLOB_RECURSE UNIT_SRC
  ${CMAKE_SOURCE_DIR}/unit/*.cpp
  ${CMAKE_SOURCE_DIR}/main.cpp
  ${CMAKE_SOURCE_DIR}/test_framework.h
)
add_executable(server_unit_tests ${UNIT_SRC})
target_link_libraries(server_unit_tests PRIVATE http::http)
set_target_properties(server_unit_tests PROPERTIES CXX_STANDARD 20)

# -------- Integration tests --------
file(GLOB_RECURSE INTEG_SRC
  ${CMAKE_SOURCE_DIR}/integration/*.cpp
  ${CMAKE_SOURCE_DIR}/main.cpp
  ${CMAKE_SOURCE_DIR}/test_framework.h
)
add_executable(server_integration_tests ${INTEG_SRC})
target_link_libraries(server_integration_tests PRIVATE http::http)
set_target_properties(server_integration_tests PROPERTIES CXX_STANDARD 20)
