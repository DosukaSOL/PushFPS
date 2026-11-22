# SPDX-License-Identifier: GPL-3.0-or-later
# PushFPS for Linux and macOS: the Qt application.
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Multimedia ShaderTools)
qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(pushfps_portable
  src/main.cpp
  src/AppController.cpp src/AppController.h
  src/AudioPassthrough.cpp src/AudioPassthrough.h
  src/CaptureSource.cpp src/CaptureSource.h
  src/FrameView.cpp src/FrameView.h
  src/Settings.cpp src/Settings.h
  src/TestPattern.cpp src/TestPattern.h
  src/Translator.cpp src/Translator.h
  src/VideoFrames.cpp src/VideoFrames.h)
set_target_properties(pushfps_portable PROPERTIES
  OUTPUT_NAME PushFPS
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in
  MACOSX_BUNDLE_GUI_IDENTIFIER io.github.dosukasol.pushfps
  MACOSX_BUNDLE_BUNDLE_NAME PushFPS
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
  MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
target_compile_definitions(pushfps_portable PRIVATE PUSHFPS_PORTABLE_VERSION="${PROJECT_VERSION}")
if(APPLE)
  # App Nap off while PushFPS runs (src/MacActivity.mm; OBJCXX is enabled in ../CMakeLists.txt).
  target_sources(pushfps_portable PRIVATE src/MacActivity.mm src/MacActivity.h)
  set_source_files_properties(src/MacActivity.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  target_link_libraries(pushfps_portable PRIVATE "-framework Foundation")
  target_compile_definitions(pushfps_portable PRIVATE PUSHFPS_MAC_ACTIVITY=1)
endif()
# The QML type registration (generated) includes FrameView.h by name.
target_include_directories(pushfps_portable PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# The files are kept in qml/ here but must sit next to the module's qmldir in the resources: in a
# folder of their own, that folder is also an implicit import that offers Theme.qml as a plain type,
# and Qt picks one or the other at random on each start (Qt 6.12: about four starts in five drew the
# whole interface without its colours, sizes and fonts).
set(pushfps_qml_files
    qml/Main.qml qml/HomePage.qml qml/PlayerPage.qml qml/SettingsPage.qml qml/Theme.qml qml/Card.qml
    qml/SettingRow.qml qml/Segmented.qml qml/Toggle.qml qml/Choice.qml qml/Slider2.qml qml/Button2.qml
    qml/GpuCheck.qml)
set(pushfps_qml_resources qml/logo.png qml/icon.png)
foreach(file IN LISTS pushfps_qml_files pushfps_qml_resources)
  get_filename_component(name "${file}" NAME)
  set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
qt_add_qml_module(pushfps_portable
  URI PushFPS
  VERSION 1.0
  # Not build/app/PushFPS: that is the program's own name.
  OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/PushFPS
  QML_FILES ${pushfps_qml_files}
  RESOURCES ${pushfps_qml_resources})

# Interpolation shader. texelFetch needs GLSL ES 3.0 / GLSL 3.3 at least.
# BATCHABLE: Qt Quick's renderer asks for the batchable variant of a material's vertex shader.
qt_add_shaders(pushfps_portable "pushfps_shaders"
  PREFIX "/"
  BATCHABLE
  GLSL "300 es,330"
  HLSL 50
  MSL 12
  FILES
    shaders/frame.vert
    shaders/frame.frag)

# The translation catalog of the Windows build (keyed by the Chinese source text) is used as is.
qt_add_resources(pushfps_portable "pushfps_i18n"
  PREFIX "/i18n"
  BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../i18n
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../i18n/catalog.json)

target_link_libraries(pushfps_portable PRIVATE
  pushfps_portable_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia)
if(NOT MSVC)
  target_compile_options(pushfps_portable PRIVATE -Wall -Wextra)
endif()

install(TARGETS pushfps_portable
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)
