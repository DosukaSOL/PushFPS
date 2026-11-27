// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// How the picture is drawn: its shape in the window and the four picture controls. (Upscaling's switch is
// in quick setup's Picture section, as on Windows: UpscaleRow.qml.)
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import "Fill.js" as Fill

ColumnLayout {
    id: picture
    spacing: 0
    Layout.fillWidth: true

    // The picture's shape in the window. Professional mode has it in its Display tab, as on Windows.
    property bool showShape: true

    component Control: VRow {
        id: row
        property real from: 0
        property real to: 1
        property real neutral: 0
        property real current: 0
        signal changed(real v)
        value: current.toFixed(2)
        VSlider {
            implicitWidth: 150
            from: row.from; to: row.to
            center: Math.abs((row.neutral - row.from) - (row.to - row.neutral)) < 1e-6
            value: row.current
            onMoved: v => row.changed(v)
        }
    }

    // The preview's own shape row. On a wide screen quick setup's Wide-screen fill (FillRow.qml, the Windows
    // row) is shown with the same choices, so this one stands aside: each setting once a page. Which of the
    // two is shown depends on the screen only, never on the choice, so the row never moves while it is used.
    VRow {
        objectName: "home-fill-row"
        visible: picture.showShape && !Fill.shown(Screen.height > 0 ? Screen.width / Screen.height : 0)
        label: qsTr("画面比例")
        hint: Fill.current(app.fillMode, app.preferences.pushAmbientFill === true) === "ambient"
              ? qsTr("完整显示，不裁切也不变形；黑边换成画面本身柔和、变暗的延伸")
              : qsTr("窗口和画面形状不同时：保留黑边、裁掉两侧铺满，或拉伸铺满")
        hintBelow: true
        VSeg {
            objectName: "home-fill"
            // Ambient fill too (Fill.js), so it can be chosen on a screen where the Windows row stays hidden.
            options: [{ id: "fit", label: qsTr("黑边") }, { id: "ambient", label: qsTr("氛围填充") },
                      { id: "crop", label: qsTr("裁切铺满") }, { id: "stretch", label: qsTr("拉伸") }]
            current: Fill.current(app.fillMode, app.preferences.pushAmbientFill === true)
            onPicked: id => Fill.choose(app, id)
        }
    }
    Control { objectName: "picture-brightness"; label: qsTr("亮度"); from: -0.5; to: 0.5; neutral: 0; current: app.brightness; onChanged: v => app.brightness = v }
    Control { objectName: "picture-contrast"; label: qsTr("对比度"); from: 0.5; to: 1.5; neutral: 1; current: app.contrast; onChanged: v => app.contrast = v }
    Control { objectName: "picture-saturation"; label: qsTr("饱和度"); from: 0; to: 2; neutral: 1; current: app.saturation; onChanged: v => app.saturation = v }
    Control { objectName: "picture-gamma"; label: qsTr("伽马"); from: 0.5; to: 2; neutral: 1; current: app.gamma; onChanged: v => app.gamma = v }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        Item { Layout.fillWidth: true }
        VButton { objectName: "picture-reset"; text: qsTr("重置"); iconName: "reset"; onClicked: app.resetPicture() }
    }
}
