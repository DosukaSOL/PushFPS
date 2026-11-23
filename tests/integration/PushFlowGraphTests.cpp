// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFlow through the real enhancement graph (pipeline/EnhanceGraph), the way the program runs it.
//
// The other PushFlow tests drive PushFlowBackend directly. That is how 3.2.0 shipped with the graph
// never offering PushFlow a pair: EnhanceGraph::fgCreated() left it out, so fgCandidates stayed 0 and
// the picture stayed at the source's frame rate (found and fixed by PichotM, 18 November 2026). This
// test is the one that would have caught it: a textured square moves 16 px per frame over a gradient,
// and every pair after the first must be offered to PushFlow, must come back with its generated
// frames, and each generated frame must show the square between its two real neighbours (closer to
// the true in-between picture than a blend of the two frames is). At 2X and at 3X.
//
// usage: veyra_push_flow_graph_tests OUTDIR RUNTIMEDIR     (RUNTIMEDIR holds the "pushflow" folder)
#include "veyra/pipeline/EnhanceGraph.h"
#include "veyra/pipeline/PushOverlay.h"
#include "veyra/pipeline/PushCharacter.h"
#include "PushCharacterGpu.h"
#include "veyra/gfx/D3D12DeviceContext.h"
#include "veyra/gfx/CommandSlotRing.h"
#include "veyra/sink/ImageExportSink.h"
#include "veyra/Log.h"
#include <d3d12sdklayers.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>
extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace {
constexpr unsigned W = 640, H = 360, Step = 16, Start = 120;

// The picture of PushGpuTests' flowFrame: a gradient with a 40 px checkered square at squareX.
std::vector<uint8_t> picture(double squareX) {
    std::vector<uint8_t> px(size_t(W) * H * 4);
    for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < W; ++x) {
            uint8_t* p = &px[(size_t(y) * W + x) * 4];
            const int base = 60 + int(40.0 * x / W) + int(30.0 * y / H);
            int r = base, g = base + 10, b = base + 20;
            const double dx = x - squareX, dy = double(y) - H * 0.35;
            if (dx >= 0 && dx < 40 && dy >= 0 && dy < 40) {
                const bool light = ((int(dx) / 8) + (int(dy) / 8)) % 2 == 0;
                r = light ? 235 : 40; g = light ? 200 : 30; b = light ? 60 : 20;
            }
            p[0] = uint8_t(r); p[1] = uint8_t(g); p[2] = uint8_t(b); p[3] = 255;
        }
    return px;
}
// PSNR over the colour channels, in the band of rows the square crosses (the rest never changes).
double psnr(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0; size_t n = 0;
    for (unsigned y = unsigned(H * 0.35) - 4; y < unsigned(H * 0.35) + 44; ++y)
        for (unsigned x = 0; x < W; ++x)
            for (int c = 0; c < 3; ++c) {
                const double d = double(a[(size_t(y) * W + x) * 4 + c]) - double(b[(size_t(y) * W + x) * 4 + c]);
                sum += d * d; ++n;
            }
    const double mse = sum / double(n);
    return mse < 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}
std::vector<uint8_t> blend(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    std::vector<uint8_t> out(a.size());
    for (size_t i = 0; i < a.size(); ++i) out[i] = uint8_t((unsigned(a[i]) + unsigned(b[i]) + 1) / 2);
    return out;
}
double squareAt(unsigned frame) { return double(Start + frame * Step); }

// For Crosshair Hold: a picture that pans (smooth noise in whole numbers, the same on every machine) under a
// small reticle that stands still in the middle: three strokes two pixels thick with a dark outline.
int lattice(int x, int y, uint32_t salt) { uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + salt * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; return int((h ^ (h >> 16)) & 255); }
int worldAt(int x, int y) {
    int sum = 0, weight = 0;
    for (int level = 0; level < 2; ++level) {
        const int cell = 32 >> level, w = 2 - level;
        const int ix = x >= 0 ? x / cell : -((-x + cell - 1) / cell), iy = y / cell, fx = x - ix * cell, fy = y - iy * cell;
        const int a = lattice(ix, iy, uint32_t(level)), b = lattice(ix + 1, iy, uint32_t(level)), c = lattice(ix, iy + 1, uint32_t(level)), d = lattice(ix + 1, iy + 1, uint32_t(level));
        sum += w * (((a * (cell - fx) + b * fx) * (cell - fy) + (c * (cell - fx) + d * fx) * fy) / (cell * cell)); weight += w;
    }
    return 50 + (sum / weight) * 110 / 255;
}
int reticleAt(int x, int y) {                      // 0 nothing, 1 outline, 2 core
    struct Stroke { int y0, y1, x0, x1; };
    const int cx = int(W) / 2, cy = int(H) / 2;
    const Stroke strokes[3] = {{cy - 3, cy - 1, cx - 12, cx - 4}, {cy - 3, cy - 1, cx + 4, cx + 12}, {cy + 1, cy + 8, cx - 1, cx + 1}};
    int kind = 0;
    for (const auto& s : strokes) {
        if (x >= s.x0 && x < s.x1 && y >= s.y0 && y < s.y1) return 2;
        if (x >= s.x0 - 1 && x < s.x1 + 1 && y >= s.y0 - 1 && y < s.y1 + 1) kind = 1;
    }
    return kind;
}
// shift: how far the world has moved, in half pixels (so the picture half-way between two frames is exact).
std::vector<uint8_t> panned(int shift, bool reticle) {
    std::vector<uint8_t> px(size_t(W) * H * 4);
    for (unsigned y = 0; y < H; ++y)
        for (unsigned x = 0; x < W; ++x) {
            int v = (shift & 1) ? (worldAt(int(x) - (shift - 1) / 2, int(y)) + worldAt(int(x) - (shift + 1) / 2, int(y)) + 1) / 2 : worldAt(int(x) - shift / 2, int(y));
            if (reticle) { const int k = reticleAt(int(x), int(y)); if (k == 2) v = (v * 45 + 230 * 55) / 100; else if (k == 1) v = v * 30 / 100; }
            uint8_t* p = &px[(size_t(y) * W + x) * 4];
            p[0] = p[1] = p[2] = uint8_t(v); p[3] = 255;
        }
    return px;
}
}  // namespace

int wmain(int argc, wchar_t** argv) {
    using namespace veyra;
    if (argc != 3) { std::cout << "usage: veyra_push_flow_graph_tests OUTDIR RUNTIMEDIR" << std::endl; return 2; }
    std::filesystem::create_directories(argv[1]);
    veyra::Logger::instance().openFile((std::filesystem::path(argv[1]) / L"engine.log").wstring());
    veyra::Logger::instance().setConsoleEnabled(false);
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) return 2;
    gfx::D3D12DeviceContext ctx; gfx::CommandSlotRing ring; Status st = Status::Ok;
    gfx::DeviceContextDesc device; device.enableDebugLayer = true;
    if (!ctx.initialize(device, st) || !ring.initialize(ctx.device(), ctx.directQueue(), ctx.fence(), ctx.fenceEvent(), 6, st)) {
        std::cout << "PUSHFLOW_GRAPH no D3D12 adapter the engine accepts" << std::endl; CoUninitialize(); return 3;
    }
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> info; ctx.device()->QueryInterface(IID_PPV_ARGS(&info));
    int failures = 0;
    auto check = [&](bool pass, const std::string& name) { std::cout << "PUSHFLOW_GRAPH " << (pass ? "PASS " : "FAIL ") << name << std::endl; failures += !pass; return pass; };

    AVFrame* frame = av_frame_alloc();
    frame->format = AV_PIX_FMT_RGBA; frame->width = W; frame->height = H;
    frame->color_range = AVCOL_RANGE_JPEG; frame->color_trc = AVCOL_TRC_IEC61966_2_1;
    if (av_frame_get_buffer(frame, 32) < 0) { std::cout << "PUSHFLOW_GRAPH no frame buffer" << std::endl; return 2; }
    auto fill = [&](const std::vector<uint8_t>& px) {
        for (unsigned y = 0; y < H; ++y) std::memcpy(frame->data[0] + size_t(y) * frame->linesize[0], px.data() + size_t(y) * W * 4, size_t(W) * 4);
    };

    // The third run is PushFlow with a motion size chosen apart from its tier (3.2.7): the Fast tier's
    // model at the picture's own size instead of half of it. Fewer frames: four times the work each.
    // The fourth is PushFlow 2's encoder cache (opt-in) at 3X: the same frames as the plain 3X run, whose
    // generated pictures it must repeat, with each real frame encoded once.
    // The fifth is PushFlow 2's Guard (opt-in) on the same frames: the square is followed here, so every
    // generated frame must be the plain run's, and nothing is mended or held.
    // The sixth is PushFlow 2's Predict (opt-in): the batch holds the real frame first and forecasts after
    // it, in order, and each forecast is at least as near to where the square really is later as the
    // real frame it follows.
    // The seventh is Predict with PushFlow 2's Dual at 2X: beside the forecasts each batch carries what a
    // recording takes instead, the real frame before this one and the true in-between frame of the pair,
    // which must be the plain 2X run's frame. (The 3X Predict run asks for Dual too: it does not act there.)
    // The eighth is Crosshair Hold (opt-in) over the same frames at 2X: nothing in them is a crosshair, so the
    // frames PushFlow is given are the real frames and every generated frame must be the plain 2X run's.
    struct Run { unsigned multiplier; int motion; bool cache; bool guard = false; bool predict = false; bool dual = false; bool hold = false; };
    std::map<std::pair<unsigned, unsigned>, std::vector<uint8_t>> plain3x;   // (frame, subframe) -> picture without the cache
    std::map<std::pair<unsigned, unsigned>, std::vector<uint8_t>> plain2x;   // the same for 2X: what Dual's in-between frames must be
    for (const Run run : {Run{2u, 0, false}, Run{3u, 0, false}, Run{2u, 540, false}, Run{3u, 0, true}, Run{3u, 0, false, true}, Run{3u, 0, false, false, true, true}, Run{2u, 0, false, false, true, true},
                          Run{2u, 0, false, false, false, false, true}}) {
        const unsigned multiplier = run.multiplier;
        const std::string tag = std::to_string(multiplier) + "X" + (run.motion ? " at a chosen motion size" : "") + (run.cache ? " with the encoder cache" : "") + (run.guard ? " with Guard" : "") + (run.predict ? " with Predict" : "") + (run.dual ? " and Dual" : "") + (run.hold ? " with Crosshair Hold" : "");
        const unsigned frames = run.motion ? 5 : multiplier == 2 ? 12 : 7;
        pipeline::EnhanceGraph graph(ctx, ring);
        pipeline::EnhanceGraphDesc desc; desc.sourceWidth = desc.workWidth = W; desc.sourceHeight = desc.workHeight = H;
        desc.rgbInput = true; desc.enableNr = desc.enableSr = false; desc.enableFg = true; desc.fgMultiplier = multiplier;
        desc.frameGenerationBackend = engine::FrameGenerationBackend::PushFlow;
        desc.vfgQuality = 0;  // the Fast tier: the software renderer in CI is slow
        desc.pushFlowMotion = run.motion;
        desc.pushFlowEncoderCache = run.cache;
        desc.pushFlowGuard = run.guard;
        desc.pushFlowPredict = run.predict;
        desc.pushFlowDual = run.dual;
        desc.crosshairHold = run.hold; pipeline::overlay::setWanted(run.hold);   // the description flag is what the graph acts on; the switch is what its state reads
        desc.runtimeAbsPath = argv[2]; desc.settingsRevision = multiplier;
        const bool opened = graph.initialize(desc) && graph.createViews();
        if (!check(opened, tag + ": the graph opens with PushFlow") || !check(graph.pushFlowActive(), tag + ": PushFlow is ready")) {
            std::cout << "PUSHFLOW_GRAPH failedBackend=" << int(graph.failedBackend()) << std::endl;
            graph.shutdown(); continue;
        }
        check(graph.fgCreated(), tag + ": the graph counts PushFlow as a frame generation method");
        check(graph.crosshairHoldState() == (run.hold ? 1 : 0), tag + (run.hold ? ": Crosshair Hold is running" : ": Crosshair Hold is not asked for and not in use") + " (" + std::to_string(graph.crosshairHoldState()) + ")");
        {
            // The size the motion is worked out at: Fast's own is half the picture; a chosen size is used
            // as far as the picture goes (never enlarged), so here the picture's own size.
            const auto summary = graph.pushFlowSummary();
            std::cout << "PUSHFLOW_GRAPH " << tag << " motion size " << summary.motionWidth << "x" << summary.motionHeight << " rebuild=" << summary.rebuild << std::endl;
            check(summary.encoderCache == (run.cache ? "on" : ""), tag + (run.cache ? ": the encoder cache is in use" : ": the encoder cache is not asked for and not in use") + " ('" + summary.encoderCache + "')");
            check(summary.guard == (run.guard ? "on" : ""), tag + (run.guard ? ": Guard is in use" : ": Guard is not asked for and not in use") + " ('" + summary.guard + "')");
            check(summary.predict == (run.predict ? "on" : "") && graph.pushFlowPredicting() == run.predict, tag + (run.predict ? ": Predict is in use" : ": Predict is not asked for and not in use") + " ('" + summary.predict + "')");
            const std::string dualWanted = !run.dual ? "" : multiplier == 2 ? "on" : "needs-2x";
            check(summary.dual == dualWanted && graph.pushFlowDualActive() == (dualWanted == "on"), tag + ": Dual is '" + summary.dual + "' (expected '" + dualWanted + "')");
            if (run.cache) std::cout << "PUSHFLOW_GRAPH " << tag << " start-up check " << summary.splitCheckDb << " dB, whole " << summary.wholeMs << " ms, split " << summary.splitMs << " ms" << std::endl;
            if (run.motion) check(summary.motionWidth == W && summary.motionHeight == H && !summary.rebuild, tag + ": the network runs at the chosen size, here the whole picture");
            else check(summary.motionWidth == W / 2 && summary.motionHeight == H / 2 && summary.rebuild, tag + ": the tier's own motion size is what it was (half the picture, rebuilt at full size)");
        }
        unsigned offered = 0, pairs = 0, generated = 0, checked = 0, between = 0, ordered = 0, resetsWithoutFrame = 0, cacheChecked = 0, cacheSame = 0;
        unsigned realFirst = 0, forecastsInOrder = 0, plainOrder = 0, notWorse = 0, nearer = 0;
        unsigned creatorSeen = 0, dualPairs = 0, dualRealSame = 0, dualBetweenSame = 0; int dualWorst = 0;
        double worstAgainstHold = 1e9;
        int cacheWorst = 0;
        double worstMargin = 1e9, realDb = 0, slowestMs = 0; bool ok = true;
        std::vector<uint8_t> previous;
        for (unsigned i = 0; ok && i < frames; ++i) {
            const auto now = picture(squareAt(i));
            fill(now);
            pipeline::EnhanceGraph::FrameOutputs out;
            const bool reset = i == 0;
            const auto t0 = std::chrono::steady_clock::now();
            ok = graph.process(frame, double(i) * 1000.0 / 30.0, reset, out, i + 1);
            if (!ok) { std::cout << "PUSHFLOW_GRAPH " << tag << " process failed frame=" << i << " failedBackend=" << int(graph.failedBackend()) << std::endl; break; }
            ok = ctx.waitForFenceValue(std::max(out.videoFenceValue, out.genFenceValue), 120000) && graph.resolveGeneration(out);
            if (!ok) { std::cout << "PUSHFLOW_GRAPH " << tag << " frame " << i << " did not finish" << std::endl; break; }
            slowestMs = std::max(slowestMs, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
            if (i == 1) {
                // The real frame comes back as it went in (no enhancement is on): the comparisons below rely on it.
                sink::RgbaImage real;
                if (sink::readRgba8(ctx, ring, graph.videoFrameResource(out.videoSlot), real) && real.width == W && real.height == H) realDb = psnr(real.pixels, now);
            }
            if (reset) { resetsWithoutFrame += !out.hasGenerated; previous = now; continue; }
            ++pairs;
            offered += out.fgCandidates == multiplier - 1;
            {
                // The batch's order: in-between frames then the real frame; with Predict the real frame, then
                // its forecasts, each later than the one before and before the next real frame is expected.
                const auto& batch = out.batch;
                const int64_t interval = batch.b100ns - batch.a100ns;
                bool first = batch.count == multiplier && batch.frames[0].kind == pipeline::FrameKind::Real && batch.frames[0].pts100ns == batch.b100ns && out.fgPredict;
                bool rising = first;
                for (unsigned k = 1; first && k < batch.count; ++k)
                    rising = rising && batch.frames[k].kind == pipeline::FrameKind::Generated && batch.frames[k].subframe == k &&
                             batch.frames[k].pts100ns > batch.frames[k - 1].pts100ns && batch.frames[k].pts100ns < batch.b100ns + interval;
                realFirst += first; forecastsInOrder += rising;
                plainOrder += batch.count == multiplier && batch.frames[batch.count - 1].kind == pipeline::FrameKind::Real && batch.frames[batch.count - 1].pts100ns == batch.b100ns && !out.fgPredict;
            }
            creatorSeen += (out.creatorReal != nullptr) + (out.creatorBetween != nullptr);
            if (graph.pushFlowDualActive()) {
                // What a recording takes in place of this batch's frames: the real frame before this one, and
                // the true in-between frame of the pair (the plain 2X run's generated frame for it).
                ++dualPairs;
                const auto same = [&](ID3D12Resource* texture, const std::vector<uint8_t>* want) {
                    sink::RgbaImage image;
                    if (!texture || !want || !sink::readRgba8(ctx, ring, texture, image) || image.pixels.size() != want->size()) return false;
                    int worst = 0;
                    for (size_t k = 0; k < image.pixels.size(); ++k) worst = std::max(worst, std::abs(int(image.pixels[k]) - int((*want)[k])));
                    dualWorst = std::max(dualWorst, worst);
                    return worst <= 1;
                };
                const auto between = plain2x.find({i, 1u});
                dualRealSame += same(out.creatorReal, &previous);
                dualBetweenSame += same(out.creatorBetween, between != plain2x.end() ? &between->second : nullptr);
            }
            if (run.predict && out.hasGenerated && out.fgEvaluated == multiplier - 1) {
                ++generated;
                for (unsigned sub = 1; sub < multiplier; ++sub) {
                    sink::RgbaImage image;
                    if (!sink::readRgba8(ctx, ring, graph.generatedFrameResource(out.genSlot + (sub - 1) * 2), image) || image.width != W || image.height != H) { ok = false; break; }
                    if (i == 3) sink::saveImage((std::filesystem::path(argv[1]) / (L"forecast-" + std::to_wstring(multiplier) + L"x-" + std::to_wstring(sub) + L".png")).wstring(), image);
                    // Where the square really is at the forecast's time, against holding the newest real frame.
                    const auto truth = picture(squareAt(i) + double(Step) * double(sub) / double(multiplier));
                    const double forecastDb = psnr(image.pixels, truth), holdDb = psnr(now, truth);
                    ++checked; notWorse += forecastDb >= holdDb - 0.05; nearer += forecastDb > holdDb + 1.0; worstAgainstHold = std::min(worstAgainstHold, forecastDb - holdDb);
                    if (i <= 2) std::cout << "PUSHFLOW_GRAPH " << tag << " frame=" << i << " sub=" << sub << " forecast " << forecastDb << " dB, holding the real frame " << holdDb << " dB" << std::endl;
                }
                previous = now;
                continue;
            }
            if (out.hasGenerated && out.fgEvaluated == multiplier - 1) {
                ++generated;
                const auto mix = blend(previous, now);
                std::vector<std::vector<uint8_t>> subs;
                for (unsigned sub = 1; sub < multiplier; ++sub) {
                    sink::RgbaImage image;
                    if (!sink::readRgba8(ctx, ring, graph.generatedFrameResource(out.genSlot + (sub - 1) * 2), image) || image.width != W || image.height != H) { ok = false; break; }
                    if (i == 3) sink::saveImage((std::filesystem::path(argv[1]) / (L"generated-" + std::to_wstring(multiplier) + L"x-" + std::to_wstring(sub) + (run.motion ? L"-motion" : L"") + L".png")).wstring(), image);
                    // Where the square truly is at this subframe's time, against a plain blend of the two frames.
                    const auto truth = picture(squareAt(i - 1) + double(Step) * double(sub) / double(multiplier));
                    const double flowDb = psnr(image.pixels, truth), blendDb = psnr(mix, truth);
                    ++checked; between += flowDb > blendDb + 2.0; worstMargin = std::min(worstMargin, flowDb - blendDb);
                    if (i <= 2) std::cout << "PUSHFLOW_GRAPH " << tag << " frame=" << i << " sub=" << sub << " generated " << flowDb << " dB, blend " << blendDb << " dB" << std::endl;
                    if (multiplier == 3 && !run.motion && !run.cache && !run.guard && !run.hold) plain3x[{i, sub}] = image.pixels;
                    if (multiplier == 2 && !run.motion && !run.cache && !run.guard && !run.hold) plain2x[{i, sub}] = image.pixels;
                    if (run.cache || run.guard || run.hold) {
                        const auto& plain = multiplier == 2 ? plain2x : plain3x;
                        const auto found = plain.find({i, sub});
                        int worst = 255;
                        if (found != plain.end() && found->second.size() == image.pixels.size()) {
                            worst = 0;
                            for (size_t k = 0; k < image.pixels.size(); ++k) worst = std::max(worst, std::abs(int(image.pixels[k]) - int(found->second[k])));
                        }
                        ++cacheChecked; cacheSame += worst <= 1; cacheWorst = std::max(cacheWorst, worst);
                    }
                    subs.push_back(std::move(image.pixels));
                }
                if (ok && multiplier == 3 && subs.size() == 2) {
                    // The two generated frames are in order: each is closer to its own moment than to the other's.
                    const auto third = picture(squareAt(i - 1) + Step / 3.0), twoThirds = picture(squareAt(i - 1) + 2.0 * Step / 3.0);
                    ordered += psnr(subs[0], third) > psnr(subs[0], twoThirds) && psnr(subs[1], twoThirds) > psnr(subs[1], third);
                }
            }
            previous = now;
        }
        check(ok, tag + ": every frame processed");
        check(realDb > 40.0, tag + ": a real frame comes back as it went in (" + std::to_string(realDb) + " dB)");
        check(resetsWithoutFrame == 1, tag + ": the first frame (a reset) has no generated frame");
        check(pairs > 0 && offered == pairs, tag + ": every pair after the first is offered to PushFlow (" + std::to_string(offered) + "/" + std::to_string(pairs) + ")");
        check(pairs > 0 && generated == pairs, tag + ": and comes back with its generated frames (" + std::to_string(generated) + "/" + std::to_string(pairs) + ")");
        if (run.predict) {
            const auto summary = graph.pushFlowSummary();
            check(pairs > 0 && realFirst == pairs && forecastsInOrder == pairs, tag + ": every batch holds the real frame first and its forecasts after it, in order (" + std::to_string(realFirst) + ", " +
                                                                                std::to_string(forecastsInOrder) + " of " + std::to_string(pairs) + ")");
            check(checked > 0 && notWorse == checked, tag + ": no forecast is further from where the square really is than the real frame before it (" + std::to_string(notWorse) + "/" +
                                                      std::to_string(checked) + ", worst " + std::to_string(worstAgainstHold) + " dB)");
            std::cout << "PUSHFLOW_GRAPH " << tag << " forecasts clearly nearer than holding: " << nearer << "/" << checked << "; forecasts counted " << summary.predictFrames << ", pixels counted "
                      << summary.predictCounted << ", moving " << summary.predictMoving << ", held " << summary.predictHeld << std::endl;
            // One run of the network for a pair, however many forecasts are made from it.
            check(summary.predictFrames > 0, tag + ": the forecasts are counted");
            // It steps aside without anything being rebuilt: the next pair is an ordinary one again.
            if (run.dual && multiplier == 2)
                check(dualPairs == pairs && dualRealSame == pairs && dualBetweenSame == pairs, tag + ": for every pair a recording is given the real frame before it and the true in-between frame (" +
                      std::to_string(dualRealSame) + " and " + std::to_string(dualBetweenSame) + " of " + std::to_string(pairs) + ", worst difference " + std::to_string(dualWorst) + " of 255)");
            else check(creatorSeen == 0, tag + ": Dual does not act, and no batch carries a frame for recordings");
            graph.setPushFlowPredictPaused(true);
            check(!graph.pushFlowPredicting() && !graph.pushFlowDualActive(), tag + ": it steps aside when asked to (a measurement is running)");
            graph.setPushFlowPredictPaused(false);
            check(graph.pushFlowPredicting(), tag + ": and comes back");
        } else {
            check(pairs > 0 && plainOrder == pairs && realFirst == 0, tag + ": every batch ends with its real frame, as it always did (" + std::to_string(plainOrder) + "/" + std::to_string(pairs) + ")");
            check(creatorSeen == 0, tag + ": no batch carries a frame for recordings in place of the one shown");
            check(checked > 0 && between == checked, tag + ": each generated frame shows the square between its neighbours (" + std::to_string(between) + "/" + std::to_string(checked) +
                                                         ", worst margin over a blend " + std::to_string(worstMargin) + " dB)");
            if (multiplier == 3) check(generated > 0 && ordered == generated, tag + ": the two generated frames of a pair are in order (" + std::to_string(ordered) + "/" + std::to_string(generated) + ")");
        }
        if (run.guard) {
            const auto summary = graph.pushFlowSummary();
            check(cacheChecked == checked && cacheChecked > 0 && cacheSame == cacheChecked, tag + ": a followed object leaves every generated frame as PushFlow makes it without Guard (" + std::to_string(cacheSame) + "/" +
                                                                                             std::to_string(cacheChecked) + ", worst difference " + std::to_string(cacheWorst) + " of 255)");
            check(summary.guardFrames > 0 && summary.guardMended == 0 && summary.guardHeld == 0,
                  tag + ": judged " + std::to_string(summary.guardFrames) + " frames, mended " + std::to_string(summary.guardMended) + ", held " + std::to_string(summary.guardHeld));
        }
        if (run.hold) {
            check(cacheChecked == checked && cacheChecked > 0 && cacheSame == cacheChecked, tag + ": a picture without a crosshair comes out as it does without Crosshair Hold, frame for frame (" + std::to_string(cacheSame) + "/" +
                                                                                             std::to_string(cacheChecked) + ", worst difference " + std::to_string(cacheWorst) + " of 255)");
            check(graph.crosshairHoldFrames() > 0 && graph.crosshairHoldPixels() == 0, tag + ": and the pass says it saw " + std::to_string(graph.crosshairHoldFrames()) + " real frames and holds " +
                                                                                         std::to_string(graph.crosshairHoldPixels()) + " pixels");
        }
        if (run.cache) {
            const auto summary = graph.pushFlowSummary();
            check(cacheChecked == checked && cacheChecked > 0 && cacheSame == cacheChecked, tag + ": every generated frame is the one PushFlow makes without the cache (" + std::to_string(cacheSame) + "/" +
                                                                                             std::to_string(cacheChecked) + ", worst difference " + std::to_string(cacheWorst) + " of 255)");
            // Every pair after the first uses the features its first frame left behind; each real frame is encoded once.
            check(pairs > 1 && summary.featuresMade == 1 && summary.featuresKept == pairs - 1 && summary.encoderRuns == pairs + 1,
                  tag + ": one encoder run per real frame (" + std::to_string(summary.encoderRuns) + " runs for " + std::to_string(pairs) + " pairs, kept " + std::to_string(summary.featuresKept) +
                  ", made " + std::to_string(summary.featuresMade) + ")");
        }
        std::cout << "PUSHFLOW_GRAPH " << tag << " pairs=" << pairs << " offered=" << offered << " generated=" << generated << " checked=" << checked << " slowestFrameMs=" << slowestMs << std::endl;
        ring.drainQueue(); graph.shutdown();
    }
    pipeline::overlay::setWanted(false);
    // Crosshair Hold with something to hold: the picture pans four pixels a frame under a reticle that stands
    // still in the middle, half see-through. Without the switch PushFlow is given the frames as they are; with
    // it the reticle is found after it has stood for four moving frames, taken out of what PushFlow is given and
    // laid back over what PushFlow makes. From then on its pixels in every generated frame must be the reticle
    // over the picture half-way between the two frames.
    {
        constexpr int Pan = 4;                    // pixels a frame; the half-way picture is then exact
        constexpr unsigned Frames = 12, From = 6; // found at the fifth frame; measured from the pair after it
        double meanOf[2] = {-1, -1}, worstOf[2] = {-1, -1};
        for (int on = 0; on < 2; ++on) {
            const std::string tag = on ? "a reticle over a panned picture with Crosshair Hold" : "a reticle over a panned picture without Crosshair Hold";
            pipeline::EnhanceGraph graph(ctx, ring);
            pipeline::EnhanceGraphDesc desc; desc.sourceWidth = desc.workWidth = W; desc.sourceHeight = desc.workHeight = H;
            desc.rgbInput = true; desc.enableNr = desc.enableSr = false; desc.enableFg = true; desc.fgMultiplier = 2;
            desc.frameGenerationBackend = engine::FrameGenerationBackend::PushFlow;
            desc.vfgQuality = 0;
            desc.crosshairHold = on != 0; pipeline::overlay::setWanted(on != 0);
            desc.runtimeAbsPath = argv[2]; desc.settingsRevision = 20u + unsigned(on);
            const bool opened = graph.initialize(desc) && graph.createViews();
            if (!check(opened && graph.pushFlowActive(), tag + ": the graph opens with PushFlow")) { graph.shutdown(); continue; }
            check(graph.crosshairHoldState() == (on ? 1 : 0), tag + ": its state is " + std::to_string(graph.crosshairHoldState()));
            bool ok = true; unsigned pairs = 0, generated = 0, measured = 0; double sum = 0, worst = 0;
            for (unsigned i = 0; ok && i < Frames; ++i) {
                const auto now = panned(2 * Pan * int(i), true);
                fill(now);
                pipeline::EnhanceGraph::FrameOutputs out;
                ok = graph.process(frame, double(i) * 1000.0 / 30.0, i == 0, out, i + 1);
                if (!ok) { std::cout << "PUSHFLOW_GRAPH " << tag << " process failed frame=" << i << " failedBackend=" << int(graph.failedBackend()) << std::endl; break; }
                ok = ctx.waitForFenceValue(std::max(out.videoFenceValue, out.genFenceValue), 120000) && graph.resolveGeneration(out);
                if (!ok) { std::cout << "PUSHFLOW_GRAPH " << tag << " frame " << i << " did not finish" << std::endl; break; }
                if (i == 0) continue;
                ++pairs;
                if (!out.hasGenerated || out.fgEvaluated != 1) continue;
                ++generated;
                sink::RgbaImage image;
                if (!sink::readRgba8(ctx, ring, graph.generatedFrameResource(out.genSlot), image) || image.width != W || image.height != H) { ok = false; break; }
                if (i == 8) sink::saveImage((std::filesystem::path(argv[1]) / (on ? L"reticle-with-hold.png" : L"reticle-without-hold.png")).wstring(), image);
                if (i < From) continue;
                // The picture half-way between the two frames, with the reticle where it stands.
                const auto truth = panned(2 * Pan * int(i) - Pan, true);
                double error = 0; unsigned pixels = 0;
                for (unsigned y = H / 2 - 16; y < H / 2 + 16; ++y)
                    for (unsigned x = W / 2 - 16; x < W / 2 + 16; ++x) {
                        if (!reticleAt(int(x), int(y))) continue;
                        const size_t k = (size_t(y) * W + x) * 4;
                        error += (std::abs(int(image.pixels[k]) - int(truth[k])) + std::abs(int(image.pixels[k + 1]) - int(truth[k + 1])) + std::abs(int(image.pixels[k + 2]) - int(truth[k + 2]))) / 3.0;
                        ++pixels;
                    }
                if (!pixels) continue;
                error /= double(pixels);
                ++measured; sum += error; worst = std::max(worst, error);
            }
            check(ok, tag + ": every frame processed");
            check(pairs == Frames - 1 && generated == pairs, tag + ": every pair comes back with its generated frame (" + std::to_string(generated) + "/" + std::to_string(pairs) + ")");
            // What the interface shows: the pass says how many real frames it saw and how many pixels it holds
            // (counted from a frame sampled a few frames back, never waited for).
            if (on) check(graph.crosshairHoldFrames() == Frames && graph.crosshairHoldPixels() > 0, tag + ": the pass says it saw " + std::to_string(graph.crosshairHoldFrames()) + " real frames and holds " +
                                                                                                     std::to_string(graph.crosshairHoldPixels()) + " pixels");
            else check(graph.crosshairHoldFrames() == 0 && graph.crosshairHoldPixels() == 0, tag + ": no pass, nothing held");
            if (measured) { meanOf[on] = sum / double(measured); worstOf[on] = worst; }
            std::cout << "PUSHFLOW_GRAPH " << tag << ": the reticle's pixels in " << measured << " generated frames are off by " << meanOf[on] << " of 255 on average, " << worstOf[on] << " in the worst frame" << std::endl;
            ring.drainQueue(); graph.shutdown();
        }
        pipeline::overlay::setWanted(false);
        // Lost or doubled, the reticle is off by 70 of 255 here; held exactly, by about 1 (what filling it in costs).
        check(meanOf[1] >= 0 && worstOf[1] < 16.0, "with Crosshair Hold the reticle is where it stands in every generated frame (" + std::to_string(meanOf[1]) + " of 255 on average, worst frame " +
                                                    std::to_string(worstOf[1]) + "; without it " + std::to_string(meanOf[0]) + ")");
    }
    // Character Hold through the real graph, with PushFlow at 2X (the pass alone, against the rule, is
    // PushGpuTests' testCharacter). A drawn figure, the one the matting model's own check draws, moves slowly
    // before a picture that changes. With the switch every pixel away from the figure must be what the run
    // without the switch made (the pass writes nowhere else, and frame generation is given what it always was),
    // the figure itself is at least as near the true in-between picture, and the pass says it found someone.
    // Then a picture without a person: frame for frame the run without the switch, and nobody found. The debug
    // layer checks the pass's resource states through the graph's own lists.
    if (pushflow::PushMatteBackend::installed(argv[2])) {
        for (int person = 1; person >= 0; --person) {
            constexpr unsigned Frames = 9;
            std::vector<std::vector<uint8_t>> plain;
            double figureError[2] = {-1, -1};
            for (int on = 0; on < 2; ++on) {
                const std::string tag = std::string(person ? "a figure" : "a picture without a person") + (on ? " with Character Hold" : " without Character Hold");
                pipeline::EnhanceGraph graph(ctx, ring);
                pipeline::EnhanceGraphDesc desc; desc.sourceWidth = desc.workWidth = W; desc.sourceHeight = desc.workHeight = H;
                desc.rgbInput = true; desc.enableNr = desc.enableSr = false; desc.enableFg = true; desc.fgMultiplier = 2;
                desc.frameGenerationBackend = engine::FrameGenerationBackend::PushFlow;
                desc.vfgQuality = 0;
                desc.characterHold = on != 0; pipeline::character::setWanted(on != 0);
                desc.runtimeAbsPath = argv[2]; desc.settingsRevision = 30u + unsigned(on) + 2u * unsigned(person);
                const bool opened = graph.initialize(desc) && graph.createViews();
                if (!check(opened && graph.pushFlowActive(), tag + ": the graph opens with PushFlow")) { graph.shutdown(); continue; }
                check(graph.characterHoldState() == (on ? 1 : 0), tag + ": its state is " + std::to_string(graph.characterHoldState()) + (on ? " (" + graph.characterHoldReason() + ")" : ""));
                bool ok = true; unsigned pairs = 0, generated = 0, compared = 0, measured = 0; size_t awayChanged = 0, figureChanged = 0; double sum = 0;
                for (unsigned i = 0; ok && i < Frames; ++i) {
                    fill(charactergpu::draw(int(W), int(H), float(i), person != 0).rgba);
                    pipeline::EnhanceGraph::FrameOutputs out;
                    ok = graph.process(frame, double(i) * 1000.0 / 30.0, i == 0, out, i + 1);
                    if (!ok) { std::cout << "PUSHFLOW_GRAPH " << tag << " process failed frame=" << i << " failedBackend=" << int(graph.failedBackend()) << std::endl; break; }
                    ok = ctx.waitForFenceValue(std::max(out.videoFenceValue, out.genFenceValue), 180000) && graph.resolveGeneration(out);
                    if (!ok) { std::cout << "PUSHFLOW_GRAPH " << tag << " frame " << i << " did not finish" << std::endl; break; }
                    if (i == 0) continue;
                    ++pairs;
                    if (!out.hasGenerated || out.fgEvaluated != 1) continue;
                    ++generated;
                    sink::RgbaImage image;
                    if (!sink::readRgba8(ctx, ring, graph.generatedFrameResource(out.genSlot), image) || image.width != W || image.height != H) { ok = false; break; }
                    if (i == 5 && person) sink::saveImage((std::filesystem::path(argv[1]) / (on ? L"figure-with-hold.png" : L"figure-without-hold.png")).wstring(), image);
                    if (!on) { plain.push_back(image.pixels); }
                    else if (plain.size() >= generated) {
                        const auto& before = plain[generated - 1];
                        ++compared;
                        for (unsigned y = 0; y < H; ++y)
                            for (unsigned x = 0; x < W; ++x) {
                                const size_t k = (size_t(y) * W + x) * 4;
                                if (std::memcmp(&before[k], &image.pixels[k], 4) == 0) continue;
                                // Away from the figure in both real frames and between them (2 = well outside its outline).
                                const float u = (float(x) + 0.5f) * 1920.0f / float(W), v = (float(y) + 0.5f) * 1080.0f / float(H);
                                const bool away = !person || (charactergpu::figureInside(float(i), u, v) > 2.0f && charactergpu::figureInside(float(i) - 1.0f, u, v) > 2.0f);
                                ++(away ? awayChanged : figureChanged);
                            }
                    }
                    if (person) {
                        // The figure's inside against the picture truly half-way between the two frames.
                        const auto truth = charactergpu::draw(int(W), int(H), float(i) - 0.5f, true).rgba;
                        double error = 0; unsigned pixels = 0;
                        for (unsigned y = 0; y < H; ++y)
                            for (unsigned x = 0; x < W; ++x) {
                                const float u = (float(x) + 0.5f) * 1920.0f / float(W), v = (float(y) + 0.5f) * 1080.0f / float(H);
                                if (charactergpu::figureInside(float(i) - 0.5f, u, v) > 0.8f) continue;
                                const size_t k = (size_t(y) * W + x) * 4;
                                error += (std::abs(int(image.pixels[k]) - int(truth[k])) + std::abs(int(image.pixels[k + 1]) - int(truth[k + 1])) + std::abs(int(image.pixels[k + 2]) - int(truth[k + 2]))) / 3.0;
                                ++pixels;
                            }
                        if (pixels) { sum += error / double(pixels); ++measured; }
                    }
                }
                check(ok, tag + ": every frame processed");
                check(pairs == Frames - 1 && generated == pairs, tag + ": every pair comes back with its generated frame (" + std::to_string(generated) + "/" + std::to_string(pairs) + ")");
                if (measured) figureError[on] = sum / double(measured);
                if (on) {
                    const unsigned blocks = graph.characterHoldBlocks();
                    std::cout << "PUSHFLOW_GRAPH " << tag << ": the pass saw " << graph.characterHoldFrames() << " real frames and counts " << blocks << " blocks with a person; pixels changed away from the figure "
                              << awayChanged << ", at the figure " << figureChanged << " (" << compared << " generated frames compared)" << std::endl;
                    check(graph.characterHoldFrames() == Frames && compared == generated && awayChanged == 0, tag + ": nothing is changed away from the figure (" + std::to_string(awayChanged) + " pixels)");
                    // PushFlow has this figure right, and what is right is left alone: the pass must find the figure, and
                    // whatever it lays over must not move it away from the true picture (checked below).
                    if (person) check(blocks > 0, tag + ": the figure is found (" + std::to_string(blocks) + " blocks; " + std::to_string(figureChanged) + " pixels laid over)");
                    else check(blocks == 0 && figureChanged == 0, tag + ": nobody is found and nothing is laid over");
                } else check(graph.characterHoldFrames() == 0 && graph.characterHoldBlocks() == 0, tag + ": no pass, nothing counted");
                ring.drainQueue(); graph.shutdown();
            }
            pipeline::character::setWanted(false);
            if (person) {
                std::cout << "PUSHFLOW_GRAPH a figure: its inside is off the true in-between picture by " << figureError[0] << " of 255 without Character Hold and " << figureError[1] << " with it" << std::endl;
                check(figureError[0] >= 0 && figureError[1] >= 0 && figureError[1] <= figureError[0] + 0.5, "with Character Hold the figure is at least as near the true in-between picture (" +
                      std::to_string(figureError[1]) + " against " + std::to_string(figureError[0]) + " of 255)");
            }
        }
    } else std::cout << "PUSHFLOW_GRAPH SKIP Character Hold: the matting model is not in the runtime folder" << std::endl;
    unsigned errors = 0;
    if (info) for (UINT64 m = 0; m < info->GetNumStoredMessagesAllowedByRetrievalFilter(); ++m) {
        SIZE_T size = 0; info->GetMessage(m, nullptr, &size); std::vector<uint8_t> data(size);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(data.data());
        if (SUCCEEDED(info->GetMessage(m, message, &size)) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) { ++errors; if (errors <= 8) std::cout << "GPU_ERROR " << message->pDescription << std::endl; }
    }
    check(errors == 0, "no D3D12 debug-layer error (" + std::to_string(errors) + ")");
    std::cout << "PUSHFLOW_GRAPH " << (failures ? "FAILED" : "passed") << " failures=" << failures << std::endl;
    av_frame_free(&frame);
    ring.shutdown(); ctx.shutdown(); CoUninitialize();
    return failures ? 1 : 0;
}
