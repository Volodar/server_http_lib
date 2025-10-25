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

# Root to app libs used by targets below
set(SERVER_WEB_LIBS ${CMAKE_SOURCE_DIR}/../../src)

# Bring in http library (and its transitive deps)
include("${SERVER_WEB_LIBS}/cmake/http.cmake")

include_directories(
  ${CMAKE_SOURCE_DIR}
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
  ${CMAKE_SOURCE_DIR}/src/gateway/*.cpp
  ${CMAKE_SOURCE_DIR}/src/gateway/*.h
)
add_executable(gateway_service ${GATEWAY_SRC})
target_link_libraries(gateway_service PRIVATE http::http)
set_target_properties(gateway_service PROPERTIES CXX_STANDARD 20)
configure_macos_rpath(gateway_service)

#
# Users service
#
file(GLOB_RECURSE USERS_SRC
  ${CMAKE_SOURCE_DIR}/src/users/*.cpp
  ${CMAKE_SOURCE_DIR}/src/users/*.h
)
add_executable(users_service ${USERS_SRC})
target_link_libraries(users_service PRIVATE http::http)
set_target_properties(users_service PROPERTIES CXX_STANDARD 20)
configure_macos_rpath(users_service)

#
# Tasks service
#
file(GLOB_RECURSE TASKS_SRC
  ${CMAKE_SOURCE_DIR}/src/tasks/*.cpp
  ${CMAKE_SOURCE_DIR}/src/tasks/*.h
)
add_executable(tasks_service ${TASKS_SRC})
target_link_libraries(tasks_service PRIVATE http::http)
set_target_properties(tasks_service PROPERTIES CXX_STANDARD 20)
configure_macos_rpath(tasks_service)

#
# Auth service
#
file(GLOB_RECURSE AUTH_SRC
  ${CMAKE_SOURCE_DIR}/src/auth/*.cpp
  ${CMAKE_SOURCE_DIR}/src/auth/*.h
)
add_executable(auth_service ${AUTH_SRC})
target_link_libraries(auth_service PRIVATE http::http)
set_target_properties(auth_service PROPERTIES CXX_STANDARD 20)
configure_macos_rpath(auth_service)
