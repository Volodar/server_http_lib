## CMake include to expose MySQL Connector/C++

if(TARGET mysqlconnector::mysqlconnector)
  return()
endif()

add_library(mysqlconnector INTERFACE)
add_library(mysqlconnector::mysqlconnector ALIAS mysqlconnector)

get_target_property(_mysqlconnector_target_type mysqlconnector TYPE)
if(_mysqlconnector_target_type AND NOT _mysqlconnector_target_type STREQUAL "INTERFACE_LIBRARY")
  set_target_properties(mysqlconnector PROPERTIES FOLDER libs)
endif()

#
# Предпочитаем системные заголовки/библиотеки (Ubuntu, Linux)
# Если не найдены — используем встроенные заголовки и (на macOS) локальные dylib.
#

set(_USE_SYSTEM 0)
if(UNIX AND NOT APPLE)
  find_path(CPPCONN_INCLUDE_DIR NAMES cppconn/driver.h PATHS /usr/include /usr/local/include)
  find_library(CPPCONN_LIBRARY NAMES mysqlcppconn PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
  if(CPPCONN_INCLUDE_DIR AND CPPCONN_LIBRARY)
    set(_USE_SYSTEM 1)
  endif()
endif()

if(_USE_SYSTEM)
  target_include_directories(mysqlconnector INTERFACE ${CPPCONN_INCLUDE_DIR})
  target_link_libraries(mysqlconnector INTERFACE ${CPPCONN_LIBRARY})
else()
  # Встроенные заголовки/библиотеки (для macOS, где поставляется комплект)
  get_filename_component(_MYSQL_LIBS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
  set(_MYSQL_INC_DIR "${_MYSQL_LIBS_ROOT}/mysqlconnector/include")
  set(_MYSQL_JDBC_INC_DIR "${_MYSQL_LIBS_ROOT}/mysqlconnector/include/jdbc")
  set(_MYSQL_LIB_DIR "${_MYSQL_LIBS_ROOT}/mysqlconnector/lib64")

  if(NOT EXISTS "${_MYSQL_INC_DIR}")
    message(FATAL_ERROR "mysqlconnector.cmake: Could not locate MySQL Connector includes at ${_MYSQL_INC_DIR}")
  endif()

  target_include_directories(mysqlconnector INTERFACE
    $<BUILD_INTERFACE:${_MYSQL_INC_DIR}>
    $<BUILD_INTERFACE:${_MYSQL_JDBC_INC_DIR}>
  )

  set(_MYSQL_LIB_FILE "")
  if(EXISTS "${_MYSQL_LIB_DIR}")
    if(APPLE)
      file(GLOB _cand "${_MYSQL_LIB_DIR}/libmysqlcppconn*.dylib")
    elseif(WIN32)
      file(GLOB _cand "${_MYSQL_LIB_DIR}/mysqlcppconn*.lib")
    else()
      file(GLOB _cand
        "${_MYSQL_LIB_DIR}/libmysqlcppconn*.so"
        "${_MYSQL_LIB_DIR}/libmysqlcppconn*-static.a"
        "${_MYSQL_LIB_DIR}/libmysqlcppconn*.a"
      )
    endif()
    list(LENGTH _cand _cand_len)
    if(_cand_len GREATER 0)
      list(GET _cand 0 _MYSQL_LIB_FILE)
    endif()
  endif()

  if(_MYSQL_LIB_FILE)
    target_link_libraries(mysqlconnector INTERFACE "${_MYSQL_LIB_FILE}")
  else()
    # На хосте должна быть доступна системная библиотека по имени
    target_link_libraries(mysqlconnector INTERFACE mysqlcppconn)
  endif()

  set(MYSQLCONNECTOR_LIB_DIR "${_MYSQL_LIB_DIR}" CACHE STRING "Path to bundled MySQL Connector/C++ libraries")

  unset(_MYSQL_LIB_FILE)
  unset(_cand)
  unset(_cand_len)
  unset(_MYSQL_LIB_DIR)
  unset(_MYSQL_INC_DIR)
  unset(_MYSQL_JDBC_INC_DIR)
  unset(_MYSQL_LIBS_ROOT)
endif()

# Фича-макрос для условной компиляции
target_compile_definitions(mysqlconnector INTERFACE SERVER_WEB_HAVE_MYSQLCONNECTOR=1)

unset(_mysqlconnector_target_type)
