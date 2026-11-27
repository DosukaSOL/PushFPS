// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// PushFPS streaming mode (page "live"; dock button 直播模式), laid out like OBS Studio: the live
// preview in the middle, PushFPS's own features on the left (the quick setup: frame generation,
// adaptive multiplier, pacing repair, HUD lock, AI enhancement, lowest latency), the controls on the
// right (go live, record, instant replay, comparison clip, screenshot, outputs), the audio mixer
// across the bottom and a status bar under it.
//
// Audio mixer (engine/PushAudioMix.h, bridge PushStudio.cpp): game sound and microphone faders,
// mutes and level meters for what recordings and streams take, plus the monitor volume (what you
// hear yourself, Veyra's own volume). The microphone's gear opens its filters (PushMicFiltersDialog:
// rumble filter, EQ, noise gate, compressor). While this page is on screen the meters keep the
// game sound tap open, and the microphone too when recordings or streams use it.
// Desktop audio (everything Windows plays except PushFPS) and an extra input (a second microphone,
// a line-in) have their own strips (engine/PushAuxAudio.h).
//
// Sources (engine/PushLayerSources.h): a scene can add up to four webcams, windows or looping video
// files in a corner or the middle, at a size, with rounded corners or a circle. They run while this
// page is on screen or a recording or stream is going, so a camera is not on while you just play.
// Scene changes cut or fade (pref studioTransitionMs). The layout editor (PushLayoutDialog.qml, the
// 布局 button) moves, resizes and crops the game picture and the sources on the output picture.
// Geometry: StudioLayout.js, also used by Main.qml to place the native video window.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "StudioLayout.js" as StudioGeometry
import "PushRecord.js" as Record

VPage {
    id: studio
    signal requestPage(string page)
    // Opens the layout editor (PushLayoutDialog.qml) on a game scene.
    signal requestLayout(string sceneId)
    function openLayout() {
        const editing = studio.sceneById(studio.editingScene)
        const active = studio.sceneById(studio.activeScene)
        const scene = editing && editing.type === "game" ? editing : active && active.type === "game" ? active : studio.scenes.find(x => x.type === "game")
        studio.requestLayout(scene ? scene.id : "")
    }
    readonly property var geometry: StudioGeometry.calculate(width, height)
    readonly property bool compact: geometry.compact
    readonly property rect videoViewport: Qt.rect(geometry.video.x, geometry.video.y, geometry.video.width, geometry.video.height)

    component FieldLabel: Text { color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11 }
    component StatusText: Text {
        color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 11
    }

    // --- live state, polled while the page is on screen ------------------------------------
    property var stream: ({})
    property var levels: ({})
    property var stats: ({})
    readonly property bool live: stream.active === true
    readonly property bool recordingNow: veyra.recording === true
    readonly property bool micUsed: veyra.recordingMicrophone === true || veyra.preferences.streamMic === true
    // Shown meter levels with OBS-like fall-off (StudioLayout.fall).
    property real gameRms: -100
    property real gamePeak: -100
    property real micRms: -100
    property real micPeak: -100
    property real desktopRms: -100
    property real desktopPeak: -100
    property real auxRms: -100
    property real auxPeak: -100
    property double lastTick: 0

    function poll() {
        const now = Date.now()
        const dt = studio.lastTick > 0 ? Math.min(0.5, (now - studio.lastTick) / 1000) : 0.05
        studio.lastTick = now
        const l = veyra.pushStudioLevels() || ({})
        studio.levels = l
        studio.gameRms = StudioGeometry.fall(studio.gameRms, Number(l.gameRmsDb ?? -100), dt, 24)
        studio.gamePeak = StudioGeometry.fall(studio.gamePeak, Number(l.gamePeakDb ?? -100), dt, 24)
        studio.micRms = StudioGeometry.fall(studio.micRms, Number(l.micRmsDb ?? -100), dt, 24)
        studio.micPeak = StudioGeometry.fall(studio.micPeak, Number(l.micPeakDb ?? -100), dt, 24)
        studio.desktopRms = StudioGeometry.fall(studio.desktopRms, Number(l.desktopRmsDb ?? -100), dt, 24)
        studio.desktopPeak = StudioGeometry.fall(studio.desktopPeak, Number(l.desktopPeakDb ?? -100), dt, 24)
        studio.auxRms = StudioGeometry.fall(studio.auxRms, Number(l.auxRmsDb ?? -100), dt, 24)
        studio.auxPeak = StudioGeometry.fall(studio.auxPeak, Number(l.auxPeakDb ?? -100), dt, 24)
    }
    function pollSlow() {
        studio.stream = veyra.pushStreamStatus() || ({})
        studio.stats = veyra.pushLiveStats() || ({})
        studio.sourceStatus = veyra.pushSourceStatus() || []
    }
    function meters(on) {
        veyra.pushSetStudioMeters(on)
        if (!on) { studio.gameRms = studio.gamePeak = studio.micRms = studio.micPeak = -100; studio.desktopRms = studio.desktopPeak = studio.auxRms = studio.auxPeak = -100 }
    }
    onCurrentChanged: {
        if (current) { veyra.refreshMicrophones(); studio.pollSlow(); studio.lastTick = 0 }
        studio.meters(current)
    }
    // The microphone meter follows whether recordings or streams use it, and the chosen device.
    onMicUsedChanged: if (current) studio.meters(true)
    Connections {
        target: veyra
        // Local control (ui/PushControl.h) asks for a scene by its number, as Ctrl+1..9 does.
        function onPushControlScene(number) {
            const n = Math.round(Number(number))
            if (n >= 1 && n <= studio.scenes.length) studio.activate(studio.scenes[n - 1].id)
        }
        function onPreferencesChanged() {
            const id = veyra.recordingMicrophoneId
            if (studio.current && studio.micUsed && id !== studio.meterDevice) { studio.meterDevice = id; studio.meters(false); studio.meters(true) }
        }
    }
    property string meterDevice: veyra.recordingMicrophoneId
    Timer { interval: 50; repeat: true; running: studio.current; onTriggered: studio.poll() }
    Timer { interval: 1000; repeat: true; running: studio.current; onTriggered: studio.pollSlow() }

    function toggleMicRecording() { veyra.setPreference("recordMic", !veyra.recordingMicrophone); veyra.refreshMicrophones() }
    function toggleMicStreaming() { veyra.setPreference("streamMic", !(veyra.preferences.streamMic === true)) }
    // --- scenes (PushStreamOverlay.h): what recordings and streams carry ------------------------
    // pref studioScenes: [{id, name, type: "game"|"card", title, subtitle, image, overlayImage,
    // overlayText, corner, muteGame}]; the first game scene cannot be removed. Ctrl+1..9 switch.
    readonly property var defaultScenes: [
        { id: "game", name: qsTr("游戏"), type: "game", overlayImage: "", overlayText: "", corner: "tr" },
        { id: "soon", name: qsTr("即将开始"), type: "card", title: qsTr("即将开始"), subtitle: "", image: "", muteGame: true },
        { id: "brb", name: qsTr("马上回来"), type: "card", title: qsTr("马上回来"), subtitle: "", image: "", muteGame: true },
        { id: "end", name: qsTr("感谢观看"), type: "card", title: qsTr("感谢观看"), subtitle: "", image: "", muteGame: true }
    ]
    readonly property var scenes: {
        let list = null
        try { list = JSON.parse(String(veyra.preferences.studioScenes || "")) } catch (e) { list = null }
        return Array.isArray(list) && list.length > 0 && list.some(x => x.type === "game") ? list : studio.defaultScenes
    }
    property string activeScene: "game"
    // A card silences the game in what carries the scene. That is the engine's own state
    // (pushSetCardMute, never saved) and no longer the mixer's saved mute: until 3.2.3 a card wrote
    // mixGameMuted, so closing PushFPS on a card left the game muted in every later recording, and a
    // comparison clip made with a card selected (it never shows the card) was silent in any mode.
    property bool cardMute: false
    function setCardMute(on) { studio.cardMute = on === true; veyra.pushSetCardMute(studio.cardMute) }
    property string editingScene: ""
    function sceneById(id) { return studio.scenes.find(x => x.id === id) || null }
    function saveScenes(list) { veyra.setPreference("studioScenes", JSON.stringify(list)) }
    function activate(id) {
        const scene = studio.sceneById(id)
        if (!scene) return
        veyra.pushSetStreamScene(scene)
        studio.setCardMute(scene.type === "card" && scene.muteGame !== false)
        studio.activeScene = id
        studio.applySources()
        veyra.logUi("push-studio", "scene " + id + " (" + scene.type + ")")
    }
    function addScene(type) {
        const list = studio.scenes.slice()
        if (list.length >= 12) return
        const id = "s" + Date.now().toString(36)
        list.push(type === "card" ? { id: id, name: qsTr("新场景"), type: "card", title: qsTr("新场景"), subtitle: "", image: "", muteGame: true }
                                  : { id: id, name: qsTr("新场景"), type: "game", overlayImage: "", overlayText: "", corner: "tr" })
        studio.saveScenes(list)
        studio.editingScene = id
    }
    function updateScene(id, changes) {
        const list = studio.scenes.map(x => x.id === id ? Object.assign({}, x, changes) : x)
        studio.saveScenes(list)
        if (id === studio.activeScene) Qt.callLater(() => studio.activate(id))
    }
    function removeScene(id) {
        const games = studio.scenes.filter(x => x.type === "game")
        const target = studio.sceneById(id)
        if (!target || (target.type === "game" && games.length <= 1)) return false
        if (id === studio.activeScene) studio.activate(games.find(x => x.id !== id).id)
        if (id === studio.editingScene) studio.editingScene = ""
        studio.saveScenes(studio.scenes.filter(x => x.id !== id))
        return true
    }
    // A copy right under the original, with its own ids (the sources too: each scene's are its own).
    function duplicateScene(id) {
        const scene = studio.sceneById(id)
        const list = studio.scenes.slice()
        if (!scene || list.length >= 12) return ""
        const stamp = Date.now().toString(36) + Math.floor(Math.random() * 46656).toString(36)
        const copy = JSON.parse(JSON.stringify(scene))
        copy.id = "s" + stamp
        copy.name = (String(scene.name || "") + " 2").slice(0, 40)
        if (Array.isArray(copy.sources)) copy.sources = copy.sources.map((x, i) => Object.assign({}, x, { id: "src" + stamp + i }))
        list.splice(list.findIndex(x => x.id === id) + 1, 0, copy)
        studio.saveScenes(list)
        return copy.id
    }
    // Right-click in the scenes list (as in OBS): on a scene its own actions, anywhere else in the
    // list only the two ways to add one.
    function openSceneMenu(id, x, y) {
        sceneMenu.targetId = studio.sceneById(id) ? id : ""
        sceneMenuAnchor.x = x; sceneMenuAnchor.y = y
        sceneMenu.openAt(sceneMenuAnchor, "at")
    }
    function sceneMenuPick(action, id) {
        if (action === "add-game" || action === "add-card") { studio.addScene(action === "add-game" ? "game" : "card"); return }
        if (!studio.sceneById(id)) return
        if (action === "show") studio.activate(id)
        else if (action === "edit") studio.editingScene = id
        else if (action === "copy") { const copy = studio.duplicateScene(id); if (copy) studio.editingScene = copy }
        else if (action === "delete") studio.removeScene(id)
    }
    // The scene under a point of the controls card, "" for the rest of the scenes list and null
    // outside it (the buttons under the list keep their own right-click: none).
    function sceneAt(point) {
        for (let i = 0; i < sceneRows.count; ++i) {
            const row = sceneRows.itemAt(i)
            if (!row) continue
            const q = row.mapFromItem(controlsCard, point.x, point.y)
            if (q.x >= 0 && q.y >= 0 && q.x < row.width && q.y < row.height) return row.modelData.id
        }
        const c = controls.mapFromItem(controlsCard, point.x, point.y)
        return c.y >= scenesHeader.y - 8 && c.y <= sceneAddRow.y + sceneAddRow.height + 8 ? "" : null
    }
    // --- sources (PushLayerSources.h): webcams, windows and video files in a scene ----------------
    // scene.sources: [{id, kind: "camera"|"window"|"media", target, name, corner, size, shape, mirror, loop}].
    property var sourceStatus: []
    property string editingSource: ""
    // Sources run while this page is on screen or something records or streams.
    readonly property bool sourcesActive: studio.current || studio.recordingNow || studio.live
    onSourcesActiveChanged: studio.applySources()
    // The layout editor writes the scenes straight to the preference; sources follow it.
    onScenesChanged: Qt.callLater(studio.applySources)
    function applySources() {
        const scene = studio.sceneById(studio.activeScene)
        veyra.pushSetSceneSources(studio.sourcesActive && scene && Array.isArray(scene.sources) ? scene.sources : [])
    }
    function statusOf(id) { return studio.sourceStatus.find(x => x.id === id) || null }
    // What a camera offers, largest first: a chosen resolution gives the closest of these.
    function cameraSizes(status) {
        const sizes = status && status.sizes ? Array.from(status.sizes) : []   // a C++ list is not an Array to Array.isArray
        if (sizes.length === 0) return ""
        return qsTr("摄像头提供：%1。相机在网络摄像头模式下送出的画面可能低于它的录制分辨率")
            .arg(sizes.slice(0, 5).map(x => x.width + "×" + x.height + " (" + Math.round(Number(x.fps || 0)) + " FPS)").join(", "))
    }
    function addSource(kind) {
        const scene = studio.sceneById(studio.editingScene)
        if (!scene) return false
        const list = Array.isArray(scene.sources) ? scene.sources.slice() : []
        if (list.length >= 4) return false
        let target = "", name = ""
        if (kind === "camera") {
            const cams = veyra.pushCameras() || []
            if (cams.length === 0) { studio.sourceNote = qsTr("没有找到摄像头"); return false }
            target = cams[0].id; name = cams[0].name
        } else if (kind === "window") {
            const wins = veyra.pushWindows() || []
            if (wins.length === 0) { studio.sourceNote = qsTr("没有可用的窗口"); return false }
            target = wins[0].id; name = wins[0].name
        } else {
            target = veyra.pushPickMedia()
            if (!target) return false
            name = target.split(/[\\/]/).pop()
        }
        studio.sourceNote = ""
        const id = "src" + Date.now().toString(36)
        list.push({ id: id, kind: kind, target: target, name: name, corner: kind === "camera" ? "br" : "tr",
                    size: kind === "camera" ? 0.24 : 0.3, shape: kind === "camera" ? "round" : "rect", mirror: false, loop: true })
        studio.updateScene(scene.id, { sources: list })
        studio.editingSource = id
        return true
    }
    function updateSource(sourceId, changes) {
        const scene = studio.sceneById(studio.editingScene)
        if (!scene || !Array.isArray(scene.sources)) return
        studio.updateScene(scene.id, { sources: scene.sources.map(x => x.id === sourceId ? Object.assign({}, x, changes) : x) })
    }
    function removeSource(sourceId) {
        const scene = studio.sceneById(studio.editingScene)
        if (!scene || !Array.isArray(scene.sources)) return
        studio.updateScene(scene.id, { sources: scene.sources.filter(x => x.id !== sourceId) })
        if (studio.editingSource === sourceId) studio.editingSource = ""
    }
    property string sourceNote: ""
    // Scene transitions: cut, fade or slow fade (pref studioTransitionMs).
    readonly property int transitionMs: Number(veyra.preferences.studioTransitionMs ?? 300)
    // Starts on the game scene: a card left on from a previous session never surprises a new stream.
    Component.onCompleted: Qt.callLater(() => {
        const g = studio.scenes.find(x => x.type === "game")
        if (g) veyra.pushSetStreamScene(g)
        studio.setCardMute(false)
    })
    Repeater {
        model: 9
        delegate: Item {
            required property int index
            Shortcut {
                sequence: "Ctrl+" + (index + 1)
                enabled: studio.current && index < studio.scenes.length
                onActivated: studio.activate(studio.scenes[index].id)
            }
        }
    }

    VMenu {
        id: sceneMenu
        objectName: "studio-scene-menu"
        property string targetId: ""
        readonly property var target: studio.sceneById(targetId)
        readonly property bool lastGame: target !== null && target.type === "game" && studio.scenes.filter(x => x.type === "game").length <= 1
        readonly property bool full: studio.scenes.length >= 12
        title: target ? String(target.name || "") : qsTr("场景")
        items: (target ? [
                { label: qsTr("切换到此场景"), id: "show", disabled: studio.activeScene === targetId },
                { label: qsTr("编辑场景"), id: "edit" },
                { label: qsTr("复制场景"), id: "copy", disabled: full, note: full ? qsTr("最多 12 个场景") : "" },
                { label: qsTr("删除场景"), id: "delete", danger: true, disabled: lastGame, note: lastGame ? qsTr("至少保留一个画面场景") : "" },
                { sep: true }
            ] : []).concat([
                { label: qsTr("添加画面场景"), id: "add-game", icon: "plus", disabled: full, note: full ? qsTr("最多 12 个场景") : "" },
                { label: qsTr("添加图卡场景"), id: "add-card", icon: "plus", disabled: full }
            ])
        onPicked: (i, item) => studio.sceneMenuPick(item.id, targetId)
    }

    function goLive() {
        if (studio.live) { veyra.pushStopStream(); studio.pollSlow(); return }
        let targets = []
        try { targets = JSON.parse(String(veyra.preferences.streamTargets || "[]")) } catch (e) { targets = [] }
        if (!Array.isArray(targets) || targets.filter(t => t.enabled !== false).length === 0) { veyra.navigate("stream"); return }
        veyra.pushStartStream()
        studio.pollSlow()
    }
    readonly property real streamKbps: {
        const outs = studio.stream.outputs || []
        let sum = 0
        for (const o of outs) sum += Number(o.kbps || 0)
        return sum
    }
    readonly property string sourceText: veyra.hasSource ? (veyra.sourceTitle.length > 0 ? veyra.sourceTitle : qsTr("片源")) : qsTr("没有画面")

    // --- header ----------------------------------------------------------------------------
    RowLayout {
        id: head
        x: 12; y: 12; width: parent.width - 24; height: 32
        spacing: 12
        VH2 { text: qsTr("直播模式") }
        Text {
            visible: !studio.compact
            text: qsTr("直播和录制 PushFPS 的画面，混音、开播、录像都在这一页")
            color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12
        }
        Item { Layout.fillWidth: true }
        // On-air and recording lights, as OBS shows them.
        Rectangle {
            objectName: "studio-live-badge"
            visible: studio.live
            implicitWidth: liveText.implicitWidth + 26; implicitHeight: 24; radius: 12
            color: Theme.errGlow; border.width: 1; border.color: Theme.err
            Rectangle { x: 9; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: Theme.err }
            Text {
                id: liveText
                x: 20; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("直播中 %1").arg(StudioGeometry.duration(studio.stream.seconds))
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
            }
        }
        Rectangle {
            objectName: "studio-rec-badge"
            visible: studio.recordingNow
            implicitWidth: recText.implicitWidth + 26; implicitHeight: 24; radius: 12
            color: Theme.ink(0.06); border.width: 1; border.color: Theme.err
            Rectangle { x: 9; anchors.verticalCenter: parent.verticalCenter; width: 7; height: 7; radius: 4; color: Theme.err }
            Text {
                id: recText
                x: 20; anchors.verticalCenter: parent.verticalCenter
                text: qsTr("录制 %1").arg(StudioGeometry.duration(veyra.recordingState.seconds))
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
            }
        }
        // PushFPS 4.1: the window as large as the screen, and back, in sight (PushMaximizeButton.qml).
        PushMaximizeButton { objectName: "studio-maximize" }
    }

    // --- left: PushFPS features --------------------------------------------------------------
    Rectangle {
        objectName: "studio-features"
        visible: !studio.compact
        x: studio.geometry.left.x; y: studio.geometry.left.y
        width: studio.geometry.left.width; height: studio.geometry.left.height
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        clip: true
        Flickable {
            id: featureScroll
            anchors.fill: parent
            anchors.margins: 10
            contentWidth: width
            contentHeight: featureColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: featureColumn
                width: featureScroll.width
                spacing: 10
                PushQuickSetup {
                    Layout.fillWidth: true
                    compact: true
                    compactPro: true
                    shortForm: !Theme.modern   // (the new design: every row of quick setup, here too)
                }
                // PushFPS measurements (quality test, PS5 latency test).
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    VButton { objectName: "studio-quality-test"; Layout.fillWidth: true; ghost: true; iconName: "activity"; text: qsTr("画质测试"); enabled: veyra.hasSource && veyra.fgEnabled; onClicked: veyra.navigate("qualitytest") }
                    VButton { objectName: "studio-latency-test"; Layout.fillWidth: true; ghost: true; iconName: "gauge"; text: qsTr("延迟测试"); visible: veyra.sourceKind === "ps5"; onClicked: veyra.navigate("latencytest") }
                }
            }
        }
    }

    // --- middle: the preview ----------------------------------------------------------------
    Rectangle {
        objectName: "studio-preview"
        x: studio.geometry.preview.x; y: studio.geometry.preview.y
        width: studio.geometry.preview.width; height: studio.geometry.preview.height
        radius: Theme.rCard; color: Theme.videoBlack; border.color: Theme.stroke
        RowLayout {
            x: 12; width: parent.width - 24; height: studio.geometry.headerHeight
            spacing: 8
            VIcon {
                name: veyra.sourceKind === "ps5" || veyra.sourceKind === "xbox" ? "gamepad" : veyra.sourceKind === "moonlight" ? "cast" : "video"
                size: 14; color: Theme.t2
            }
            Text {
                Layout.fillWidth: true
                text: studio.sourceText
                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                elide: Text.ElideRight
            }
            Text {
                visible: veyra.hasSource
                text: {
                    const out = Number(studio.stats.submitFps || 0), game = Number(studio.stats.contentFps || 0)
                    return (game > 0 ? qsTr("游戏 %1 FPS").arg(Math.round(game)) + " → " : "") + (out > 0 ? qsTr("%1 FPS").arg(Math.round(out)) : "")
                }
                color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 11
            }
        }
        // Nothing playing: where to start. The native video window covers this area once a picture runs.
        ColumnLayout {
            visible: !veyra.hasSource
            anchors.centerIn: parent
            anchors.verticalCenterOffset: studio.geometry.headerHeight / 2
            spacing: 12
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("先连接画面，再开始直播或录制")
                color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 13
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8
                VButton { objectName: "studio-open-capture"; primary: true; iconName: "video"; text: qsTr("采集卡"); onClicked: veyra.navigate("capture") }
                VButton { iconName: "gamepad"; text: "PS5"; onClicked: veyra.navigate("ps5") }
                VButton { iconName: "gamepad"; text: "Xbox"; onClicked: veyra.navigate("xbox") }
                VButton { iconName: "cast"; text: "Moonlight"; onClicked: veyra.navigate("moonlight") }
                VButton { objectName: "studio-open-frame-input"; iconName: "import"; text: qsTr("其他应用"); onClicked: veyra.navigate("frameinput") }
            }
        }
    }

    // --- right: controls ------------------------------------------------------------------
    Rectangle {
        id: controlsCard
        objectName: "studio-controls"
        x: studio.geometry.right.x; y: studio.geometry.right.y
        width: studio.geometry.right.width; height: studio.geometry.right.height
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        clip: true
        // Right-click in the scenes list: one handler for the whole card, so a click on a scene and
        // a click beside it never both answer.
        Item { id: sceneMenuAnchor; width: 1; height: 1 }
        TapHandler {
            objectName: "studio-scene-context"
            acceptedButtons: Qt.RightButton
            onTapped: eventPoint => {
                const id = studio.sceneAt(eventPoint.position)
                if (id !== null) studio.openSceneMenu(id, eventPoint.position.x, eventPoint.position.y)
            }
        }
        Flickable {
            id: controlScroll
            anchors.fill: parent
            anchors.margins: 12
            contentWidth: width
            contentHeight: controls.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: controls
                width: controlScroll.width
                spacing: 8
                Text {
                    id: scenesHeader
                    text: qsTr("场景")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                Repeater {
                    id: sceneRows
                    model: studio.scenes
                    delegate: Rectangle {
                        id: sceneRow
                        required property var modelData
                        required property int index
                        objectName: "studio-scene-" + modelData.id
                        readonly property bool active: studio.activeScene === modelData.id
                        Layout.fillWidth: true
                        implicitHeight: 34
                        radius: 8
                        color: active ? Theme.errGlow : sceneHover.hovered ? Theme.ink(0.07) : Theme.ink(0.03)
                        border.width: 1
                        border.color: active ? Theme.err : Theme.stroke
                        HoverHandler { id: sceneHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: studio.activate(sceneRow.modelData.id) }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 4
                            spacing: 8
                            VIcon { name: sceneRow.modelData.type === "card" ? "image" : "gamepad"; size: 13; color: sceneRow.active ? Theme.err : Theme.t2 }
                            Text {
                                Layout.fillWidth: true
                                text: sceneRow.modelData.name
                                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                visible: sceneRow.index < 9
                                text: "Ctrl+" + (sceneRow.index + 1)
                                color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 10
                            }
                            VButton {
                                icon: true; ghost: true; iconName: "settings"
                                tip: qsTr("编辑场景")
                                onClicked: studio.editingScene = studio.editingScene === sceneRow.modelData.id ? "" : sceneRow.modelData.id
                            }
                        }
                    }
                }
                RowLayout {
                    id: sceneAddRow
                    Layout.fillWidth: true
                    spacing: 8
                    VButton { objectName: "studio-add-game-scene"; Layout.fillWidth: true; ghost: true; iconName: "plus"; text: qsTr("画面场景"); onClicked: studio.addScene("game") }
                    VButton { objectName: "studio-add-card-scene"; Layout.fillWidth: true; ghost: true; iconName: "plus"; text: qsTr("图卡场景"); onClicked: studio.addScene("card") }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    FieldLabel { text: qsTr("转场") }
                    VSeg {
                        objectName: "studio-transition"
                        Layout.fillWidth: true
                        options: [{ id: "0", label: qsTr("切换") }, { id: "300", label: qsTr("淡入淡出") }, { id: "800", label: qsTr("慢速淡入") }]
                        current: studio.transitionMs <= 0 ? "0" : studio.transitionMs >= 600 ? "800" : "300"
                        onPicked: id => veyra.setPreference("studioTransitionMs", Number(id))
                    }
                }
                // The scene editor, under the list while a scene is being edited.
                Rectangle {
                    id: editor
                    objectName: "studio-scene-editor"
                    readonly property var scene: studio.sceneById(studio.editingScene)
                    visible: scene !== null
                    Layout.fillWidth: true
                    implicitHeight: editorColumn.implicitHeight + 20
                    radius: 8
                    color: Theme.ink(0.04)
                    border.width: 1
                    border.color: Theme.stroke2
                    ColumnLayout {
                        id: editorColumn
                        x: 10; y: 10; width: parent.width - 20
                        spacing: 6
                        FieldLabel { text: qsTr("名称") }
                        DialogTextField {
                            objectName: "scene-name"
                            Layout.fillWidth: true
                            text: editor.scene ? editor.scene.name : ""
                            onEdited: t => { if (editor.scene && t.trim().length) studio.updateScene(editor.scene.id, { name: t.trim().slice(0, 40) }) }
                        }
                        FieldLabel { visible: editor.scene && editor.scene.type === "card"; text: qsTr("标题") }
                        DialogTextField {
                            objectName: "scene-title"
                            visible: editor.scene && editor.scene.type === "card"
                            Layout.fillWidth: true
                            text: editor.scene ? (editor.scene.title || "") : ""
                            onEdited: t => { if (editor.scene) studio.updateScene(editor.scene.id, { title: t.slice(0, 60) }) }
                        }
                        FieldLabel { visible: editor.scene && editor.scene.type === "card"; text: qsTr("副标题") }
                        DialogTextField {
                            objectName: "scene-subtitle"
                            visible: editor.scene && editor.scene.type === "card"
                            Layout.fillWidth: true
                            text: editor.scene ? (editor.scene.subtitle || "") : ""
                            onEdited: t => { if (editor.scene) studio.updateScene(editor.scene.id, { subtitle: t.slice(0, 120) }) }
                        }
                        FieldLabel { visible: editor.scene && editor.scene.type === "game"; text: qsTr("角标文字（例如频道名）") }
                        DialogTextField {
                            objectName: "scene-overlay-text"
                            visible: editor.scene && editor.scene.type === "game"
                            Layout.fillWidth: true
                            text: editor.scene ? (editor.scene.overlayText || "") : ""
                            onEdited: t => { if (editor.scene) studio.updateScene(editor.scene.id, { overlayText: t.slice(0, 80) }) }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            VButton {
                                objectName: "scene-pick-image"
                                Layout.fillWidth: true
                                ghost: true
                                iconName: "image"
                                readonly property string current: editor.scene ? String(editor.scene.type === "card" ? (editor.scene.image || "") : (editor.scene.overlayImage || "")) : ""
                                text: current.length ? current.split(/[\\/]/).pop() : (editor.scene && editor.scene.type === "card" ? qsTr("背景图片") : qsTr("角标图片"))
                                maxTextWidth: 150
                                onClicked: {
                                    const path = veyra.pushPickImage()
                                    if (path && editor.scene) studio.updateScene(editor.scene.id, editor.scene.type === "card" ? { image: path } : { overlayImage: path })
                                }
                            }
                            VButton {
                                icon: true; ghost: true; iconName: "x"
                                tip: qsTr("移除图片")
                                onClicked: if (editor.scene) studio.updateScene(editor.scene.id, editor.scene.type === "card" ? { image: "" } : { overlayImage: "" })
                            }
                        }
                        VSeg {
                            objectName: "scene-corner"
                            visible: editor.scene && editor.scene.type === "game"
                            options: [{ id: "tl", label: qsTr("左上") }, { id: "tr", label: qsTr("右上") }, { id: "bl", label: qsTr("左下") }, { id: "br", label: qsTr("右下") }]
                            current: editor.scene ? (editor.scene.corner || "tr") : "tr"
                            onPicked: id => { if (editor.scene) studio.updateScene(editor.scene.id, { corner: id }) }
                        }
                        // Sources: webcams, windows and video files over the picture.
                        FieldLabel { Layout.topMargin: 4; text: qsTr("来源（摄像头、窗口、视频）") }
                        Repeater {
                            model: editor.scene && Array.isArray(editor.scene.sources) ? editor.scene.sources : []
                            delegate: Rectangle {
                                id: sourceRow
                                required property var modelData
                                objectName: "studio-source-" + modelData.id
                                readonly property var status: studio.statusOf(modelData.id)
                                readonly property bool open: studio.editingSource === modelData.id
                                Layout.fillWidth: true
                                implicitHeight: sourceColumn.implicitHeight + 12
                                radius: 6
                                color: Theme.ink(0.03)
                                border.width: 1
                                border.color: open ? Theme.stroke2 : Theme.stroke
                                ColumnLayout {
                                    id: sourceColumn
                                    x: 8; y: 6; width: parent.width - 16
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        VIcon { name: sourceRow.modelData.kind === "camera" ? "camera" : sourceRow.modelData.kind === "window" ? "monitor" : "video"; size: 13; color: Theme.t2 }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 0
                                            Text {
                                                Layout.fillWidth: true
                                                text: String(sourceRow.modelData.name || "")
                                                color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                Layout.fillWidth: true
                                                visible: studio.sourcesActive
                                                text: !sourceRow.status ? qsTr("正在启动…")
                                                    : String(sourceRow.status.error || "").length ? String(sourceRow.status.error)
                                                    : sourceRow.status.running ? qsTr("%1×%2 · %3 FPS").arg(sourceRow.status.width).arg(sourceRow.status.height).arg(Math.round(Number(sourceRow.status.fps || 0)))
                                                    : qsTr("正在启动…")
                                                // A source running under 15 FPS is worth a second look.
                                                color: sourceRow.status && String(sourceRow.status.error || "").length ? Theme.err
                                                     : sourceRow.status && sourceRow.status.running && Number(sourceRow.status.fps || 0) > 0 && Number(sourceRow.status.fps) < 15 ? Theme.warn : Theme.t3
                                                font.family: Theme.fontUi; font.pixelSize: 10
                                                elide: Text.ElideRight
                                            }
                                        }
                                        VButton {
                                            icon: true; ghost: true; iconName: "settings"
                                            tip: qsTr("位置和外观")
                                            onClicked: studio.editingSource = sourceRow.open ? "" : sourceRow.modelData.id
                                        }
                                        VButton {
                                            objectName: "source-remove-" + sourceRow.modelData.id
                                            icon: true; ghost: true; iconName: "x"
                                            tip: qsTr("移除来源")
                                            onClicked: studio.removeSource(sourceRow.modelData.id)
                                        }
                                    }
                                    // Settings, while open.
                                    VSelect {
                                        objectName: "source-target"
                                        visible: sourceRow.open && sourceRow.modelData.kind !== "media"
                                        Layout.fillWidth: true
                                        readonly property var choices: !sourceRow.open ? [] : (sourceRow.modelData.kind === "camera" ? veyra.pushCameras() : veyra.pushWindows()) || []
                                        options: choices.map(c => ({ id: c.id, label: c.name }))
                                        value: String(sourceRow.modelData.name || "")
                                        onPicked: id => { const c = choices.find(x => x.id === id); if (c) studio.updateSource(sourceRow.modelData.id, { target: c.id, name: c.name }) }
                                    }
                                    // The source, live: a new picture for every frame it publishes
                                    // (image://push-source, PushSourceImageProvider). Until 3.2.3 this was a
                                    // still fetched twice a second, and every camera looked like a slide show.
                                    Image {
                                        objectName: "source-preview"
                                        property int serial: 0
                                        visible: sourceRow.open && serial > 0
                                        Layout.alignment: Qt.AlignHCenter
                                        Layout.preferredWidth: Math.min(240, sourceColumn.width)
                                        Layout.preferredHeight: visible ? Layout.preferredWidth * 9 / 16 : 0
                                        fillMode: Image.PreserveAspectFit
                                        cache: false
                                        sourceSize.width: 480
                                        source: serial > 0 ? "image://push-source/" + sourceRow.modelData.id + "/" + serial : ""
                                        Timer {
                                            interval: 33; repeat: true; triggeredOnStart: true
                                            running: sourceRow.open && studio.sourcesActive
                                            onTriggered: parent.serial = veyra.pushSourceSerial(sourceRow.modelData.id)
                                        }
                                    }
                                    RowLayout {
                                        visible: sourceRow.open && sourceRow.modelData.kind === "camera"
                                        Layout.fillWidth: true
                                        spacing: 8
                                        FieldLabel { text: qsTr("分辨率") }
                                        VSeg {
                                            objectName: "source-resolution"
                                            Layout.fillWidth: true
                                            options: [{ id: "0", label: qsTr("自动") }, { id: "1080", label: "1080p" }, { id: "1440", label: "1440p" }, { id: "2160", label: "4K" }]
                                            current: String(Number(sourceRow.modelData.resolution || 0))
                                            onPicked: id => studio.updateSource(sourceRow.modelData.id, { resolution: Number(id) })
                                        }
                                    }
                                    Text {
                                        objectName: "source-sizes"
                                        visible: sourceRow.open && sourceRow.modelData.kind === "camera" && text.length > 0
                                        Layout.fillWidth: true
                                        text: studio.cameraSizes(sourceRow.status)
                                        color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 10
                                        wrapMode: Text.WordWrap
                                    }
                                    VSeg {
                                        objectName: "source-corner"
                                        visible: sourceRow.open
                                        Layout.fillWidth: true
                                        options: [{ id: "tl", label: qsTr("左上") }, { id: "tr", label: qsTr("右上") }, { id: "c", label: qsTr("中间") }, { id: "bl", label: qsTr("左下") }, { id: "br", label: qsTr("右下") }]
                                        current: String(sourceRow.modelData.corner || "br")
                                        onPicked: id => studio.updateSource(sourceRow.modelData.id, { corner: id })
                                    }
                                    RowLayout {
                                        visible: sourceRow.open
                                        Layout.fillWidth: true
                                        spacing: 8
                                        FieldLabel { text: qsTr("大小") }
                                        VSlider {
                                            objectName: "source-size"
                                            Layout.fillWidth: true
                                            from: 0.1; to: 1; stepSize: 0.01
                                            value: Number(sourceRow.modelData.size ?? 0.25)
                                            onMoved: v => studio.updateSource(sourceRow.modelData.id, { size: Math.round(v * 100) / 100 })
                                        }
                                        Text { text: Math.round(Number(sourceRow.modelData.size ?? 0.25) * 100) + "%"; color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 10 }
                                    }
                                    VSeg {
                                        objectName: "source-shape"
                                        visible: sourceRow.open
                                        Layout.fillWidth: true
                                        options: [{ id: "rect", label: qsTr("方形") }, { id: "round", label: qsTr("圆角") }, { id: "circle", label: qsTr("圆形") }]
                                        current: String(sourceRow.modelData.shape || "rect")
                                        onPicked: id => studio.updateSource(sourceRow.modelData.id, { shape: id })
                                    }
                                    RowLayout {
                                        visible: sourceRow.open && sourceRow.modelData.kind === "camera"
                                        Layout.fillWidth: true
                                        Text { Layout.fillWidth: true; text: qsTr("镜像"); color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12 }
                                        VSwitch {
                                            objectName: "source-mirror"
                                            checked: sourceRow.modelData.mirror === true
                                            onToggled: v => studio.updateSource(sourceRow.modelData.id, { mirror: v })
                                        }
                                    }
                                    RowLayout {
                                        visible: sourceRow.open && sourceRow.modelData.kind === "media"
                                        Layout.fillWidth: true
                                        Text { Layout.fillWidth: true; text: qsTr("循环播放"); color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12 }
                                        VSwitch {
                                            checked: sourceRow.modelData.loop !== false
                                            onToggled: v => studio.updateSource(sourceRow.modelData.id, { loop: v })
                                        }
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            readonly property bool full: editor.scene && Array.isArray(editor.scene.sources) && editor.scene.sources.length >= 4
                            VButton { objectName: "source-add-camera"; Layout.fillWidth: true; ghost: true; iconName: "camera"; text: qsTr("摄像头"); enabled: !parent.full; maxTextWidth: 70; onClicked: studio.addSource("camera") }
                            VButton { objectName: "source-add-window"; Layout.fillWidth: true; ghost: true; iconName: "monitor"; text: qsTr("窗口"); enabled: !parent.full; maxTextWidth: 70; onClicked: studio.addSource("window") }
                            VButton { objectName: "source-add-media"; Layout.fillWidth: true; ghost: true; iconName: "video"; text: qsTr("视频"); enabled: !parent.full; maxTextWidth: 70; onClicked: studio.addSource("media") }
                        }
                        VButton {
                            objectName: "scene-layout"
                            visible: editor.scene && editor.scene.type === "game"
                            Layout.fillWidth: true
                            iconName: "layers"
                            text: qsTr("在画面上摆放和裁剪…")
                            onClicked: studio.openLayout()
                        }
                        Text {
                            visible: studio.sourceNote.length > 0
                            Layout.fillWidth: true
                            text: studio.sourceNote
                            color: Theme.warn; font.family: Theme.fontUi; font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                        RowLayout {
                            visible: editor.scene && editor.scene.type === "card"
                            Layout.fillWidth: true
                            Text { Layout.fillWidth: true; text: qsTr("切换到此场景时静音游戏声音"); color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            VSwitch {
                                objectName: "scene-mute"
                                checked: editor.scene ? editor.scene.muteGame !== false : true
                                onToggled: v => { if (editor.scene) studio.updateScene(editor.scene.id, { muteGame: v }) }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            VButton { objectName: "scene-delete"; ghost: true; text: qsTr("删除"); onClicked: { if (editor.scene && studio.removeScene(editor.scene.id)) studio.editingScene = "" } }
                            Item { Layout.fillWidth: true }
                            VButton { objectName: "scene-done"; primary: true; text: qsTr("完成"); onClicked: studio.editingScene = "" }
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("场景只改变录制和直播的画面；你自己看到的仍是游戏")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.topMargin: 6
                    text: qsTr("控制")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                VButton {
                    objectName: "studio-go-live"
                    Layout.fillWidth: true
                    primary: !studio.live
                    iconName: "cast"
                    text: studio.live ? qsTr("停止直播") : qsTr("开始直播")
                    enabled: studio.live || veyra.hasSource
                    onClicked: studio.goLive()
                }
                VButton {
                    objectName: "studio-record"
                    Layout.fillWidth: true
                    iconName: "video"
                    text: studio.recordingNow ? qsTr("停止录制") : qsTr("开始录制")
                    enabled: studio.recordingNow || veyra.hasSource
                    // PushFPS 4.0: the choices first (PushRecordDialog), unless that was switched off.
                    onClicked: !studio.recordingNow && Record.asks(veyra.preferences) ? veyra.navigate("record") : veyra.toggleRecording("studio")
                }
                // PushFPS clip markers: this moment, noted in a text file beside the recording (also the
                // shortcut, F6 unless changed). Only while a recording runs; never in a comparison clip.
                VButton {
                    objectName: "studio-marker"
                    Layout.fillWidth: true
                    visible: studio.recordingNow && veyra.recordingState.compare !== true
                    iconName: "plus"
                    readonly property int count: Number(veyra.recordingState.markers || 0)
                    text: count > 0 ? qsTr("标记这一刻（已有 %1 个）").arg(count) : qsTr("标记这一刻")
                    onClicked: veyra.pushAddMarker("studio")
                }
                VButton {
                    objectName: "studio-replay"
                    Layout.fillWidth: true
                    iconName: "clock"
                    text: veyra.replayEnabled ? qsTr("保存即时回放（%1 秒）").arg(veyra.replaySeconds) : qsTr("开启即时回放")
                    onClicked: veyra.replayEnabled ? veyra.saveReplay("studio") : veyra.setPreference("replayEnabled", true)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    VButton {
                        objectName: "studio-compare"
                        Layout.fillWidth: true
                        iconName: "split"
                        // A running comparison clip (one without a length above all) is stopped here too.
                        readonly property bool running: studio.recordingNow && (veyra.recordingState || {}).compare === true
                        text: running ? qsTr("停止对比片段") : qsTr("对比片段")
                        onClicked: running ? veyra.stopRecording("studio-compare") : veyra.navigate("compare")
                    }
                    VButton { objectName: "studio-screenshot"; icon: true; iconName: "camera"; tip: qsTr("截图"); enabled: veyra.hasSource; onClicked: veyra.takeScreenshot() }
                }
                // One per row: side by side their labels were cut off in the narrow column.
                VButton { objectName: "studio-layout"; Layout.fillWidth: true; ghost: true; iconName: "layers"; text: qsTr("布局"); onClicked: studio.openLayout() }
                VButton { objectName: "studio-outputs"; Layout.fillWidth: true; ghost: true; iconName: "settings"; text: qsTr("直播平台"); onClicked: veyra.navigate("stream") }
                VButton { objectName: "studio-folder"; Layout.fillWidth: true; ghost: true; iconName: "folder"; text: qsTr("录制文件夹"); onClicked: veyra.openRecordingDirectory() }

                // Outputs and their health (one line per platform while live).
                Text {
                    Layout.topMargin: 6
                    text: qsTr("直播输出")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                Text {
                    visible: (studio.stream.outputs || []).length === 0
                    Layout.fillWidth: true
                    text: studio.live ? qsTr("正在连接…") : qsTr("开播后这里显示每个平台的状态和码率")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: studio.stream.outputs || []
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 6
                        Rectangle {
                            width: 8; height: 8; radius: 4
                            color: modelData.state === "live" ? Theme.ok : modelData.state === "failed" ? Theme.err : Theme.warn
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                        Text {
                            text: Math.round(Number(modelData.kbps || 0)) + " kbps"
                            color: Theme.t3; font.family: Theme.fontMono; font.pixelSize: 11
                        }
                    }
                }
                Text {
                    visible: studio.stream.failed === true && String(studio.stream.error || "").length > 0
                    Layout.fillWidth: true
                    text: String(studio.stream.error || "")
                    color: Theme.err; font.family: Theme.fontUi; font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    // --- bottom: the audio mixer ----------------------------------------------------------
    Rectangle {
        objectName: "studio-mixer"
        x: studio.geometry.mixer.x; y: studio.geometry.mixer.y
        width: studio.geometry.mixer.width; height: studio.geometry.mixer.height
        radius: Theme.rCard; color: Theme.card; border.color: Theme.stroke
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6
            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("音频混音器")
                    color: Theme.t2; font.family: Theme.fontUi; font.pixelSize: 12
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                Text {
                    Layout.fillWidth: true
                    visible: !studio.compact
                    text: qsTr("游戏声音和麦克风的推子只影响录制和直播；监听只改变你听到的声音")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }
            GridLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                columns: 2
                columnSpacing: 8
                rowSpacing: 6
            PushMixerStrip {
                objectName: "mixer-game"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("游戏声音")
                subtitle: studio.cardMute ? qsTr("图卡场景显示时静音") : qsTr("采集卡或串流")
                iconName: "gamepad"
                from: -60; to: 12
                value: Number(veyra.preferences.mixGameDb ?? 0)
                muted: veyra.preferences.mixGameMuted === true || studio.cardMute
                rmsDb: studio.gameRms; peakDb: studio.gamePeak
                meterActive: veyra.hasSource
                gear: true
                gearTip: qsTr("音频设置")
                onMoved: v => veyra.setPreference("mixGameDb", Math.round(v))
                // Unmute means heard: a card's mute is lifted too (until the scene changes).
                onMuteToggled: {
                    const muted = veyra.preferences.mixGameMuted === true || studio.cardMute
                    if (studio.cardMute) studio.setCardMute(false)
                    veyra.setPreference("mixGameMuted", !muted)
                }
                onGearClicked: veyra.navigate("audio")
            }
            PushMixerStrip {
                objectName: "mixer-mic"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("麦克风")
                subtitle: !studio.micUsed ? qsTr("未用于录制或直播")
                        : studio.levels.micError ? String(studio.levels.micError)
                        : String(studio.levels.micDevice || qsTr("系统默认麦克风"))
                iconName: "activity"
                from: -60; to: 20
                value: Number(veyra.preferences.mixMicDb ?? 0)
                muted: veyra.preferences.mixMicMuted === true
                rmsDb: studio.micRms; peakDb: studio.micPeak
                meterActive: studio.micUsed && studio.levels.micOpen === true
                gear: true
                gearTip: qsTr("麦克风滤镜（均衡器、噪声门、压缩）")
                onMoved: v => veyra.setPreference("mixMicDb", Math.round(v))
                onMuteToggled: veyra.setPreference("mixMicMuted", !(veyra.preferences.mixMicMuted === true))
                onGearClicked: veyra.navigate("micfilters")
                // Where the microphone goes: recordings (recordMic) and streams (streamMic).
                VTag {
                    objectName: "mixer-mic-record"
                    text: qsTr("录制")
                    kind: veyra.recordingMicrophone ? "ok" : ""
                    opacity: veyra.recordingMicrophone ? 1 : 0.6
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: studio.toggleMicRecording() }
                }
                VTag {
                    objectName: "mixer-mic-stream"
                    text: qsTr("直播")
                    kind: veyra.preferences.streamMic === true ? "ok" : ""
                    opacity: veyra.preferences.streamMic === true ? 1 : 0.6
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: studio.toggleMicStreaming() }
                }
            }
            // Desktop audio: everything Windows plays except PushFPS (Discord, music, a browser).
            PushMixerStrip {
                objectName: "mixer-desktop"
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property bool on: veyra.preferences.mixDesktopOn === true
                title: qsTr("桌面音频")
                subtitle: !on ? qsTr("关闭：不进入录制和直播")
                        : String(studio.levels.desktopError || "").length ? String(studio.levels.desktopError)
                        : qsTr("除 PushFPS 外的所有声音")
                iconName: "monitor"
                from: -60; to: 12
                value: Number(veyra.preferences.mixDesktopDb ?? 0)
                muted: veyra.preferences.mixDesktopMuted === true
                rmsDb: studio.desktopRms; peakDb: studio.desktopPeak
                meterActive: on && studio.levels.desktopOpen === true
                opacity: on ? 1 : 0.75
                onMoved: v => veyra.setPreference("mixDesktopDb", Math.round(v))
                onMuteToggled: veyra.setPreference("mixDesktopMuted", !(veyra.preferences.mixDesktopMuted === true))
                VTag {
                    objectName: "mixer-desktop-on"
                    text: veyra.preferences.mixDesktopOn === true ? qsTr("开") : qsTr("关")
                    kind: veyra.preferences.mixDesktopOn === true ? "ok" : ""
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: veyra.setPreference("mixDesktopOn", !(veyra.preferences.mixDesktopOn === true)) }
                }
            }
            // Extra input: a second microphone, a line-in or an audio interface.
            PushMixerStrip {
                objectName: "mixer-aux"
                Layout.fillWidth: true
                Layout.fillHeight: true
                readonly property bool on: veyra.preferences.mixAuxOn === true
                title: qsTr("额外输入")
                subtitle: !on ? qsTr("关闭：不进入录制和直播")
                        : String(studio.levels.auxError || "").length ? String(studio.levels.auxError)
                        : String(studio.levels.auxDevice || qsTr("系统默认麦克风"))
                iconName: "activity"
                from: -60; to: 12
                value: Number(veyra.preferences.mixAuxDb ?? 0)
                muted: veyra.preferences.mixAuxMuted === true
                rmsDb: studio.auxRms; peakDb: studio.auxPeak
                meterActive: on && studio.levels.auxOpen === true
                opacity: on ? 1 : 0.75
                onMoved: v => veyra.setPreference("mixAuxDb", Math.round(v))
                onMuteToggled: veyra.setPreference("mixAuxMuted", !(veyra.preferences.mixAuxMuted === true))
                VSelect {
                    objectName: "mixer-aux-device"
                    implicitWidth: 130
                    options: [{ id: "", label: qsTr("系统默认麦克风") }].concat(veyra.microphoneDevices || [])
                    value: (options.find(o => o.id === String(veyra.preferences.mixAuxDevice || "")) || options[0]).label
                    onPicked: id => veyra.setPreference("mixAuxDevice", id)
                }
                VTag {
                    objectName: "mixer-aux-on"
                    text: veyra.preferences.mixAuxOn === true ? qsTr("开") : qsTr("关")
                    kind: veyra.preferences.mixAuxOn === true ? "ok" : ""
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: veyra.setPreference("mixAuxOn", !(veyra.preferences.mixAuxOn === true)) }
                }
            }
            PushMixerStrip {
                objectName: "mixer-monitor"
                Layout.fillWidth: true
                Layout.fillHeight: true
                title: qsTr("监听")
                subtitle: qsTr("只改变你听到的声音")
                iconName: "vol"
                decibels: false
                from: 0; to: 1
                value: Number(veyra.volume ?? 1)
                muted: veyra.muted === true
                meterActive: false
                onMoved: v => veyra.volume = v
                onMuteToggled: veyra.muted = !veyra.muted
            }
            }
        }
    }

    // --- status bar -----------------------------------------------------------------------
    RowLayout {
        objectName: "studio-status"
        x: studio.geometry.status.x; y: studio.geometry.status.y
        width: studio.geometry.status.width; height: studio.geometry.status.height
        spacing: 16
        StatusText {
            objectName: "studio-status-live"
            color: studio.live ? Theme.t1 : Theme.t3
            text: studio.live ? qsTr("直播 %1 · %2 Mbps · 丢帧 %3").arg(StudioGeometry.duration(studio.stream.seconds))
                                    .arg((studio.streamKbps / 1000).toFixed(1)).arg(Number(studio.stream.dropped || 0))
                              : qsTr("直播 未开始")
        }
        StatusText {
            color: studio.recordingNow ? Theme.t1 : Theme.t3
            text: studio.recordingNow ? qsTr("录制 %1 · 丢帧 %2").arg(StudioGeometry.duration(veyra.recordingState.seconds)).arg(Number(veyra.recordingState.dropped || 0))
                                      : qsTr("录制 未开始")
        }
        StatusText {
            visible: Number(studio.stats.latencyMs || 0) > 0
            text: qsTr("处理延迟 %1 ms").arg(Number(studio.stats.latencyMs || 0).toFixed(0))
        }
        Item { Layout.fillWidth: true }
        StatusText {
            visible: String(studio.stream.encoder || veyra.recordingState.encoder || "").length > 0
            text: String(studio.stream.encoder || veyra.recordingState.encoder || "")
        }
    }
}
