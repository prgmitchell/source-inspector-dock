# OBS's property editor is not exported by the plugin SDK.
include(FetchContent)
find_package(Git REQUIRED)
FetchContent_Declare(
  obs_editor
  URL https://github.com/obsproject/obs-studio/archive/refs/tags/32.2.1.tar.gz
  URL_HASH SHA256=db034bc3d2c137ec7f1809cbef4fc90c8948652b90a860078410482457d8a574
  SOURCE_SUBDIR
  shared
  PATCH_COMMAND "${GIT_EXECUTABLE}" apply "${CMAKE_CURRENT_LIST_DIR}/obs-properties.patch"
)
FetchContent_MakeAvailable(obs_editor)

add_library(sid-properties-view STATIC)
set_target_properties(
  sid-properties-view
  PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON COMPILE_WARNING_AS_ERROR OFF
)
foreach(
  directory
  IN
  ITEMS properties-view qt/wrappers qt/vertical-scroll-area qt/slider-ignorewheel qt/plain-text-edit qt/icon-label
)
  set(directory "${obs_editor_SOURCE_DIR}/shared/${directory}")
  file(GLOB files CONFIGURE_DEPENDS "${directory}/*.cpp" "${directory}/*.hpp")
  target_sources(sid-properties-view PRIVATE ${files})
  target_include_directories(sid-properties-view PUBLIC "${directory}")
endforeach()
target_link_libraries(sid-properties-view PUBLIC OBS::libobs OBS::obs-frontend-api Qt6::Widgets)
