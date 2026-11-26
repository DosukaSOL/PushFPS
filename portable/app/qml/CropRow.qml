// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Crop and resize in quick setup, the Windows row `pushfps-crop-row` with its words: the button opens the
// editor (CropDialog.qml, through app.openDialog), the switch shows the whole picture again without losing
// the values (preference pushCropOn; the values and their maths are PushCrop.js's).
import QtQuick
import QtQuick.Layouts
import "PushCrop.js" as Crop

VRow {
    id: row
    objectName: "pushfps-crop-row"
    readonly property var cropState: Crop.read(app.preferences)
    label: qsTr("裁剪和缩放")
    // The Windows help without its sentence on recordings and streams: the preview records nothing.
    help: qsTr("裁掉画面的任意一边，例如游戏自带的黑边或采集卡多出来的边框，并调整画面在屏幕上的大小和位置。只改变屏幕上的显示")
    hint: Crop.neutral(cropState) ? qsTr("显示完整画面")
        : !cropState.on ? qsTr("已关闭，设置保留")
        : qsTr("左 %1% · 上 %2% · 右 %3% · 下 %4% · 大小 %5%").arg(cropState.l / 10).arg(cropState.t / 10).arg(cropState.r / 10).arg(cropState.b / 10).arg(cropState.zoom)
    hintBelow: true
    RowLayout {
        spacing: 8
        VButton {
            objectName: "pushfps-crop-edit"
            ghost: true
            text: qsTr("调整…")
            onClicked: app.openDialog("crop")
        }
        VSwitch {
            objectName: "pushfps-crop"
            enabled: !Crop.neutral(row.cropState)
            opacity: enabled ? 1 : 0.4
            checked: row.cropState.on
            onToggled: checked => app.setPreference("pushCropOn", checked)
        }
    }
}
