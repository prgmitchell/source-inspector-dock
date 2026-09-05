cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)
option(SID_USE_SYSTEM_DEPS "Use an existing OBS/Qt SDK from CMAKE_PREFIX_PATH" OFF)

if(NOT ENABLE_FRONTEND_API OR NOT ENABLE_QT)
  message(FATAL_ERROR "Source Inspector requires ENABLE_FRONTEND_API and ENABLE_QT.")
endif()

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON)

include(cmake/obs-properties.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE sid-properties-view)
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/inspector-dock.cpp
    src/inspector-dock.hpp
    src/source-properties.cpp
    src/source-properties.hpp
    src/transform-editor.cpp
    src/transform-editor.hpp
    src/section.hpp
    src/undo.cpp
    src/undo.hpp
)

if(APPLE)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE LICENSE)
  set_source_files_properties(LICENSE PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
elseif(UNIX)
  install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/${CMAKE_PROJECT_NAME}")
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
