cmake_minimum_required(VERSION 3.10)
project(server_example_site)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-D_GNU_SOURCE -DASIO_HAS_STD_ALLOCATOR)

if (NOT MSVC AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
endif()

# Root to app libs used by targets below (robust for subdirectory builds)
get_filename_component(SERVER_WEB_LIBS "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)

# Bring in http library (and its transitive deps)
include("${SERVER_WEB_LIBS}/cmake/http.cmake")

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${SERVER_WEB_LIBS}
)

file(GLOB_RECURSE SRC
  ${CMAKE_CURRENT_LIST_DIR}/src/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/*.h
)

if(SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${SRC})
  add_executable(example_site ${SRC})
  target_link_libraries(example_site PRIVATE http::http)
  set_target_properties(example_site PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(example_site PROPERTIES FOLDER examples)

  # macOS: копируем MySQL *.dylib рядом с бинарём, если есть
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET example_site POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:example_site>
          COMMENT "example_site: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "example_site: пропускаю сборку — не найдены исходники")
endif()
