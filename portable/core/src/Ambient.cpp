// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#include "veyra/portable/Ambient.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace veyra::portable::ambient {
namespace {

// The pixels a dimension's samples read: for every small pixel, n positions spread evenly over its share
// (the shader's origin + (i + 0.5) * step), each the pixel it falls in.
std::vector<int> samples(int size, int cells, int n) {
    std::vector<int> at(size_t(cells) * size_t(n));
    for (int c = 0; c < cells; ++c)
        for (int i = 0; i < n; ++i) {
            const double u = (double(c) + (double(i) + 0.5) / double(n)) / double(cells);
            at[size_t(c) * size_t(n) + size_t(i)] = std::clamp(int(std::floor(u * double(size))), 0, size - 1);
        }
    return at;
}

int tapsFor(int size, int cells) {
    // As many as the share has pixels, at most kTaps: a small frame is not read twice over.
    return std::clamp(int(std::lround(double(size) / double(cells))), 1, kTaps);
}

}  // namespace

Small average(const Bgra& frame) {
    Small out;
    if (frame.empty() || frame.px.size() < size_t(frame.width) * size_t(frame.height) * 4) return out;
    out.width = kWidth;
    out.height = kHeight;
    out.bgr.assign(size_t(kWidth) * size_t(kHeight) * 3, 0.f);
    const int nx = tapsFor(frame.width, kWidth), ny = tapsFor(frame.height, kHeight);
    const std::vector<int> xs = samples(frame.width, kWidth, nx), ys = samples(frame.height, kHeight, ny);
    std::vector<uint32_t> row(size_t(kWidth) * 3);
    for (int cy = 0; cy < kHeight; ++cy) {
        std::fill(row.begin(), row.end(), 0u);
        for (int j = 0; j < ny; ++j) {
            const uint8_t* line = frame.pixel(0, ys[size_t(cy) * size_t(ny) + size_t(j)]);
            for (int cx = 0; cx < kWidth; ++cx) {
                uint32_t b = 0, g = 0, r = 0;
                const int* at = xs.data() + size_t(cx) * size_t(nx);
                for (int i = 0; i < nx; ++i) {
                    const uint8_t* p = line + size_t(at[i]) * 4;
                    b += p[0];
                    g += p[1];
                    r += p[2];
                }
                row[size_t(cx) * 3 + 0] += b;
                row[size_t(cx) * 3 + 1] += g;
                row[size_t(cx) * 3 + 2] += r;
            }
        }
        const float scale = 1.f / float(nx * ny);
        for (size_t k = 0; k < row.size(); ++k) out.bgr[size_t(cy) * size_t(kWidth) * 3 + k] = float(row[k]) * scale;
    }
    return out;
}

Small blur(const Small& small) {
    if (small.width <= 0 || small.height <= 0 || small.bgr.size() != size_t(small.width) * size_t(small.height) * 3) return {};
    std::array<float, 2 * kRadius + 1> w{};
    float sum = 0.f;
    for (int d = -kRadius; d <= kRadius; ++d) {
        w[size_t(d + kRadius)] = std::exp(-float(d * d) / (2.f * kSigma * kSigma));
        sum += w[size_t(d + kRadius)];
    }
    for (float& v : w) v /= sum;
    const int W = small.width, H = small.height;
    const auto at = [](int x, int y, int width) { return (size_t(y) * size_t(width) + size_t(x)) * 3; };
    Small across{W, H, std::vector<float>(small.bgr.size(), 0.f)};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int d = -kRadius; d <= kRadius; ++d) {
                const size_t from = at(std::clamp(x + d, 0, W - 1), y, W), to = at(x, y, W);
                const float k = w[size_t(d + kRadius)];
                for (int c = 0; c < 3; ++c) across.bgr[to + size_t(c)] += small.bgr[from + size_t(c)] * k;
            }
    Small out{W, H, std::vector<float>(small.bgr.size(), 0.f)};
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            for (int d = -kRadius; d <= kRadius; ++d) {
                const size_t from = at(x, std::clamp(y + d, 0, H - 1), W), to = at(x, y, W);
                const float k = w[size_t(d + kRadius)];
                for (int c = 0; c < 3; ++c) out.bgr[to + size_t(c)] += across.bgr[from + size_t(c)] * k;
            }
    return out;
}

Bgra make(const Bgra& frame, float gain) {
    const Small blurred = blur(average(frame));
    if (blurred.width <= 0) return {};
    const float g = std::isfinite(gain) ? std::clamp(gain, 0.f, 1.f) : 0.f;
    Bgra out(blurred.width, blurred.height);
    for (size_t i = 0, n = size_t(blurred.width) * size_t(blurred.height); i < n; ++i)
        for (size_t c = 0; c < 3; ++c)
            out.px[i * 4 + c] = uint8_t(std::clamp(std::lround(blurred.bgr[i * 3 + c] * g), 0L, 255L));
    return out;
}

}  // namespace veyra::portable::ambient
