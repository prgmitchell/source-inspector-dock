add_executable(
  inspector-test
  inspector-test.cpp
  ../src/inspector-dock.cpp
  ../src/source-properties.cpp
  ../src/transform-editor.cpp
  ../src/undo.cpp
)
target_include_directories(inspector-test PRIVATE ../src)
set_target_properties(inspector-test PROPERTIES AUTOMOC ON)
target_link_libraries(inspector-test PRIVATE sid-properties-view)
add_test(
  NAME inspector-integration
  COMMAND
    inspector-test "${CMAKE_SOURCE_DIR}/data/locale/en-US.ini" "${CMAKE_BINARY_DIR}/inspector-preview.png"
    "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE:OBS::obs-frontend-api>"
)
add_dependencies(inspector-test ${CMAKE_PROJECT_NAME})
set_tests_properties(inspector-integration PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 45)
if(WIN32)
  get_target_property(obs_dll OBS::libobs IMPORTED_LOCATION_RELEASE)
  get_filename_component(obs_bin "${obs_dll}" DIRECTORY)
  get_target_property(qt_dll Qt6::Core IMPORTED_LOCATION_RELEASE)
  get_filename_component(qt_bin "${qt_dll}" DIRECTORY)
  get_filename_component(qt_root "${qt_bin}" DIRECTORY)
  set_tests_properties(
    inspector-integration
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=windows;QT_PLUGIN_PATH=${qt_root}/plugins;QT_LOGGING_TO_CONSOLE=1"
  )
  set(extra_bin "")
  foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${prefix}/bin")
      list(APPEND extra_bin "PATH=path_list_prepend:${prefix}/bin")
    endif()
  endforeach()
  set_tests_properties(
    inspector-integration
    PROPERTIES
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${obs_bin};PATH=path_list_prepend:${qt_bin};${extra_bin}"
  )
endif()
