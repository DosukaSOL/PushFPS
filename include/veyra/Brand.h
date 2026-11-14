#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS product identity: the one place that names the product and its GitHub home.
//
// PushFPS is a fork of Veyra (https://github.com/Likely7/Veyra-NRVideo, GPL-3.0). Internal
// identifiers (namespaces, target names, the qml/Veyra module, window classes, data folders)
// keep Veyra's names on purpose so upstream fixes keep merging cleanly; only what a user sees
// uses these constants.

namespace veyra::brand {
inline constexpr const char* kProductName = "PushFPS";
inline constexpr const wchar_t* kProductNameW = L"PushFPS";

// GitHub repository that publishes PushFPS releases. The update checker only accepts a release
// page under kReleaseTagPathPrefix, so these must match the repository's exact owner/name case.
inline constexpr const char* kRepositoryUrl = "https://github.com/DosukaSOL/PushFPS";
inline constexpr const char* kIssuesUrl = "https://github.com/DosukaSOL/PushFPS/issues";
inline constexpr const char* kReleasesUrl = "https://github.com/DosukaSOL/PushFPS/releases";
inline constexpr const char* kLatestReleaseApi = "https://api.github.com/repos/DosukaSOL/PushFPS/releases/latest";
inline constexpr const char* kReleaseTagPathPrefix = "/DosukaSOL/PushFPS/releases/tag/";

// The project PushFPS is built on; shown in About and the README credits.
inline constexpr const char* kUpstreamName = "Veyra";
inline constexpr const char* kUpstreamUrl = "https://github.com/Likely7/Veyra-NRVideo";

// Veyra's ANR (Apex Neural Rendering) frame generation host stays in the source so upstream keeps
// merging, but PushFPS does not offer it: its runtime may not be redistributed yet and its quality
// is not suitable for console play. False hides it from every menu and reports it unavailable.
inline constexpr bool kAnrFrameGeneration = false;
}
