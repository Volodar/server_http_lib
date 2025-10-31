# Build the HTTP support library from app/libs/http and expose as http::http

if(TARGET http::http)
  return()
endif()

get_filename_component(_HTTP_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_HTTP_SRC_DIR "${_HTTP_ROOT}/http")

if(NOT EXISTS "${_HTTP_SRC_DIR}")
  message(FATAL_ERROR "http.cmake: Could not locate ${_HTTP_SRC_DIR}")
endif()

file(GLOB _HTTP_SOURCES
  "${_HTTP_SRC_DIR}/*.cpp"
  "${_HTTP_SRC_DIR}/*.h"
  "${_HTTP_SRC_DIR}/*.hpp"
)

# Отображение исходников как в файловой системе (Xcode/VS)
source_group(TREE "${_HTTP_ROOT}" FILES ${_HTTP_SOURCES})

add_library(http STATIC ${_HTTP_SOURCES})
add_library(http::http ALIAS http)

set_target_properties(http PROPERTIES FOLDER libs)

target_include_directories(http PUBLIC
  $<BUILD_INTERFACE:${_HTTP_ROOT}>
)

# Bring in dependencies and link them so consumers don't have to
include("${_HTTP_ROOT}/cmake/asio.cmake")
include("${_HTTP_ROOT}/cmake/mysqlconnector.cmake")
include("${_HTTP_ROOT}/cmake/jsoncpp.cmake")
include("${_HTTP_ROOT}/cmake/pugixml.cmake")
find_package(OpenSSL REQUIRED)

target_link_libraries(http PUBLIC
  asio::asio
  mysqlconnector::mysqlconnector
  jsoncpp::jsoncpp
  pugixml::pugixml
  OpenSSL::SSL
  OpenSSL::Crypto
)

# Define feature macro for conditional compilation in sources
target_compile_definitions(http PUBLIC SERVER_WEB_HAVE_HTTP=1)

unset(_HTTP_SOURCES)
unset(_HTTP_SRC_DIR)
unset(_HTTP_ROOT)
