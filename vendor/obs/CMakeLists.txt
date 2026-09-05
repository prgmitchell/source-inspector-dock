# OBS does not export its property editor as part of the plugin SDK.
# Build the vendored editor privately; do not depend on OBS executable symbols.
add_library(sid-properties-view STATIC)
set_target_properties(sid-properties-view PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)
foreach(
  directory
  IN
  ITEMS
    shared/properties-view
    shared/qt/wrappers
    shared/qt/vertical-scroll-area
    shared/qt/slider-ignorewheel
    shared/qt/plain-text-edit
    shared/qt/icon-label
)
  file(GLOB files CONFIGURE_DEPENDS "${directory}/*.cpp" "${directory}/*.hpp")
  target_sources(sid-properties-view PRIVATE ${files})
  target_include_directories(sid-properties-view PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${directory}")
endforeach()
target_link_libraries(sid-properties-view PUBLIC OBS::libobs OBS::obs-frontend-api Qt6::Widgets)
# Upstream code is kept separate from the plugin's warning policy.
set_target_properties(sid-properties-view PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
