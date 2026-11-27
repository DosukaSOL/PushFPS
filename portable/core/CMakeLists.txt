# SPDX-License-Identifier: GPL-3.0-or-later
# PushFPS for Linux and macOS: the platform-independent core (no Qt, no GPU, no vendor SDK).
add_library(pushfps_portable_core STATIC
  src/Convert.cpp
  src/Motion.cpp
  src/Interpolate.cpp
  src/CrosshairHold.cpp
  src/Engine.cpp
  # Automatic HUD detection: the Windows build's detector (pure C++), used as it is.
  ${CMAKE_CURRENT_SOURCE_DIR}/../../src/engine/PushHud.cpp)
target_include_directories(pushfps_portable_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  # PushPacingRepair.h (pure C++) is shared with the Windows build.
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_compile_features(pushfps_portable_core PUBLIC cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(pushfps_portable_core PUBLIC Threads::Threads)
if(MSVC)
  target_compile_options(pushfps_portable_core PRIVATE /W4)
else()
  target_compile_options(pushfps_portable_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
endif()

if(PUSHFPS_PORTABLE_TESTS)
  add_executable(pushfps_portable_core_tests tests/CoreTests.cpp)
  target_link_libraries(pushfps_portable_core_tests PRIVATE pushfps_portable_core)
  add_test(NAME portable_core COMMAND pushfps_portable_core_tests)
endif()
