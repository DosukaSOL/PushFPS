// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Wide-screen fill in quick setup, the Windows row `pushfps-fill-row` with its words: bars, ambient fill
// (a soft, dimmed copy of the picture in the bars: portable/core Ambient.h, FrameView `ambient`), fill and
// crop, or stretch. Shown on a screen clearly wider than 16:9 (Fill.js), as on Windows; elsewhere the
// preview's own shape rows hold the same choices. The rules are Fill.js's; the picture's own fill is app.fillMode.
import QtQuick
import QtQuick.Window
import "Fill.js" as Fill

VRow {
    id: row
    objectName: "pushfps-fill-row"
    property real screenAspectOverride: 0   // tests
    readonly property real screenAspect: row.screenAspectOverride > 0 ? row.screenAspectOverride
                                         : (Screen.height > 0 ? Screen.width / Screen.height : 0)
    readonly property bool ambientOn: app.preferences.pushAmbientFill === true
    readonly property string fillId: Fill.current(app.fillMode, row.ambientOn)
    visible: Fill.shown(row.screenAspect)
    label: qsTr("宽屏填充")
    // The Windows help without its sentence on recordings and streams: the preview records nothing.
    help: qsTr("主机只输出 16:9 画面，带鱼屏两侧会空出来。这里选择怎么处理：留黑边、用画面本身的柔和延伸填满（氛围填充）、裁掉上下铺满，或者拉伸铺满。游戏本身不会显示更宽的视野")
    hint: {
        const crop = Fill.cropPercent(row.screenAspect), stretch = Fill.stretchPercent(row.screenAspect)
        if (row.fillId === "fit") return qsTr("主机输出的是 16:9 画面：完整显示，两侧留黑边")
        if (row.fillId === "ambient") return qsTr("完整显示，不裁切也不变形；黑边换成画面本身柔和、变暗的延伸")
        if (row.fillId === "crop") return crop > 0 ? qsTr("铺满屏幕，但上下共裁掉约 %1% 的画面，顶部和底部的 HUD 可能看不到").arg(crop)
                                                    : qsTr("铺满窗口，超出的部分被裁掉")
        return stretch > 0 ? qsTr("铺满屏幕，但画面被横向拉宽约 %1%").arg(stretch) : qsTr("铺满窗口，画面比例会改变")
    }
    hintBelow: true
    VSeg {
        objectName: "pushfps-fill"
        options: [{ id: "fit", label: qsTr("黑边") }, { id: "ambient", label: qsTr("氛围填充") },
                  { id: "crop", label: qsTr("裁切") }, { id: "stretch", label: qsTr("拉伸") }]
        current: row.fillId
        onPicked: id => Fill.choose(app, id)
    }
}
