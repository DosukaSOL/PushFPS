// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Console and game profiles, as the Windows build's quick setup has them (PushQuickSetup.qml, section
// "setup"), with its words and its preferences: `pushConsole` and `pushProfiles` (a JSON object
// { v, consoles: { <console>: snapshot }, games: [ snapshot + name, console ], game }).
// A console profile remembers the quick choices per console: switching console keeps what was set for
// the old one and brings back the new one's. A game profile is a named snapshot. Both follow what is
// changed while they are chosen (as on Windows since 4.5.1).
// What a snapshot holds is what the preview has of the Windows snapshot: the target, Frame Pacing
// Repair, Live HUD Lock, the one-tap lowest latency and the crop. The preview has no game frame rate to choose
// (its engine shows a new picture at every refresh and finds the real frames itself), so none is kept;
// a profile's other keys (from a newer version) are kept as they are.
import QtQuick
import QtQuick.Layouts
import "PushCaptureCards.js" as Cards
import "PushCrop.js" as Crop

ColumnLayout {
    id: prof
    spacing: 0
    Layout.fillWidth: true

    readonly property var consoles: [
        { id: "ps5", label: "PlayStation 5" }, { id: "ps5pro", label: "PS5 Pro" },
        { id: "xbox", label: "Xbox Series X|S" }, { id: "switch", label: "Nintendo Switch" },
        { id: "switch2", label: "Nintendo Switch 2" },
        { id: "other", label: qsTr("其他主机") }
    ]
    readonly property string consoleId: {
        const c = String(app.preferences.pushConsole || "ps5")
        return consoles.some(x => x.id === c) ? c : "ps5"
    }
    readonly property string consoleLabel: (consoles.find(c => c.id === consoleId) || consoles[0]).label
    readonly property var profiles: {
        try {
            const p = JSON.parse(app.preferences.pushProfiles || "{}")
            return p && typeof p === "object" && !Array.isArray(p) ? p : ({})
        } catch (e) { return ({}) }
    }
    readonly property var games: Array.isArray(profiles.games) ? profiles.games.filter(g => g && typeof g.name === "string") : []
    readonly property string activeGame: typeof profiles.game === "string" && games.some(g => g.name === profiles.game) ? profiles.game : ""
    readonly property int maxGames: 50
    // app.targetMode 0..4 under the Windows build's target names (4 = 90 FPS came last).
    readonly property var targetIds: ["off", "60", "120", "monitor", "90"]

    function snapshot() {
        // crop as on Windows: false = off, else its values (PushCrop.js).
        const c = Crop.read(app.preferences)
        return { target: prof.targetIds[app.targetMode] || "monitor", pacing: app.pacingRepair === true,
                 hudLock: app.hudLock === true, lowLatency: app.lowLatency === true,
                 crop: c.on && !Crop.neutral(c) ? { l: c.l, t: c.t, r: c.r, b: c.b, zoom: c.zoom, x: c.x, y: c.y } : false }
    }
    function copyProfiles() { return JSON.parse(JSON.stringify(prof.profiles)) }
    function writeProfiles(p) {
        p.v = 1
        const text = JSON.stringify(p)
        app.setPreference("pushProfiles", text)
        return app.preferences.pushProfiles === text
    }
    // The snapshot's values over what a profile had, so keys this version does not know are kept.
    function mergedSnapshot(old) {
        const s = prof.snapshot()
        const out = (old && typeof old === "object") ? JSON.parse(JSON.stringify(old)) : {}
        for (const k in s) out[k] = s[k]
        return out
    }
    function applySnapshot(s) {
        if (!s || typeof s !== "object") return
        // Lowest latency first when it goes off (it gives back what it changed), last when it goes on
        // (it sets fast motion and Frame Pacing Repair off over what the profile asks).
        if (s.lowLatency === false) app.lowLatency = false
        const i = prof.targetIds.indexOf(s.target)
        if (i >= 0) app.targetMode = i
        if (typeof s.pacing === "boolean") app.pacingRepair = s.pacing
        if (typeof s.hudLock === "boolean") app.hudLock = s.hudLock
        // A profile from before crop has none and leaves it alone.
        if (s.crop === false) { if (app.preferences.pushCropOn === true) app.setPreference("pushCropOn", false) }
        else if (s.crop && typeof s.crop === "object") prof.setCrop(Crop.copy(Crop.read({}), s.crop))
        if (s.lowLatency === true) app.lowLatency = true
    }
    function setCrop(c) {
        const wanted = Crop.values(c), saved = Crop.values(Crop.read(app.preferences))
        for (const k of Crop.keys) if (wanted[k] !== saved[k]) app.setPreference(k, wanted[k])
        const on = !Crop.neutral(c)
        if ((app.preferences.pushCropOn === true) !== on) app.setPreference("pushCropOn", on)
    }

    // The chosen game profile and the current console's profile follow what is changed while they are
    // chosen. Nothing is written for a few seconds after a profile is applied, so the settings it asked
    // for are not taken back before they have arrived.
    readonly property string liveProfile: JSON.stringify(prof.snapshot())
    property double profileQuietUntil: 0
    property bool keepsProfiles: true
    function flushProfiles() { if (prof.keepsProfiles && profileKeep.running && Date.now() >= prof.profileQuietUntil) { profileKeep.stop(); prof.keepProfiles() } }
    function quietProfiles() { prof.profileQuietUntil = Date.now() + 3000; if (prof.keepsProfiles) profileKeep.restart() }
    onLiveProfileChanged: if (prof.keepsProfiles) profileKeep.restart()
    Timer {
        id: profileKeep
        interval: 1500
        onTriggered: {
            const wait = prof.profileQuietUntil - Date.now()
            if (wait > 0) { profileKeep.interval = Math.max(100, wait); profileKeep.restart(); return }
            profileKeep.interval = 1500
            prof.keepProfiles()
        }
    }
    // Returns whether anything was written.
    function keepProfiles() {
        const p = copyProfiles()
        const before = JSON.stringify(p)
        if (!p.consoles || typeof p.consoles !== "object" || Array.isArray(p.consoles)) p.consoles = {}
        p.consoles[prof.consoleId] = prof.mergedSnapshot(p.consoles[prof.consoleId])
        if (prof.activeGame.length > 0 && Array.isArray(p.games)) {
            const i = p.games.findIndex(g => g && g.name === prof.activeGame)
            if (i >= 0 && p.games[i].console === prof.consoleId) {
                const g = prof.mergedSnapshot(p.games[i])
                g.name = p.games[i].name; g.console = p.games[i].console
                p.games[i] = g
            }
        }
        p.v = 1
        if (JSON.stringify(p) === before) return false
        return writeProfiles(p)
    }
    function switchConsole(id) {
        if (id === prof.consoleId || !prof.consoles.some(c => c.id === id)) return
        prof.flushProfiles()
        prof.quietProfiles()
        const p = copyProfiles()
        if (!p.consoles || typeof p.consoles !== "object" || Array.isArray(p.consoles)) p.consoles = {}
        p.consoles[prof.consoleId] = prof.mergedSnapshot(p.consoles[prof.consoleId])
        p.game = ""
        if (!writeProfiles(p)) return
        app.setPreference("pushConsole", id)
        applySnapshot(p.consoles[id])
    }
    function saveGame(name) {
        name = String(name || "").trim().slice(0, 40)
        if (name.length === 0) return false
        const p = copyProfiles()
        const list = Array.isArray(p.games) ? p.games.filter(g => g && g.name !== name) : []
        if (list.length >= prof.maxGames) return false
        const g = prof.snapshot()
        g.name = name
        g.console = prof.consoleId
        list.push(g)
        list.sort((a, b) => a.name.localeCompare(b.name))
        p.games = list
        p.game = name
        return writeProfiles(p)
    }
    function applyGame(name) {
        prof.flushProfiles()
        prof.quietProfiles()
        const g = prof.games.find(x => x.name === name)
        const p = copyProfiles()
        p.game = g ? name : ""
        if (!g) { writeProfiles(p); return }
        if (prof.consoles.some(c => c.id === g.console)) app.setPreference("pushConsole", g.console)
        applySnapshot(g)
        writeProfiles(p)
    }
    function deleteGame(name) {
        const p = copyProfiles()
        p.games = (Array.isArray(p.games) ? p.games : []).filter(g => g && g.name !== name)
        if (p.game === name) p.game = ""
        writeProfiles(p)
    }

    // The capture card chosen, and what is known of it: the Windows row (`pushfps-card-row`) with its list
    // (PushCaptureCards.js, shared) and its words. The tag is only for a card that was tested or reported
    // working; a card with nothing known only offers the report.
    VRow {
        id: cardRow
        objectName: "pushfps-card-row"
        readonly property string deviceName: app.videoDeviceIndex >= 0 && app.videoDeviceIndex < app.videoDevices.length
                                             ? String(app.videoDevices[app.videoDeviceIndex]) : ""
        readonly property var card: Cards.find(deviceName)
        visible: deviceName.length > 0
        label: qsTr("采集卡")
        hint: card && card.status === "tested" ? qsTr("PushFPS 已实测")
            : card && card.status === "veyra" ? qsTr("Veyra 作者已实测")
            : card && card.status === "reported" ? qsTr("有 PushFPS 用户报告可用")
            : qsTr("欢迎在 GitHub 上分享这张卡的使用结果")
        hintBelow: true
        RowLayout {
            spacing: 6
            Text {
                text: cardRow.deviceName
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                elide: Text.ElideRight; Layout.maximumWidth: 180
            }
            VTag {
                objectName: "pushfps-card-status"
                visible: cardRow.card !== null && cardRow.card.status !== "untested"
                kind: "ok"
                text: qsTr("已测试")
            }
            VButton {
                objectName: "pushfps-card-report"
                visible: !(cardRow.card && cardRow.card.status === "tested")
                ghost: true
                text: qsTr("报告")
                onClicked: Qt.openUrlExternally(Cards.reportUrl)
            }
        }
    }
    VRow {
        objectName: "pushfps-console-row"
        label: qsTr("游戏主机")
        VSelect {
            objectName: "pushfps-console"
            implicitWidth: 190
            options: prof.consoles
            value: prof.consoleLabel
            onPicked: id => prof.switchConsole(id)
        }
    }
    VRow {
        id: gameRow
        objectName: "pushfps-game-row"
        label: qsTr("游戏配置")
        hint: prof.activeGame.length > 0 ? qsTr("此配置会记住你接下来做的更改") : qsTr("把当前设置存为某个游戏的配置，之后一键切换")
        property bool naming: false
        RowLayout {
            spacing: 4
            VSelect {
                objectName: "pushfps-game"
                visible: !gameRow.naming
                implicitWidth: 150
                options: [{ id: "", label: qsTr("无") }].concat(prof.games.map(g => ({ id: g.name, label: g.name })))
                value: prof.activeGame.length > 0 ? prof.activeGame : qsTr("无")
                onPicked: id => prof.applyGame(id)
            }
            DialogTextField {
                id: gameName
                objectName: "pushfps-game-name"
                visible: gameRow.naming
                implicitWidth: 150
                placeholder: qsTr("游戏名称")
            }
            VButton {
                objectName: "pushfps-game-save"
                icon: true
                iconName: gameRow.naming ? "check" : "plus"
                tip: gameRow.naming ? qsTr("保存") : (prof.activeGame.length > 0 ? qsTr("更新或另存为游戏配置") : qsTr("存为游戏配置"))
                enabled: gameRow.naming ? gameName.text.trim().length > 0 : prof.games.length < prof.maxGames || prof.activeGame.length > 0
                onClicked: {
                    if (gameRow.naming) { if (prof.saveGame(gameName.text)) gameRow.naming = false; return }
                    gameName.text = prof.activeGame
                    gameRow.naming = true
                    gameName.focusEditor()
                }
            }
            VButton {
                objectName: "pushfps-game-delete"
                icon: true
                iconName: gameRow.naming ? "x" : "trash"
                tip: gameRow.naming ? qsTr("取消") : qsTr("删除此游戏配置")
                visible: gameRow.naming || prof.activeGame.length > 0
                onClicked: {
                    if (gameRow.naming) { gameRow.naming = false; return }
                    prof.deleteGame(prof.activeGame)
                }
            }
        }
    }
    // Game presets (GamePresetsDialog.qml): the Windows tool tile as a row, with its words.
    VRow {
        objectName: "pushfps-game-presets-row"
        label: qsTr("游戏预设")
        help: qsTr("GTA VI 等游戏的推荐设置，小地图和 HUD 保持清晰")
        hint: qsTr("GTA VI 等，HUD 保持清晰")
        hintBelow: true
        VButton { objectName: "pushfps-game-presets"; iconName: "gamepad"; text: qsTr("打开"); onClicked: app.openDialog("games") }
    }
}
