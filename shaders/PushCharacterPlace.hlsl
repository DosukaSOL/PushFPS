// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS Character Hold: one block's place in the frame the method made (pipeline/PushCharacter.h,
// placeBlock(): the same statements, whole numbers decide). The tables of its 3 x 3 neighbourhood are added up
// offset by offset (each neighbour's table stands round its own centre), so that both edges of a head count;
// the cheapest offset within 3, with a step's cost for leaving the search's answer; then a parabola.
// Written: (x, y) from a generated pixel to its place in the source frame, in pixels of 960 x 540, what is
// left apart there a pixel, and 1 where a place was found (the search's answer times the share otherwise).
Texture2D<uint> tablesIn : register(t0);
Texture2D<float4> blocksIn : register(t1);  // (x, y, how distinct, judged)
Texture2D<float4> field : register(t2);     // (x, y, trust, 0)
RWTexture2D<float4> placedOut : register(u0);
cbuffer Constants : register(b0) {
    float share;
    uint unused0, unused1, unused2;
};
static const int kReach = 4, kAcross = 9;
static const uint kLeast = 576u, kScale = 32u, kStep = 48u, kNoAnswer = 0xFFFFFFFFu;
static const int2 kBlocks = int2(60, 34);
float parabola(uint left, uint middle, uint right) {
    const int den = int(left) - 2 * int(middle) + int(right);
    if (den <= 0) return 0.0;
    return clamp(0.5 * float(int(left) - int(right)) / float(den), -0.5, 0.5);
}
int2 centreOf(float2 motion) {              // as PushCharacterTables
    precise float2 v = share * motion;
    return int2(floor(v + 0.5));
}
[numthreads(4, 4, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= 60 || id.y >= 34) return;
    const int2 b = int2(id.xy);
    const float2 start = share * field[b].xy;
    if (blocksIn[b].w < 0.5) { placedOut[id.xy] = float4(start, 0.0, 0.0); return; }
    const int2 centre = centreOf(field[b].xy);
    uint apart[81];
    uint solid[81];
    uint means[81];
    [loop] for (int n = 0; n < 81; ++n) { apart[n] = 0u; solid[n] = 0u; means[n] = kNoAnswer; }
    [loop] for (int j = -1; j <= 1; ++j)
        [loop] for (int i = -1; i <= 1; ++i) {
            const int2 nb = b + int2(i, j);
            if (nb.x < 0 || nb.x >= kBlocks.x || nb.y < 0 || nb.y >= kBlocks.y) continue;
            if (blocksIn[nb].w < 0.5) continue;
            const int2 s = centre - centreOf(field[nb].xy);
            [loop] for (int ey = -kReach; ey <= kReach; ++ey) {
                const int ny = ey + s.y;
                if (ny < -kReach || ny > kReach) continue;
                [loop] for (int ex = -kReach; ex <= kReach; ++ex) {
                    const int nx = ex + s.x;
                    if (nx < -kReach || nx > kReach) continue;
                    const uint v = tablesIn[int2(kAcross * nb.x + nx + kReach, kAcross * nb.y + ny + kReach)];
                    const int to = (ey + kReach) * kAcross + ex + kReach;
                    apart[to] += v >> 11; solid[to] += v & 2047u;
                }
            }
        }
    [loop] for (int m = 0; m < 81; ++m)
        if (solid[m] >= kLeast) means[m] = (kScale * apart[m]) / solid[m];
    uint best = kNoAnswer;
    int2 be = int2(0, 0);
    [loop] for (int cy = 1 - kReach; cy <= kReach - 1; ++cy)
        [loop] for (int cx = 1 - kReach; cx <= kReach - 1; ++cx) {
            const uint candidate = means[(cy + kReach) * kAcross + cx + kReach];
            if (candidate == kNoAnswer) continue;
            const uint cost = candidate + kStep * uint(abs(cx) + abs(cy));
            if (cost < best) { best = cost; be = int2(cx, cy); }
        }
    if (best == kNoAnswer) { placedOut[id.xy] = float4(start, 0.0, 0.0); return; }
    const int at = (be.y + kReach) * kAcross + be.x + kReach;
    const uint own = means[at], left = means[at - 1], right = means[at + 1], up = means[at - kAcross], down = means[at + kAcross];
    float2 found = float2(centre + be);
    if (left != kNoAnswer && right != kNoAnswer) found.x += parabola(left, own, right);
    if (up != kNoAnswer && down != kNoAnswer) found.y += parabola(up, own, down);
    placedOut[id.xy] = float4(found, float(own) / float(kScale), 1.0);
}
