# SPDX-License-Identifier: GPL-3.0-or-later
# PushFPS for Linux and macOS: the Qt application.
# 6.6: Qt's rendering interface (QRhi) and the command buffer of a scene graph render node, which
# upscaling uses (src/NisNode.h).
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick Multimedia ShaderTools)
# Since Qt 6.10 the private targets (Qt6::GuiPrivate, for rhi/qrhi.h) are a package of their own; before,
# Gui brought them.
# Using them ties the program to the Qt it was built with, which is how it ships: the AppImage and the
# macOS app carry their own Qt.
set(QT_NO_PRIVATE_MODULE_WARNING ON)
if(NOT TARGET Qt6::GuiPrivate)
  find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
endif()
qt_standard_project_setup(REQUIRES 6.6)

qt_add_executable(pushfps_portable
  src/main.cpp
  src/AppController.cpp src/AppController.h
  src/AudioPassthrough.cpp src/AudioPassthrough.h
  src/CaptureSource.cpp src/CaptureSource.h
  src/FrameView.cpp src/FrameView.h
  src/HudDetection.cpp src/HudDetection.h
  src/NisNode.cpp src/NisNode.h
  src/Settings.cpp src/Settings.h
  src/TestPattern.cpp src/TestPattern.h
  src/Translator.cpp src/Translator.h
  src/VideoFrames.cpp src/VideoFrames.h
  src/VeyraShim.cpp src/VeyraShim.h)
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

# The interface parts shared with the Windows build: the same files, compiled into this module, so a
# change to the design reaches both. They read a few things from the Windows bridge, which the preview
# answers with its own `veyra` object (src/VeyraShim.h).
set(pushfps_shared_dir ${CMAKE_CURRENT_SOURCE_DIR}/../../qml/Veyra)
set(pushfps_shared_names
    Theme.qml IconData.js PushShell.js VIcon.qml VAccentFill.qml VButton.qml VSwitch.qml VSeg.qml VSelect.qml VMenu.qml
    VRow.qml VHelp.qml VSliderValue.qml VSlider.qml VCard.qml VDot.qml VTag.qml VScrollBar.qml VAccordion.qml VSubGroup.qml VEyebrow.qml PushFoldHead.qml
    PushSection.qml PushSidebar.qml PushTitleBar.qml PushStatusStrip.qml PushWordmark.qml DialogTextField.qml PushLatencyBadge.qml PushDiagnose.js
    PushCaptureCards.js PushCrop.js PushGamePresets.js)
set(pushfps_shared_files)
foreach(name IN LISTS pushfps_shared_names)
  list(APPEND pushfps_shared_files ${pushfps_shared_dir}/${name})
endforeach()
set_source_files_properties(${pushfps_shared_dir}/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# The preview's own pages and panels.
set(pushfps_own_files
    qml/Main.qml qml/HomePage.qml qml/ProPage.qml qml/MinimalPage.qml qml/SettingsPage.qml qml/GpuCheck.qml
    qml/SourcesPanel.qml qml/QuickSettings.qml qml/PictureSettings.qml qml/SoundSettings.qml qml/LiveFigures.qml
    qml/PicturePlaceholder.qml qml/PanelTitle.qml qml/UpscaleRow.qml qml/SuperResolutionCard.qml qml/ConsoleProfiles.qml qml/CheckSetupDialog.qml
    qml/CropDialog.qml qml/CropRow.qml
    qml/HudDialog.qml qml/GamePresetsDialog.qml qml/RowList.qml)
set(pushfps_qml_resources qml/logo.png qml/icon.png)
# Every file sits next to the module's qmldir in the resources: in a folder of its own, that folder is
# also an implicit import that offers Theme.qml as a plain type, and Qt picks one or the other at random
# on each start (Qt 6.12: about four starts in five drew the whole interface without its colours, sizes
# and fonts).
set(pushfps_qml_files ${pushfps_own_files} ${pushfps_shared_files})
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

# Upscaling with NVIDIA Image Scaling (src/NisNode.h). The compute shader is NVIDIA's NIS_Scaler.h,
# unchanged, inside shaders/nis.comp.in: qsb has no #include, so the two are put together here into the
# build folder (written again only when either changes). A compute shader needs GLSL 4.30 on OpenGL.
set(pushfps_nis_header ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/nis/NIS_Scaler.h)
set(pushfps_nis_wrapper ${CMAKE_CURRENT_SOURCE_DIR}/shaders/nis.comp.in)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${pushfps_nis_header} ${pushfps_nis_wrapper})
file(READ ${pushfps_nis_wrapper} pushfps_nis_text)
file(READ ${pushfps_nis_header} pushfps_nis_scaler)
string(REPLACE "// @NIS_SCALER_H@" "${pushfps_nis_scaler}" pushfps_nis_text "${pushfps_nis_text}")
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/nis.comp CONTENT "${pushfps_nis_text}" @ONLY)
qt_add_shaders(pushfps_portable "pushfps_nis_compute"
  PREFIX "/"
  GLSL "430"
  HLSL 50
  MSL 12
  FILES ${CMAKE_CURRENT_BINARY_DIR}/shaders/nis.comp
  OUTPUTS shaders/nis.comp.qsb)
qt_add_shaders(pushfps_portable "pushfps_nis_blit"
  PREFIX "/"
  GLSL "300 es,330"
  HLSL 50
  MSL 12
  FILES
    shaders/blit.vert
    shaders/blit.frag)

# The translation catalog of the Windows build (keyed by the Chinese source text) is used as is.
qt_add_resources(pushfps_portable "pushfps_i18n"
  PREFIX "/i18n"
  BASE ${CMAKE_CURRENT_SOURCE_DIR}/../../i18n
  FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../i18n/catalog.json)

target_link_libraries(pushfps_portable PRIVATE
  pushfps_portable_core Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia
  # QRhi's headers (rhi/qrhi.h) come with Qt's GUI module as an API with limited compatibility promises.
  Qt6::GuiPrivate)
if(NOT MSVC)
  target_compile_options(pushfps_portable PRIVATE -Wall -Wextra)
endif()

install(TARGETS pushfps_portable
  BUNDLE DESTINATION .
  RUNTIME DESTINATION bin)

# NVIDIA Image Scaling is MIT-licensed: its notice goes with every copy of the program, which carries its
# shader (Linux: usr/share/doc/pushfps in the AppImage and the folder; macOS: the bundle's Resources).
set(pushfps_nis_licence ${CMAKE_CURRENT_SOURCE_DIR}/../../licenses/NVIDIA_Image_Scaling_SDK_MIT.txt)
if(APPLE)
  target_sources(pushfps_portable PRIVATE ${pushfps_nis_licence})
  set_source_files_properties(${pushfps_nis_licence} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/licenses)
else()
  install(FILES ${pushfps_nis_licence} DESTINATION share/doc/pushfps)
endif()
