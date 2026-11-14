# NVIDIA Image Scaling SDK 1.0.3

Upstream: https://github.com/NVIDIAGameWorks/NVIDIAImageScaling
Commit: 35e13ba316c98eeecf16f37eae70ce88019911f6 (2022-08-22)
Licence: MIT; see LICENSE.txt (upstream `licence.txt`) and the header of each file.
Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES.

Copied unchanged:

| File | SHA-256 |
|---|---|
| NIS_Scaler.h | 2d3bd5b45d6a0c4169fe36dfe349e7ff5a099dff6fb6876f8a3286489f74d3bf |
| NIS_Config.h | f33c5dd82bb63f3c1cc7cefd86690053de0bd3fe0e730463a5e63e90e7778754 |

Source code only: no NVIDIA binary, runtime or model is included. The upstream samples and their
third-party libraries are not copied.

PushFPS compiles `NIS_Scaler.h` through its own wrapper `shaders/NisScaler.hlsl` (registers chosen to fit
PushFPS's compute pass helper) and uses `NIS_Config.h` on the CPU for the shader constants and the
filter coefficient tables (`src/pipeline/NisPass.cpp`). NIS is offered as the "NVIDIA Image Scaling"
choice of the super-resolution stage.
