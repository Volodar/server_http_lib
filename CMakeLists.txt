cmake_minimum_required(VERSION 3.10)
project(server_http_lib CXX)

# Глобальные настройки компилятора
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_LIBS "Собирать библиотеки из src/cmake" ON)
option(BUILD_TESTS "Собирать тесты" ON)
option(BUILD_EXAMPLES "Собирать примеры" ON)

# Включаем группировку целей по папкам в IDE
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Корень с библиотеками и вспомогательными cmake-файлами
set(SERVER_WEB_LIBS ${CMAKE_SOURCE_DIR}/src)

# Соберем библиотеки заранее, чтобы цели были доступны повсюду
if(BUILD_LIBS)
  include("${SERVER_WEB_LIBS}/cmake/http.cmake")
endif()

# Тесты
if(BUILD_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()

# Примеры
if(BUILD_EXAMPLES)
  if(EXISTS "${CMAKE_SOURCE_DIR}/examples/site/CMakeLists.txt")
    add_subdirectory(examples/site)
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/examples/micro_todo/CMakeLists.txt")
    add_subdirectory(examples/micro_todo)
  endif()
endif()

# macOS: копирование dylib MySQL Connector в корень build/
if(APPLE)
  # Предпочитаем путь из включенного mysqlconnector.cmake, но добавим запасной
  if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
    set(MYSQLCONNECTOR_LIB_DIR "${SERVER_WEB_LIBS}/mysqlconnector/lib64")
  endif()
  if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
    file(GLOB _MYSQL_DYLIBS
      "${MYSQLCONNECTOR_LIB_DIR}/*.dylib"
    )
    if(_MYSQL_DYLIBS)
      add_custom_target(copy_mysql_dylibs ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} "${CMAKE_BINARY_DIR}"
        COMMENT "Копирование MySQL Connector/C++ *.dylib в build/"
        VERBATIM
      )
      set_target_properties(copy_mysql_dylibs PROPERTIES FOLDER libs)
    endif()
    unset(_MYSQL_DYLIBS)
  endif()
endif()
