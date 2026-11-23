// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: the character's own motion between two real frames, for one block a thread
// (pipeline/PushCharacter.h, searchBlock(): the same statements, in whole numbers, so the two cannot differ by
// a rounding). A block of 8 x 8 at 480 x 270 looks over a 16 x 16 window for the half-motion d that makes the
// older matted picture at p - d and the newer at p + d alike, then looks again at 960 x 540 round twice that
// answer, and a parabola brings it under a pixel. Written: (x, y) in pixels of 960 x 540, how distinct the
// answer is, and 1 for a block that has matte in its window (0 0 0 0 otherwise).
Texture2D<float4> older0 : register(t0);   // 480 x 270
Texture2D<float4> newer0 : register(t1);
Texture2D<float4> older1 : register(t2);   // 960 x 540
Texture2D<float4> newer1 : register(t3);
RWTexture2D<float4> blocksOut : register(u0);
cbuffer Constants : register(b0) {
    uint unused0, unused1, unused2, unused3;
};
static const int kBlock = 8, kWindow = 16, kReachX = 8, kReachY = 4;
static const int kCap = 382, kStepCost = 3, kMass = 1020;
static const float kWhole = 1530.0;
static const int2 kCoarse = int2(480, 270), kFine = int2(960, 540);
static const int kAcross = 2 * kReachX + 1;
int4 at(Texture2D<float4> t, int2 p, int2 size) { return int4(t[clamp(p, int2(0, 0), size - 1)] * 255.0 + 0.5); }
int difference(int4 a, int4 b) {
    const int4 d = abs(a - b);
    return 2 * (d.x + d.y + d.z) + 3 * d.w;
}
uint apart0(int2 lo, int2 hi, int2 d) {
    uint sum = 0;
    [loop] for (int y = lo.y; y < hi.y; ++y)
        [loop] for (int x = lo.x; x < hi.x; ++x) sum += uint(min(difference(at(older0, int2(x, y) - d, kCoarse), at(newer0, int2(x, y) + d, kCoarse)), kCap));
    return sum;
}
uint apart1(int2 lo, int2 hi, int2 d) {
    uint sum = 0;
    [loop] for (int y = lo.y; y < hi.y; ++y)
        [loop] for (int x = lo.x; x < hi.x; ++x) sum += uint(min(difference(at(older1, int2(x, y) - d, kFine), at(newer1, int2(x, y) + d, kFine)), kCap));
    return sum;
}
float parabola(uint left, uint middle, uint right) {
    const int den = int(left) - 2 * int(middle) + int(right);
    if (den <= 0) return 0.0;
    return clamp(0.5 * float(int(left) - int(right)) / float(den), -0.5, 0.5);
}
[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 60 || id.y >= 34) return;
    const int2 b = int2(id.xy);
    const int edge = (kWindow - kBlock) / 2;
    const int2 lo = max(kBlock * b - edge, int2(0, 0)), hi = min(kBlock * b - edge + kWindow, kCoarse);
    uint mass = 0;
    [loop] for (int y = lo.y; y < hi.y; ++y)
        [loop] for (int x = lo.x; x < hi.x; ++x) mass += uint(max(at(older0, int2(x, y), kCoarse).w, at(newer0, int2(x, y), kCoarse).w));
    if (mass < uint(kMass)) { blocksOut[id.xy] = float4(0.0, 0.0, 0.0, 0.0); return; }
    const uint pixels = uint((hi.x - lo.x) * (hi.y - lo.y));
    uint sums[153];
    [loop] for (int n = 0; n < 153; ++n) sums[n] = 0;
    uint best = 0xFFFFFFFFu;
    int2 bd = int2(0, 0);
    [loop] for (int dy = -kReachY; dy <= kReachY; ++dy)
        [loop] for (int dx = -kReachX; dx <= kReachX; ++dx) {
            const uint s = apart0(lo, hi, int2(dx, dy));
            sums[(dy + kReachY) * kAcross + dx + kReachX] = s;
            const uint cost = 2u * s + uint(kStepCost) * pixels * uint(abs(dx) + abs(dy));
            if (cost < best) { best = cost; bd = int2(dx, dy); }
        }
    uint ring = 0, count = 0;
    [loop] for (int ey = -1; ey <= 1; ++ey)
        [loop] for (int ex = -1; ex <= 1; ++ex) {
            if (ex == 0 && ey == 0) continue;
            const int2 d = bd + int2(ex, ey);
            if (d.x < -kReachX || d.x > kReachX || d.y < -kReachY || d.y > kReachY) continue;
            ring += sums[(d.y + kReachY) * kAcross + d.x + kReachX];
            ++count;
        }
    const uint own = sums[(bd.y + kReachY) * kAcross + bd.x + kReachX];
    const float gain = float(ring) / float(count) - float(own);
    const float sure = (gain > 0.0 ? gain : 0.0) / (float(pixels) * kWhole);
    const int fineEdge = kWindow - kBlock;
    const int2 flo = max(2 * kBlock * b - fineEdge, int2(0, 0)), fhi = min(2 * kBlock * b - fineEdge + 2 * kWindow, kFine);
    uint fineSums[25];
    [loop] for (int q = 0; q < 25; ++q) fineSums[q] = 0;
    [loop] for (int fy = -2; fy <= 2; ++fy)
        [loop] for (int fx = -2; fx <= 2; ++fx) fineSums[(fy + 2) * 5 + fx + 2] = apart1(flo, fhi, 2 * bd + int2(fx, fy));
    uint fineBest = 0xFFFFFFFFu;
    int2 fe = int2(0, 0);
    [loop] for (int gy = -1; gy <= 1; ++gy)
        [loop] for (int gx = -1; gx <= 1; ++gx) {
            const uint s = fineSums[(gy + 2) * 5 + gx + 2];
            if (s < fineBest) { fineBest = s; fe = int2(gx, gy); }
        }
    const int k = (fe.y + 2) * 5 + fe.x + 2;
    const float mx = float(2 * bd.x + fe.x) + parabola(fineSums[k - 1], fineSums[k], fineSums[k + 1]);
    const float my = float(2 * bd.y + fe.y) + parabola(fineSums[k - 5], fineSums[k], fineSums[k + 5]);
    blocksOut[id.xy] = float4(mx, my, sure, 1.0);
}
