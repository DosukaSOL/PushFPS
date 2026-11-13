# PushFPS notice

PushFPS: console frame generation from a capture card.
Copyright (C) 2026 DosukaSOL and the PushFPS contributors.

PushFPS is a **modified version of Veyra** (https://github.com/Likely7/Veyra-NRVideo),
Copyright (C) Likely7 and the Veyra contributors.

## Modifications (GPL-3.0 section 5a)

PushFPS was forked from Veyra on **2026-11-12** and has been modified by the PushFPS contributors since
then (latest change: see the date of the newest commit). The modifications, in short:

- Product name, logo, icon, update channel and links (`include/veyra/Brand.h` and the files that use it).
- Interface: quick setup with console and game profiles, home screen layout, row layout and theme
  (`qml/Veyra/PushQuickSetup.qml`, `HomePage.qml`, `VRow.qml`, `Theme.qml`, `ProPage.qml`, `TopDock.qml`
  and others), with their translations in `i18n/catalog.json`.
- Builds without the NVIDIA Optical Flow SDK (`src/ngx/NvOfSessionUnavailable.cpp`).
- Windows installer and release packaging (`packaging/`), build and test workflows (`.github/workflows/`).
- OBS Studio plugin (`obs-plugin/`).

The complete, dated list of changes is the Git history of https://github.com/DosukaSOL/PushFPS, starting
after Veyra's commit that PushFPS was forked from.

## License

PushFPS is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License version 3 as published by the Free Software Foundation (see [LICENSE](LICENSE)). The files
written for PushFPS are additionally available under any later version of that license.

PushFPS is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

The PS5 Remote Play part (Chiaki) is also under the AGPL-3.0 with an OpenSSL exception; see
[licenses/remoteplay](licenses/remoteplay). PushFPS release builds leave that part out.

## Additional permission under GNU GPL version 3 section 7

This permission is given by the copyright holders of the parts written for PushFPS (DosukaSOL and the
PushFPS contributors). It applies to those parts only; it does not change the terms of the parts written
by Veyra's authors (see below).

If you modify PushFPS, or any covered work, by linking or combining it with any of the following, containing
parts covered by the terms of their own licenses: the NVIDIA RTX SDKs (DLSS SDK and NGX SDK), the NVIDIA
Video Effects and Video Frame Generation SDKs, the NVIDIA Optical Flow SDK, the NVIDIA CUDA driver API
headers, the Intel XeSS SDK, the AMD FidelityFX SDK, or the unmodified runtime libraries and models these SDKs
provide (together, "the SDKs"), the licensors of PushFPS grant you additional permission to convey the
resulting work. Corresponding Source for a non-source form of such a combination does not need to include
the source code of the parts of the SDKs used, but shall include the source code of PushFPS. The SDKs remain
under their own licenses, and nothing here places them under the GPL.

**Veyra's code.** PushFPS.exe also contains code written by Veyra's authors, which they license under
GPL-3.0. Veyra's author builds and distributes Veyra with the same SDKs, and PushFPS distributes the same
combination. A permission like the one above for Veyra's code can only come from Veyra's copyright holders.

## NVIDIA SDK attribution

PushFPS.exe incorporates the NVIDIA DLSS SDK and NGX SDK (version 310.7.0) under the NVIDIA RTX SDKs License,
whose text is in `licenses/sdk` of every release. NVIDIA, NVIDIA RTX and DLSS are trademarks of NVIDIA
Corporation. PushFPS is not sponsored or endorsed by NVIDIA. The app shows this attribution under
*Settings, About*.

## What the installer leaves out

The installer downloads Veyra's runtime package from Veyra's own release and installs only files that may
be passed on under their vendors' terms:

- Files that say they are from NVIDIA but are not originals with a valid NVIDIA signature (for example a
  community-modified DLSS build) are not installed.
- On AMD installs, the experimental AI enhancement runtimes that run NVIDIA's model on Radeon cards
  (`runtime\amd-nr-rdna3`, `runtime\amd-nr`) are not installed: NVIDIA licenses its DLSS runtimes for
  systems with NVIDIA graphics only. FSR and XeSS frame generation are not affected.

The installer lists anything it left out in `PushFPS-VERSION.txt` in the install folder, and CI fails a build
whose test install contains such a file. `.github/workflows/audit-runtime.yml` produces a full report of the
runtime package (publisher, version and signature of every program file).

## Source code

The Corresponding Source of every PushFPS release is the Git tag of the same name in
https://github.com/DosukaSOL/PushFPS. The build recipe (exact dependency versions and commands) is
`.github/workflows/build-windows.yml` in that tag; the OBS plugin is built by the `obs-plugin` job of the
same workflow.

## Third-party components

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the [licenses](licenses) folder. Vendor SDKs
(NVIDIA DLSS, Intel XeSS, AMD FidelityFX) are downloaded at build time from their publishers under their
own licenses and are not part of this repository. The runtime package the installer downloads is
distributed by Veyra's author from Veyra's GitHub releases; PushFPS does not rehost it.
