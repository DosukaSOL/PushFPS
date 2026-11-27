#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS product identity: the one place that names the product and its GitHub home.
//
// PushFPS is built on Veyra (https://github.com/Likely7/Veyra-NRVideo, GPL-3.0). Internal
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

// Signed updates (ui/UpdateVerify.h): the public half of the key that signs each release's
// SHA256SUMS.txt in CI (repository secret UPDATE_SIGNING_KEY, scripts/release/publish.sh), RSA-3072,
// set by the owner on 2026-11-17. A release without a valid signature only opens its page.
inline constexpr const char* kUpdatePublicKeyPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIIBojANBgkqhkiG9w0BAQEFAAOCAY8AMIIBigKCAYEAzde5/JxCb5Lmm9Vn1AO8\n"
    "+p1catx28QV1oxczwjP/5U7bqEQRMcn2rkQSnaGCoRb8c8WPaKpuB3kxb1iftzxL\n"
    "c4KXQXhlMaaZhQJdqA4UAxv2Jn9Kt1+Fcr/9xmpX/ysAvzcX5rJp76sMOXxN6824\n"
    "x/O+6sEzFQg+6/hBzYrh107owbtSYMOlL41NwK2JlhS8BFVzNW+mpL2eqaZfdSdW\n"
    "+ScEIjvuV0h/4sfsVKFh+TgF1pJ+ScaoPEMub9K51zz9oTXoPk0SyF5+8tBcDf/w\n"
    "fNvIYmHT3El93rNiBtrSlhMBbtVPoum+ozwy3i8hTKqqNwnueTQT0ckrfTuaK31E\n"
    "FvbffEOn6NMixTLACR3fUKGLURCYUp6IOgc7SOe9zU8cWz9S3fIcgQJS/zGvJwaZ\n"
    "V0WtBCCQBFj3+x1AUIid2/BQxFrHivoKlreiQ45eLHSuHnS3P7h17oEz3gEjW9jZ\n"
    "dcO8aD0TnNU2tDY1OUYPWvFr8yn0iDsEEXmh0Zb98blNAgMBAAE=\n"
    "-----END PUBLIC KEY-----\n";
inline constexpr const char* kReleaseDownloadPathPrefix = "/DosukaSOL/PushFPS/releases/download/";

// The project PushFPS is built on; shown in About and the README credits.
inline constexpr const char* kUpstreamName = "Veyra";
inline constexpr const char* kUpstreamUrl = "https://github.com/Likely7/Veyra-NRVideo";

// Veyra's ANR (Apex Neural Rendering) frame generation, offered exactly as Veyra 2.1.6 does: the
// host is in the program, but ANR's own runtime may not be redistributed yet, so no ANR file ships.
// The choice is usable once the user places ANR's files in runtime\experimental\anr. False would
// hide it from every menu and report it unavailable.
inline constexpr bool kAnrFrameGeneration = true;
// ANR is made by Apex Compute (credited in About, the README and the components page). The
// app links to their releases and installs the package a user downloaded from there into
// runtime\experimental\anr on that user's PC; PushFPS itself never ships or mirrors it.
inline constexpr const char* kAnrName = "Apex Neural Rendering (ANR)";
inline constexpr const char* kAnrMaker = "Apex Compute";
inline constexpr const char* kAnrUrl = "https://github.com/apex-compute/ANR";
inline constexpr const char* kAnrReleasesUrl = "https://github.com/apex-compute/ANR/releases";
// PushFlow (PushFPS's own frame generation) runs RIFE (MIT, hzwer); credited in About.
inline constexpr const char* kRifeUrl = "https://github.com/hzwer/Practical-RIFE";
inline constexpr const char* kApexUrl = "https://apexcompute.com";
// PushFPS is made by DosukaSOL; credited in About with a link to X.
inline constexpr const char* kCreatorName = "DosukaSOL";
inline constexpr const char* kCreatorUrl = "https://x.com/DosukaSOL";
// The PushFPS community (the same invitation the README and the website show); About row about-open-discord.
inline constexpr const char* kDiscordUrl = "https://discord.gg/N88znNpSgC";
// PushFPS's website (the button of the Discord status points to it).
inline constexpr const char* kWebsiteUrl = "https://dosukasol.github.io/PushFPS/";
// Discord status (ui/PushDiscordPresence.h): the application the owner made in Discord's developer portal.
// Its id is public (Discord shows it to anyone who looks at an activity); there is no secret for this.
// Discord shows the application's name, PushFPS, after "Playing"; its art asset "pushfps" is the logo.
inline constexpr const char* kDiscordApplicationId = "1575866445412433962";
}
