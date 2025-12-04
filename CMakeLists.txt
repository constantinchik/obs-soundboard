cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core Multimedia)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)

  # Qt Multimedia is optional - only link if found
  if(TARGET Qt6::Multimedia)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Multimedia)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_QT_MULTIMEDIA)
    message(STATUS "Qt6 Multimedia found - system audio output will be available")
  else()
    message(WARNING "Qt6 Multimedia not found - system audio output will be disabled")
  endif()

  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
  set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY AUTOUIC_SEARCH_PATHS src/forms)
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/components/AbsoluteSlider.cpp
    src/components/AbsoluteSlider.hpp
    src/components/ClickableLabel.hpp
    src/components/SceneTree.cpp
    src/components/SceneTree.hpp
    src/components/SliderIgnorewheel.hpp
    src/components/SliderIgnorewheel.cpp
    src/components/MediaControls.cpp
    src/components/MediaControls.hpp
    src/dialogs/MediaEdit.hpp
    src/dialogs/MediaEdit.cpp
    src/models/MediaData.hpp
    src/models/MediaData.cpp
    src/forms/MediaControls.ui
    src/forms/MediaEdit.ui
    src/forms/Soundboard.ui
    src/Soundboard.hpp
    src/Soundboard.cpp
    src/plugin-support.c.in
    src/plugin-support.h
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

# Bundle Qt Multimedia framework for macOS (only if available)
if(APPLE AND ENABLE_QT AND TARGET Qt6::Multimedia)
  # Get Qt Multimedia framework location
  get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION_RELWITHDEBINFO)
  if(NOT QT6_MULTIMEDIA_LOCATION)
    get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION_RELEASE)
  endif()
  if(NOT QT6_MULTIMEDIA_LOCATION)
    get_target_property(QT6_MULTIMEDIA_LOCATION Qt6::Multimedia IMPORTED_LOCATION)
  endif()

  if(QT6_MULTIMEDIA_LOCATION)
    # Get framework directory (go up from lib to framework root)
    get_filename_component(QT6_MULTIMEDIA_FRAMEWORK "${QT6_MULTIMEDIA_LOCATION}/../../.." ABSOLUTE)

    # Install Qt Multimedia framework into plugin bundle
    install(CODE "
      file(COPY \"${QT6_MULTIMEDIA_FRAMEWORK}\"
           DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_name}.plugin/Contents/Frameworks\"
           PATTERN \"Headers\" EXCLUDE
           PATTERN \"*.prl\" EXCLUDE)
    " COMPONENT Runtime)

    # Update RPATH to look in plugin's own Frameworks folder
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
      INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks"
    )
    message(STATUS "Qt6 Multimedia framework will be bundled")
  endif()
endif()
