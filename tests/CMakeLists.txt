cmake_minimum_required(VERSION 3.10)
project(server_web_tests)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-D_GNU_SOURCE -DASIO_HAS_STD_ALLOCATOR)

if (NOT MSVC AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
endif()

# Root to app libs used by targets below (robust for subdirectory builds)
get_filename_component(SERVER_WEB_LIBS "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)

# Bring in http library (and its transitive deps)
include("${SERVER_WEB_LIBS}/cmake/http.cmake")

# Включаем тестовый хук в реализации LRUCache для сборки тестов
if (TARGET http)
  target_compile_definitions(http PRIVATE LRU_TEST_HOOK=1)
endif()

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${SERVER_WEB_LIBS}
)

# -------- Unit tests --------
file(GLOB_RECURSE UNIT_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/unit/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_framework.h
)
if(UNIT_SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${UNIT_SRC})
  add_executable(server_unit_tests ${UNIT_SRC})
  target_link_libraries(server_unit_tests PRIVATE http::http)
  set_target_properties(server_unit_tests PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(server_unit_tests PROPERTIES FOLDER tests)
  target_compile_definitions(server_unit_tests PRIVATE LRU_TEST_HOOK=1)
  # macOS: копируем MySQL *.dylib рядом с бинарём, если есть
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET server_unit_tests POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:server_unit_tests>
          COMMENT "server_unit_tests: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "tests: пропускаю server_unit_tests — не найдены исходники")
endif()

# -------- Integration tests --------
file(GLOB_RECURSE INTEG_SRC CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_LIST_DIR}/integration/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_framework.h
)
if(INTEG_SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${INTEG_SRC})
  add_executable(server_integration_tests ${INTEG_SRC})
  target_link_libraries(server_integration_tests PRIVATE http::http)
  set_target_properties(server_integration_tests PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(server_integration_tests PROPERTIES FOLDER tests)
  # macOS: копируем MySQL *.dylib рядом с бинарём, если есть
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET server_integration_tests POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:server_integration_tests>
          COMMENT "server_integration_tests: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "tests: пропускаю server_integration_tests — не найдены исходники")
endif()
