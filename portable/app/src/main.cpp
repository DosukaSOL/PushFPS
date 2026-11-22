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
//   PushFPS --screenshot <png> [--page home|player|settings] [--lang <code>]
#include "MacActivity.h"
#include "AppController.h"
#include "FrameView.h"
#include "Settings.h"
#include "TestPattern.h"
#include "Translator.h"
#include "VideoFrames.h"

#include "veyra/portable/Interpolate.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QMediaDevices>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
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
                if (interpolate(*p.a, *p.b, p.fields->forward, p.fields->backward, p.fields->hudMask.get(), p.t, out, 1)) {
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
    const QImage ia = TestPattern::render(w, h, 2.0), ib = TestPattern::render(w, h, 2.0 + 1.0 / 30.0);
    const Bgra a = bgraFromImage(ia), b = bgraFromImage(ib);
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
    FrameView::setFixedPick(pick);
    Bgra reference;
    interpolate(a, b, fields->forward, fields->backward, nullptr, 0.5f, reference, 2);
    const QImage ref = imageFromBgra(reference);

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setColor(Qt::black);
    view.loadFromModule(QStringLiteral("PushFPS"), QStringLiteral("GpuCheck"));
    if (view.status() != QQuickView::Ready) {
        print(QStringLiteral("GPU CHECK RESULT\nresult: FAIL (GpuCheck.qml did not load)"));
        return 1;
    }
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
        code = !software && match > 38.0 && match > againstA + 3.0 ? 0 : 4;
        print(QStringLiteral("GPU CHECK RESULT\ngraphics api: %1\nGPU vs CPU reference: %2 dB\nGPU vs real frame A: %3 dB (CPU reference vs A: %4 dB)\n"
                             "diagnostics: upside down vs reference %6 dB, GPU vs real frame B %7 dB\nresult: %5")
                  .arg(int(api)).arg(match, 0, 'f', 1).arg(againstA, 0, 'f', 1).arg(refVsA, 0, 'f', 1)
                  .arg(code == 0 ? QStringLiteral("PASS") : (software ? QStringLiteral("FAIL (software renderer, no shaders)") : QStringLiteral("FAIL (mismatch)")))
                  .arg(flipped, 0, 'f', 1).arg(againstB, 0, 'f', 1));
        app.exit(code);
    });
    return app.exec();
}

}  // namespace

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
    QCommandLineOption save(QStringLiteral("save"), QStringLiteral("Save a picture (self-test / GPU check)."), QStringLiteral("png"));
    QCommandLineOption shot(QStringLiteral("screenshot"), QStringLiteral("Open the interface, save a screenshot, quit."), QStringLiteral("png"));
    QCommandLineOption page(QStringLiteral("page"), QStringLiteral("Start page: home, player or settings."), QStringLiteral("page"), QStringLiteral("home"));
    QCommandLineOption lang(QStringLiteral("lang"), QStringLiteral("Interface language code."), QStringLiteral("code"));
    cli.addOptions({list, self, src, target, pacing, hud, gpu, save, shot, page, lang});
    cli.process(app);

    if (cli.isSet(list)) return listDevices();
    if (cli.isSet(self))
        return selftest(app, std::clamp(cli.value(self).toDouble(), 1.0, 600.0), cli.value(src), cli.value(target).toInt(),
                        cli.isSet(pacing), cli.isSet(hud), cli.value(save));
    if (cli.isSet(gpu)) return gpucheck(app, cli.value(save));

    CatalogTranslator translator;
    {
        auto settings = Storage::open();
        translator.load(cli.isSet(lang) ? cli.value(lang) : settings->value(QStringLiteral("language"), QStringLiteral("auto")).toString());
    }
    app.installTranslator(&translator);

    QQmlApplicationEngine qml;
    const QString startPage = cli.isSet(shot) ? cli.value(page) : QStringLiteral("home");
    AppController controller(&qml, &translator, startPage);
    if (cli.isSet(lang)) translator.load(cli.value(lang));  // a one-off choice, not saved
    qml.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    qml.loadFromModule(QStringLiteral("PushFPS"), QStringLiteral("Main"));
    if (qml.rootObjects().isEmpty()) return 1;

    if (cli.isSet(shot)) {
        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().first());
        if (startPage == QStringLiteral("player")) controller.startTestPattern(false);
        const QString path = cli.value(shot);
        QTimer::singleShot(2500, &app, [&, window, path] {
            const QImage image = window ? window->grabWindow() : QImage();
            const bool ok = !image.isNull() && image.save(path);
            print(QStringLiteral("screenshot %1: %2").arg(path, ok ? QStringLiteral("saved") : QStringLiteral("FAILED")));
            controller.stop();
            app.exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
