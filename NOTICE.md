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
