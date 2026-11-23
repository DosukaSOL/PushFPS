// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// QML bridge: PushFPS features that only read engine numbers or reuse existing
// settings paths - the latency readout and "Optimise for my PC" measurements,
// game-preset HUD regions, and sharing clips (clipboard, Discord).
#include "veyra/engine/PushHalfApproval.h"
#include "../QmlPlayerBridge_p.h"
#include "veyra/engine/PushArchivePaths.h"
#include "veyra/engine/PushCompareName.h"
#include "veyra/engine/PushBenchmarkProbe.h"
#include "veyra/pushflow/PushFlow2.h"
#include "veyra/engine/PushResponse.h"
#include "veyra/pushflow/PushFlowMotion.h"
#include "veyra/Brand.h"
#include "veyra/RuntimePaths.h"
#include "veyra/ngx/AnrProvider.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHttpMultiPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QCoreApplication>
#include <QFontMetricsF>
#include <cstring>
#include <cmath>
#include <QTimer>
#include <QDateTime>
#include "veyra/gfx/D3D12DeviceContext.h"

namespace veyra::ui {
namespace {
QVariant optionalNumber(const std::optional<double>& v) { return v && std::isfinite(*v) ? QVariant(*v) : QVariant(); }
// Discord's upload limit without server boosts or Nitro (2024 onwards).
constexpr qint64 kDiscordUploadLimit = 10ll * 1024 * 1024;
}

QVariantMap QmlPlayerBridge::pushLiveStats() const {
    const auto& s = impl_->snapshot;
    const auto& flow = s.metrics.flow;
    const bool facadeApplying = impl_->facade.applying();
    QVariantMap m;
    m["running"] = s.running;
    m["capture"] = s.capture;
    m["failed"] = s.failed || s.gpuFailed;
    m["sessionId"] = QVariant::fromValue(qulonglong(s.sessionId));
    m["desiredRevision"] = QVariant::fromValue(qulonglong(s.desired.revision));
    m["appliedRevision"] = QVariant::fromValue(qulonglong(s.applied.revision));
    m["frameRevision"] = QVariant::fromValue(qulonglong(flow.latest.frame.settingsRevision));
    m["applying"] = facadeApplying || s.applying;
    // Frames are being produced with the settings the interface asked for.
    m["settled"] = s.running && !facadeApplying && !s.applying && s.desired.revision == s.applied.revision &&
                   flow.latest.frame.settingsRevision == s.applied.revision;
    m["submitFps"] = optionalNumber(s.submissionFps);
    m["submitLowFps"] = optionalNumber(s.submissionLow1Fps);
    m["sourceFps"] = s.fps;
    m["contentFps"] = s.contentFps;
    m["captureFps"] = s.captureFps;
    m["captureReceived"] = QVariant::fromValue(qulonglong(s.captureReceived));
    m["captureDropped"] = QVariant::fromValue(qulonglong(s.captureDropped));
    m["latencyMs"] = optionalNumber(flow.softwareLatencyMs);
    m["latencyP95Ms"] = optionalNumber(flow.softwareLatencyP95Ms);
    m["latencySamples"] = QVariant::fromValue(qulonglong(flow.latencySamples));
    m["fgActive"] = s.fgActive;
    // The multiplier the last pair used (adaptive mode, output cap or a reduced group).
    m["fgMultiplierNow"] = s.previewFgMultiplier;
    // Frame Pacing Repair: kept frames seen and re-timed since the source opened.
    m["pacingFrames"] = QVariant::fromValue(qulonglong(s.pacingFrames));
    m["hudLockState"] = s.hudLockState;
    m["hudStillState"] = s.hudStillState;
    // Crosshair Hold: 0 not asked, 1 running, 2 could not be set up, 3 not with this method or without frame generation.
    m["crosshairHoldState"] = s.crosshairHoldState;
    m["crosshairHoldPixels"] = s.crosshairHoldPixels;
    m["crosshairHoldFrames"] = double(s.crosshairHoldFrames);
    // Character Hold: the same four states; the blocks (of 2040) with a person in the pair sampled last; why it could not start.
    m["characterHoldState"] = s.characterHoldState;
    m["characterHoldBlocks"] = s.characterHoldBlocks;
    m["characterHoldFrames"] = double(s.characterHoldFrames);
    m["characterHoldReason"] = QString::fromStdString(s.characterHoldReason);
    // PushFPS 4.2.1: the FSR upscaler really running (quick setup's upscale row says it).
    m["fsrUpscalerVersion"] = QString::fromStdString(s.fsrSrProviderVersion);
    m["pushFlowPrecision"] = QString::fromStdString(s.pushFlowPrecision);
    m["pushFlowRebuild"] = s.pushFlowRebuild;
    m["pushFlowP50Ms"] = s.pushFlowP50Ms;
    m["pushFlowP95Ms"] = s.pushFlowP95Ms;
    // The size PushFlow works the motion out at right now (0 while it is not running).
    m["pushFlowMotionWidth"] = int(s.pushFlowMotionWidth);
    m["pushFlowMotionHeight"] = int(s.pushFlowMotionHeight);
    // PushFlow 2's encoder cache: "" not asked for, "on", or why it is not in use; and what it did.
    m["pushFlowEncoderCache"] = QString::fromStdString(s.pushFlowEncoderCache);
    m["pushFlowFeaturesKept"] = QVariant::fromValue(qulonglong(s.pushFlowFeaturesKept));
    m["pushFlowFeaturesMade"] = QVariant::fromValue(qulonglong(s.pushFlowFeaturesMade));
    m["pushFlowWholeMs"] = s.pushFlowWholeMs;
    m["pushFlowSplitMs"] = s.pushFlowSplitMs;
    m["pushFlow2Stopped"] = pushflow2::emergencyOff();
    // PushFlow 2's Guard: "" not asked for, "on", or why not; generated frames judged, mended in places, held whole.
    m["pushFlowGuard"] = QString::fromStdString(s.pushFlowGuard);
    m["pushFlowGuardFrames"] = QVariant::fromValue(qulonglong(s.pushFlowGuardFrames));
    m["pushFlowGuardMended"] = QVariant::fromValue(qulonglong(s.pushFlowGuardMended));
    m["pushFlowGuardHeld"] = QVariant::fromValue(qulonglong(s.pushFlowGuardHeld));
    // PushFlow 2's Predict: "" not asked for, "on", or why not; forecast frames made, the share of the
    // moving picture it did not trust and held (percent), forecasts not shown because the next real frame
    // was already due, and whether it has stepped aside for a measurement.
    m["pushFlowPredict"] = QString::fromStdString(s.pushFlowPredict);
    m["pushFlowPredictFrames"] = QVariant::fromValue(qulonglong(s.pushFlowPredictFrames));
    m["pushFlowPredictHeldPercent"] = s.pushFlowPredictMoving ? 100.0 * double(s.pushFlowPredictHeld) / double(s.pushFlowPredictMoving) : 0.0;
    m["pushFlowPredictMovingPercent"] = s.pushFlowPredictCounted ? 100.0 * double(s.pushFlowPredictMoving) / double(s.pushFlowPredictCounted) : 0.0;
    m["pushFlowPredictSuperseded"] = QVariant::fromValue(qulonglong(s.pushFlowPredictSuperseded));
    m["pushFlowPredictPaused"] = pushflow2::predictPaused() || engine::pushBenchmarkProbe().active() || impl_->engine.qualityTest().running;
    // PushFlow 2's Dual: "" not asked for, "on", "needs-predict", "needs-2x"; frames handed to recordings
    // and streams in place of the ones shown (real frames, true in-between frames).
    m["pushFlowDual"] = QString::fromStdString(s.pushFlowDual);
    m["pushFlowDualReal"] = QVariant::fromValue(qulonglong(s.pushFlowDualReal));
    m["pushFlowDualBetween"] = QVariant::fromValue(qulonglong(s.pushFlowDualBetween));
    // PushFlow 2's Response, first step (a measurement): "" not asked for, "needs-predict", "no-stick" (no
    // right stick is being sent through PushFPS), or "on" with what was found so far.
    {
        auto& watch = engine::response::observer();
        QString state;
        if (watch.listening()) {
            const auto fit = watch.fit();
            state = s.pushFlowPredict != "on" ? QStringLiteral("needs-predict") : watch.sticksSeen() == 0 ? QStringLiteral("no-stick") : QStringLiteral("on");
            m["pushFlowResponseFrames"] = QVariant::fromValue(qulonglong(fit.frames));
            m["pushFlowResponseActive"] = QVariant::fromValue(qulonglong(fit.active));
            m["pushFlowResponseEnough"] = fit.enough;
            m["pushFlowResponseFollows"] = fit.follows;
            m["pushFlowResponseDelayMs"] = double(fit.delayMs);
            m["pushFlowResponseGainX"] = double(fit.gainX);
            m["pushFlowResponseGainY"] = double(fit.gainY);
            m["pushFlowResponseExplained"] = double(fit.explained);
        }
        m["pushFlowResponse"] = state;
    }
    // PushFlow 2's Memory, first step (a measurement): "" not asked for, "needs-predict", or "on" with how
    // far apart two real frames are where the motion joins them (0..255) over the pairs measured so far.
    {
        const bool asked = pushflow2::wanted(pushflow2::Experiment::Memory);
        m["pushFlowMemory"] = !asked ? QString() : s.pushFlowPredict != "on" ? QStringLiteral("needs-predict") : QStringLiteral("on");
        m["pushFlowMemoryPairs"] = QVariant::fromValue(qulonglong(asked ? s.pushFlowMotionPairs : 0));
        m["pushFlowMemoryApart"] = asked ? double(s.pushFlowAlignedApart) : 0.0;
    }
    m["pacingRepaired"] = QVariant::fromValue(qulonglong(s.pacingRepaired));
    m["srActive"] = s.srActive;
    m["nrActive"] = s.nrActive;
    m["fgBudgetLimited"] = s.fgBudgetLimited;
    m["backendWarning"] = QString::fromStdWString(s.backendWarning);
    // What is really shown, a second: the game's own frames and the generated ones (FrameFlowWindow.h).
    m["realFps"] = flow.realPresentFps;
    m["generatedFps"] = flow.generatedPresentFps;
    // Frame generation health (engine/PushFgHealth.h): 0 fine, 1 nothing to add at this target or on
    // this screen, 2 the method is not running, 3 the card does not keep up, 4 nothing generated is shown.
    m["fgIdle"] = impl_->fgIdle;
    m["fgIdleText"] = pushFgIdleText(impl_->fgIdle);
    m["displayRefreshHz"] = impl_->displayRefreshHz;
    m["gpuUtilization"] = gpuUtilizationKnown() ? QVariant(gpuUtilization()) : QVariant();
    m["sourceWidth"] = int(s.sourceWidth);
    m["sourceHeight"] = int(s.sourceHeight);
    m["outputWidth"] = int(s.metrics.resolution.output.width);
    m["outputHeight"] = int(s.metrics.resolution.output.height);
    m["hdr"] = s.sourceHdr;
    return m;
}

void QmlPlayerBridge::pushBenchmarkBegin() { engine::pushBenchmarkProbe().begin(); }

QVariantMap QmlPlayerBridge::pushBenchmarkEnd() {
    const auto r = engine::pushBenchmarkProbe().end();
    veyra::log::info("push-benchmark", std::format("window valid={} seconds={:.2f} fps={:.2f} low1={:.2f} low01={:.2f} jitterMs={:.3f} "
        "presents={} generated={} gaps={} latencyMean={:.2f} p50={:.2f} p95={:.2f} samples={}",
        r.valid, r.seconds, r.fps, r.low1Fps, r.low01Fps, r.intervalStdMs, r.presents, r.generatedPresents, r.gaps,
        r.latencyMeanMs, r.latencyP50Ms, r.latencyP95Ms, r.latencySamples));
    return {{"valid", r.valid}, {"seconds", r.seconds}, {"fps", r.fps}, {"low1Fps", r.low1Fps}, {"low01Fps", r.low01Fps},
            {"jitterMs", r.intervalStdMs}, {"presents", QVariant::fromValue(qulonglong(r.presents))},
            {"generatedPresents", QVariant::fromValue(qulonglong(r.generatedPresents))},
            {"realPresents", QVariant::fromValue(qulonglong(r.realPresents))}, {"gaps", QVariant::fromValue(qulonglong(r.gaps))},
            {"latencyMeanMs", r.latencyMeanMs}, {"latencyP50Ms", r.latencyP50Ms}, {"latencyP95Ms", r.latencyP95Ms},
            {"latencySamples", QVariant::fromValue(qulonglong(r.latencySamples))}};
}

bool QmlPlayerBridge::pushSetExclusionRegions(const QVariantList& regions, double feather) {
    std::array<engine::ProtectionRect, 4> rects{};
    size_t count = 0;
    for (const auto& item : regions) {
        if (count >= rects.size()) break;
        const auto r = item.toMap();
        const auto num = [&](const char* key) {
            const double v = r.value(key).toDouble();
            return float(std::isfinite(v) ? std::clamp(v, 0.0, 1.0) : 0.0);
        };
        engine::ProtectionRect q{std::min(num("left"), num("right")), std::min(num("top"), num("bottom")),
                                 std::max(num("left"), num("right")), std::max(num("top"), num("bottom")), r.value("ellipse").toBool()};
        if (q.right - q.left < 0.005f || q.bottom - q.top < 0.005f) continue;
        rects[count++] = q;
    }
    const float px = float(std::isfinite(feather) ? std::clamp(feather, 0.0, 64.0) : 8.0);
    if (count == 0 && !impl_->chain.firstOf(engine::EffectType::Exclusion)) return true;  // nothing to turn off
    const bool ok = editProtection([&](engine::ChainNode& node) {
        node.protection.regions = rects;
        node.protection.featherPixels = px;
        node.protection.enabled = count > 0;
        node.enabled = count > 0;
        return true;
    }, true);
    veyra::log::info("push-presets", std::format("exclusion regions={} feather={:.1f} accepted={}", count, px, ok));
    return ok;
}

bool QmlPlayerBridge::pushCopyText(const QString& text) {
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard || text.size() > 1 << 20) return false;
    clipboard->setText(text);
    return true;
}

bool QmlPlayerBridge::pushCopyFile(const QString& path) {
    const QFileInfo info(path);
    auto* clipboard = QGuiApplication::clipboard();
    if (!clipboard || !info.isFile()) { emit notice(tr("文件不存在：%1").arg(path), true); return false; }
    auto* data = new QMimeData;
    data->setUrls({QUrl::fromLocalFile(info.absoluteFilePath())});
    clipboard->setMimeData(data);  // the clipboard owns it
    return true;
}

bool QmlPlayerBridge::pushOpenDiscord() {
    if (QDesktopServices::openUrl(QUrl(QStringLiteral("discord://-/channels/@me")))) return true;
    return QDesktopServices::openUrl(QUrl(QStringLiteral("https://discord.com/app")));
}

bool QmlPlayerBridge::pushShareToDiscord(const QString& path) {
    const QString webhook = impl_->prefString("pushDiscordWebhook").trimmed();
    const QFileInfo info(path);
    if (webhook.isEmpty()) { emit pushShareFinished(false, tr("请先在 设置 → 录制 中填写 Discord Webhook 地址")); return false; }
    if (!info.isFile()) { emit pushShareFinished(false, tr("文件不存在：%1").arg(path)); return false; }
    if (info.size() > kDiscordUploadLimit) {
        emit pushShareFinished(false, tr("片段有 %1 MB，Discord 默认只接收 10 MB 以内的文件。已复制到剪贴板，可在 Discord 中粘贴（会提示压缩或使用 Nitro）")
                                       .arg(double(info.size()) / (1024.0 * 1024.0), 0, 'f', 1));
        pushCopyFile(path);
        return false;
    }
    auto* file = new QFile(info.absoluteFilePath());
    if (!file->open(QIODevice::ReadOnly)) { delete file; emit pushShareFinished(false, tr("无法读取文件：%1").arg(path)); return false; }
    auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart payload;
    payload.setHeader(QNetworkRequest::ContentDispositionHeader, QStringLiteral("form-data; name=\"payload_json\""));
    payload.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    payload.setBody(QJsonDocument(QJsonObject{{"content", QStringLiteral("%1 · %2").arg(QString::fromLatin1(brand::kProductName), info.completeBaseName())}})
                        .toJson(QJsonDocument::Compact));
    multi->append(payload);
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"files[0]\"; filename=\"%1\"").arg(info.fileName().replace(QLatin1Char('"'), QLatin1Char('_'))));
    part.setHeader(QNetworkRequest::ContentTypeHeader, info.suffix().compare(QStringLiteral("mkv"), Qt::CaseInsensitive) == 0
                       ? QStringLiteral("video/x-matroska") : QStringLiteral("video/mp4"));
    part.setBodyDevice(file);
    file->setParent(multi);
    multi->append(part);
    auto* network = findChild<QNetworkAccessManager*>(QStringLiteral("pushfps-share"));
    if (!network) { network = new QNetworkAccessManager(this); network->setObjectName(QStringLiteral("pushfps-share")); }
    QNetworkRequest request{QUrl(webhook)};
    request.setTransferTimeout(120000);
    auto* reply = network->post(request, multi);
    multi->setParent(reply);
    const QString name = info.fileName();
    connect(reply, &QNetworkReply::finished, this, [this, reply, name] {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        veyra::log::info("push-share", std::format("discord webhook status={} error={}", status, int(reply->error())));
        if (ok) emit pushShareFinished(true, tr("已发送到 Discord：%1").arg(name));
        else if (status == 413) emit pushShareFinished(false, tr("Discord 拒绝了文件：太大"));
        else if (status == 401 || status == 404) emit pushShareFinished(false, tr("Discord Webhook 地址无效或已被删除"));
        else emit pushShareFinished(false, tr("发送到 Discord 失败（%1）").arg(status ? QString::number(status) : reply->errorString()));
        reply->deleteLater();
    });
    return true;
}

QString QmlPlayerBridge::pushImportTextFile(const QString& title) {
    const QString path = QFileDialog::getOpenFileName(nullptr, title, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      tr("PushFPS 游戏预设 (*.pushfps);;所有文件 (*)"));
    if (path.isEmpty()) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 256 * 1024) { emit notice(tr("无法读取文件：%1").arg(path), true); return {}; }
    return QString::fromUtf8(file.readAll());
}

bool QmlPlayerBridge::pushExportTextFile(const QString& title, const QString& suggestedName, const QString& text) {
    QString base = suggestedName;
    for (const QChar c : QStringLiteral("\\/:*?\"<>|")) base.replace(c, QLatin1Char('_'));
    const QString path = QFileDialog::getSaveFileName(nullptr, title,
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).filePath(base + QStringLiteral(".pushfps")),
        tr("PushFPS 游戏预设 (*.pushfps)"));
    if (path.isEmpty()) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(text.toUtf8()) < 0) {
        emit notice(tr("无法保存文件：%1").arg(path), true);
        return false;
    }
    emit notice(tr("已保存：%1").arg(QDir::toNativeSeparators(path)), false);
    return true;
}

// --- Apex Neural Rendering (ANR) ------------------------------------------------
namespace {
QString anrDirectory() { return QString::fromStdWString(ngx::AnrProvider::directory(runtime::localRuntimeDirectory().wstring())); }
// The folder of the package that holds ANR's frame generation DLL (Apex ships it as
// "...\Apex Neural Rendering FG\dlssg\nvngx_dlssg.dll"; the DLL's folder is self-contained).
QString findAnrDll(const QString& root) {
    QDirIterator it(root, {QStringLiteral("nvngx_dlssg.dll")}, QDir::Files, QDirIterator::Subdirectories);
    QString best;
    while (it.hasNext()) {
        const QString path = it.next();
        // Prefer ANR's own copy over any other nvngx_dlssg.dll a package might carry.
        if (best.isEmpty() || path.contains(QStringLiteral("Apex"), Qt::CaseInsensitive)) best = path;
    }
    return best;
}
// Copies a folder tree for the ANR add-on: links are never followed, programs and scripts are left
// out (engine/PushArchivePaths.h), and the file count and size are capped.
bool copyTree(const QString& from, const QString& to, int& files, int& skipped, qint64& bytes) {
    QDir source(from);
    if (!QDir().mkpath(to)) return false;
    for (const QFileInfo& info : source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden)) {
        if (info.isSymLink() || info.isJunction() || !engine::archive::safeEntry(info.fileName().toStdString())) { ++skipped; continue; }
        const QString target = QDir(to).filePath(info.fileName());
        if (info.isDir()) { if (!copyTree(info.filePath(), target, files, skipped, bytes)) return false; continue; }
        if (!info.isFile() || !engine::archive::copiedType(info.fileName().toStdString())) { ++skipped; continue; }
        bytes += info.size();
        if (++files > engine::archive::kMaxFiles || bytes > engine::archive::kMaxBytes) return false;
        QFile::remove(target);
        if (!QFile::copy(info.filePath(), target)) return false;
    }
    return true;
}
QString sha256Of(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return {};
    return QString::fromLatin1(h.result().toHex());
}
}

QVariantMap QmlPlayerBridge::pushAnrState() const {
    const QString dir = anrDirectory();
    return {{"installed", ngx::AnrProvider::available(runtime::localRuntimeDirectory().wstring())},
            {"folder", QDir::toNativeSeparators(dir)}, {"name", QString::fromLatin1(brand::kAnrName)},
            {"maker", QString::fromLatin1(brand::kAnrMaker)}, {"url", QString::fromLatin1(brand::kAnrUrl)}};
}

bool QmlPlayerBridge::pushInstallAnr() {
    const QString picked = QFileDialog::getOpenFileName(nullptr, tr("选择从 Apex Compute 下载的 ANR 压缩包"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        tr("ANR 安装包 (*.zip nvngx_dlssg.dll);;所有文件 (*)"));
    if (picked.isEmpty()) return false;
    QTemporaryDir staging;
    QString root = QFileInfo(picked).absolutePath();
    if (picked.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        if (!staging.isValid()) { emit notice(tr("无法创建临时文件夹"), true); return false; }
        const QString tar = QDir(QProcessEnvironment::systemEnvironment().value(QStringLiteral("SystemRoot"), QStringLiteral("C:/Windows")))
                                .filePath(QStringLiteral("System32/tar.exe"));
        // Look at every entry first: nothing that would land outside the staging folder is extracted.
        QProcess list;
        list.start(tar, {QStringLiteral("-tf"), picked});
        if (!list.waitForFinished(60000) || list.exitStatus() != QProcess::NormalExit || list.exitCode() != 0) {
            emit notice(tr("无法解压 ANR 压缩包：%1").arg(QFileInfo(picked).fileName()), true);
            return false;
        }
        const QStringList entries = QString::fromUtf8(list.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (entries.isEmpty() || entries.size() > engine::archive::kMaxFiles * 2) {
            emit notice(tr("ANR 压缩包的内容不对，已停止安装"), true);
            return false;
        }
        for (const QString& raw : entries) {
            const QString entry = raw.trimmed();
            if (!engine::archive::safeEntry(entry.toStdString())) {
                veyra::log::warn("anr-install", std::format("refused archive entry: {}", entry.toStdString()));
                emit notice(tr("ANR 压缩包里有指向安装文件夹以外的路径，为了安全已停止安装"), true);
                return false;
            }
        }
        QProcess unzip;
        unzip.start(tar, {QStringLiteral("-xf"), picked, QStringLiteral("-C"), staging.path()});
        if (!unzip.waitForFinished(120000) || unzip.exitStatus() != QProcess::NormalExit || unzip.exitCode() != 0) {
            emit notice(tr("无法解压 ANR 压缩包：%1").arg(QFileInfo(picked).fileName()), true);
            return false;
        }
        root = staging.path();
    }
    const QString dll = findAnrDll(root);
    if (dll.isEmpty()) { emit notice(tr("这个文件里没有 ANR 的 nvngx_dlssg.dll，请选择 Apex 的 anr-fg 安装包"), true); return false; }
    // Install into a new folder next to the old one, then swap: a failed copy leaves the previous
    // installation as it was.
    const QString target = anrDirectory();
    const QString fresh = target + QStringLiteral(".new"), previous = target + QStringLiteral(".old");
    QDir(fresh).removeRecursively();
    QDir(previous).removeRecursively();
    int files = 0, skipped = 0;
    qint64 bytes = 0;
    if (!copyTree(QFileInfo(dll).absolutePath(), fresh, files, skipped, bytes) || !QFileInfo::exists(QDir(fresh).filePath(QStringLiteral("nvngx_dlssg.dll")))) {
        QDir(fresh).removeRecursively();
        emit notice(tr("无法复制 ANR 文件到 %1").arg(QDir::toNativeSeparators(target)), true);
        return false;
    }
    const bool hadOld = QFileInfo::exists(target);
    if (hadOld && !QDir().rename(target, previous)) {
        QDir(fresh).removeRecursively();
        emit notice(tr("ANR 文件正在使用中，请关闭画面后再安装"), true);
        return false;
    }
    if (!QDir().rename(fresh, target)) {
        if (hadOld) QDir().rename(previous, target);
        QDir(fresh).removeRecursively();
        emit notice(tr("无法复制 ANR 文件到 %1").arg(QDir::toNativeSeparators(target)), true);
        return false;
    }
    QDir(previous).removeRecursively();
    // What was installed, for the log and the user (ANR has no published checksums to compare with).
    const QString hash = sha256Of(QDir(target).filePath(QStringLiteral("nvngx_dlssg.dll")));
    veyra::log::info("anr-install", std::format("installed files={} skipped={} bytes={} nvngx_dlssg.dll sha256={} from={}",
                                                files, skipped, bytes, hash.toStdString(), QFileInfo(picked).fileName().toStdString()));
    refreshComponentList();
    emit notice(tr("ANR 已安装（%1 个文件）。重新启动 PushFPS 后，可在补帧方式中选择 ANR").arg(files)
                + (skipped > 0 ? tr("；没有复制 %1 个程序或脚本文件").arg(skipped) : QString())
                + QStringLiteral(" · SHA-256 ") + hash.left(16), false);
    return true;
}

bool QmlPlayerBridge::pushOpenAnrFolder() {
    const QString dir = anrDirectory();
    QDir().mkpath(dir);
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

QString QmlPlayerBridge::pushLinkUrl(const QString& key) const {
    if (key == QLatin1String("repo")) return QString::fromLatin1(brand::kRepositoryUrl);
    if (key == QLatin1String("issues")) return QString::fromLatin1(brand::kIssuesUrl);
    if (key == QLatin1String("veyra")) return QString::fromLatin1(brand::kUpstreamUrl);
    if (key == QLatin1String("anr")) return QString::fromLatin1(brand::kAnrUrl);
    if (key == QLatin1String("rife")) return QString::fromLatin1(brand::kRifeUrl);
    if (key == QLatin1String("anrReleases")) return QString::fromLatin1(brand::kAnrReleasesUrl);
    if (key == QLatin1String("apex")) return QString::fromLatin1(brand::kApexUrl);
    if (key == QLatin1String("creator")) return QString::fromLatin1(brand::kCreatorUrl);
    if (key == QLatin1String("discord")) return QString::fromLatin1(brand::kDiscordUrl);
    if (key == QLatin1String("frameInputDocs")) return QString::fromLatin1(brand::kRepositoryUrl) + QStringLiteral("/blob/main/docs/FRAME-INPUT-API.md");
    if (key == QLatin1String("controlDocs")) return QString::fromLatin1(brand::kRepositoryUrl) + QStringLiteral("/blob/main/docs/LOCAL-CONTROL.md");
    return {};
}

bool QmlPlayerBridge::pushOpenLink(const QString& key) {
    const QString url = pushLinkUrl(key);
    return !url.isEmpty() && QDesktopServices::openUrl(QUrl(url));
}


// --- PushFPS automatic HUD detection ------------------------------------------
bool QmlPlayerBridge::pushStartHudDetection(int samples) {
    if (!hasSource()) { emit notice(tr("没有正在播放的画面，无法识别 HUD"), true); return false; }
    impl_->engine.startHudDetection(uint32_t(std::clamp(samples, 1, 32)), 350);
    return true;
}

void QmlPlayerBridge::pushCancelHudDetection() { impl_->engine.cancelHudDetection(); }

QVariantMap QmlPlayerBridge::pushHudDetection() {
    const auto st = impl_->engine.hudDetection();
    QVariantMap m;
    m["running"] = st.running;
    m["done"] = st.done;
    m["failed"] = st.failed;
    m["taken"] = int(st.taken);
    m["target"] = int(st.target);
    m["error"] = st.error.empty() ? QString() : uiText(st.error);
    m["tooStill"] = st.detection.tooStill;
    m["motion"] = st.detection.motion;
    m["revision"] = QVariant::fromValue(qulonglong(st.revision));
    QVariantList regions;
    for (const auto& r : st.detection.regions)
        regions << QVariantMap{{"left", r.left}, {"top", r.top}, {"right", r.right}, {"bottom", r.bottom}, {"ellipse", false}};
    m["regions"] = regions;
    // The last frame taken, for the box editor: written once per run.
    if (st.done && st.preview.size() == size_t(engine::hud::kPreviewWidth) * engine::hud::kPreviewHeight * 3 &&
        st.revision != impl_->hudPreviewRevision) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        const QString path = QDir(dir).filePath(QStringLiteral("pushfps-hud-%1.png").arg(qulonglong(st.revision)));
        const QImage image(st.preview.data(), int(engine::hud::kPreviewWidth), int(engine::hud::kPreviewHeight),
                           int(engine::hud::kPreviewWidth * 3), QImage::Format_RGB888);
        if (image.save(path)) {
            if (!impl_->hudPreviewPath.isEmpty() && impl_->hudPreviewPath != path) QFile::remove(impl_->hudPreviewPath);
            impl_->hudPreviewPath = path;
        }
        impl_->hudPreviewRevision = st.revision;
    }
    m["preview"] = st.done && !impl_->hudPreviewPath.isEmpty() && st.revision == impl_->hudPreviewRevision
        ? QUrl::fromLocalFile(impl_->hudPreviewPath).toString() : QString();
    return m;
}

// --- PushFPS before/after comparison clips ------------------------------------
namespace {
// The comparison clip's two badges, drawn here (UI thread) as straight-alpha RGBA and blended
// onto the video by the recorder (engine/RecordingCompare.h). Both are dark translucent pills:
//  - console side: a grey dot, the word "Console" and its frame rate;
//  - PushFPS side: the PushFPS logo (qml/Veyra/pushfps-badge-logo.png) and its frame rate,
//    with a logo-green outline and "FPS" in logo green.
// Sized for the picture height; the recorder scales them when the recorded size differs.
constexpr QRgb kBadgeGreen = 0xff8df237; // the logo green (Brand rule)
QFont badgeFont(int pixels, int weight) {
    QFont font(QStringLiteral("Bahnschrift"));
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(std::max(8, pixels));
    font.setWeight(QFont::Weight(weight));
    return font;
}
veyra::engine::RecordingLabel labelOf(const QImage& painted) {
    const QImage image = painted.convertToFormat(QImage::Format_RGBA8888); // straight alpha, R G B A bytes
    veyra::engine::RecordingLabel label;
    label.width = uint32_t(image.width()); label.height = uint32_t(image.height());
    label.rgba.resize(size_t(image.width()) * image.height() * 4);
    for (int y = 0; y < image.height(); ++y)
        std::memcpy(label.rgba.data() + size_t(y) * image.width() * 4, image.constScanLine(y), size_t(image.width()) * 4);
    return label;
}
}
// What the original's side of a comparison clip is called: the video's "Original", a console by its
// name where it is known, "Console" otherwise (engine/PushCompareName.h). The dialog's preview asks too.
QString QmlPlayerBridge::pushCompareSourceName() const {
    const std::string kind = sourceKind().toStdString();
    const std::string console = impl_->prefs.contains(QStringLiteral("pushConsole")) ? impl_->prefs.value(QStringLiteral("pushConsole")).toString().toStdString() : std::string();
    const auto name = engine::pushCompareSourceName(kind, console);
    return name.side == engine::PushCompareSide::Original ? tr("原画") : name.side == engine::PushCompareSide::Console ? tr("主机原画") : QString::fromLatin1(name.name);
}
namespace {
veyra::engine::RecordingLabel drawCompareBadge(bool pushFps, const QString& word, int fps, uint32_t pictureHeight) {
    const int text = std::max(14, int(pictureHeight / 30));             // 36 px at 1080p
    const int height = int(std::lround(text * 1.9)) & ~1;
    const int pad = int(std::lround(text * 0.8));
    const int gap = int(std::lround(text * 0.55));
    const qreal outline = std::max<qreal>(1.5, pictureHeight / 540.0);
    const QFont label = badgeFont(text, QFont::Medium), number = badgeFont(text, QFont::Bold), unit = badgeFont(int(text * 0.72), QFont::DemiBold);
    const QString value = QString::number(fps), fpsText = QStringLiteral("FPS");
    const QFontMetricsF ml(label), mn(number), mu(unit);
    // The left part: the logo, or a dot and the word.
    QImage logo;
    if (pushFps) {
        logo = QImage(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("qml/Veyra/pushfps-badge-logo.png")));
        if (!logo.isNull()) logo = logo.scaledToHeight(int(std::lround(text * 1.55)), Qt::SmoothTransformation);
    }
    const qreal dot = text * 0.42;
    qreal leftWidth;
    if (pushFps && !logo.isNull()) leftWidth = logo.width();
    else if (pushFps) leftWidth = QFontMetricsF(badgeFont(text, QFont::Bold)).horizontalAdvance(QStringLiteral("PushFPS"));
    else leftWidth = dot + gap * 0.7 + ml.horizontalAdvance(word);
    const qreal valueWidth = mn.horizontalAdvance(value) + text * 0.18 + mu.horizontalAdvance(fpsText);
    int width = int(std::ceil(pad + leftWidth + gap + outline + gap + valueWidth + pad));
    width += width & 1;

    QImage image(width, height, QImage::Format_RGBA8888_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
    const QRectF pill(outline / 2, outline / 2, width - outline, height - outline);
    painter.setBrush(pushFps ? QColor(10, 14, 8, 214) : QColor(14, 15, 18, 204));
    painter.setPen(QPen(pushFps ? QColor::fromRgba(kBadgeGreen) : QColor(255, 255, 255, 76), outline));
    painter.drawRoundedRect(pill, pill.height() / 2, pill.height() / 2);
    const qreal mid = height / 2.0;
    qreal x = pad;
    if (pushFps && !logo.isNull()) {
        painter.drawImage(QPointF(x, mid - logo.height() / 2.0), logo);
    } else if (pushFps) {
        const QFont bold = badgeFont(text, QFont::Bold);
        const QFontMetricsF mb(bold);
        painter.setFont(bold);
        const qreal base = mid + (mb.ascent() - mb.descent()) / 2;
        painter.setPen(QColor(245, 245, 245));
        painter.drawText(QPointF(x, base), QStringLiteral("Push"));
        painter.setPen(QColor::fromRgba(kBadgeGreen));
        painter.drawText(QPointF(x + mb.horizontalAdvance(QStringLiteral("Push")), base), fpsText);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(154, 160, 166));
        painter.drawEllipse(QPointF(x + dot / 2, mid), dot / 2, dot / 2);
        painter.setFont(label);
        painter.setPen(QColor(205, 208, 214));
        painter.drawText(QPointF(x + dot + gap * 0.7, mid + (ml.ascent() - ml.descent()) / 2), word);
    }
    x += leftWidth + gap;
    // A thin divider between the name and the frame rate.
    painter.setPen(QPen(pushFps ? QColor(141, 242, 55, 110) : QColor(255, 255, 255, 60), outline));
    painter.drawLine(QPointF(x, height * 0.28), QPointF(x, height * 0.72));
    x += outline + gap;
    const qreal base = mid + (mn.ascent() - mn.descent()) / 2;
    painter.setFont(number);
    painter.setPen(QColor(255, 255, 255));
    painter.drawText(QPointF(x, base), value);
    x += mn.horizontalAdvance(value) + text * 0.18;
    painter.setFont(unit);
    painter.setPen(pushFps ? QColor::fromRgba(kBadgeGreen) : QColor(185, 189, 196));
    painter.drawText(QPointF(x, base), fpsText);
    painter.end();
    return labelOf(image);
}
}

bool QmlPlayerBridge::pushStartCompareClip(int seconds, int layout) {
    if (recording()) { emit notice(tr("录制已在进行中"), true); return false; }
    if (!hasSource()) { emit notice(tr("没有正在播放的画面，无法录制"), true); return false; }
    const auto& s = impl_->snapshot;
    if (!s.fgActive) { emit notice(tr("请先开启补帧：对比片段把主机原本的帧率和补帧后的画面放在一起"), true); return false; }
    // XeSS frame generation makes its frames in the swap chain, after the recorder has read the
    // picture: the clip would hold only real frames and both sides would look the same.
    if (engine::presentSinkFrameGeneration(s.applied.frameGenerationBackend)) {
        emit notice(tr("XeSS 补帧的画面在显示时才生成，录不进对比片段。请换用其他补帧方式后再录"), true);
        return false;
    }
    // 0 (or less): no length. The clip runs until it is stopped (the tile, the page's own button, the
    // recording shortcut), in every layout: nothing below depends on how long it lasts.
    const bool untilStopped = seconds <= 0;
    seconds = untilStopped ? 0 : std::clamp(seconds, 5, 120);
    layout = std::clamp(layout, 0, 3);
    // Video playback (a window, a screen, a file, a web video): the picture repeats each video frame
    // inside the capture's refresh. Without duplicate removal frame generation interpolates between
    // identical pictures and both halves of the clip look equally smooth, so Auto is switched on.
    const QString kindNow = sourceKind();
    const bool videoNow = kindNow == QLatin1String("file") || kindNow == QLatin1String("url") || kindNow == QLatin1String("screen")
        || kindNow == QLatin1String("frameinput");
    if (videoNow && contentRate() == int(engine::ContentRate::Transport)) {
        setContentRate(int(engine::ContentRate::Auto));
        emit notice(tr("已开启去除重复帧（自动）：视频在捕获中会重复帧，否则补帧看不出差别"), false);
        veyra::log::info("push-compare", "duplicate removal set to auto for a video source");
    }
    auto options = recordingOptions();
    options.compare = true;
    options.compareLayout = layout == 3 ? engine::CompareLayout::Reveal : layout == 2 ? engine::CompareLayout::Vertical
                          : layout == 1 ? engine::CompareLayout::SideBySide : engine::CompareLayout::Split;
    // Split and Reveal fill the frame, so they can be recorded at a chosen height (pref
    // pushCompareQuality: 0 automatic, at most 1440 as before; 1080, 1440 or 2160). Side by side and
    // Vertical have sizes of their own and stay exactly as they were.
    const bool reveal = layout == 3;
    const int quality = impl_->prefInt("pushCompareQuality", 0);
    options.compareMaxHeight = (layout == 0 || reveal) && (quality == 1080 || quality == 1440 || quality == 2160) ? uint32_t(quality) : 0u;
    // Every layout: the picture quality (pref pushComparePicture: 0 standard, 1 high, 2 near lossless;
    // high unless set). High and near lossless are encoded at a fixed quality level, so a clip is as
    // clean as it was on screen whatever bitrate recordings are set to.
    options.comparePicture = uint32_t(std::clamp(impl_->prefInt("pushComparePicture", 1), 0, 2));
    // Reveal: a sweep starts every so many seconds (pref pushCompareRevealSeconds, 1 to 8; 3 unless set).
    const int revealEvery = std::clamp(impl_->prefInt("pushCompareRevealSeconds", 3), 1, 8);
    options.compareRevealSeconds = double(revealEvery);
    // Frame rates for the labels: the console's own (distinct game frames),
    // and what PushFPS presents.
    const int gameFps = s.contentFps > 1 ? s.contentFps : std::clamp(impl_->prefInt("pushGameFps", 30), 24, 120);
    // What the clip shows: the game rate times the multiplier in use now (the adaptive multiplier's
    // choice, not the configured ceiling), never above the output cap or the display the cap follows.
    // The old label took the ceiling, so a 60 FPS target with a 3X ceiling read "180 FPS".
    const int multiplierNow = s.previewFgMultiplier > 1 ? int(s.previewFgMultiplier) : std::max(2, int(s.applied.multiplier));
    double shown = double(gameFps) * multiplierNow;
    if (outputRateMode() == 2 && outputCustomFps() > 1) shown = std::min(shown, outputCustomFps());
    else if (outputRateMode() == 1 && impl_->displayRefreshHz > 1) shown = std::min(shown, double(impl_->displayRefreshHz));
    const int shownFps = std::max(gameFps, int(std::lround(shown)));
    // Video playback mode (a video file, a web video, a window or screen, the frame input API):
    // the left side is the video's own picture, not a console's.
    // PushFPS 4.1: a console is called by its name where that is known (engine/PushCompareName.h).
    const QString leftName = pushCompareSourceName();
    const QString leftText = QStringLiteral("%1 · %2 FPS").arg(leftName).arg(gameFps);
    const QString rightText = QStringLiteral("%1 · %2 FPS").arg(QString::fromLatin1(brand::kProductName)).arg(shownFps);
    // Drawn here (UI thread) for the expected picture height; the recorder blends them in.
    // Vertical clips are always 1920 high (RecordingCompare.h); their badges are drawn as for a
    // 1440-high picture, a size that reads well on a phone without covering the game.
    const auto out = s.metrics.resolution.output;
    const bool vertical = layout == 2;
    const uint32_t height = vertical ? 1920u : out.height > 0 ? uint32_t(out.height) : 1080;
    const uint32_t badgeHeight = vertical ? 1440u : height;
    const auto left = drawCompareBadge(false, leftName, gameFps, badgeHeight), right = drawCompareBadge(true, {}, shownFps, badgeHeight);
    // Labels are sized for the picture height; a recorded file of another height (the picture
    // changed size, or the expected height was not known yet) gets them scaled to match.
    options.compareLabels = [left, right, height](uint32_t, uint32_t recorded, engine::RecordingLabel& l, engine::RecordingLabel& r) {
        l = left; r = right;
        if (!recorded || recorded == height) return;
        const double f = double(recorded) / double(height);
        const auto scaled = [f](const engine::RecordingLabel& label) {
            return engine::scaleRecordingLabel(label, std::max(2u, uint32_t(label.width * f)), std::max(2u, uint32_t(label.height * f)));
        };
        l = scaled(left); r = scaled(right);
    };
    std::wstring error;
    if (!impl_->engine.requestRecording(options, error)) { emit notice(uiText(error), true); return false; }
    pushRecordingNotices(options.audio, options.microphone, true);   // the mixer's own mute still applies; a card does not (PushStudio.cpp)
    veyra::log::info("push-compare", std::format("comparison clip seconds={} layout={} labels=\"{}\" / \"{}\" height={} quality={} revealEvery={} picture={}",
                                                 untilStopped ? std::string("until-stopped") : std::to_string(seconds),
                                                 reveal ? "reveal" : layout == 2 ? "vertical" : layout == 1 ? "side-by-side" : "split", leftText.toStdString(), rightText.toStdString(),
                                                 height, options.compareMaxHeight, revealEvery,
                                                 options.comparePicture == 2 ? "near-lossless" : options.comparePicture == 1 ? "high" : "standard"));
    impl_->recordingState["active"] = true;
    impl_->recordingState["failed"] = false;
    impl_->recordingState["compare"] = true;
    emit recordingChanged();
    // Counted for a clip without a length too: an earlier clip's timer, still pending because that
    // clip was stopped by hand, then finds another clip and leaves it alone.
    const int generation = ++impl_->compareGeneration;
    if (untilStopped) {
        emit notice(reveal ? tr("正在录制滑动揭示对比片段，直到你停止：先是主机原本的帧率，线条扫过后是 PushFPS，来回切换。再点一次“对比片段”或按录制快捷键停止")
                  : vertical ? tr("正在录制竖屏对比片段，直到你停止：上面是主机原本的帧率，下面是 PushFPS。再点一次“对比片段”或按录制快捷键停止")
                             : tr("正在录制对比片段，直到你停止：左边是主机原本的帧率，右边是 PushFPS。再点一次“对比片段”或按录制快捷键停止"), false);
        return true;
    }
    QTimer::singleShot(seconds * 1000, this, [this, generation] {
        if (generation == impl_->compareGeneration && recording() && impl_->recordingState.value("compare").toBool())
            stopRecording(QStringLiteral("compare-timer"));
    });
    emit notice(reveal ? tr("正在录制 %1 秒的滑动揭示对比片段：先是主机原本的帧率，线条扫过后是 PushFPS，来回切换").arg(seconds)
              : vertical ? tr("正在录制 %1 秒的竖屏对比片段：上面是主机原本的帧率，下面是 PushFPS").arg(seconds)
                         : tr("正在录制 %1 秒的对比片段：左边是主机原本的帧率，右边是 PushFPS").arg(seconds), false);
    return true;
}


// PushFPS 窗口置顶. HWND_TOPMOST through SetWindowPos changes only the z-order: Qt's own
// WindowStaysOnTopHint can recreate the native window, which the video child window cannot follow.
bool QmlPlayerBridge::pushSetAlwaysOnTop(QObject* window, bool onTop) {
    auto* w = qobject_cast<QWindow*>(window);
    if (!w) return false;
    const auto hwnd = reinterpret_cast<HWND>(w->winId());
    if (!hwnd) return false;
    const BOOL ok = SetWindowPos(hwnd, onTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    if (!ok) veyra::log::warn("push-ontop", std::format("SetWindowPos failed error={}", GetLastError()));
    return ok != FALSE;
}

}

// --- PushFPS ground-truth quality test (engine/PushQualityProbe.h) -----------------------------
namespace veyra::ui {
bool QmlPlayerBridge::pushQualityBegin(int seconds) {
    if (!hasSource()) { emit notice(tr("请先连接画面"), true); return false; }
    engine::PushQualityProbe::Options o;
    o.seconds = uint32_t(std::clamp(seconds, 2, 30));
    // The HUD boxes: the exclusion regions already set (game preset, HUD detection or by hand).
    const QVariantMap ex = exclusionState();
    if (ex.value(QStringLiteral("enabled")).toBool())
        for (const auto& v : ex.value(QStringLiteral("regions")).toList()) {
            const auto r = v.toMap();
            o.hud.push_back({float(r.value("left").toDouble()), float(r.value("top").toDouble()), float(r.value("right").toDouble()), float(r.value("bottom").toDouble())});
        }
    impl_->engine.startQualityTest(o);
    return true;
}
void QmlPlayerBridge::pushQualityCancel() { impl_->engine.cancelQualityTest(); }
QVariantMap QmlPlayerBridge::pushQualityStatus() {
    const auto st = impl_->engine.qualityTest();
    const auto& r = st.report;
    return QVariantMap{
        {"running", st.running}, {"done", st.done}, {"failed", st.failed},
        {"error", st.error.empty() ? QString() : uiText(st.error)},
        {"samples", int(st.samples)}, {"target", int(st.target)}, {"withheld", qulonglong(st.withheld)},
        {"psnr", r.psnr}, {"blendPsnr", r.blendPsnr}, {"gainOverBlend", r.gainOverBlend()}, {"realPsnr", r.realPsnr},
        {"hudPsnr", r.hudPsnr}, {"hudSamples", int(r.hudSamples)}, {"meanError", r.meanError}, {"badShare", r.badShare},
        {"onTime", r.onTimeShare()}, {"generatedShown", qulonglong(r.generatedShown)}, {"score", r.score()},
        {"worstFramePsnr", r.worstFramePsnr}, {"worstPlacePsnr", r.worstPlacePsnr},
        {"revision", qulonglong(st.revision)}};
}
} // namespace veyra::ui

namespace veyra::ui {
// PushFlow too slow for the frame budget. The engine skips every pair it cannot finish in time, so a
// tier this graphics card cannot sustain shows as "PushFlow stuck at the game's frame rate" with no
// error anywhere. After the budget has skipped nearly every pair for eight seconds in a row (output
// under 1.25x the game's rate), PushFlow moves one tier down and says why; on the Fast tier it says
// what else would help, once per source. A tier the user picks again after that is left alone.
// What to say for a frame generation health cause (engine::PushFgIdle); empty when there is no fault.
QString QmlPlayerBridge::pushFgIdleText(int cause) const {
    switch (engine::PushFgIdle(cause)) {
    case engine::PushFgIdle::Inactive:
        return tr("补帧已开启，但所选的补帧方式没有运行：画面仍是游戏原本的帧率。请换一种补帧方式");
    case engine::PushFgIdle::TooSlow:
        return tr("补帧已开启，但显卡跟不上：补出的画面都被跳过了。请降低倍率或输出分辨率，或换一种补帧方式");
    case engine::PushFgIdle::NoFrames:
        return tr("补帧已开启，但没有补出的画面被显示：画面仍是游戏原本的帧率。请换一种补帧方式；若一直如此，请在设置里复制性能报告发给我们");
    default: return {};
    }
}

void QmlPlayerBridge::pushFlowBudgetWatch() {
    auto& w = impl_->pushFlowWatch;
    const auto now = std::chrono::steady_clock::now();
    if (now - w.last < std::chrono::seconds(1)) return;
    w.last = now;
    pushNoteSession();   // what is playing, for the crash notice (PushTools.cpp)
    {   // PushFlow 2's Predict steps aside for the whole of "Optimise for my PC", which measures in-between frames.
        const QString restoring = impl_->prefs.value(QStringLiteral("pushOptimizeRestore")).toString();
        pushflow2::setPredictPaused(!restoring.isEmpty() && restoring != QLatin1String("{}"));
    }
    pushSourceReference();   // scene sources fit their frames to the picture's width (PushStudio.cpp)
    const auto& s = impl_->snapshot;
    // The cadence notice (same second tick): the game is set to 30 FPS, the capture runs at 60 and
    // automatic duplicate removal still sees a new picture in every sample. Some capture cards add
    // noise to repeated pictures; frame generation then works on 17 ms pairs and live play judders.
    // The quick setup has the one-click fix; this says so once per source, on whatever page is open.
    // It is not switched automatically: a game that really runs at 60 would be halved.
    {
        const bool mismatch = s.running && s.capture && s.fgActive && contentRate() == int(engine::ContentRate::Auto) &&
            // (An unset game frame rate is the quick setup's default, 30.)
            impl_->prefs.value(QStringLiteral("pushGameFps"), 30).toInt() == 30 && std::abs(s.captureFps - 60.0) < 3.0 && s.contentFps > 45;
        w.cadenceSeconds = mismatch ? w.cadenceSeconds + 1 : 0;
        if (w.cadenceSeconds >= 8 && w.cadenceSession != s.sessionId) {
            w.cadenceSession = s.sessionId;
            veyra::log::warn("push-cadence", std::format("game set to 30 but content measured {} at capture {:.1f}; suggested the fixed 60->30 mode", s.contentFps, s.captureFps));
            emit notice(tr("游戏帧率设为 30，但每一帧采集画面都被当作新画面（%1 FPS），实时画面会不如录像流畅。如果游戏确实是 30 FPS，请在快速设置里把去重改为「60→30」").arg(s.contentFps), false);
        }
    }
    // Frame generation health (engine/PushFgHealth.h): switched on, and nothing generated is shown.
    // The quick setup shows the cause for as long as it lasts (pushLiveStats().fgIdle); a notice says
    // it once per source and cause, on whatever page is open. Nothing is switched automatically.
    {
        const auto& flow = s.metrics.flow;
        const QString optimizingNow = impl_->prefs.value(QStringLiteral("pushOptimizeRestore")).toString();
        engine::PushFgSample sample;
        sample.running = s.running && !s.image;
        sample.wanted = s.applied.multiplier > 1;
        // XeSS makes its frames in the swap chain: they are not among the presented frames counted here.
        sample.measurable = !engine::presentSinkFrameGeneration(s.applied.frameGenerationBackend);
        // A comparison view pauses frame generation; the optimiser, the quality test and the benchmark
        // window hold frames back or change methods themselves.
        sample.paused = impl_->compareMode != 0 || engine::pushBenchmarkProbe().active() || impl_->engine.qualityTest().running ||
                        (!optimizingNow.isEmpty() && optimizingNow != QLatin1String("{}"));
        sample.settled = !impl_->facade.applying() && !s.applying && s.desired.revision == s.applied.revision &&
                         flow.latest.frame.settingsRevision == s.applied.revision && flow.rateWindowReady;
        sample.active = s.fgActive;
        sample.budgetLimited = s.fgBudgetLimited;
        sample.displayLimited = s.displayFgSuggestion > 0;   // DisplayFgBudget.h: the output exceeds the screen's refresh rate
        sample.contentFps = double(s.contentFps);
        sample.realFps = flow.realPresentFps;
        sample.generatedFps = flow.generatedPresentFps;
        sample.multiplierNow = double(s.previewFgMultiplier);
        const engine::PushFgIdle idle = impl_->fgHealth.update(sample);
        impl_->fgIdle = int(idle);
        if (engine::pushFgIdleIsFault(idle) && (impl_->fgIdleSession != s.sessionId || impl_->fgIdleNotified != int(idle))) {
            impl_->fgIdleSession = s.sessionId;
            impl_->fgIdleNotified = int(idle);
            veyra::log::warn("push-fg-health", std::format("frame generation on but idle: cause={} backend={} multiplier={} now={} content={} real={:.1f} generated={:.1f} active={} budgetLimited={}",
                int(idle), fgBackendName().toStdString(), s.applied.multiplier, s.previewFgMultiplier, s.contentFps, flow.realPresentFps,
                flow.generatedPresentFps, s.fgActive, s.fgBudgetLimited));
            // PushFlow that cannot keep up has its own, more exact notice below (and a faster tier).
            const bool pushFlowSlow = idle == engine::PushFgIdle::TooSlow && fgBackendName() == QLatin1String("pushflow");
            if (!pushFlowSlow) emit notice(pushFgIdleText(int(idle)), false);
        }
    }
    const auto reset = [&] { w.starved = 0; w.settledSeconds = 0; };
    if (!s.running || !s.fgActive || s.pushFlowPrecision.empty() || fgBackendName() != QLatin1String("pushflow")) { reset(); return; }
    // "Optimise for my PC" measures every tier itself: never change one under it.
    const QString optimizing = impl_->prefs.value(QStringLiteral("pushOptimizeRestore")).toString();
    if (engine::pushBenchmarkProbe().active() || impl_->engine.qualityTest().running || (!optimizing.isEmpty() && optimizing != QLatin1String("{}"))) { reset(); return; }
    if (s.sessionId != w.session) { w.session = s.sessionId; w.slowNoticeShown = false; reset(); }
    if (impl_->facade.applying() || s.applying || s.desired.revision != s.applied.revision) { reset(); return; }
    if (s.applied.revision != w.revision) {
        if (w.awaitingOwn) { w.loweredRevision = s.applied.revision; w.awaitingOwn = false; }
        else if (w.loweredRevision && w.loweredFrom >= 0 && vfgQuality() >= w.loweredFrom) w.userChose = true;
        else if (w.loweredRevision && w.motionFrom > 0 && pushflow::motion::motionHeight() == w.motionFrom) w.userChose = true;
        w.revision = s.applied.revision;
        reset();
    }
    if (++w.settledSeconds <= 4) return;   // the budget learns its costs first
    const double content = double(s.contentFps), output = s.submissionFps.value_or(0);
    const bool starved = s.fgBudgetLimited && content > 1 && output > 0 && output < content * 1.25;
    // One second in which a probe got a few frames through (FgCostSettling retries after 2, 4, then
    // every 8 s) takes two off instead of starting the count again.
    w.starved = starved ? w.starved + 1 : std::max(0, w.starved - 2);
    if (w.starved < 8 || w.userChose) return;
    w.starved = 0;
    const int tier = std::clamp(vfgQuality(), 0, 2);
    const QString names[3] = {tr("快速"), tr("均衡"), tr("高质量")};
    const QString ms = QString::number(s.pushFlowP50Ms, 'f', 1);
    veyra::log::warn("pushflow-watch", std::format("budget skipped nearly every pair for 8 s: content={:.1f} output={:.1f} network p50={:.1f} ms tier={}",
        content, output, s.pushFlowP50Ms, tier));
    // The step that makes PushFlow cheaper (pushflow/PushFlowMotion.h slowStep): a motion size chosen in
    // Professional mode that is larger than the tier's own goes back to automatic before any tier is lowered.
    const int motion = pushflow::motion::motionHeight();
    const auto step = pushflow::motion::slowStep(uint32_t(tier), motion);
    using pushflow::motion::SlowStep;
    if (step == SlowStep::MotionToAuto || step == SlowStep::TierDownAndMotionToAuto) {
        w.motionFrom = motion;
        w.awaitingOwn = true;
        if (step == SlowStep::TierDownAndMotionToAuto) { w.loweredFrom = tier; setVfgQuality(tier - 1); }
        setPreference(QStringLiteral("pushFlowMotion"), 0);
        emit notice(step == SlowStep::MotionToAuto
            ? tr("PushFlow 在所选的运动估算尺寸（%1p）下每帧需要 %2 ms，这块显卡跟不上当前帧率，几乎每一对画面都被跳过。运动估算尺寸已恢复为自动").arg(motion).arg(ms)
            : tr("PushFlow 在%1档每帧需要 %2 ms，这块显卡跟不上当前帧率，几乎每一对画面都被跳过。已自动换成%3档，运动估算尺寸恢复为自动")
                .arg(names[tier], ms, names[tier - 1]), false);
    } else if (step == SlowStep::TierDown) {
        w.loweredFrom = tier;
        w.awaitingOwn = true;
        setVfgQuality(tier - 1);
        emit notice(tr("PushFlow 在%1档每帧需要 %2 ms，这块显卡跟不上当前帧率，几乎每一对画面都被跳过。已自动换成%3档")
            .arg(names[tier], ms, names[tier - 1]), false);
    } else if (!w.slowNoticeShown) {
        w.slowNoticeShown = true;
        emit notice(tr("PushFlow 即使在快速档每帧也需要 %1 ms，这块显卡跟不上当前帧率，几乎没有补出的画面。可以降低输出分辨率、改用 2×，或换一种补帧方式（FSR、DLSS、AMD FRC）")
            .arg(ms), false);
    }
}
}  // namespace veyra::ui

namespace veyra::ui {
bool QmlPlayerBridge::pushFlowHalfTesting() const { return impl_->pushFlowHalfTest != nullptr; }

// What a passed half-precision test holds for (engine/PushHalfApproval.h): the card PushFPS renders
// on, its driver and this PushFPS version.
QString QmlPlayerBridge::pushHalfApprovalKey() const {
    const auto* adapter = engineAdapterDesc();
    if (!adapter) return {};
    return QString::fromStdString(engine::pushHalfApprovalKey(adapter->VendorId, adapter->DeviceId, engineAdapterDriverVersion().toStdString(), version().toStdString()));
}
// At every start, before the preference is applied: a pass measured with another card, driver or
// PushFPS version (or stored before 3.2.5, without any of them) is not carried over. Half precision
// goes off, PushFlow runs in full precision, and a notice says to test again.
void QmlPlayerBridge::pushValidateHalfApproval() {
    if (!impl_->prefBool("pushFlowHalf", false)) return;
    const QJsonObject probe = QJsonDocument::fromJson(impl_->prefs.value(QStringLiteral("pushFlowHalfProbe")).toString().toUtf8()).object();
    const std::string stored = probe.value(QStringLiteral("key")).toString().toStdString(), current = pushHalfApprovalKey().toStdString();
    if (engine::pushHalfApprovalHolds(stored, current)) return;
    veyra::log::warn("pushflow", std::format("half precision approval no longer holds (stored=\"{}\" now=\"{}\"): switched off until tested again", stored, current));
    QJsonObject r{{QStringLiteral("ok"), false}, {QStringLiteral("reason"), QStringLiteral("retest")},
                  {QStringLiteral("when"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
    impl_->prefs[QStringLiteral("pushFlowHalfProbe")] = QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact));
    impl_->prefs[QStringLiteral("pushFlowHalf")] = false;
    if (!impl_->savePrefs()) veyra::log::warn("ui-prefs", "half precision reset not saved");
    QTimer::singleShot(6000, this, [this] {
        emit notice(tr("PushFlow 半精度已关闭：显卡、驱动或 PushFPS 版本和测试时不同了，需要重新测试（专业模式 → PushFlow 半精度）。现在使用全精度"), false);
    });
}

// Half precision runs PushFlow's network roughly twice as fast on most cards, but whether the fp16
// model works depends on the graphics card, driver and DirectML: on Windows' software renderer it
// removed the D3D12 device. So it is tested in a separate process (PushFlowBench.exe, shipped next to
// PushFPS.exe): if the driver faults, that process ends and PushFPS keeps running.
bool QmlPlayerBridge::pushTestPushFlowHalf() {
    if (impl_->pushFlowHalfTest) return false;
    if (impl_->snapshot.running) { emit notice(tr("请先停止播放再测试：测试会占用显卡几秒钟"), true); return false; }
    const QString exe = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("PushFlowBench.exe"));
    if (!QFileInfo::exists(exe)) { emit notice(tr("没有找到 PushFlowBench.exe，请重新安装 PushFPS"), true); return false; }
    const QString out = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("pushfps-fp16-probe.json"));
    QFile::remove(out);
    QStringList args{QStringLiteral("--fp16-probe"), QStringLiteral("--out"), out};
    if (const uint64_t luid = gfx::preferredAdapterLuid()) args << QStringLiteral("--adapter-luid") << QString::number(luid);
    auto* proc = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.remove(QStringLiteral("VEYRA_PUSHFLOW_PRECISION"));
    proc->setProcessEnvironment(env);
    proc->setProgram(exe);
    proc->setArguments(args);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    impl_->pushFlowHalfTest = proc;
    auto* timeout = new QTimer(proc);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, proc, [proc] { proc->kill(); });
    connect(proc, &QProcess::finished, this, [this, proc, out](int code, QProcess::ExitStatus status) {
        const QByteArray console = proc->readAll();
        impl_->pushFlowHalfTest = nullptr;
        proc->deleteLater();
        QFile file(out);
        QJsonObject r;
        if (file.open(QIODevice::ReadOnly)) r = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QFile::remove(out);
        const bool crashed = status != QProcess::NormalExit || code != 0 || r.isEmpty();
        veyra::log::info("pushflow", std::format("fp16 probe exit={} crashed={} result={} output={}", code, crashed ? 1 : 0,
            QJsonDocument(r).toJson(QJsonDocument::Compact).toStdString(), console.left(400).simplified().toStdString()));
        if (crashed) { r = QJsonObject{{QStringLiteral("ok"), false}, {QStringLiteral("reason"), QStringLiteral("probe-stopped")}}; }
        r[QStringLiteral("when")] = QDateTime::currentDateTime().toString(Qt::ISODate);
        // What the result holds for: this card, this driver, this PushFPS version (checked at every start).
        r[QStringLiteral("key")] = pushHalfApprovalKey();
        const bool ok = r.value(QStringLiteral("ok")).toBool();
        setPreference(QStringLiteral("pushFlowHalfProbe"), QString::fromUtf8(QJsonDocument(r).toJson(QJsonDocument::Compact)));
        setPreference(QStringLiteral("pushFlowHalf"), ok);
        const QString reason = r.value(QStringLiteral("reason")).toString();
        const QString full = QString::number(r.value(QStringLiteral("fullMs")).toDouble(), 'f', 1), half = QString::number(r.value(QStringLiteral("halfMs")).toDouble(), 'f', 1);
        if (ok)
            emit notice(tr("半精度在这块显卡上可用：每帧 %1 ms，全精度 %2 ms，画面一致（%3 dB）。已开启，下次启动 PushFlow 时生效")
                .arg(half, full, QString::number(r.value(QStringLiteral("matchDb")).toDouble(), 'f', 1)), false);
        else if (reason == QLatin1String("not-faster"))
            emit notice(tr("半精度在这块显卡上没有更快（%1 ms，全精度 %2 ms），继续使用全精度").arg(half, full), false);
        else if (reason == QLatin1String("fp16-mismatch"))
            emit notice(tr("半精度的画面和全精度不一致（%1 dB），继续使用全精度").arg(QString::number(r.value(QStringLiteral("matchDb")).toDouble(), 'f', 1)), false);
        else
            emit notice(tr("半精度测试没有完成（显卡驱动或 DirectML 在半精度下出错），继续使用全精度。PushFPS 不受影响"), false);
    });
    proc->start();
    if (!proc->waitForStarted(5000)) {
        impl_->pushFlowHalfTest = nullptr;
        proc->deleteLater();
        emit notice(tr("PushFlowBench.exe 无法启动"), true);
        return false;
    }
    timeout->start(180000);
    return true;
}
}  // namespace veyra::ui
