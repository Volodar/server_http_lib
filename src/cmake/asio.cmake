if(TARGET asio::asio)
  return()
endif()

get_filename_component(_ASIO_LIBS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT EXISTS "${_ASIO_LIBS_ROOT}/asio.hpp" AND NOT EXISTS "${_ASIO_LIBS_ROOT}/asio")
  message(FATAL_ERROR "asio.cmake: Could not locate Asio headers under ${_ASIO_LIBS_ROOT}")
endif()

option(ASIO_USE_STANDALONE "Define ASIO_STANDALONE for standalone Asio" ON)

add_library(asio INTERFACE)
add_library(asio::asio ALIAS asio)

# Группировка в IDE
get_target_property(_asio_target_type asio TYPE)
if(_asio_target_type AND NOT _asio_target_type STREQUAL "INTERFACE_LIBRARY")
  set_target_properties(asio PROPERTIES FOLDER libs)
endif()

target_include_directories(asio INTERFACE
  $<BUILD_INTERFACE:${_ASIO_LIBS_ROOT}>
)

if(ASIO_USE_STANDALONE)
  target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
endif()

# Define feature macro for conditional compilation in sources
target_compile_definitions(asio INTERFACE SERVER_WEB_HAVE_ASIO=1)

unset(_ASIO_LIBS_ROOT)
unset(_asio_target_type)
