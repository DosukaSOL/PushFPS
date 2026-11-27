// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Where the picture comes from: the capture card, a video file, the test picture. The same panel stands in
// the home page's left pane and in its one column; what it opens plays wherever the page shows the picture.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts

ColumnLayout {
    id: panel
    spacing: 12

    FileDialog {
        id: fileDialog
        title: qsTr("打开视频")
        nameFilters: [qsTr("视频文件") + " (*.mp4 *.mkv *.mov *.webm *.avi *.ts)"]
        onAccepted: app.startFile(selectedFile)
    }
    function openFile() { fileDialog.open() }

    // Errors stay until the next action.
    Rectangle {
        objectName: "home-error"
        visible: app.errorText.length > 0
        Layout.fillWidth: true
        radius: Theme.rCard
        color: Qt.rgba(Theme.err.r, Theme.err.g, Theme.err.b, 0.12)
        border.width: 1
        border.color: Theme.err
        implicitHeight: errorText.implicitHeight + 24
        Text {
            id: errorText
            x: 14; y: 12; width: parent.width - 52
            text: app.errorText
            color: Theme.t1
            wrapMode: Text.WordWrap
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsBody
        }
        VIcon {
            anchors.right: parent.right; anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            name: "x"; size: 14; color: Theme.t2
            TapHandler { onTapped: app.clearError() }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
        }
    }

    VCard {
        Layout.fillWidth: true
        PanelTitle { text: qsTr("采集卡"); hint: qsTr("HDMI 采集设备") }
        VRow {
            label: qsTr("设备")
            hintBelow: true
            VSelect {
                objectName: "home-device"
                implicitWidth: 190
                enabled: app.videoDevices.length > 0
                value: app.videoDevices.length > 0 ? app.videoDevices[app.videoDeviceIndex] || "" : qsTr("未检测到采集设备")
                options: app.videoDevices.map((name, i) => ({ id: String(i), label: name }))
                onPicked: id => app.videoDeviceIndex = Number(id)
            }
        }
        VRow {
            label: qsTr("格式")
            visible: app.formats.length > 0
            VSelect {
                objectName: "home-format"
                implicitWidth: 190
                value: app.formats[app.formatIndex] || ""
                options: app.formats.map((name, i) => ({ id: String(i), label: name }))
                onPicked: id => app.formatIndex = Number(id)
            }
        }
        VRow {
            label: qsTr("打开时自动连接")
            hint: qsTr("打开 PushFPS 时直接连接上次使用的采集卡")
            VSwitch { objectName: "home-autoconnect"; checked: app.autoConnect; onToggled: on => app.autoConnect = on }
        }
        // A Flow, not a row: where the two buttons do not fit beside each other (German in the narrow left pane)
        // Refresh goes under Connect instead of pushing the card, and every row in it, past the list's edge.
        // Right to left, so Connect stands at the right edge as it did.
        Flow {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8
            layoutDirection: Qt.RightToLeft
            VButton {
                objectName: "home-connect"
                text: qsTr("连接")
                primary: true
                enabled: app.videoDevices.length > 0
                onClicked: app.startCamera()
            }
            VButton { text: qsTr("刷新"); iconName: "refresh"; onClicked: app.refreshDevices() }
        }
    }

    VCard {
        Layout.fillWidth: true
        PanelTitle { text: qsTr("打开视频"); hint: "MP4 · MKV · MOV" }
        RowLayout {
            VButton { objectName: "home-open"; text: qsTr("选择文件"); iconName: "folder"; onClicked: panel.openFile() }
        }
    }

    VCard {
        Layout.fillWidth: true
        PanelTitle { text: qsTr("测试画面"); hint: qsTr("没有采集卡也能看看补帧效果") }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            VButton { objectName: "home-test"; text: qsTr("开始"); onClicked: app.startTestPattern(false) }
            VButton { text: qsTr("不均匀出帧"); onClicked: app.startTestPattern(true) }
        }
    }
}
