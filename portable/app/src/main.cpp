// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS for Linux and macOS (preview).
//
//   PushFPS                          the application
//   PushFPS --list-devices           capture cards and their formats, sound devices
//   PushFPS --selftest 6 [--source testpattern|testpattern-uneven|file:<path>|camera[:N]]
//                                    runs the pipeline without a window, prints a SELFTEST RESULT
//                                    block; exit 0 ok, 2 source failed, 3 no frames, 1 other
//   PushFPS --gpucheck [--save <png>] draws a known generated frame on the GPU and compares it with the
//                                    CPU reference; exit 0 match, 4 mismatch or no GPU renderer
//   PushFPS --nischeck [--save <png>] upscales a known picture 2x with NIS (FrameView's upscaling) and
//                                    checks it is that picture, upright, and that sharpening reaches the
//                                    shader; exit 0 pass, 4 fail or no compute
//   PushFPS --screenshot <png> [--page home|pro|min|set] [--lang <code>]
#include "MacActivity.h"
#include "AppController.h"
#include "FrameView.h"
#include "Settings.h"
#include "TestPattern.h"
#include "Translator.h"
#include "VeyraShim.h"
#include "VideoFrames.h"

#include "veyra/engine/PushAmbient.h"  // pure C++, shared with the Windows build
#include "veyra/portable/Ambient.h"
#include "veyra/portable/Interpolate.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMediaDevices>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <memory>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <functional>
#include <thread>
#include <vector>

using namespace pushfps;
using namespace veyra::portable;

namespace {

void print(const QString& s) {
    std::fputs(s.toUtf8().constData(), stdout);
    std::fputs("\n", stdout);
    std::fflush(stdout);
}

int listDevices() {
    print(QStringLiteral("Video capture devices:"));
    const auto cams = QMediaDevices::videoInputs();
    if (cams.isEmpty()) print(QStringLiteral("  (none)"));
    for (int i = 0; i < cams.size(); ++i) {
        print(QStringLiteral("  [%1] %2").arg(i).arg(cams[i].description()));
        const auto formats = cams[i].videoFormats();
        const int best = CaptureSource::preferredFormat(formats);
        for (int k = 0; k < formats.size(); ++k) {
            const auto& f = formats[k];
            print(QStringLiteral("      %1%2x%3 %4-%5 fps format %6").arg(k == best ? QStringLiteral("* ") : QStringLiteral("  "))
                      .arg(f.resolution().width()).arg(f.resolution().height()).arg(double(f.minFrameRate())).arg(double(f.maxFrameRate()))
                      .arg(int(f.pixelFormat())));
        }
    }
    print(QStringLiteral("Audio inputs:"));
    for (const auto& d : QMediaDevices::audioInputs()) print(QStringLiteral("  %1").arg(d.description()));
    print(QStringLiteral("Audio outputs:"));
    for (const auto& d : QMediaDevices::audioOutputs()) print(QStringLiteral("  %1").arg(d.description()));
    return 0;
}

double psnr(const QImage& a, const QImage& b, int border) {
    if (a.size() != b.size()) return 0;
    double se = 0;
    qint64 n = 0;
    for (int y = border; y < a.height() - border; ++y)
        for (int x = border; x < a.width() - border; ++x) {
            const QRgb p = a.pixel(x, y), q = b.pixel(x, y);
            const int d[3] = {qRed(p) - qRed(q), qGreen(p) - qGreen(q), qBlue(p) - qBlue(q)};
            for (int c : d) se += double(c) * c;
            n += 3;
        }
    if (n == 0) return 0;
    const double mse = se / double(n);
    return mse < 1e-9 ? 99.0 : 10.0 * std::log10(255.0 * 255.0 / mse);
}

// image://pushfps-frame/<anything>: the newest real frame the engine keeps, at most 960 wide (the crop
// dialog's picture). Empty before a picture has arrived.
class NewestFrameProvider : public QQuickImageProvider {
public:
    explicit NewestFrameProvider(AppController& controller) : QQuickImageProvider(QQuickImageProvider::Image), controller_(controller) {}
    QImage requestImage(const QString&, QSize* size, const QSize& requested) override {
        QImage out;
        if (auto engine = controller_.engine())
            if (auto image = engine->newestImage(); image && image->width > 0 && image->height > 0) out = imageFromBgra(*image);
        const int wide = requested.width() > 0 ? std::min(requested.width(), 960) : 960;
        if (!out.isNull() && out.width() > wide) out = out.scaledToWidth(wide, Qt::SmoothTransformation);
        if (size) *size = out.size();
        return out;
    }

private:
    AppController& controller_;
};

// Runs the pipeline without a window: a 120 Hz "display" asks the engine every 8.3 ms.
int selftest(QGuiApplication& app, double seconds, const QString& sourceSpec, int target, bool pacing, bool hud, const QString& saveFrame) {
    auto engine = std::make_shared<Engine>();
    EngineSettings s;
    s.targetFps = target;
    s.pacingRepair = pacing;
    s.hudLock = hud;
    engine->setSettings(s);
    CaptureSource source(engine);
    QString failure;
    QObject::connect(&source, &CaptureSource::errorOccurred, [&](const QString& m) { failure = m; });
    bool expectGame = false;
    if (sourceSpec.startsWith(QStringLiteral("testpattern"))) {
        TestPattern::Options o;
        o.uneven = sourceSpec == QStringLiteral("testpattern-uneven");
        source.startTestPattern(o);
        expectGame = true;
    } else if (sourceSpec.startsWith(QStringLiteral("file:"))) {
        source.startFile(QUrl::fromLocalFile(QDir::current().absoluteFilePath(sourceSpec.mid(5))), 0.0f, false);
    } else if (sourceSpec.startsWith(QStringLiteral("camera"))) {
        const auto cams = QMediaDevices::videoInputs();
        const int index = sourceSpec.contains(QLatin1Char(':')) ? sourceSpec.section(QLatin1Char(':'), 1).toInt() : 0;
        if (index < 0 || index >= cams.size()) {
            print(QStringLiteral("SELFTEST RESULT\nsource: camera %1 not found\nresult: FAIL (source)").arg(index));
            return 2;
        }
        const auto formats = cams[index].videoFormats();
        const int best = CaptureSource::preferredFormat(formats);
        source.startCamera(cams[index], best >= 0 ? formats[best] : QCameraFormat());
    } else {
        print(QStringLiteral("unknown --source %1").arg(sourceSpec));
        return 1;
    }
    int picks = 0, generated = 0;
    std::atomic<int> checked{0};
    QImage sample;
    std::vector<std::thread> references;  // the CPU reference runs beside the display, not in it
    QTimer display;
    display.setTimerType(Qt::PreciseTimer);
    display.setInterval(8);
    QObject::connect(&display, &QTimer::timeout, [&] {
        const Pick p = engine->pick(nowNs() + 8'333'333);
        if (!p.valid()) return;
        ++picks;
        if (!p.generated()) return;
        ++generated;
        // Three times per run, the CPU reference on what the display would draw (on its own thread, so
        // the display timing stays real).
        if ((generated == 40 || generated == 240 || generated == 440) && references.size() < 3) {
            const bool keep = references.empty() && !saveFrame.isEmpty();
            references.emplace_back([p, keep, &checked, &sample] {
                Bgra out;
                if (interpolate(*p.a, *p.b, p.fields->forward, p.fields->backward, p.fields->hudMask.get(), p.t, out, 1, {}, p.hold.get(), p.subHold.get())) {
                    ++checked;
                    if (keep) sample = imageFromBgra(out);
                }
            });
        }
    });
    display.start();
    QTimer::singleShot(int(seconds * 1000), &app, &QCoreApplication::quit);
    app.exec();
    display.stop();
    for (auto& t : references) t.join();
    const EngineStats st = engine->stats();
    const qint64 read = source.framesRead();
    source.stop();
    if (!saveFrame.isEmpty() && !sample.isNull()) sample.save(saveFrame);
    // Share of refreshes that had to show a real frame because the motion was not ready yet.
    const double lateShare = st.picks > 0 ? double(st.lateFields) / double(st.picks) : 0;
    int code = 0;
    QString verdict = QStringLiteral("PASS");
    if (!failure.isEmpty() && read == 0) { code = 2; verdict = QStringLiteral("FAIL (source: %1)").arg(failure); }
    else if (read == 0) { code = 3; verdict = QStringLiteral("FAIL (no frames)"); }
    else if (expectGame) {
        // The test picture is 30 FPS on 60 Hz: duplicates must go, the output must reach the target.
        const double wantOut = target > 0 ? target : 120.0;
        if (std::abs(st.transportFps - 60) > 6 || std::abs(st.contentFps - 30) > 4) { code = 1; verdict = QStringLiteral("FAIL (rates)"); }
        else if (st.outputFps < wantOut * 0.85) { code = 1; verdict = QStringLiteral("FAIL (output %1 < %2)").arg(st.outputFps, 0, 'f', 1).arg(wantOut * 0.85, 0, 'f', 1); }
        else if (generated == 0 || checked.load() == 0) { code = 1; verdict = QStringLiteral("FAIL (no generated frames)"); }
        else if (lateShare > 0.05) { code = 1; verdict = QStringLiteral("FAIL (motion late for %1% of refreshes)").arg(lateShare * 100, 0, 'f', 1); }
    }
    QString r;
    QTextStream o(&r);
    o << "SELFTEST RESULT\n";
    o << "source: " << sourceSpec << "\n";
    o << "size: " << st.width << "x" << st.height << "\n";
    o << "frames read: " << read << ", duplicates removed: " << qulonglong(st.duplicates) << ", real frames: " << qulonglong(st.kept) << "\n";
    o << QStringLiteral("signal %1 fps, game %2 fps, output %3 fps\n").arg(st.transportFps, 0, 'f', 1).arg(st.contentFps, 0, 'f', 1).arg(st.outputFps, 0, 'f', 1);
    o << QStringLiteral("delay %1 ms, latency %2 ms, motion estimation %3 ms per pair\n").arg(st.delayMs, 0, 'f', 1).arg(st.latencyMs, 0, 'f', 1).arg(st.estimateMs, 0, 'f', 1);
    o << "picks: " << picks << ", generated: " << generated << ", CPU reference runs: " << checked.load() << ", scene cuts: " << qulonglong(st.sceneCuts)
      << ", late motion: " << qulonglong(st.lateFields) << ", pacing repaired: " << qulonglong(st.pacingRepaired) << "\n";
    o << "result: " << verdict;
    print(r);
    return code;
}

// Draws one generated frame of the test picture through FrameView and the shader, compares it with the
// CPU reference.
int gpucheck(QGuiApplication& app, const QString& savePath) {
    const int w = 640, h = 360;
    QImage ia = TestPattern::render(w, h, 2.0).convertToFormat(QImage::Format_RGB32);
    QImage ib = TestPattern::render(w, h, 2.0 + 1.0 / 30.0).convertToFormat(QImage::Format_RGB32);
    // A crosshair standing at the centre of both pictures, so the check also covers Crosshair Hold: what
    // the shader samples in place of it, and how it lays it back (CrosshairHold.h, Interpolate.cpp).
    const auto onCrosshair = [&](int x, int y) {
        const int dx = x - w / 2, dy = y - h / 2;
        return ((dy == 0 || dy == -1) && std::abs(dx) >= 4 && std::abs(dx) <= 10) || ((dx == 0 || dx == -1) && std::abs(dy) >= 4 && std::abs(dy) <= 10);
    };
    // And a short line of "text" in the subtitle band, for Subtitle Hold (the same machinery in its own zone).
    using namespace veyra::pipeline::overlay;
    const Zone band = subtitleZoneFor(uint32_t(w), uint32_t(h));
    const auto onText = [&](int x, int y) {
        if (band.size == 0) return false;
        // over the car of the test picture (it drives left), so the text stands while what is behind it moves
        const int lx = x - int(band.x) - 100, ly = y - int(band.y + band.h / 2) + 6;
        if (lx < 0 || lx >= 80 || ly < 0 || ly >= 12) return false;
        const int cx = lx % 10;
        return cx < 8 && (cx < 2 || ly < 2 || ly >= 10);
    };
    for (QImage* image : {&ia, &ib}) {
        for (int y = h / 2 - 12; y <= h / 2 + 12; ++y)
            for (int x = w / 2 - 12; x <= w / 2 + 12; ++x)
                if (onCrosshair(x, y)) image->setPixel(x, y, qRgb(250, 250, 250));
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (onText(x, y)) image->setPixel(x, y, qRgb(250, 250, 250));
    }
    const Bgra a = bgraFromImage(ia), b = bgraFromImage(ib);
    // Each zone's part for this pair, with the mask set by hand (finding an overlay is the core tests' matter).
    const Zone zone = zoneFor(uint32_t(w), uint32_t(h));
    const Rule rule = ruleFor(uint32_t(h));
    const auto partOf = [&](const Bgra& frame, const Zone& z, const auto& onShape) {
        HoldFrame part;
        const int zw = int(z.w ? z.w : z.size), zh = int(z.h ? z.h : z.size);
        part.x = int(z.x); part.y = int(z.y); part.size = zw; part.w = zw; part.h = zh;
        part.zone = Bgra(zw, zh);
        part.mask = Plane(zw, zh, 0.0f);
        Picture original(zw, zh);
        for (int y = 0; y < zh; ++y)
            for (int x = 0; x < zw; ++x) {
                const uint8_t* p = frame.pixel(part.x + x, part.y + y);
                std::copy(p, p + 4, part.zone.pixel(x, y));
                original.at(x, y) = {float(p[2]) / 255.0f, float(p[1]) / 255.0f, float(p[0]) / 255.0f};
                bool beside = false;
                for (int j = -1; j <= 1 && !beside; ++j)
                    for (int i = -1; i <= 1 && !beside; ++i) beside = onShape(part.x + x + i, part.y + y + j);
                if (beside) { part.mask.at(x, y) = 1.0f; ++part.masked; }
            }
        part.filled = fill(original, part.mask, rule);
        return part;
    };
    const std::shared_ptr<const HoldPair> held = zone.size > 0 ? CrosshairHold::pair(partOf(a, zone, onCrosshair), partOf(b, zone, onCrosshair)) : nullptr;
    const std::shared_ptr<const HoldPair> heldText = band.size > 0 ? CrosshairHold::pair(partOf(a, band, onText), partOf(b, band, onText)) : nullptr;
    const Pyramid pa = Pyramid::of(lumaOf(a), 4), pb = Pyramid::of(lumaOf(b), 4);
    const MotionEstimator est;
    auto fields = std::make_shared<PairFields>();
    fields->aId = 1;
    fields->bId = 2;
    fields->forward = est.estimate(pa, pb);
    fields->backward = est.estimate(pb, pa);
    auto pick = std::make_shared<Pick>();
    pick->a = std::make_shared<const Bgra>(a);
    pick->b = std::make_shared<const Bgra>(b);
    pick->aId = 1;
    pick->bId = 2;
    pick->t = 0.5f;
    pick->fields = fields;
    pick->hold = held;
    pick->subHold = heldText;
    // Ambient fill's copies (Ambient.h): drawn only where the picture leaves bars, so not in the first,
    // 16:9 grab; the second grab makes the window 4:3 and compares the bars with the CPU reference below.
    pick->ambientA = std::make_shared<const Bgra>(veyra::portable::ambient::make(a, veyra::engine::kPushAmbientEncodedGain));
    pick->ambientB = std::make_shared<const Bgra>(veyra::portable::ambient::make(b, veyra::engine::kPushAmbientEncodedGain));
    FrameView::setFixedPick(pick);
    // Two HUD protection boxes (Protection.h), a rectangle and an ellipse, away from the crosshair and the text:
    // inside them the shader's picture has to be the older real frame, as the reference's is.
    const ProtectBox boxList[] = {{0.62f, 0.06f, 0.94f, 0.40f, false}, {0.05f, 0.06f, 0.33f, 0.40f, true}};
    const Protection boxes = Protection::clean(boxList, 2, 6.f);
    QVariantList boxVariants;
    for (const ProtectBox& bx : boxList)
        boxVariants << QVariantMap{{"left", bx.left}, {"top", bx.top}, {"right", bx.right}, {"bottom", bx.bottom}, {"ellipse", bx.ellipse}};
    Bgra reference, withoutHold;
    interpolate(a, b, fields->forward, fields->backward, nullptr, 0.5f, reference, 2, {}, held.get(), heldText.get(), &boxes);
    interpolate(a, b, fields->forward, fields->backward, nullptr, 0.5f, withoutHold, 2);
    const QImage ref = imageFromBgra(reference);
    // The square round the crosshair, for the two comparisons that say the shader held it.
    const QRect aim(w / 2 - 16, h / 2 - 16, 32, 32);
    const QImage refAim = ref.copy(aim), plainAim = imageFromBgra(withoutHold).copy(aim);
    // The same for the line of text.
    const QRect text(int(band.x) + 92, int(band.y + band.h / 2) - 14, 100, 28);
    const QImage refText = ref.copy(text), plainText = imageFromBgra(withoutHold).copy(text);

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setColor(Qt::black);
    view.loadFromModule(QStringLiteral("PushFPS"), QStringLiteral("GpuCheck"));
    if (view.status() != QQuickView::Ready) {
        print(QStringLiteral("GPU CHECK RESULT\nresult: FAIL (GpuCheck.qml did not load)"));
        return 1;
    }
    if (auto* frame = view.rootObject() ? view.rootObject()->findChild<FrameView*>() : nullptr) {
        frame->setProtectFeather(6.0);
        frame->setProtect(boxVariants);
        frame->setAmbient(true);
    }
    // The rectangle's inside (6 px of feather left out), for the comparison that says the shader held it.
    const QRect boxRect(int(0.62f * w) + 7, int(0.06f * h) + 7, int(0.32f * w) - 14, int(0.34f * h) - 14);
    const QImage boxA = ia.copy(boxRect), boxPlain = imageFromBgra(withoutHold).copy(boxRect);
    view.resize(w, h);
    view.show();
    int code = 4;
    QTimer::singleShot(1500, &app, [&] {
        const QImage shot = view.grabWindow().convertToFormat(QImage::Format_RGB32).scaled(w, h);
        const auto api = view.rendererInterface()->graphicsApi();
        const double match = psnr(shot, ref, 4), againstA = psnr(shot, ia.convertToFormat(QImage::Format_RGB32), 4);
        const double refVsA = psnr(ref, ia.convertToFormat(QImage::Format_RGB32), 4);
        // Diagnostics for a mismatch: an upside-down grab, or the shader showing frame B.
        const double flipped = psnr(shot.mirrored(false, true), ref, 4);
        const double againstB = psnr(shot, ib.convertToFormat(QImage::Format_RGB32), 4);
        if (!savePath.isEmpty()) {
            shot.save(savePath);
            ref.save(QFileInfo(savePath).absolutePath() + QStringLiteral("/gpucheck-cpu.png"));
        }
        const bool software = api == QSGRendererInterface::Software;
        // Crosshair Hold: round the crosshair the shader's picture is the reference with it held, and is
        // clearly not the reference without it.
        const QImage shotAim = shot.copy(aim);
        const double aimHeld = psnr(shotAim, refAim, 0), aimPlain = psnr(shotAim, plainAim, 0);
        const bool holdOk = held != nullptr && aimHeld > 32.0 && aimHeld > aimPlain + 3.0;
        const QImage shotText = shot.copy(text);
        const double textHeld = psnr(shotText, refText, 0), textPlain = psnr(shotText, plainText, 0);
        const bool subOk = heldText != nullptr && textHeld > 32.0 && textHeld > textPlain + 3.0;
        const QImage shotBox = shot.copy(boxRect);
        const double boxHeld = psnr(shotBox, boxA, 0), boxPlainDb = psnr(shotBox, boxPlain, 0);
        const bool boxOk = boxHeld > 32.0 && boxHeld > boxPlainDb + 3.0;
        code = !software && match > 38.0 && match > againstA + 3.0 && holdOk && subOk && boxOk ? 0 : 4;
        print(QStringLiteral("GPU CHECK RESULT\ngraphics api: %1\nGPU vs CPU reference: %2 dB\nGPU vs real frame A: %3 dB (CPU reference vs A: %4 dB)\n"
                             "crosshair hold: round the crosshair %8 dB against the reference that holds it, %9 dB against the one that does not (%10 px held)\n"
                             "subtitle hold: round the text %11 dB against the reference that holds it, %12 dB against the one that does not (%13 px held)\n"
                             "hud boxes: inside the box %14 dB against real frame A, %15 dB against the picture without boxes\n"
                             "diagnostics: upside down vs reference %6 dB, GPU vs real frame B %7 dB\n%5")
                  .arg(int(api)).arg(match, 0, 'f', 1).arg(againstA, 0, 'f', 1).arg(refVsA, 0, 'f', 1)
                  .arg(software ? QStringLiteral("result: FAIL (software renderer, no shaders)") : code == 0 ? QStringLiteral("picture: PASS") : QStringLiteral("picture: FAIL (mismatch)"))
                  .arg(flipped, 0, 'f', 1).arg(againstB, 0, 'f', 1).arg(aimHeld, 0, 'f', 1).arg(aimPlain, 0, 'f', 1).arg(held ? held->held : 0)
                  .arg(textHeld, 0, 'f', 1).arg(textPlain, 0, 'f', 1).arg(heldText ? heldText->held : 0)
                  .arg(boxHeld, 0, 'f', 1).arg(boxPlainDb, 0, 'f', 1));
        if (software) { app.exit(code); return; }
        // Ambient fill: a 4:3 window leaves bars above and below the 16:9 picture. What the bars show has to
        // be the two copies blended by the phase, at the size that covers the window (engine/PushAmbient.h),
        // sampled as a linear texture is; and not black, as they are without it.
        const int bh = 480;
        view.resize(w, bh);
        QTimer::singleShot(1000, &app, [&, bh] {
            const QImage bars = view.grabWindow().convertToFormat(QImage::Format_RGB32).scaled(w, bh);
            const auto [cw, ch] = veyra::engine::pushAmbientCover(float(w), float(bh), float(w), float(h));
            const Bgra& sa = *pick->ambientA;
            const Bgra& sb = *pick->ambientB;
            const auto texel = [&](int x, int y, int c) {
                x = std::clamp(x, 0, sa.width - 1);
                y = std::clamp(y, 0, sa.height - 1);
                return 0.5 * sa.pixel(x, y)[c] + 0.5 * sb.pixel(x, y)[c];
            };
            QImage ref(w, bh, QImage::Format_RGB32);
            for (int y = 0; y < bh; ++y)
                for (int x = 0; x < w; ++x) {
                    // The window's pixel centre in the cover, then in the copy's texels.
                    const double u = ((x + 0.5) - (w / 2.0 - cw / 2.0)) / cw * sa.width - 0.5;
                    const double v = ((y + 0.5) - (bh / 2.0 - ch / 2.0)) / ch * sa.height - 0.5;
                    const int x0 = int(std::floor(u)), y0 = int(std::floor(v));
                    const double fx = u - x0, fy = v - y0;
                    int rgb[3];
                    for (int c = 0; c < 3; ++c)
                        rgb[c] = int(std::lround((texel(x0, y0, c) * (1 - fx) + texel(x0 + 1, y0, c) * fx) * (1 - fy) +
                                                 (texel(x0, y0 + 1, c) * (1 - fx) + texel(x0 + 1, y0 + 1, c) * fx) * fy));
                    ref.setPixel(x, y, qRgb(rgb[2], rgb[1], rgb[0]));
                }
            // The bars, two pixels away from the picture's edge.
            const int bar = (bh - h) / 2 - 2;
            const QImage top = bars.copy(0, 0, w, bar), bottom = bars.copy(0, bh - bar, w, bar);
            const double barsDb = std::min(psnr(top, ref.copy(0, 0, w, bar), 0), psnr(bottom, ref.copy(0, bh - bar, w, bar), 0));
            QImage black(w, bar, QImage::Format_RGB32);
            black.fill(Qt::black);
            const double againstBlack = std::max(psnr(top, black, 0), psnr(bottom, black, 0));
            const bool ambientOk = barsDb > 32.0 && barsDb > againstBlack + 3.0;
            print(QStringLiteral("ambient fill: the bars %1 dB against the reference, %2 dB against black bars\nresult: %3")
                      .arg(barsDb, 0, 'f', 1).arg(againstBlack, 0, 'f', 1).arg(code == 0 && ambientOk ? QStringLiteral("PASS") : QStringLiteral("FAIL (mismatch)")));
            app.exit(code == 0 && ambientOk ? 0 : 4);
        });
    });
    return app.exec();
}


// Upscales the test picture with NIS (twice its size when the window gets the size asked for) (FrameView's upscaling, NisNode.cpp) and
// checks what comes out. There is no CPU reference of NIS here: what is checked is that the picture drawn
// is the picture (close to a plain smooth enlargement of it, far from it upside down), that upscaling
// says it runs, and that the sharpening reaches the shader (0 and 1 give different pictures).
int nischeck(QGuiApplication& app, const QString& savePath) {
    const int w = 640, h = 360;
    const QImage ia = TestPattern::render(w, h, 2.0).convertToFormat(QImage::Format_RGB32);
    auto pick = std::make_shared<Pick>();
    pick->a = std::make_shared<const Bgra>(bgraFromImage(ia));
    pick->b = pick->a;
    pick->aId = pick->bId = 1;
    pick->t = 0.0f;
    FrameView::setFixedPick(pick);

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setColor(Qt::black);
    view.loadFromModule(QStringLiteral("PushFPS"), QStringLiteral("GpuCheck"));
    FrameView* frame = view.rootObject() ? view.rootObject()->findChild<FrameView*>() : nullptr;
    if (view.status() != QQuickView::Ready || !frame) {
        print(QStringLiteral("NIS CHECK RESULT\nresult: FAIL (GpuCheck.qml did not load)"));
        return 1;
    }
    frame->setUpscale(true);
    frame->setUpscaleTarget(2);
    frame->setSharpness(0.0);
    view.resize(2 * w, 2 * h);
    view.show();
    // The window manager may not give the window the size asked for: the picture is cut out where
    // FrameView puts it (whole, with bars), and the smooth enlargement made at that size.
    const auto grab = [&] {
        const QImage shot = view.grabWindow().convertToFormat(QImage::Format_RGB32);
        const double scale = std::min(double(shot.width()) / w, double(shot.height()) / h);
        const int pw = int(std::lround(w * scale)), ph = int(std::lround(h * scale));
        return shot.copy((shot.width() - pw) / 2, (shot.height() - ph) / 2, pw, ph);
    };
    int code = 4;
    QImage soft;
    int softState = -1;
    QString softSizes;
    QTimer::singleShot(1500, &app, [&] {
        soft = grab();
        softState = frame->upscaleState();
        softSizes = frame->upscaleSizes();
        frame->setSharpness(1.0);
        QTimer::singleShot(800, &app, [&] {
            const QImage sharp = grab();
            const QImage smooth = ia.scaled(soft.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            const auto api = view.rendererInterface()->graphicsApi();
            const double match = psnr(soft, smooth, 4), flipped = psnr(soft.mirrored(false, true), smooth, 4);
            const double sharpVsSoft = psnr(sharp, soft, 4), sharpMatch = psnr(sharp, smooth, 4);
            if (!savePath.isEmpty()) {
                soft.save(savePath);
                sharp.save(QFileInfo(savePath).absolutePath() + QStringLiteral("/nischeck-sharp.png"));
                smooth.save(QFileInfo(savePath).absolutePath() + QStringLiteral("/nischeck-smooth.png"));
            }
            const bool running = softState == FrameView::UpscaleRunning && frame->upscaleState() == FrameView::UpscaleRunning;
            code = running && match > 28.0 && match > flipped + 6.0 && sharpMatch > 24.0 && sharpVsSoft < 50.0 ? 0 : 4;
            print(QStringLiteral("NIS CHECK RESULT\ngraphics api: %1\nupscaling state: %2 (%3)\nNIS at sharpness 0 vs a smooth enlargement: %4 dB (upside down: %5 dB)\n"
                                 "NIS at sharpness 1 vs the smooth enlargement: %6 dB, vs sharpness 0: %7 dB\nresult: %8")
                      .arg(int(api)).arg(softState).arg(softSizes).arg(match, 0, 'f', 1).arg(flipped, 0, 'f', 1)
                      .arg(sharpMatch, 0, 'f', 1).arg(sharpVsSoft, 0, 'f', 1)
                      .arg(code == 0 ? QStringLiteral("PASS") : running ? QStringLiteral("FAIL (picture)") : QStringLiteral("FAIL (upscaling did not run)")));
            app.exit(code);
        });
    });
    return app.exec();
}

}  // namespace


// The layout check (--layoutcheck): opens every page and every dialog of the real window and checks that no
// row's hover light (VRow.qml: 7 px past the row on each side, in the new design) is cut by a list that
// clips it, or runs under that list's scroll bar (its width and 4 px). A row's light that is cut shows as a
// row whose left margin is off. Returns the rows at fault, as lines to print.
QStringList hoverRoomFaults(QQuickWindow* window, const QString& where) {
    QStringList faults;
    constexpr qreal kLight = 7, kGap = 4;
    std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
        if (!item || !item->isVisible()) return;
        const QString type = QString::fromLatin1(item->metaObject()->className());
        if ((type == QLatin1String("VRow") || type.startsWith(QLatin1String("VRow_QML"))) && item->width() > 1 && item->height() > 1) {
            const QRectF light = item->mapRectToScene(QRectF(-kLight, 0, item->width() + 2 * kLight, item->height()));
            for (QQuickItem* a = item->parentItem(); a; a = a->parentItem()) {
                if (!a->clip()) continue;
                const QRectF box = a->mapRectToScene(QRectF(0, 0, a->width(), a->height()));
                qreal right = box.right();
                for (QQuickItem* c : a->childItems())
                    if (c->inherits("QQuickScrollBar") && c->property("orientation").toInt() == Qt::Vertical)
                        right = std::min(right, c->mapRectToScene(QRectF(0, 0, c->width(), c->height())).left() - kGap);
                if (light.left() < box.left() - 0.5 || light.right() > right + 0.5) {
                    const QString name = item->objectName().isEmpty() ? item->property("label").toString() : item->objectName();
                    faults << QStringLiteral("%1: row \"%2\" light %3..%4, room %5..%6 (%7)")
                                  .arg(where, name).arg(light.left(), 0, 'f', 1).arg(light.right(), 0, 'f', 1)
                                  .arg(box.left(), 0, 'f', 1).arg(right, 0, 'f', 1)
                                  .arg(a->objectName().isEmpty() ? QString::fromLatin1(a->metaObject()->className()) : a->objectName());
                    break;
                }
            }
        }
        for (QQuickItem* c : item->childItems()) walk(c);
    };
    walk(window->contentItem());
    return faults;
}

int main(int argc, char** argv) {
    QCoreApplication::setOrganizationName(QStringLiteral("PushFPS"));
    QCoreApplication::setApplicationName(QStringLiteral("PushFPS"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PUSHFPS_PORTABLE_VERSION));
    QGuiApplication app(argc, argv);
#ifdef PUSHFPS_MAC_ACTIVITY
    keepTimingPrecise();
#endif
    app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/PushFPS/icon.png")));

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("PushFPS for Linux and macOS (preview)"));
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption list(QStringLiteral("list-devices"), QStringLiteral("List capture cards and sound devices."));
    QCommandLineOption self(QStringLiteral("selftest"), QStringLiteral("Run the pipeline without a window for N seconds."), QStringLiteral("seconds"));
    QCommandLineOption src(QStringLiteral("source"), QStringLiteral("testpattern, testpattern-uneven, file:<path> or camera[:N]."), QStringLiteral("source"), QStringLiteral("testpattern"));
    QCommandLineOption target(QStringLiteral("target"), QStringLiteral("Target frame rate (0 = match display)."), QStringLiteral("fps"), QStringLiteral("0"));
    QCommandLineOption pacing(QStringLiteral("pacing"), QStringLiteral("Frame Pacing Repair on."));
    QCommandLineOption hud(QStringLiteral("hud"), QStringLiteral("Live HUD Lock on."));
    QCommandLineOption gpu(QStringLiteral("gpucheck"), QStringLiteral("Compare the GPU shader with the CPU reference."));
    QCommandLineOption nis(QStringLiteral("nischeck"), QStringLiteral("Check upscaling with NVIDIA Image Scaling."));
    QCommandLineOption save(QStringLiteral("save"), QStringLiteral("Save a picture (self-test / GPU check)."), QStringLiteral("png"));
    QCommandLineOption shot(QStringLiteral("screenshot"), QStringLiteral("Open the interface, save a screenshot, quit."), QStringLiteral("png"));
    QCommandLineOption page(QStringLiteral("page"), QStringLiteral("Start page: home, pro, min or set."), QStringLiteral("page"), QStringLiteral("home"));
    QCommandLineOption lang(QStringLiteral("lang"), QStringLiteral("Interface language code."), QStringLiteral("code"));
    QCommandLineOption dialog(QStringLiteral("dialog"), QStringLiteral("With --screenshot: open a dialog first (crop, diagnose, hud or games)."), QStringLiteral("key"));
    QCommandLineOption layout(QStringLiteral("layoutcheck"), QStringLiteral("Open every page and dialog and check that no row's hover light is cut."));
    cli.addOptions({list, self, src, target, pacing, hud, gpu, nis, save, shot, page, lang, dialog, layout});
    cli.process(app);

    if (cli.isSet(list)) return listDevices();
    if (cli.isSet(self))
        return selftest(app, std::clamp(cli.value(self).toDouble(), 1.0, 600.0), cli.value(src), cli.value(target).toInt(),
                        cli.isSet(pacing), cli.isSet(hud), cli.value(save));
    if (cli.isSet(gpu)) return gpucheck(app, cli.value(save));
    if (cli.isSet(nis)) return nischeck(app, cli.value(save));

    CatalogTranslator translator;
    {
        auto settings = Storage::open();
        translator.load(cli.isSet(lang) ? cli.value(lang) : settings->value(QStringLiteral("language"), QStringLiteral("auto")).toString());
    }
    app.installTranslator(&translator);

    // The engine is closed before `controller` and `shim` go (see the end): the interface reads both while
    // it is taken down, and they are declared after it.
    auto engine = std::make_unique<QQmlApplicationEngine>();
    QQmlApplicationEngine& qml = *engine;
    // The pages: home, pro, min (the picture alone) and set. "player" and "settings" are their names before 0.2.
    QString startPage = cli.isSet(shot) ? cli.value(page) : QStringLiteral("home");
    if (startPage == QLatin1String("player")) startPage = QStringLiteral("min");
    if (startPage == QLatin1String("settings")) startPage = QStringLiteral("set");
    AppController controller(&qml, &translator, startPage);
    if (cli.isSet(lang)) translator.load(cli.value(lang));  // a one-off choice, not saved
    qml.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    qml.addImageProvider(QStringLiteral("pushfps-frame"), new NewestFrameProvider(controller));
    // What the interface parts shared with the Windows build read as `veyra` (VeyraShim.h).
    VeyraShim shim(&controller);
    qml.rootContext()->setContextProperty(QStringLiteral("veyra"), &shim);
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    qml.loadFromModule(QStringLiteral("PushFPS"), QStringLiteral("Main"));
    if (qml.rootObjects().isEmpty()) return 1;

    if (cli.isSet(shot)) {
        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().first());
        if (startPage == QStringLiteral("min") || startPage == QStringLiteral("pro")) controller.startTestPattern(false);
        const QString path = cli.value(shot);
        if (cli.isSet(dialog)) QTimer::singleShot(1500, &app, [&controller, key = cli.value(dialog)] { controller.openDialog(key); });
        QTimer::singleShot(3000, &app, [&, window, path] {
            const QImage image = window ? window->grabWindow() : QImage();
            const bool ok = !image.isNull() && image.save(path);
            print(QStringLiteral("screenshot %1: %2").arg(path, ok ? QStringLiteral("saved") : QStringLiteral("FAILED")));
            controller.stop();
            app.exit(ok ? 0 : 1);
        });
    }
    if (cli.isSet(layout)) {
        // Every page with the test picture playing (some rows show only then), then every dialog over the home
        // page. Each step waits for the page or the dialog to be laid out and faded in.
        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().first());
        QObject* root = qml.rootObjects().first();
        controller.startTestPattern(false);
        struct Step { QString page, dialog; };
        const QList<Step> steps{{"home", {}}, {"pro", {}}, {"set", {}}, {"min", {}},
                                {"home", "crop"}, {"home", "diagnose"}, {"home", "hud"}, {"home", "games"}};
        auto faults = std::make_shared<QStringList>();
        auto next = std::make_shared<std::function<void(int)>>();
        *next = [&, window, root, steps, faults, next](int i) {
            if (i >= steps.size()) {
                for (const QString& f : *faults) print(f);
                print(QStringLiteral("LAYOUT CHECK RESULT\nsteps: %1\nrows whose hover light is cut: %2\nresult: %3")
                          .arg(steps.size()).arg(faults->size()).arg(faults->isEmpty() ? QStringLiteral("PASS") : QStringLiteral("FAIL")));
                controller.stop();
                app.exit(faults->isEmpty() ? 0 : 5);
                return;
            }
            const Step step = steps[i];
            QMetaObject::invokeMethod(root, "go", Q_ARG(QVariant, step.page));
            if (!step.dialog.isEmpty()) QTimer::singleShot(500, &app, [&controller, d = step.dialog] { controller.openDialog(d); });
            QTimer::singleShot(1500, &app, [&, window, root, step, faults, next, i] {
                *faults << hoverRoomFaults(window, step.dialog.isEmpty() ? step.page : step.page + QStringLiteral("+") + step.dialog);
                // Close what was opened: every Popup that is open.
                for (QObject* o : root->findChildren<QObject*>())
                    if (o->inherits("QQuickPopup") && o->property("opened").toBool()) QMetaObject::invokeMethod(o, "close");
                QTimer::singleShot(400, &app, [next, i] { (*next)(i + 1); });
            });
        };
        QTimer::singleShot(1500, &app, [next] { (*next)(0); });
    }
    const int result = app.exec();
    engine.reset();
    return result;
}
