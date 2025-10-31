# Lightweight CMake include to expose header-only WebSocket++ from app/libs

# Option to enable/disable WebSocket++ integration (headers and include dirs)
option(SERVER_WEB_WITH_WEBSOCKETPP "Enable WebSocket++ integration" ON)

if(TARGET websocketpp::websocketpp)
  # Already defined by a previous include
  return()
endif()

# Resolve libs root based on this file location (app/libs/cmake)
get_filename_component(_WS_LIBS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# Expect headers under ${_WS_LIBS_ROOT}/websocketpp
if(NOT EXISTS "${_WS_LIBS_ROOT}/websocketpp/version.hpp" AND
   NOT EXISTS "${_WS_LIBS_ROOT}/websocketpp/websocketpp.hpp")
  if(NOT EXISTS "${_WS_LIBS_ROOT}/websocketpp")
    message(FATAL_ERROR "websocketpp.cmake: Could not locate WebSocket++ headers under ${_WS_LIBS_ROOT}")
  endif()
endif()

add_library(websocketpp INTERFACE)
add_library(websocketpp::websocketpp ALIAS websocketpp)

# Группировка в IDE
set_target_properties(websocketpp PROPERTIES FOLDER libs)

if(SERVER_WEB_WITH_WEBSOCKETPP)
  target_include_directories(websocketpp INTERFACE
    $<BUILD_INTERFACE:${_WS_LIBS_ROOT}>
  )
endif()

# Define feature macro for conditional compilation in sources
target_compile_definitions(websocketpp INTERFACE
  SERVER_WEB_HAVE_WEBSOCKETPP=$<IF:$<BOOL:${SERVER_WEB_WITH_WEBSOCKETPP}>,1,0>
)

# Example usage in a CMakeLists.txt:
#   set(SERVER_WEB_LIBS "${CMAKE_SOURCE_DIR}/app/libs")
#   include("${SERVER_WEB_LIBS}/cmake/websocketpp.cmake")
#   target_link_libraries(my_target PRIVATE websocketpp::websocketpp)

unset(_WS_LIBS_ROOT)
