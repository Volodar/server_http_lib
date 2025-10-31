# Build pugixml library from app/libs/pugixml and expose as pugixml::pugixml

if(TARGET pugixml::pugixml)
  return()
endif()

get_filename_component(_PUGI_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(_PUGI_SRC_DIR "${_PUGI_ROOT}/pugixml")

if(NOT EXISTS "${_PUGI_SRC_DIR}")
  message(FATAL_ERROR "pugixml.cmake: Could not locate ${_PUGI_SRC_DIR}")
endif()

set(_PUGI_SOURCES
  "${_PUGI_SRC_DIR}/pugixml.cpp"
)

# Отображение исходников как в файловой системе (Xcode/VS)
source_group(TREE "${_PUGI_ROOT}" FILES ${_PUGI_SOURCES})

add_library(pugixml STATIC ${_PUGI_SOURCES})
add_library(pugixml::pugixml ALIAS pugixml)

set_target_properties(pugixml PROPERTIES FOLDER libs)

target_include_directories(pugixml PUBLIC
  $<BUILD_INTERFACE:${_PUGI_ROOT}>
)

# Define feature macro for conditional compilation in sources
target_compile_definitions(pugixml PUBLIC SERVER_WEB_HAVE_PUGIXML=1)

unset(_PUGI_SOURCES)
unset(_PUGI_SRC_DIR)
unset(_PUGI_ROOT)
