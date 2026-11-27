#version 440
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
layout(location = 0) in vec4 qt_VertexPosition;
layout(location = 1) in vec2 qt_VertexTexCoord;
layout(location = 0) out vec2 vTex;

// Must match frame.frag and FrameView.cpp (std140, 288 bytes).
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float phase;
    float mode;
    float hudOn;
    vec2 frameSize;
    vec2 fieldGrid;
    float blockSize;
    float errLow;
    float errHigh;
    float brightness;
    float contrast;
    float saturation;
    float gamma;
    float pad0;
    vec4 holdZone;
    vec4 subZone;
    vec4 subFlags;
    vec4 compare;
    vec4 compareLine;
    vec4 boxes[4];
    vec4 boxFlags;
};

void main() {
    vTex = qt_VertexTexCoord;
    gl_Position = qt_Matrix * qt_VertexPosition;
}
