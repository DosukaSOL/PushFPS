// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Check my setup, as the Windows build's (PushDiagnoseDialog.qml): looks at the running picture and says
// what costs quality, smoothness or sound, with the one change that answers each. The rules are the
// Windows build's own file (PushDiagnose.js, shared as it is); this file reads the preview's values for
// them, has the words and the changes, and shows only the findings the preview has (`shown`): the rules
// about AI picture enhancement, NVIDIA VFG, PushFlow, FSR and the Windows latency settings never apply.
// Nothing changes until a button is pressed, every change can be undone from its row.
// The figures are read once a second, three times, and their mean is judged.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "PushDiagnose.js" as Diagnose

Popup {
    id: diag
    objectName: "pushfps-diagnose-dialog"
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, (parent ? parent.width : 640) - 40)
    height: Math.min(implicitHeight, (parent ? parent.height : 600) - 60)
    padding: 20
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property int sampleCount: 3
    property int sampleInterval: 1000
    property var samples: []
    property var found: []
    property var changed: ({})
    property string phase: "idle"   // idle | reading | done
    readonly property bool playing: app.running && Number(app.stats.framesRead || 0) > 0
    readonly property var counts: Diagnose.count(diag.found)
    // The findings this build has words and a change for.
    readonly property var shown: ["fg-behind", "upscale-off", "fg-off", "target-above-screen", "target-uneven",
                                  "capture-compressed", "no-sound", "latency-pacing-repair"]
    property real screenHighOverride: 0   // tests
    property real refreshHzOverride: 0    // tests
    readonly property int screenHigh: diag.screenHighOverride > 0 ? diag.screenHighOverride
                                      : Math.round(Screen.height * Math.max(1, Screen.devicePixelRatio))
    readonly property real refreshHz: diag.refreshHzOverride > 0 ? diag.refreshHzOverride : Screen.refreshRate > 1 ? Screen.refreshRate : 60

    // The preview's values under the names PushDiagnose.js reads. The preview shows a new picture at every
    // refresh up to the target, so the screen's rate capped by the target is what frame generation is asked
    // for, and what is shown beyond the game's own pictures is what it generated.
    function readState() {
        const game = Diagnose.mean(diag.samples, "contentFps", 0)
        const output = Diagnose.mean(diag.samples, "outputFps", 0)
        const target = app.targetMode === 1 ? 60 : app.targetMode === 2 ? 120 : app.targetMode === 4 ? 90 : 0
        const cap = target > 0 ? target : diag.refreshHz
        const comparing = app.compareMode !== 0   // frame generation pauses while comparing
        const sharper = app.sharperFormat()
        const camera = app.sourceKind === "camera"
        return {
            playing: diag.playing,
            kind: camera ? "capture" : String(app.sourceKind),
            fgPossible: !comparing,
            fgOn: app.targetMode !== 0 && !comparing,
            multiplier: game > 0 ? Math.max(2, Math.ceil(cap / game)) : 1,
            realFps: game,
            generatedFps: Math.max(0, output - game),
            outputCap: cap,
            refreshHz: diag.refreshHz,
            fgIdle: 0,
            pictureHeight: Number(app.stats.height || 0),
            screenHeight: diag.screenHigh,
            upscalePossible: app.upscaleState !== 3 && app.upscaleState !== 4,
            upscaleOn: app.srEnabled === true,
            targetFixed: target > 0,
            targetFps: target,
            vsync: true,        // Qt draws every picture at a refresh of the screen
            vrr: false,         // not known here
            sharperFps: camera && sharper.fps !== undefined ? Number(sharper.fps) : 0,
            sharperPixel: sharper.pixel !== undefined ? String(sharper.pixel) : "",
            audioChoice: camera && !app.audioPassthrough ? -1 : 0,
            audioSuggestion: camera && app.captureSoundFound() ? 0 : -1,
            pacingRepair: app.pacingRepair === true
        }
    }
    function judge() {
        diag.found = Diagnose.findings(diag.readState()).filter(f => diag.shown.indexOf(f.id) >= 0)
        diag.phase = "done"
    }
    function check() {
        diag.samples = []
        diag.found = []
        diag.changed = ({})
        if (!diag.playing) { diag.phase = "done"; return }
        diag.phase = "reading"
        diag.sample()
    }
    function sample() {
        const list = diag.samples.slice()
        list.push(Object.assign({}, app.stats))
        diag.samples = list
        if (list.length >= Math.max(1, diag.sampleCount)) diag.judge()
    }
    onOpened: diag.check()
    onClosed: { diag.phase = "idle"; diag.samples = [] }
    Timer {
        interval: Math.max(10, diag.sampleInterval); repeat: true
        running: diag.opened && diag.phase === "reading"
        onTriggered: diag.sample()
    }

    // --- words ---------------------------------------------------------------------------
    function levelName(level) {
        return level === "problem" ? qsTr("问题") : level === "better" ? qsTr("建议更改") : level === "try" ? qsTr("可以试试") : qsTr("提示")
    }
    function levelColor(level) { return level === "problem" ? Theme.warn : level === "note" ? Theme.t3 : Theme.accent }
    function titleOf(f) {
        switch (f.id) {
        case "fg-behind": return qsTr("显卡跟不上：每秒显示 %1 帧，目标是 %2 帧").arg(f.shown).arg(f.wanted)
        case "upscale-off": return qsTr("画面是 %1p，屏幕是 %2p").arg(f.height).arg(f.screen)
        case "fg-off": return qsTr("补帧没有开")
        case "target-above-screen": return qsTr("目标是 %1 FPS，这块屏幕是 %2 Hz").arg(f.target).arg(f.hz)
        case "target-uneven": return qsTr("目标 %1 FPS 除不尽屏幕的 %2 Hz").arg(f.target).arg(f.hz)
        case "capture-compressed": return qsTr("采集卡送来的是压缩画面")
        case "no-sound": return qsTr("没有选声音输入")
        case "latency-pacing-repair": return qsTr("帧节奏修复开着")
        }
        return f.id
    }
    function bodyOf(f) {
        switch (f.id) {
        // The preview's own words where the Windows ones name what it does not have.
        case "fg-behind": return app.quality === 1
            ? qsTr("生成的画面来不及做完，看起来不流畅。可以把运动估计改为「快速」，或降低目标帧率")
            : qsTr("生成的画面来不及做完，看起来不流畅。可以降低目标帧率")
        case "upscale-off": return qsTr("全屏时画面只是被拉大到屏幕大小，所以看起来发软。开启放大后由显卡把画面放大到屏幕分辨率，更清晰，也更吃显卡；帧率掉了就关掉")
        case "fg-off": return qsTr("现在看到的是主机原本的帧率。在快速设置里选一个目标帧率就会开启")
        case "target-above-screen": return qsTr("屏幕显示不了更多，每秒最多显示 %1 帧。可以在系统的显示设置里把刷新率调高，或换一个目标").arg(f.hz)
        case "target-uneven": return qsTr("每帧在屏幕上停留的时间不一样长，看起来有顿挫。选一个能整除刷新率的目标，或开启可变刷新率（G-SYNC / FreeSync）")
        case "capture-compressed": return qsTr("在 %1 FPS 下它有不压缩的格式（%2），更清晰；代价是采集帧率变成 %1。60 FPS 的游戏会少一半画面，30 FPS 的游戏没有损失").arg(f.fps).arg(f.pixel)
        case "no-sound": return qsTr("这张采集卡自己的声音输入已经找到，但「播放采集卡声音」是关着的。开启后就能听到主机的声音")
        case "latency-pacing-repair": return qsTr("它把画面重新对齐到稳定的节奏，最多晚一次采集刷新。帧率稳定的游戏关掉它，延迟更低")
        }
        return ""
    }
    function buttonsOf(f) {
        switch (f.id) {
        case "fg-behind": return app.quality === 1 ? [{ label: qsTr("改用「快速」运动估计") }] : []
        case "upscale-off": return [{ label: qsTr("开启放大") }]
        case "capture-compressed": return [{ label: qsTr("改用这个格式") }]
        case "no-sound": return [{ label: qsTr("开启声音") }]
        case "latency-pacing-repair": return [{ label: qsTr("关闭帧节奏修复") }]
        }
        return []
    }

    // --- changes -------------------------------------------------------------------------
    function remember(id, before) { const m = Object.assign({}, diag.changed); m[id] = before; diag.changed = m }
    function forget(id) { const m = Object.assign({}, diag.changed); delete m[id]; diag.changed = m }
    // As UpscaleRow.qml's setUpscale: 2K for a screen up to 1440 lines, 4K above, only between those two.
    function upscaleTarget(now) {
        if (!(diag.screenHigh > 0) || (now !== 1 && now !== 2)) return 0
        const want = diag.screenHigh <= 1440 ? 1 : 2
        return want === now ? 0 : want
    }
    function apply(f) {
        switch (f.id) {
        case "fg-behind":
            diag.remember(f.id, app.quality)
            app.quality = 0
            break
        case "upscale-off": {
            diag.remember(f.id, { target: app.srTargetIndex, algorithm: app.videoSrQuality })
            const size = diag.upscaleTarget(app.srTargetIndex)
            if (size > 0) app.srTargetIndex = size
            app.videoSrQuality = 6
            app.srEnabled = true
            break
        }
        case "capture-compressed": {
            const s = app.sharperFormat()
            if (s.index === undefined) break
            diag.remember(f.id, app.formatIndex)
            app.formatIndex = Number(s.index)
            app.startCamera()
            break
        }
        case "no-sound":
            diag.remember(f.id, false)
            app.audioPassthrough = true
            break
        case "latency-pacing-repair":
            diag.remember(f.id, true)
            app.pacingRepair = false
            break
        }
    }
    function undo(f) {
        const before = diag.changed[f.id]
        if (before === undefined) return
        if (f.id === "fg-behind") app.quality = Number(before)
        else if (f.id === "upscale-off") {
            app.srEnabled = false
            app.srTargetIndex = before.target
            app.videoSrQuality = before.algorithm
        }
        else if (f.id === "capture-compressed") { app.formatIndex = Number(before); app.startCamera() }
        else if (f.id === "no-sound") app.audioPassthrough = false
        else if (f.id === "latency-pacing-repair") app.pacingRepair = true
        diag.forget(f.id)
    }

    // --- what is shown -------------------------------------------------------------------
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.45) }
    background: Rectangle {
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke
    }
    contentItem: Flickable {
        implicitHeight: body.implicitHeight
        contentHeight: body.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: VScrollBar {}
        ColumnLayout {
            id: body
            width: parent.width
            spacing: 0
            Text {
                text: qsTr("检查我的设置")
                color: Theme.t1
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsH3
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.bottomMargin: 14
                wrapMode: Text.WordWrap
                text: qsTr("看看正在运行的画面里，是什么在影响清晰度、流畅度和声音。只有按下按钮才会改动设置")
                color: Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsBody
            }
            Text {
                objectName: "pushfps-diagnose-state"
                Layout.fillWidth: true
                Layout.bottomMargin: 10
                wrapMode: Text.WordWrap
                color: Theme.t1
                font.family: Theme.fontUi
                font.pixelSize: 13
                text: !diag.playing ? qsTr("先让画面播放起来（连接主机），再回来检查：这里看的是正在运行的画面")
                    : diag.phase === "reading" ? qsTr("正在读取数据…")
                    : diag.found.length === 0 ? qsTr("没有发现需要改的地方")
                    : diag.counts.act > 0 ? qsTr("发现 %1 处可以改进").arg(diag.counts.act)
                    : qsTr("没有需要改的地方，有 %1 条提示").arg(diag.counts.note)
            }
            Text {
                objectName: "pushfps-diagnose-checked"
                Layout.fillWidth: true
                Layout.bottomMargin: 6
                visible: diag.playing && diag.phase === "done" && diag.found.length === 0
                wrapMode: Text.WordWrap
                color: Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: 12
                text: qsTr("检查了：补帧是否跟得上、放大、目标帧率与屏幕、采集格式、声音和帧节奏修复")
            }
            Repeater {
                model: diag.phase === "done" ? diag.found : []
                delegate: Rectangle {
                    id: item
                    required property var modelData
                    readonly property var finding: modelData
                    readonly property bool done: diag.changed[finding.id] !== undefined
                    readonly property var buttons: diag.buttonsOf(finding)
                    objectName: "pushfps-diagnose-item-" + finding.id
                    Layout.fillWidth: true
                    Layout.bottomMargin: 8
                    implicitHeight: column.implicitHeight + 24
                    radius: 12
                    color: Theme.card2
                    border.width: 1
                    border.color: Theme.stroke
                    ColumnLayout {
                        id: column
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Rectangle {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 1
                                implicitWidth: levelText.implicitWidth + 14
                                implicitHeight: 20
                                radius: 10
                                color: "transparent"
                                border.width: 1
                                border.color: diag.levelColor(item.finding.level)
                                Text {
                                    id: levelText
                                    anchors.centerIn: parent
                                    text: diag.levelName(item.finding.level)
                                    color: diag.levelColor(item.finding.level)
                                    font.family: Theme.fontUi
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                text: diag.titleOf(item.finding)
                                color: Theme.t1
                                font.family: Theme.fontUi
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: diag.bodyOf(item.finding)
                            color: Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsBody
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 2
                            visible: item.buttons.length > 0
                            spacing: 8
                            Text {
                                Layout.fillWidth: true
                                text: item.done ? qsTr("已更改") : ""
                                color: Theme.ok
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                            }
                            VButton {
                                objectName: "pushfps-diagnose-undo-" + item.finding.id
                                visible: item.done
                                ghost: true
                                text: qsTr("撤销")
                                onClicked: diag.undo(item.finding)
                            }
                            Repeater {
                                model: item.done ? [] : item.buttons
                                delegate: VButton {
                                    required property var modelData
                                    required property int index
                                    objectName: "pushfps-diagnose-fix-" + item.finding.id + "-" + index
                                    primary: index === 0 && item.finding.level !== "try"
                                    text: modelData.label
                                    onClicked: diag.apply(item.finding)
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                spacing: 8
                Item { Layout.fillWidth: true }
                VButton { objectName: "pushfps-diagnose-close"; text: qsTr("关闭"); onClicked: diag.close() }
                VButton {
                    objectName: "pushfps-diagnose-again"
                    text: qsTr("重新检查")
                    primary: diag.found.length === 0
                    enabled: diag.phase !== "reading"
                    onClicked: diag.check()
                }
            }
        }
    }
}
