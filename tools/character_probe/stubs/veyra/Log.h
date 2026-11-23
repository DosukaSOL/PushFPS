// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// tools/character_probe only: a log that prints, in place of the program's own (veyra/Log.h), so the probe
// builds from three source files with the compiler alone.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
namespace veyra::log {
inline void trace(const char*, const std::string&) {}
inline void info(const char* component, const std::string& message) { std::printf("[INFO] [%s] %s\n", component, message.c_str()); }
inline void warn(const char* component, const std::string& message) { std::printf("[WARN] [%s] %s\n", component, message.c_str()); }
inline void error(const char* component, const std::string& message) { std::printf("[ERROR] [%s] %s\n", component, message.c_str()); }
}
