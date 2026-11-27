// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// HUD protection, as the Windows build's PushHudDialog.qml (its words, its object names and its logic):
// "Detect automatically" watches the playing picture for a few seconds (the Windows detector, through
// HudDetection.h) and proposes up to four boxes; the player moves, resizes, adds or deletes boxes on a
// frame of their own game, then applies them (app.setHudRegions), so frame generation leaves those parts
// alone (FrameView protect). Opened with the boxes already set, so it also edits what a game preset set.
// A Popup, as CropDialog.qml: the Windows dialog host is not in the preview. The frame shown is the
// engine's newest real frame (image://pushfps-frame), taken when the dialog opens and after a detection.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: hud
    objectName: "pushfps-hud-dialog"
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(760, (parent ? parent.width : 760) - 40)
    height: Math.min(implicitHeight, (parent ? parent.height : 700) - 60)
    padding: 20
    leftPadding: 13
    rightPadding: 13
    closePolicy: hud.phase === "watching" ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property int maxBoxes: 4
    readonly property real minSize: 0.02
    // idle or watching (detecting).
    property string phase: "idle"
    property var lastState: ({})
    property string picture: ""
    property int pictureSerial: 0
    property int selected: -1
    property string message: ""
    property bool messageError: false
    property real startedAt: 0
    property real feather: 8
    // Tests replace the bridge calls' timing: no timeout while false.
    property bool timeoutEnabled: true
    // Tests set the picture's shape directly.
    property real aspectOverride: 0
    readonly property real frameAspect: hud.aspectOverride > 0.2 ? hud.aspectOverride
                                      : shot.status === Image.Ready && shot.implicitHeight > 0 ? shot.implicitWidth / shot.implicitHeight : 16 / 9
    readonly property bool hasSource: app.running

    ListModel { id: boxModel }
    readonly property alias boxes: boxModel

    function clamp01(v) { const n = Number(v); return isFinite(n) ? Math.max(0, Math.min(1, n)) : 0 }
    function say(text, error) { hud.message = text; hud.messageError = error === true }
    function regions() {
        const out = []
        for (let i = 0; i < boxModel.count; ++i) {
            const b = boxModel.get(i)
            out.push({ left: b.l, top: b.t, right: b.r, bottom: b.b, ellipse: b.ellipse === true })
        }
        return out
    }
    function setBoxes(list) {
        boxModel.clear()
        for (const r of (list || []).slice(0, hud.maxBoxes)) {
            const l = hud.clamp01(Math.min(r.left, r.right)), rr = hud.clamp01(Math.max(r.left, r.right))
            const t = hud.clamp01(Math.min(r.top, r.bottom)), b = hud.clamp01(Math.max(r.top, r.bottom))
            if (rr - l < 0.005 || b - t < 0.005) continue
            boxModel.append({ l: l, t: t, r: rr, b: b, ellipse: r.ellipse === true })
        }
        hud.selected = -1
    }
    function loadCurrent() {
        hud.feather = Number(app.hudFeather)
        hud.setBoxes(app.hudRegions)
    }
    function addBox() {
        if (boxModel.count >= hud.maxBoxes) return -1
        boxModel.append({ l: 0.4, t: 0.42, r: 0.6, b: 0.58, ellipse: false })
        hud.selected = boxModel.count - 1
        return hud.selected
    }
    function removeBox(i) {
        if (i < 0 || i >= boxModel.count) return
        boxModel.remove(i)
        hud.selected = -1
    }
    function clearBoxes() { boxModel.clear(); hud.selected = -1 }
    // Moves box i so its top-left corner is at (l, t), keeping its size inside the picture.
    function moveBox(i, l, t) {
        if (i < 0 || i >= boxModel.count) return
        const b = boxModel.get(i)
        const w = b.r - b.l, h = b.b - b.t
        const nl = Math.max(0, Math.min(1 - w, Number(l))), nt = Math.max(0, Math.min(1 - h, Number(t)))
        boxModel.set(i, { l: nl, t: nt, r: nl + w, b: nt + h })
    }
    // Drags corner c (0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right) of box i to (x, y).
    function resizeBox(i, c, x, y) {
        if (i < 0 || i >= boxModel.count) return
        const b = boxModel.get(i)
        x = hud.clamp01(x); y = hud.clamp01(y)
        const next = { l: b.l, t: b.t, r: b.r, b: b.b }
        if (c === 0 || c === 2) next.l = Math.min(x, b.r - hud.minSize); else next.r = Math.max(x, b.l + hud.minSize)
        if (c === 0 || c === 1) next.t = Math.min(y, b.b - hud.minSize); else next.b = Math.max(y, b.t + hud.minSize)
        boxModel.set(i, next)
    }

    function takePicture() {
        hud.pictureSerial += 1
        hud.picture = hud.hasSource ? "image://pushfps-frame/hud-" + hud.pictureSerial : ""
    }
    function detect() {
        if (!hud.hasSource) { hud.say(qsTr("请先连接采集卡并开始播放"), true); return }
        if (!app.startHudDetection(12)) return
        hud.phase = "watching"
        hud.startedAt = Date.now()
        hud.say(qsTr("正在观察画面，大约 5 秒。请在游戏里转动镜头或走动"), false)
        poll.start()
    }
    function cancel() {
        app.cancelHudDetection()
        poll.stop()
        hud.phase = "idle"
        hud.say(qsTr("已取消"), false)
    }
    function pollOnce() {
        const s = app.hudDetection()
        hud.lastState = s
        if (s.failed) { poll.stop(); hud.phase = "idle"; hud.say(s.error || qsTr("HUD 识别失败"), true); return }
        if (s.done) {
            poll.stop()
            const watching = hud.phase === "watching"
            hud.phase = "idle"
            hud.takePicture()
            if (watching) hud.finishDetection(s)
            return
        }
        if (hud.timeoutEnabled && Date.now() - hud.startedAt > 15000 && !(s.taken > 0)) {
            const watching = hud.phase === "watching"
            app.cancelHudDetection()
            poll.stop()
            hud.phase = "idle"
            if (watching) hud.say(qsTr("没有收到画面。请确认画面正在播放，并且 PushFPS 窗口没有最小化"), true)
        }
    }
    function finishDetection(s) {
        if (s.tooStill) { hud.say(qsTr("画面几乎没有动，分不出哪些是 HUD。请在游戏里转动镜头或移动，然后再试一次"), true); return }
        const found = Array.isArray(s.regions) ? s.regions : []
        if (found.length === 0) { hud.say(qsTr("没有找到 HUD。可以在画面上拖动，手动画出方框"), false); return }
        hud.setBoxes(found)
        hud.say(qsTr("找到 %1 个 HUD 区域。请检查绿色方框，必要时拖动修正，然后点「应用」").arg(boxModel.count), false)
    }
    function apply() {
        if (!app.setHudRegions(hud.regions(), hud.feather)) { hud.say(qsTr("HUD 保护区域没有应用"), true); return false }
        hud.say(boxModel.count > 0 ? qsTr("已应用：%1 个区域保持主机原画").arg(boxModel.count)
                                   : qsTr("已关闭 HUD 保护，整个画面都补帧"), false)
        return true
    }

    Timer { id: poll; interval: 200; repeat: true; onTriggered: hud.pollOnce() }

    onAboutToShow: {
        hud.message = ""
        hud.loadCurrent()
        hud.takePicture()
    }
    onAboutToHide: {
        if (hud.phase !== "idle") app.cancelHudDetection()
        poll.stop()
        hud.phase = "idle"
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.45) }
    background: Rectangle {
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke
    }
    // RowList keeps a row's hover light inside the list (its `room` comes out of this dialog's side padding).
    contentItem: RowList {
        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 0
            Text {
                text: qsTr("HUD 保护")
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
                text: qsTr("小地图、血量和弹药等不动的部分保持主机原画，不补帧，所以不会糊。自动识别后可以拖动方框修正")
                color: Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsBody
            }

            // The picture with the boxes.
            Item {
                id: stageFrame
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(400, width / hud.frameAspect)
                Item {
                    id: stage
                    objectName: "pushfps-hud-stage"
                    width: Math.min(parent.width, parent.height * hud.frameAspect)
                    height: width / hud.frameAspect
                    anchors.centerIn: parent
                    Rectangle {
                        anchors.fill: parent
                        radius: 8
                        color: "#000000"
                        border.width: 1
                        border.color: Theme.stroke2
                    }
                    Image {
                        id: shot
                        objectName: "pushfps-hud-picture"
                        anchors.fill: parent
                        anchors.margins: 1
                        source: hud.picture
                        cache: false
                        fillMode: Image.Stretch
                    }
                    Text {
                        anchors.centerIn: parent
                        width: parent.width - 60
                        horizontalAlignment: Text.AlignHCenter
                        visible: shot.status !== Image.Ready
                        text: hud.hasSource ? qsTr("正在获取画面…") : qsTr("没有画面。连接采集卡并开始播放后，可以在你自己的游戏画面上调整方框")
                        color: Theme.onVideo
                        opacity: 0.7
                        font.family: Theme.fontUi
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    // Drag on an empty spot to draw a new box.
                    MouseArea {
                        id: drawArea
                        objectName: "pushfps-hud-draw"
                        anchors.fill: parent
                        enabled: hud.phase !== "watching"
                        // Inside the dialog's Flickable: a drag draws instead of scrolling.
                        preventStealing: true
                        property int drawing: -1
                        property real ox: 0
                        property real oy: 0
                        cursorShape: boxModel.count < hud.maxBoxes ? Qt.CrossCursor : Qt.ArrowCursor
                        onPressed: m => {
                            hud.selected = -1
                            if (boxModel.count >= hud.maxBoxes) { hud.say(qsTr("最多 4 个方框，请先删除一个"), true); return }
                            ox = hud.clamp01(m.x / width); oy = hud.clamp01(m.y / height)
                            boxModel.append({ l: ox, t: oy, r: ox, b: oy, ellipse: false })
                            drawing = boxModel.count - 1
                            hud.selected = drawing
                        }
                        onPositionChanged: m => {
                            if (drawing < 0) return
                            const x = hud.clamp01(m.x / width), y = hud.clamp01(m.y / height)
                            boxModel.set(drawing, { l: Math.min(ox, x), t: Math.min(oy, y), r: Math.max(ox, x), b: Math.max(oy, y) })
                        }
                        onReleased: {
                            if (drawing < 0) return
                            const b = boxModel.get(drawing)
                            if (b.r - b.l < 0.015 || b.b - b.t < 0.015) { boxModel.remove(drawing); hud.selected = -1 }
                            drawing = -1
                        }
                    }
                    Repeater {
                        model: boxModel
                        delegate: Rectangle {
                            id: box
                            required property int index
                            required property real l
                            required property real t
                            required property real r
                            required property real b
                            required property bool ellipse
                            readonly property bool chosen: hud.selected === index
                            objectName: "pushfps-hud-box-" + index
                            x: l * stage.width
                            y: t * stage.height
                            width: Math.max(2, (r - l) * stage.width)
                            height: Math.max(2, (b - t) * stage.height)
                            radius: ellipse ? Math.min(width, height) / 2 : 3
                            color: Qt.rgba(0.537, 0.961, 0.165, chosen ? 0.30 : 0.18)
                            border.width: chosen ? 2 : 1
                            border.color: Theme.brandGreen
                            MouseArea {
                                objectName: "pushfps-hud-box-move"
                                anchors.fill: parent
                                enabled: hud.phase !== "watching"
                                preventStealing: true
                                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                property real gx: 0
                                property real gy: 0
                                property real sl: 0
                                property real st: 0
                                onPressed: m => {
                                    hud.selected = box.index
                                    const p = mapToItem(stage, m.x, m.y)
                                    gx = p.x; gy = p.y; sl = box.l; st = box.t
                                }
                                onPositionChanged: m => {
                                    if (!pressed) return
                                    const p = mapToItem(stage, m.x, m.y)
                                    hud.moveBox(box.index, sl + (p.x - gx) / stage.width, st + (p.y - gy) / stage.height)
                                }
                            }
                            Text {
                                x: 5; y: 3
                                text: String(box.index + 1)
                                color: Theme.onVideo
                                font.family: Theme.fontUi
                                font.pixelSize: 11
                                font.weight: Font.Bold
                                style: Text.Outline
                                styleColor: "#000000"
                            }
                            // Corner handles of the selected box.
                            Repeater {
                                model: box.chosen && hud.phase !== "watching" ? 4 : 0
                                delegate: Rectangle {
                                    id: handle
                                    required property int index
                                    objectName: "pushfps-hud-handle-" + index
                                    width: 12; height: 12; radius: 3
                                    x: (index % 2 ? box.width : 0) - 6
                                    y: (index >= 2 ? box.height : 0) - 6
                                    color: Theme.brandGreen
                                    border.width: 1
                                    border.color: "#0D1A04"
                                    MouseArea {
                                        anchors.fill: parent
                                        anchors.margins: -5
                                        preventStealing: true
                                        cursorShape: handle.index === 0 || handle.index === 3 ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
                                        onPositionChanged: m => {
                                            if (!pressed) return
                                            const p = mapToItem(stage, m.x, m.y)
                                            hud.resizeBox(box.index, handle.index, p.x / stage.width, p.y / stage.height)
                                        }
                                    }
                                }
                            }
                            // Delete button of the selected box.
                            Rectangle {
                                objectName: "pushfps-hud-box-delete"
                                visible: box.chosen && hud.phase !== "watching"
                                width: 20; height: 20; radius: 10
                                x: box.width - 10
                                y: box.y > 24 ? -24 : box.height + 4
                                color: Theme.card3
                                border.width: 1
                                border.color: Theme.stroke2
                                VIcon { anchors.centerIn: parent; name: "x"; size: 12; color: Theme.t1 }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: hud.removeBox(box.index) }
                            }
                        }
                    }
                    // While watching: progress over the picture.
                    Rectangle {
                        objectName: "pushfps-hud-watching"
                        anchors.fill: parent
                        visible: hud.phase === "watching"
                        radius: 8
                        color: Qt.rgba(0, 0, 0, 0.55)
                        Column {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("正在观察画面 %1 / %2").arg(hud.lastState.taken || 0).arg(hud.lastState.target || 12)
                                color: Theme.onVideo
                                font.family: Theme.fontUi
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: qsTr("转动镜头或走动，HUD 不动，其余画面在动")
                                color: Theme.onVideo
                                opacity: 0.75
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                            }
                        }
                    }
                }
            }

            RowLayout {
                objectName: "pushfps-hud-tools"
                Layout.fillWidth: true
                Layout.topMargin: 10
                spacing: 6
                VButton {
                    objectName: "pushfps-hud-detect"
                    primary: true
                    iconName: "scan"
                    text: qsTr("自动识别")
                    tip: qsTr("观察画面几秒，找出不动的 HUD")
                    enabled: hud.hasSource && hud.phase !== "watching"
                    onClicked: hud.detect()
                }
                VButton {
                    objectName: "pushfps-hud-add"
                    ghost: true
                    iconName: "plus"
                    text: qsTr("添加方框")
                    enabled: boxModel.count < hud.maxBoxes && hud.phase !== "watching"
                    onClicked: hud.addBox()
                }
                VButton {
                    objectName: "pushfps-hud-clear"
                    ghost: true
                    iconName: "trash"
                    text: qsTr("全部清除")
                    enabled: boxModel.count > 0 && hud.phase !== "watching"
                    onClicked: hud.clearBoxes()
                }
                Item { Layout.fillWidth: true }
                VButton {
                    objectName: "pushfps-hud-refresh"
                    ghost: true
                    icon: true
                    iconName: "refresh"
                    tip: qsTr("重新获取画面")
                    enabled: hud.hasSource && hud.phase === "idle"
                    onClicked: hud.takePicture()
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: qsTr("在画面上拖动可以画新方框（最多 4 个）。拖动方框可以移动，拖动四个角可以调整大小")
                color: Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                objectName: "pushfps-hud-message"
                Layout.fillWidth: true
                Layout.topMargin: 6
                visible: hud.message.length > 0
                text: hud.message
                color: hud.messageError ? Theme.warn : Theme.accent
                font.family: Theme.fontUi
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12
                spacing: 8
                Item { Layout.fillWidth: true }
                VButton {
                    objectName: "pushfps-hud-cancel"
                    visible: hud.phase === "watching"
                    text: qsTr("取消")
                    onClicked: hud.cancel()
                }
                VButton {
                    objectName: "pushfps-hud-close"
                    visible: hud.phase !== "watching"
                    text: qsTr("关闭")
                    onClicked: hud.close()
                }
                VButton {
                    objectName: "pushfps-hud-apply"
                    visible: hud.phase !== "watching"
                    primary: true
                    text: qsTr("应用")
                    onClicked: hud.apply()
                }
            }
        }
    }
}
