cmake_minimum_required(VERSION 3.10)
project(micro_todo)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_definitions(-D_GNU_SOURCE -DASIO_HAS_STD_ALLOCATOR)

if (NOT MSVC AND NOT MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -Wall")
endif()

if(APPLE)
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# Root to app libs used by targets below (robust for subdirectory builds)
get_filename_component(SERVER_WEB_LIBS "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)

# Bring in http library (and its transitive deps)
include("${SERVER_WEB_LIBS}/cmake/http.cmake")

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${SERVER_WEB_LIBS}
)

#
# Common RPATH configuration helper for macOS
#
function(configure_macos_rpath target_name)
  if(APPLE)
    # Try to determine the OpenSSL library directory from the imported target
    if(TARGET OpenSSL::SSL)
      get_target_property(_OPENSSL_SSL_LIB OpenSSL::SSL IMPORTED_LOCATION)
      if(_OPENSSL_SSL_LIB)
        get_filename_component(_OPENSSL_LIB_DIR "${_OPENSSL_SSL_LIB}" DIRECTORY)
      endif()
    endif()

    # Fallbacks for common Homebrew locations if the above is unavailable
    if(NOT _OPENSSL_LIB_DIR AND EXISTS "/opt/homebrew/opt/openssl@3/lib")
      set(_OPENSSL_LIB_DIR "/opt/homebrew/opt/openssl@3/lib")
    elseif(NOT _OPENSSL_LIB_DIR AND EXISTS "/usr/local/opt/openssl@3/lib")
      set(_OPENSSL_LIB_DIR "/usr/local/opt/openssl@3/lib")
    endif()

    if(DEFINED _OPENSSL_LIB_DIR)
      list(APPEND _RPATHS "${_OPENSSL_LIB_DIR}")
    endif()
    if(DEFINED MYSQLCONNECTOR_LIB_DIR)
      list(APPEND _RPATHS "${MYSQLCONNECTOR_LIB_DIR}")
    endif()
    list(APPEND _RPATHS "@loader_path" "@loader_path/../lib")

    if(_RPATHS)
      set_target_properties(${target_name} PROPERTIES
        BUILD_RPATH "${_RPATHS}"
        INSTALL_RPATH "${_RPATHS}"
      )
    endif()
    unset(_RPATHS)
    unset(_OPENSSL_SSL_LIB)
    unset(_OPENSSL_LIB_DIR)
  endif()
endfunction()

#
# Gateway service
#
file(GLOB_RECURSE GATEWAY_SRC
  ${CMAKE_CURRENT_LIST_DIR}/src/gateway/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/gateway/*.h
)
if(GATEWAY_SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${GATEWAY_SRC})
  add_executable(gateway_service ${GATEWAY_SRC})
  target_link_libraries(gateway_service PRIVATE http::http)
  set_target_properties(gateway_service PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(gateway_service PROPERTIES FOLDER examples)
  configure_macos_rpath(gateway_service)
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET gateway_service POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:gateway_service>
          COMMENT "gateway_service: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "micro_todo: пропускаю gateway_service — не найдены исходники")
endif()

#
# Users service
#
file(GLOB_RECURSE USERS_SRC
  ${CMAKE_CURRENT_LIST_DIR}/src/users/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/users/*.h
)
if(USERS_SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${USERS_SRC})
  add_executable(users_service ${USERS_SRC})
  target_link_libraries(users_service PRIVATE http::http)
  set_target_properties(users_service PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(users_service PROPERTIES FOLDER examples)
  configure_macos_rpath(users_service)
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET users_service POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:users_service>
          COMMENT "users_service: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "micro_todo: пропускаю users_service — не найдены исходники")
endif()

#
# Tasks service
#
file(GLOB_RECURSE TASKS_SRC
  ${CMAKE_CURRENT_LIST_DIR}/src/tasks/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/tasks/*.h
)
if(TASKS_SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${TASKS_SRC})
  add_executable(tasks_service ${TASKS_SRC})
  target_link_libraries(tasks_service PRIVATE http::http)
  set_target_properties(tasks_service PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(tasks_service PROPERTIES FOLDER examples)
  configure_macos_rpath(tasks_service)
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET tasks_service POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:tasks_service>
          COMMENT "tasks_service: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "micro_todo: пропускаю tasks_service — не найдены исходники")
endif()

#
# Auth service
#
file(GLOB_RECURSE AUTH_SRC
  ${CMAKE_CURRENT_LIST_DIR}/src/auth/*.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/auth/*.h
)
if(AUTH_SRC)
  # Группировка исходников как в ФС
  source_group(TREE "${CMAKE_CURRENT_LIST_DIR}" FILES ${AUTH_SRC})
  add_executable(auth_service ${AUTH_SRC})
  target_link_libraries(auth_service PRIVATE http::http)
  set_target_properties(auth_service PROPERTIES CXX_STANDARD 20)
  # Папка в IDE
  set_target_properties(auth_service PROPERTIES FOLDER examples)
  configure_macos_rpath(auth_service)
  if(APPLE)
    if(NOT DEFINED MYSQLCONNECTOR_LIB_DIR)
      get_filename_component(_DEF_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../src" ABSOLUTE)
      set(MYSQLCONNECTOR_LIB_DIR "${_DEF_ROOT}/mysqlconnector/lib64")
    endif()
    if(EXISTS "${MYSQLCONNECTOR_LIB_DIR}")
      file(GLOB _MYSQL_DYLIBS "${MYSQLCONNECTOR_LIB_DIR}/*.dylib")
      if(_MYSQL_DYLIBS)
        add_custom_command(TARGET auth_service POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_MYSQL_DYLIBS} $<TARGET_FILE_DIR:auth_service>
          COMMENT "auth_service: копирование MySQL *.dylib в каталог таргета"
        )
      endif()
      unset(_MYSQL_DYLIBS)
    endif()
    unset(_DEF_ROOT)
  endif()
else()
  message(WARNING "micro_todo: пропускаю auth_service — не найдены исходники")
endif()
