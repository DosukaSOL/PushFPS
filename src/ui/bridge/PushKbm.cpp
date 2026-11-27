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
    if (i.videoWindow) i.kbm->setWindow(GetAncestor(i.videoWindow, GA_ROOT));
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
    i.kbm->setAvailable(target != T::None && i.kbm->settings().on);
#endif
    refreshKbmState();
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
    next[QStringLiteral("listenControl")] = have ? i.kbm->listeningControl() : -1;
    next[QStringLiteral("listenSlot")] = have ? i.kbm->listeningSlot() : -1;
#else
    next[QStringLiteral("built")] = false;
    next[QStringLiteral("on")] = false;
    next[QStringLiteral("available")] = false;
    next[QStringLiteral("captured")] = false;
    next[QStringLiteral("target")] = QString();
    next[QStringLiteral("releaseName")] = QString();
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
    if (i.videoWindow) i.kbm->setWindow(GetAncestor(i.videoWindow, GA_ROOT));
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
