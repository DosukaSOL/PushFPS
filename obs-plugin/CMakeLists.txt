# PushFPS for OBS Studio: a small libobs module (C, Windows x64).
#
# It is built against OBS's public headers only. The two import libraries come
# from the .def files in cmake/ (the exact libobs functions the plugin calls), so
# the build needs neither a full OBS build nor its dependencies. CI checks the
# result by loading it into a real OBS Studio.
#
#   cmake -S obs-plugin -B build-obs -A x64 -DOBS_SOURCE_DIR=<obs-studio checkout>
#   cmake --build build-obs --config Release
cmake_minimum_required(VERSION 3.24)
project(pushfps_obs VERSION 1.0.0 LANGUAGES C RC)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "PushFPS for OBS is Windows x64 only (PushFPS itself is Windows only).")
endif()

set(OBS_SOURCE_DIR "" CACHE PATH "obs-studio source checkout (headers only; libobs and UI/obs-frontend-api)")
if(NOT EXISTS "${OBS_SOURCE_DIR}/libobs/obs-module.h")
  message(FATAL_ERROR "Set OBS_SOURCE_DIR to an obs-studio checkout (found no libobs/obs-module.h in '${OBS_SOURCE_DIR}').")
endif()
if(EXISTS "${OBS_SOURCE_DIR}/frontend/api/obs-frontend-api.h")
  set(OBS_FRONTEND_INCLUDE "${OBS_SOURCE_DIR}/frontend/api")
else()
  set(OBS_FRONTEND_INCLUDE "${OBS_SOURCE_DIR}/UI/obs-frontend-api")
endif()

# libobs/obs-config.h includes obsconfig.h, which an OBS build generates.
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${GEN}/obsconfig.h" "#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n")

# Import libraries for obs.dll and obs-frontend-api.dll from the .def files.
foreach(lib obs obs-frontend-api)
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${lib}.lib"
    COMMAND "${CMAKE_AR}" /NOLOGO /MACHINE:X64 "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/cmake/${lib}.def"
            "/OUT:${CMAKE_CURRENT_BINARY_DIR}/${lib}.lib"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${lib}.def"
    COMMENT "Import library ${lib}.lib")
  list(APPEND IMPORT_LIBS "${CMAKE_CURRENT_BINARY_DIR}/${lib}.lib")
endforeach()
add_custom_target(pushfps_obs_import_libs DEPENDS ${IMPORT_LIBS})

add_library(pushfps-obs MODULE src/pushfps-obs.c src/pushfps-obs.rc)
add_dependencies(pushfps-obs pushfps_obs_import_libs)
target_include_directories(pushfps-obs PRIVATE "${GEN}" "${OBS_SOURCE_DIR}/libobs" "${OBS_FRONTEND_INCLUDE}")
target_compile_definitions(pushfps-obs PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
target_compile_options(pushfps-obs PRIVATE /W3 /utf-8 /wd4996)
target_link_libraries(pushfps-obs PRIVATE ${IMPORT_LIBS} shell32 user32)
set_target_properties(pushfps-obs PROPERTIES PREFIX "" C_STANDARD 17)
