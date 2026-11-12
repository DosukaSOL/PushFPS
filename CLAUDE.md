# PushFPS — notes for Claude and other agents

PushFPS is a fork of Veyra (https://github.com/Likely7/Veyra-NRVideo, GPL-3.0) focused on console
frame generation from a capture card. Owner: DosukaSOL. Repository: https://github.com/DosukaSOL/PushFPS.

These rules take precedence over `AGENTS.md`. `AGENTS.md` is Veyra upstream's own work log (Chinese,
with paths on the upstream developer's machine); read it for history and engineering contracts, but its
paths, task lists and push/release rules are not ours. Do not edit it: it changes upstream almost daily
and edits would conflict on every merge.

## Rules

1. **Keep upstream mergeable.** Internal identifiers keep Veyra's names: namespaces (`veyra::`), CMake
   targets, the `qml/Veyra` QML module, window classes, data folders, env vars (`VEYRA_*`). Only what a
   user sees says PushFPS. Product name and links live in `include/veyra/Brand.h`; use it instead of
   writing the name or URLs again.
2. **Translations are keyed by the Simplified Chinese source string.** Every user-visible string is
   written in Chinese in `qsTr(...)` / `tr(...)` and translated in `i18n/catalog.json` (zh-TW, en, ja).
   After changing or adding a string run `python3 -B scripts/i18n/extract.py`, fill the new entries, then
   `python3 -B scripts/i18n/extract.py --check` must report 0 missing and 0 placeholder problems.
3. **QML is loaded from disk next to the exe** (`qml/Veyra`), so new QML files need a line in
   `qml/Veyra/qmldir` but no CMake change. Run the QML lint workflow (`.github/workflows/checks.yml`) on
   every QML change; there is no other QML check without a Windows build.
4. **Never commit** SDKs, NVIDIA/AMD/Intel runtimes or models, build output, or user data
   (`.gitignore` covers `third_party_local/`, `runtime_local/`, `out/`, `build/`).
5. **License.** Everything stays GPL-3.0 (the Chiaki remote-play part also AGPL-3.0). Credit Veyra and
   Likely7 wherever the product is described. Copy code from other projects only with a compatible license
   and record it in `THIRD_PARTY_NOTICES.md`.
6. **Be honest about verification.** Say what was built, run and tested, and on which hardware. A change
   that only passed static checks is "unverified on Windows".

## PushFPS-specific code

| What | Where |
|---|---|
| Product name, GitHub links, upstream credit | `include/veyra/Brand.h` |
| Quick setup (target FPS, AI enhancement, low latency) | `qml/Veyra/PushQuickSetup.qml`, used in `HomePage.qml` and `ProPage.qml` |
| Program name `PushFPS.exe` | `CMakeLists.txt` (`OUTPUT_NAME` on `veyra_qml_ui`) |
| Logo and icon | `qml/Veyra/logo.png`, `assets/veyra.ico` (PushFPS artwork, upstream file name), `assets/pushfps-*.png` |

## Building and releasing

- `.github/workflows/build-windows.yml` is the build recipe (pinned, login-free dependencies). It builds
  `PushFPS.exe`, runs the GPU-free unit tests, checks the exe's Qt/FFmpeg imports against Veyra's runtime
  package, packages `packaging/make-release.ps1`, then installs with `packaging/Install-PushFPS.ps1` and
  starts the program; QML errors at startup fail the build. FFmpeg (~40 min) is cached by vcpkg commit.
- Releases: push a tag `vX.Y.Z` (stable numbers only: the in-app update checker ignores other tags). The
  workflow publishes it as a GitHub **pre-release**; mark it as the latest release only after a person has
  tested it with a real capture card and GPU. Builds without a tag are versioned `2.1.5-pushfps.<run>`.
- The upstream runtime package version is `UPSTREAM_TAG` in the workflow; its SHA-256 values are pinned into
  the installer at build time from Veyra's own `SHA256SUMS.txt`. Moving to a newer Veyra release means
  merging upstream and changing `UPSTREAM_TAG` together.
- Release builds have no NVIDIA Optical Flow SDK (login-only): `src/ngx/NvOfSessionUnavailable.cpp` makes
  NVOF report unavailable and the app uses AMD FidelityFX optical flow.

## Syncing with Veyra

```
git remote add upstream https://github.com/Likely7/Veyra-NRVideo.git   # once
git fetch upstream main
git merge upstream/main
```

Expected conflicts: `README.md`, `README_CN.md`, `README_EN.md` (keep ours), `i18n/catalog.json` (merge,
then re-run `extract.py` and `--check`), and any file listed in the table above.
