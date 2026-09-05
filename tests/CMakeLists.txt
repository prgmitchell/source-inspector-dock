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
if(APPLE)
  # The standalone test needs SDK frameworks rather than OBS's application-relative rpaths.
  set(
    test_rpaths
    "$<TARGET_FILE_DIR:OBS::libobs>/../../.."
    "$<TARGET_FILE_DIR:Qt6::Core>/../../.."
    "$<TARGET_FILE_DIR:OBS::obs-frontend-api>"
  )
  foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
    foreach(directory lib Frameworks)
      if(IS_DIRECTORY "${prefix}/${directory}")
        list(APPEND test_rpaths "${prefix}/${directory}")
      endif()
    endforeach()
  endforeach()
  list(REMOVE_DUPLICATES test_rpaths)
  string(JOIN "\" \"" test_xcode_rpaths ${test_rpaths})
  set_target_properties(
    inspector-test
    PROPERTIES
      BUILD_WITH_INSTALL_RPATH FALSE
      BUILD_RPATH "${test_rpaths}"
      XCODE_ATTRIBUTE_LD_RUNPATH_SEARCH_PATHS "\"${test_xcode_rpaths}\""
  )
endif()
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
elseif(APPLE)
  set(test_plugins "")
  foreach(prefix IN LISTS CMAKE_PREFIX_PATH)
    if(IS_DIRECTORY "${prefix}/plugins")
      list(APPEND test_plugins "QT_PLUGIN_PATH=path_list_prepend:${prefix}/plugins")
    endif()
  endforeach()
  set_tests_properties(inspector-integration PROPERTIES ENVIRONMENT_MODIFICATION "${test_plugins}")
endif()
