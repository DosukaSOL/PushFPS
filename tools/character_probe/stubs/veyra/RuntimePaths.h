// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// tools/character_probe only: where the compiled shaders are, from the environment (VEYRA_PROBE_SHADERS).
#pragma once
#include <cstdlib>
#include <filesystem>
namespace veyra::runtime {
inline std::filesystem::path shaderDirectory() {
    char* value = nullptr; size_t length = 0;
    std::filesystem::path p = ".";
    if (_dupenv_s(&value, &length, "VEYRA_PROBE_SHADERS") == 0 && value) { p = value; std::free(value); }
    return p;
}
inline std::filesystem::path localDataDirectory() { return std::filesystem::temp_directory_path(); }
}
