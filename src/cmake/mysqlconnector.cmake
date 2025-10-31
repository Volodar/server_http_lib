# CMake include to expose MySQL Connector/C++ from app/libs

if(TARGET mysqlconnector::mysqlconnector)
  # Already defined by a previous include
  return()
endif()

# Resolve libs root based on this file location (app/libs/cmake)
get_filename_component(_MYSQL_LIBS_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_MYSQL_INC_DIR "${_MYSQL_LIBS_ROOT}/mysqlconnector/include")
set(_MYSQL_JDBC_INC_DIR "${_MYSQL_LIBS_ROOT}/mysqlconnector/include/jdbc")
set(_MYSQL_LIB_DIR "${_MYSQL_LIBS_ROOT}/mysqlconnector/lib64")

if(NOT EXISTS "${_MYSQL_INC_DIR}")
  message(FATAL_ERROR "mysqlconnector.cmake: Could not locate MySQL Connector includes at ${_MYSQL_INC_DIR}")
endif()

add_library(mysqlconnector INTERFACE)
add_library(mysqlconnector::mysqlconnector ALIAS mysqlconnector)

# Группировка в IDE
set_target_properties(mysqlconnector PROPERTIES FOLDER libs)

target_include_directories(mysqlconnector INTERFACE
  $<BUILD_INTERFACE:${_MYSQL_INC_DIR}>
  $<BUILD_INTERFACE:${_MYSQL_JDBC_INC_DIR}>
)

# Define feature macro for conditional compilation in sources
target_compile_definitions(mysqlconnector INTERFACE SERVER_WEB_HAVE_MYSQLCONNECTOR=1)

# Try to provide an imported target for libmysqlcppconn if the library is bundled.
set(_MYSQL_LIB_FILE "")
if(EXISTS "${_MYSQL_LIB_DIR}")
  # Prefer shared library if present, otherwise static
  if(APPLE)
    file(GLOB _cand
      "${_MYSQL_LIB_DIR}/libmysqlcppconn*.dylib"
    )
  elseif(WIN32)
    file(GLOB _cand
      "${_MYSQL_LIB_DIR}/mysqlcppconn*.lib"
    )
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

if(_MYSQL_LIB_FILE AND NOT TARGET mysqlcppconn)
  # Create imported target pointing to the found library
  add_library(mysqlcppconn SHARED IMPORTED GLOBAL)
  set_target_properties(mysqlcppconn PROPERTIES
    IMPORTED_LOCATION "${_MYSQL_LIB_FILE}"
  )
endif()

# Link the interface to mysqlcppconn (imported or system-provided)
if(TARGET mysqlcppconn)
  # Группировка в IDE
  set_target_properties(mysqlcppconn PROPERTIES FOLDER libs)
  target_link_libraries(mysqlconnector INTERFACE mysqlcppconn)
else()
  # Fall back to system resolver if available on the host
  target_link_libraries(mysqlconnector INTERFACE mysqlcppconn)
endif()

# Also export a convenient variable for consumers that need the lib dir for rpaths or copying
set(MYSQLCONNECTOR_LIB_DIR "${_MYSQL_LIB_DIR}" CACHE STRING "Path to bundled MySQL Connector/C++ libraries")

# Example usage in a CMakeLists.txt:
#   set(SERVER_WEB_LIBS "${CMAKE_SOURCE_DIR}/app/libs")
#   include("${SERVER_WEB_LIBS}/cmake/mysqlconnector.cmake")
#   target_link_libraries(my_target PRIVATE mysqlconnector::mysqlconnector)

unset(_MYSQL_LIB_FILE)
unset(_cand)
unset(_cand_len)
unset(_MYSQL_LIB_DIR)
unset(_MYSQL_INC_DIR)
unset(_MYSQL_JDBC_INC_DIR)
unset(_MYSQL_LIBS_ROOT)
