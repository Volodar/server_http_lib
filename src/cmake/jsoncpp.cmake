if(TARGET jsoncpp::jsoncpp)
  return()
endif()

get_filename_component(_JSON_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_JSON_SRC_DIR "${_JSON_ROOT}/jsoncpp")

if(NOT EXISTS "${_JSON_SRC_DIR}")
  message(FATAL_ERROR "jsoncpp.cmake: Could not locate ${_JSON_SRC_DIR}")
endif()

set(_JSON_SOURCES
  "${_JSON_SRC_DIR}/json_reader.cpp"
  "${_JSON_SRC_DIR}/json_value.cpp"
  "${_JSON_SRC_DIR}/json_writer.cpp"
)

# Отображение исходников как в файловой системе (Xcode/VS)
source_group(TREE "${_JSON_ROOT}" FILES ${_JSON_SOURCES})

add_library(jsoncpp STATIC ${_JSON_SOURCES})
add_library(jsoncpp::jsoncpp ALIAS jsoncpp)

set_target_properties(jsoncpp PROPERTIES FOLDER libs)

target_include_directories(jsoncpp PUBLIC
  $<BUILD_INTERFACE:${_JSON_ROOT}>
)

# Define feature macro for conditional compilation in sources
target_compile_definitions(jsoncpp PUBLIC SERVER_WEB_HAVE_JSONCPP=1)

unset(_JSON_SOURCES)
unset(_JSON_SRC_DIR)
unset(_JSON_ROOT)
