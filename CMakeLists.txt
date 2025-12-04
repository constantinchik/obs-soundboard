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
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core Multimedia)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets Qt6::Multimedia)
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

# Bundle Qt Multimedia framework and plugins for macOS
if(APPLE AND ENABLE_QT)
  # Get Qt installation prefix
  get_target_property(QT6_QMAKE_EXECUTABLE Qt6::qmake IMPORTED_LOCATION)
  execute_process(
    COMMAND "${QT6_QMAKE_EXECUTABLE}" -query QT_INSTALL_PREFIX
    OUTPUT_VARIABLE QT_INSTALL_PREFIX
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  set(QT6_MULTIMEDIA_FRAMEWORK "${QT_INSTALL_PREFIX}/lib/QtMultimedia.framework")
  set(QT6_MULTIMEDIA_PLUGINS_DIR "${QT_INSTALL_PREFIX}/plugins/multimedia")

  # Install Qt Multimedia framework
  if(EXISTS "${QT6_MULTIMEDIA_FRAMEWORK}")
    install(DIRECTORY "${QT6_MULTIMEDIA_FRAMEWORK}"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/${_name}.plugin/Contents/Frameworks"
            PATTERN "Headers" EXCLUDE
            PATTERN "*.prl" EXCLUDE)
    message(STATUS "Qt6 Multimedia framework will be bundled from: ${QT6_MULTIMEDIA_FRAMEWORK}")
  else()
    message(WARNING "Qt6 Multimedia framework not found at: ${QT6_MULTIMEDIA_FRAMEWORK}")
  endif()

  # Install Qt Multimedia plugins
  if(EXISTS "${QT6_MULTIMEDIA_PLUGINS_DIR}")
    install(DIRECTORY "${QT6_MULTIMEDIA_PLUGINS_DIR}"
            DESTINATION "${CMAKE_INSTALL_PREFIX}/${_name}.plugin/Contents/PlugIns")
    message(STATUS "Qt6 Multimedia plugins will be bundled from: ${QT6_MULTIMEDIA_PLUGINS_DIR}")
  else()
    message(WARNING "Qt6 Multimedia plugins not found at: ${QT6_MULTIMEDIA_PLUGINS_DIR}")
  endif()

  # Update RPATH to look in plugin's own Frameworks folder
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "@loader_path/../Frameworks;@executable_path/../Frameworks"
  )
endif()
