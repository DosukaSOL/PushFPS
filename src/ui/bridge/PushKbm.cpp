// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS: the keyboard and mouse as a controller for the console a session plays: PS5 Remote Play, a
// capture card with "Control the PS5 with Remote Play", Xbox streaming. The rules are in
// remoteplay/PushKbm.h, the window's input in ui/PushKbmCapture.h; this joins them to the controller
// timers of those three sessions (RemotePlay.cpp) and to the interface (PushKbmDialog.qml).
#include "../QmlPlayerBridge_p.h"
#include "veyra/ui/PushKbmCapture.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <chrono>
#include <format>

#include "veyra/Log.h"

namespace veyra::ui {

namespace kbm = remoteplay::kbm;

namespace {

QString controlId(std::size_t c) {
    const auto id = kbm::kControlIds[c];
    return QString::fromLatin1(id.data(), qsizetype(id.size()));
}

// The saved preference "pushKbm" (JSON written by PushKbmDialog.qml). Anything missing, of the wrong
// kind or out of range is the default; a control the text does not name keeps the preset's inputs.
kbm::Settings settingsFromJson(const QString& text) {
    kbm::Settings s;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    if (!doc.isObject()) return kbm::clean(s);
    const QJsonObject o = doc.object();
    s.on = o.value(QStringLiteral("on")).toBool(false);
    const int preset = kbm::presetIndex(o.value(QStringLiteral("preset")).toString().toStdString());
    s.map = kbm::presetMap(preset >= 0 ? kbm::Preset(preset) : kbm::Preset::Gta6);
    s.sensitivity = o.value(QStringLiteral("sens")).toInt(s.sensitivity);
    s.verticalPercent = o.value(QStringLiteral("vert")).toInt(s.verticalPercent);
    s.deadzonePercent = o.value(QStringLiteral("dz")).toInt(s.deadzonePercent);
    s.curvePercent = o.value(QStringLiteral("curve")).toInt(s.curvePercent);
    s.invertY = o.value(QStringLiteral("invert")).toBool(false);
    s.release = std::uint16_t(std::clamp(o.value(QStringLiteral("release")).toInt(s.release), 0, 0xFFFF));
    const QJsonObject map = o.value(QStringLiteral("map")).toObject();
    for (std::size_t c = 0; c < kbm::kControls; ++c) {
        const QJsonValue v = map.value(controlId(c));
        if (!v.isArray()) continue;
        const QJsonArray a = v.toArray();
        for (std::size_t slot = 0; slot < kbm::kSlots; ++slot)
            s.map[c][slot] = slot < std::size_t(a.size()) ? std::uint16_t(std::clamp(a.at(qsizetype(slot)).toInt(0), 0, 0xFFFF)) : kbm::kNone;
    }
    return kbm::clean(s);
}

QVariantMap mapVariant(const kbm::Map& m) {
    QVariantMap out;
    for (std::size_t c = 0; c < kbm::kControls; ++c) out.insert(controlId(c), QVariantList{int(m[c][0]), int(m[c][1])});
    return out;
}

} // namespace

void QmlPlayerBridge::setupKbm() {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    i.kbm = std::make_unique<ui::PushKbmCapture>();
    connect(i.kbm.get(), &ui::PushKbmCapture::capturedChanged, this, [this](bool captured) {
        refreshKbmState();
        if (captured)
            emit notice(tr("鼠标和键盘已交给主机 · 按 %1 或 Alt+Tab 交还").arg(impl_->kbmReleaseName), false);
    });
    connect(i.kbm.get(), &ui::PushKbmCapture::releaseRequested, this, [this] {
        emit notice(tr("已交还鼠标和键盘 · 点击画面或按 %1 再交给主机").arg(impl_->kbmReleaseName), false);
    });
    connect(i.kbm.get(), &ui::PushKbmCapture::notReady, this, [this] { kbmSayWaiting(false); });
    connect(i.kbm.get(), &ui::PushKbmCapture::input, this, [this] { kbmSend(false); });
    connect(i.kbm.get(), &ui::PushKbmCapture::moved, this, [this] { kbmSend(true); });
    connect(i.kbm.get(), &ui::PushKbmCapture::listeningChanged, this, [this] { refreshKbmState(); });
    connect(i.kbm.get(), &ui::PushKbmCapture::bindingHeard, this, [this](int control, int slot, int code) {
        emit pushKbmBinding(control, slot, code);
    });
#endif
    pushApplyKbm();
}

void QmlPlayerBridge::pushApplyKbm() {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    if (!i.kbm) return;
    const kbm::Settings s = settingsFromJson(i.prefs.value(QStringLiteral("pushKbm")).toString());
    i.kbm->setSettings(s);
    i.kbmReleaseName = ui::PushKbmCapture::inputName(i.kbm->settings().release);
    veyra::log::info("kbm", std::format("settings on={} sens={} vertical={} deadzone={} curve={} invert={}",
                                        s.on, s.sensitivity, s.verticalPercent, s.deadzonePercent, s.curvePercent, s.invertY));
#endif
    refreshKbmState();
}

void QmlPlayerBridge::tickKbm() {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    if (!i.kbm) return;
    if (i.videoWindow) {
        i.kbm->setWindow(GetAncestor(i.videoWindow, GA_ROOT));
        i.kbm->setPicture(i.videoWindow);
    }
    // Who takes the keyboard and mouse now: a session whose controller timer runs (a view-only PS5
    // stream has none) and, for PS5 controller control, a session that is connected.
    const auto& s = i.snapshot;
    using T = Impl::KbmTarget;
    T target = T::None;
    if (s.remotePlay && s.running && !s.failed && i.controllerTimer && i.controllerTimer->isActive()) target = T::Ps5;
    else if (!s.remotePlay && i.ps5ControlTimer && i.ps5ControlTimer->isActive() &&
             i.ps5ControlState.value(QStringLiteral("state")).toString() == QLatin1String("connected")) target = T::Ps5Control;
#ifdef VEYRA_ENABLE_XBOX
    else if (s.xboxActive && i.xboxTimer && i.xboxTimer->isActive()) target = T::Xbox;
#endif
    if (target != i.kbmTarget) {
        // Whatever the old session was given is let go of before another one is served.
        if (i.kbm->captured()) i.kbm->capture(false);
        i.kbmTarget = target;
        veyra::log::info("kbm", std::format("target={}", int(target)));
    }
    const bool on = i.kbm->settings().on;
    i.kbm->setAvailable(target != T::None && on);

    // Nothing takes them: what is missing (said in the interface, and when they are asked for).
    kbm::Sources src;
    src.playing = s.running && !s.failed;
    src.ps5Stream = s.remotePlay && !s.failed && (s.running || s.transport == engine::TransportState::Opening);
    src.ps5StreamPad = i.controllerTimer && i.controllerTimer->isActive();
    src.ps5StreamRunning = s.running;
#ifdef VEYRA_ENABLE_XBOX
    src.xboxStream = s.xboxActive;
    src.xboxStreamPad = i.xboxTimer && i.xboxTimer->isActive();
#endif
    src.pcStream = s.moonlightActive;
    src.captureCard = captureCardLive();
    src.captureControl = i.prefBool("ps5CaptureControl", false);
    const std::string controlState = i.ps5ControlState.value(QStringLiteral("state")).toString().toStdString();
    src.controlState = controlState;
    kbm::Waiting waiting = target != T::None ? kbm::Waiting::Ready : kbm::waitingFor(src);
    if (target == T::None && waiting == kbm::Waiting::Ready) waiting = kbm::Waiting::Connecting;
    if (waiting != i.kbmWaiting && on)
        veyra::log::info("kbm", std::format("waiting={}", kbm::kWaitingIds[std::size_t(waiting)]));
    i.kbmWaiting = waiting;

    // Once per session that can take them: how to start (a click on the picture or the release key).
    if (target == T::None) i.kbmReadyNoticed = T::None;
    else if (on && i.kbmReadyNoticed != target && !i.kbm->captured()) {
        i.kbmReadyNoticed = target;
        emit notice(tr("鼠标和键盘可以用了：点击画面或按 %1 开始").arg(i.kbmReleaseName), false);
    }
#endif
    refreshKbmState();
}

QString QmlPlayerBridge::kbmWaitingText() const {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    using W = kbm::Waiting;
    switch (impl_->kbmWaiting) {
    case W::Ready: return {};
    case W::NoSource: return tr("还没有连接主机：连接 PS5 串流或 Xbox 串流，或在采集卡窗口打开“用远程游玩控制 PS5”");
    case W::OtherSource: return tr("这个画面后面没有主机：鼠标和键盘用于 PS5 串流、Xbox 串流，以及打开“用远程游玩控制 PS5”的采集卡");
    case W::PcStream: return tr("PC 串流直接把鼠标和键盘发给电脑，用不到这个功能");
    case W::ViewOnly: return tr("这个 PS5 串流是“仅观看”，电脑的输入已关闭：关掉“仅观看”后重新连接");
    case W::XboxNoPad: return tr("这个 Xbox 串流没有打开手柄：在 Xbox 窗口打开“手柄”后重新连接");
    case W::CaptureOff: return tr("采集卡只把画面和声音传进来，按键要经 PS5 远程游玩发给主机：打开“用远程游玩控制 PS5”");
    case W::CapturePair: return tr("先配对 PS5：在「PS5 串流」里配对一次，采集卡就能经远程游玩控制 PS5");
    case W::CaptureStarting: return tr("正在连接 PS5 手柄控制，连上后就能用鼠标和键盘");
    case W::CapturePin: return tr("PS5 要求输入登录 PIN：在采集卡面板「PS5 手柄控制」里填写");
    case W::CaptureFailed: return tr("PS5 手柄控制没有连上：检查 PS5 和网络后重连");
    case W::Connecting: return tr("正在连接，连上后就能用鼠标和键盘");
    case W::Count: break;
    }
#endif
    return {};
}

void QmlPlayerBridge::kbmSayWaiting(bool fromClick) {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    using W = kbm::Waiting;
    if (!i.kbm || !i.kbm->settings().on || i.kbm->available()) return;
    // A click on a picture with no console behind it is an ordinary click: that is said for the key only.
    if (fromClick && (i.kbmWaiting == W::OtherSource || i.kbmWaiting == W::PcStream || i.kbmWaiting == W::NoSource)) return;
    // The key's message can reach the filter twice (the event loop and the window procedure).
    const auto now = std::chrono::steady_clock::now();
    if (now - i.kbmWaitNoticeAt < std::chrono::seconds(fromClick ? 8 : 2)) return;
    i.kbmWaitNoticeAt = now;
    const QString text = kbmWaitingText();
    if (!text.isEmpty()) emit notice(text, true);
#else
    (void)fromClick;
#endif
}

void QmlPlayerBridge::kbmSend(bool movement) {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    if (!i.kbm || i.latency.active) return;
    using T = Impl::KbmTarget;
    // The Xbox stream sends every changed state it is given as a message of its own, and a mouse reports
    // up to thousands of times a second: movement alone goes out at most every 4 ms there (what the PS5
    // session sends at; its queue keeps only the newest stick). Keys and buttons always go at once, and
    // the controller poll (every 4 ms for Xbox) carries the newest movement anyway.
    if (movement && i.kbmTarget == T::Xbox) {
        const auto now = std::chrono::steady_clock::now();
        if (now - i.kbmMovedSent < std::chrono::milliseconds(4)) return;
        i.kbmMovedSent = now;
    }
    // Between two polls of the controller: its last state without the motion samples it already sent.
    remoteplay::ControllerState pad = i.kbmPad;
    pad.motionSampleCount = 0;
    switch (i.kbmTarget) {
    case T::Ps5:
    case T::Ps5Control: i.engine.remotePlayController(i.kbm->apply(pad, kbm::Console::Ps5)); break;
    case T::Xbox:
#ifdef VEYRA_ENABLE_XBOX
        i.engine.xboxController(i.kbm->apply(pad, kbm::Console::Xbox));
#endif
        break;
    case T::None: break;
    }
#else
    (void)movement;
#endif
}

void QmlPlayerBridge::refreshKbmState() {
    QVariantMap next;
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    const bool have = i.kbm != nullptr;
    using T = Impl::KbmTarget;
    next[QStringLiteral("built")] = have;
    next[QStringLiteral("on")] = have && i.kbm->settings().on;
    next[QStringLiteral("available")] = have && i.kbm->available();
    next[QStringLiteral("captured")] = have && i.kbm->captured();
    next[QStringLiteral("target")] = i.kbmTarget == T::Ps5 ? QStringLiteral("ps5")
                                   : i.kbmTarget == T::Ps5Control ? QStringLiteral("ps5control")
                                   : i.kbmTarget == T::Xbox ? QStringLiteral("xbox") : QString();
    next[QStringLiteral("releaseName")] = i.kbmReleaseName;
    const bool waiting = have && i.kbm->settings().on && !i.kbm->available();
    next[QStringLiteral("waiting")] = waiting ? [&] { const auto id = kbm::kWaitingIds[std::size_t(i.kbmWaiting)]; return QString::fromLatin1(id.data(), qsizetype(id.size())); }() : QString();
    next[QStringLiteral("waitingText")] = waiting ? kbmWaitingText() : QString();
    next[QStringLiteral("listenControl")] = have ? i.kbm->listeningControl() : -1;
    next[QStringLiteral("listenSlot")] = have ? i.kbm->listeningSlot() : -1;
#else
    next[QStringLiteral("built")] = false;
    next[QStringLiteral("on")] = false;
    next[QStringLiteral("available")] = false;
    next[QStringLiteral("captured")] = false;
    next[QStringLiteral("target")] = QString();
    next[QStringLiteral("releaseName")] = QString();
    next[QStringLiteral("waiting")] = QString();
    next[QStringLiteral("waitingText")] = QString();
    next[QStringLiteral("listenControl")] = -1;
    next[QStringLiteral("listenSlot")] = -1;
#endif
    if (next == impl_->kbmState) return;
    impl_->kbmState = next;
    emit pushKbmChanged();
}

QVariantMap QmlPlayerBridge::pushKbm() const { return impl_->kbmState; }

void QmlPlayerBridge::pushKbmCapture(bool on) {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    if (!i.kbm) return;
    if (!on) { i.kbm->capture(false); return; }
    if (!i.kbm->available()) { kbmSayWaiting(true); return; }
    if (i.videoWindow) {
        i.kbm->setWindow(GetAncestor(i.videoWindow, GA_ROOT));
        i.kbm->setPicture(i.videoWindow);
    }
    i.kbm->capture(true);
#else
    (void)on;
#endif
}

void QmlPlayerBridge::pushKbmListen(int control, int slot) {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    auto& i = *impl_;
    if (!i.kbm) return;
    if (i.videoWindow) i.kbm->setWindow(GetAncestor(i.videoWindow, GA_ROOT));
    i.kbm->listen(control, slot);
#else
    (void)control; (void)slot;
#endif
}

void QmlPlayerBridge::pushKbmStopListening() {
#ifdef VEYRA_ENABLE_REMOTEPLAY
    if (impl_->kbm) impl_->kbm->stopListening();
#endif
}

QString QmlPlayerBridge::pushKbmInputName(int code) const {
    if (code <= 0 || code > 0xFFFF) return QString();
    return ui::PushKbmCapture::inputName(std::uint16_t(code));
}

QVariantMap QmlPlayerBridge::pushKbmPresetMap(const QString& id) const {
    const int preset = kbm::presetIndex(id.toStdString());
    if (preset < 0) return {};
    return mapVariant(kbm::presetMap(kbm::Preset(preset)));
}

QVariantMap QmlPlayerBridge::pushKbmDefaults() const {
    const kbm::Settings s;
    return {{QStringLiteral("on"), s.on}, {QStringLiteral("preset"), QStringLiteral("gta6")},
            {QStringLiteral("sens"), s.sensitivity}, {QStringLiteral("vert"), s.verticalPercent},
            {QStringLiteral("dz"), s.deadzonePercent}, {QStringLiteral("curve"), s.curvePercent},
            {QStringLiteral("invert"), s.invertY}, {QStringLiteral("release"), int(s.release)},
            {QStringLiteral("map"), mapVariant(s.map)}};
}

QStringList QmlPlayerBridge::pushKbmControls() const {
    QStringList out;
    for (std::size_t c = 0; c < kbm::kControls; ++c) out << controlId(c);
    return out;
}

} // namespace veyra::ui
