// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS: after a recording is saved, a small card offers the next step: edit it
// in the player (picture, sound, trim, then export), send it straight to the export
// queue, or show it in its folder. It goes away after 12 s or with ×.
import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    // Main supplies its page switch, so this file stays free of Main's ids.
    property var goPage: null
    property string path: ""
    property real lastSaved: -1
    property bool on: false
    readonly property string fileName: path.split(/[\\/]/).pop()

    objectName: "pushfps-recording-saved"
    // Over the picture: cut out of the native video window (main.cpp collects covers).
    readonly property bool videoCover: card.visible
    readonly property real coverRadius: 14
    width: Math.min(420, parent ? parent.width - 48 : 420)
    height: column.implicitHeight + 24
    radius: 14
    color: Theme.card
    border.width: 1
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
    opacity: on ? 1 : 0
    visible: opacity > 0.01
    Behavior on opacity { NumberAnimation { duration: Theme.d(200) } }
    transform: Translate { y: card.on ? 0 : 10; Behavior on y { NumberAnimation { duration: Theme.d(450); easing.bezierCurve: Theme.spring } } }

    Connections {
        target: veyra
        function onRecordingChanged() {
            const s = veyra.recordingState || {}
            const saved = Number(s.saved ?? 0)
            if (card.lastSaved < 0) { card.lastSaved = saved; return }   // first state after start: no card
            if (saved > card.lastSaved && !s.active && String(s.path || "").length > 0) {
                card.path = s.path
                card.on = true
                hideTimer.restart()
            }
            card.lastSaved = saved
        }
    }
    Component.onCompleted: lastSaved = Number((veyra.recordingState || {}).saved ?? 0)
    Timer { id: hideTimer; interval: 12000; onTriggered: card.on = false }
    HoverHandler { onHoveredChanged: if (hovered) hideTimer.stop(); else if (card.on) hideTimer.restart() }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        RowLayout {
            spacing: 10
            Rectangle {
                implicitWidth: 30; implicitHeight: 30; radius: 9
                color: Theme.accentSoft
                VIcon { anchors.centerIn: parent; name: "film"; size: 15; color: Theme.accent }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text { text: qsTr("录制已保存"); color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: Theme.fsBody; font.weight: Font.DemiBold }
                Text { Layout.fillWidth: true; text: card.fileName; color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: Theme.fsHint; elide: Text.ElideMiddle }
            }
            VButton {
                objectName: "pushfps-recording-saved-close"
                icon: true; iconName: "x"; ghost: true
                tip: qsTr("关闭")
                onClicked: card.on = false
            }
        }
        RowLayout {
            spacing: 6
            VButton {
                objectName: "pushfps-recording-edit"
                primary: true
                text: qsTr("编辑")
                iconName: "sliders"
                onClicked: {
                    card.on = false
                    veyra.openPath(card.path)
                    if (card.goPage) card.goPage("pro")
                }
            }
            VButton {
                objectName: "pushfps-recording-export"
                text: qsTr("加入导出")
                iconName: "upload"
                onClicked: {
                    card.on = false
                    veyra.addExportFiles([card.path])
                    if (card.goPage) card.goPage("exp")
                }
            }
            VButton {
                objectName: "pushfps-recording-folder"
                ghost: true
                text: qsTr("打开文件夹")
                iconName: "folder"
                onClicked: { card.on = false; veyra.openRecordingDirectory() }
            }
        }
        Text {
            Layout.fillWidth: true
            visible: veyra.fgEnabled === true
            text: qsTr("录制已包含补帧。导出前请在专业模式中关闭补帧，以免重复补帧。")
            color: Theme.t3
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsHint
            wrapMode: Text.WordWrap
        }
    }
}
