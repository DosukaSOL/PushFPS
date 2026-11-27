// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS 4.5.8: the Discord status. Friends see on the user's Discord profile what is played through
// PushFPS. Off unless switched on (preference pushDiscordPresence); the game profile's name is shown unless
// that is switched off (pushDiscordGame). Once a second the facts are gathered here and handed to the
// connection (ui/PushDiscordClient.h), which decides with ui/PushDiscordPresence.h what Discord gets and
// when. Nothing here is logged but the switch going on and off.
#include "../QmlPlayerBridge_p.h"
#include "veyra/Brand.h"
#include "veyra/ui/PushDiscordClient.h"
#include "veyra/ui/PushDiscordPresence.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <chrono>
#include <ctime>

#include "veyra/Log.h"

namespace veyra::ui {

namespace {
// The chosen game profile's name, as quick setup reads it (PushQuickSetup.qml activeGame): profiles.game when
// it names a saved game; and only when that game is for the console chosen now, if both are known.
std::string chosenGame(const QString& profilesJson, const QString& console) {
    const QJsonDocument doc = QJsonDocument::fromJson(profilesJson.toUtf8());
    if (!doc.isObject()) return {};
    const QJsonObject p = doc.object();
    const QString name = p.value(QStringLiteral("game")).toString();
    if (name.isEmpty()) return {};
    for (const QJsonValue& v : p.value(QStringLiteral("games")).toArray()) {
        const QJsonObject g = v.toObject();
        if (g.value(QStringLiteral("name")).toString() != name) continue;
        const QString forConsole = g.value(QStringLiteral("console")).toString();
        if (!forConsole.isEmpty() && !console.isEmpty() && forConsole != console) return {};
        return name.toStdString();
    }
    return {};
}
} // namespace

namespace {
QVariantMap switchedOff() {
    return QVariantMap{{QStringLiteral("on"), false}, {QStringLiteral("state"), QStringLiteral("off")}, {QStringLiteral("showing"), false},
                       {QStringLiteral("picture"), false}};
}
} // namespace

QVariantMap QmlPlayerBridge::pushDiscord() const {
    return impl_->discordState.isEmpty() ? switchedOff() : impl_->discordState;
}

void QmlPlayerBridge::tickDiscord() {
    auto& i = *impl_;
    const auto now = std::chrono::steady_clock::now();
    if (now < i.discordTickAt) return;
    i.discordTickAt = now + std::chrono::seconds(1);

    QVariantMap state;
    const bool on = i.prefBool("pushDiscordPresence", false);
    if (!on) {
        if (i.discord) {
            i.discord.reset();   // clears the line and closes the connection
            veyra::log::info("discord-status", "switched off");
        }
        i.discordSession = 0;
        i.discordStart = 0;
        state = switchedOff();
    } else {
        if (!i.discord) {
            i.discord = std::make_unique<PushDiscordClient>(brand::kDiscordApplicationId);
            veyra::log::info("discord-status", "switched on");
        }
        const auto& s = i.snapshot;
        const QString kind = sourceKind();
        const bool playing = s.running && !s.failed && !kind.isEmpty();
        if (!playing) { i.discordSession = 0; i.discordStart = 0; }
        else if (i.discordSession != s.sessionId) {
            i.discordSession = s.sessionId;
            i.discordStart = int64_t(std::time(nullptr));
        }
        const QString console = i.prefs.contains(QStringLiteral("pushConsole")) ? i.prefString("pushConsole") : QString();
        const QString profiles = i.prefString("pushProfiles");
        if (profiles != i.discordProfilesRead || console != i.discordConsoleRead) {
            i.discordProfilesRead = profiles;
            i.discordConsoleRead = console;
            i.discordGame = chosenGame(profiles, console);
        }
        // The game's rate and what is shown, as the comparison clip's label works them out; no arrow while
        // frame generation shows nothing of its own (PushFgHealth: a setting or a fault).
        const int gameFps = discord::snapFps(s.contentFps > 1 ? double(s.contentFps) : double(std::clamp(i.prefInt("pushGameFps", 30), 24, 120)));
        const int multiplier = s.fgActive && i.fgIdle == 0 ? (s.previewFgMultiplier > 1 ? int(s.previewFgMultiplier) : int(s.applied.multiplier)) : 1;
        const std::string kindText = kind.toStdString(), consoleText = console.toStdString();
        discord::Facts facts;
        facts.playing = playing;
        facts.kind = kindText;
        facts.console = consoleText;
        facts.game = i.prefBool("pushDiscordGame", true) ? std::string_view(i.discordGame) : std::string_view();
        facts.gameFps = gameFps;
        facts.shownFps = discord::shownFps(gameFps, multiplier, outputRateMode(), outputCustomFps(), i.displayRefreshHz);
        facts.startUnix = i.discordStart;
        const discord::Activity activity = discord::activityFor(facts);
        i.discord->set(activity);
        const auto st = i.discord->state();
        state = {{QStringLiteral("on"), true},
                 {QStringLiteral("state"), st == PushDiscordClient::State::Connected ? QStringLiteral("connected")
                                         : st == PushDiscordClient::State::Refused   ? QStringLiteral("refused")
                                                                                     : QStringLiteral("looking")},
                 {QStringLiteral("showing"), activity.show && i.discord->showing()},
                 {QStringLiteral("picture"), activity.show}};
    }
    if (state != i.discordState) {
        i.discordState = state;
        emit pushDiscordChanged();
    }
}

} // namespace veyra::ui
