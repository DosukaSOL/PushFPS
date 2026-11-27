// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Frame generation and what it waits for. Used by the home page's quick setup, which shows them in the
// Windows build's sections (`part`: "fg" Frame Gen, "hud" HUD, "latency" Latency and sound), and by
// Professional mode's Frame Gen block, which shows them all; so each setting is written once.
import QtQuick
import QtQuick.Window
import QtQuick.Layouts

ColumnLayout {
    id: quick
    spacing: 0
    Layout.fillWidth: true
    // "" every row; otherwise only the rows of that quick setup section.
    property string part: ""
    function shows(section) { return quick.part === "" || quick.part === section }
    readonly property bool generating: app.targetMode !== 0
    // As the Windows quick setup: a 90 Hz screen (85..95) gets 90 FPS in place of 120, a screen whose rate
    // is a whole number of 90s (180, 270, 360 Hz) gets it beside 120, and a target already set stays offered.
    property real refreshHzOverride: 0    // tests
    readonly property real refreshHz: refreshHzOverride > 0 ? refreshHzOverride : Number(Screen.refreshRate || 0)
    readonly property bool screen90: refreshHz > 85 && refreshHz < 95
    readonly property bool screenFits90: {
        const n = Math.round(refreshHz / 90)
        return n >= 2 && Math.abs(refreshHz - n * 90) <= n * 1.5
    }
    readonly property var targetOptions: {
        const list = [{ id: "0", label: qsTr("关") }, { id: "1", label: "60 FPS" }]
        if (app.targetMode === 4 || quick.screen90 || quick.screenFits90) list.push({ id: "4", label: "90 FPS" })
        if (!quick.screen90 || app.targetMode === 2) list.push({ id: "2", label: "120 FPS" })
        list.push({ id: "3", label: qsTr("匹配显示器") })
        return list
    }

    VRow {
        visible: quick.shows("fg")
        objectName: "home-target-row"
        label: qsTr("目标帧率")
        hint: app.targetMode === 0 ? qsTr("补帧关闭：每一帧到达后立刻显示，延迟最低")
                                   : qsTr("补出来的帧夹在两张真实画面之间，画面会晚一张真实画面显示")
        hintBelow: true
        VSeg {
            objectName: "home-target"
            options: quick.targetOptions
            current: String(app.targetMode)
            onPicked: id => app.targetMode = Number(id)
        }
    }
    VRow {
        visible: quick.shows("latency")
        label: qsTr("最低延迟")
        hint: app.lowLatency ? qsTr("已开启：运动估计用「快速」，帧节奏修复关闭。关闭后恢复原来的设置")
                             : qsTr("一键减少等待：补帧照常，每对画面的运动更快算好，画面少等一会儿")
        enabled: quick.generating
        opacity: enabled ? 1 : 0.5
        VSwitch { objectName: "home-lowlatency"; checked: app.lowLatency; onToggled: on => app.lowLatency = on }
    }
    VRow {
        visible: quick.shows("fg")
        label: qsTr("帧节奏修复")
        hint: qsTr("把出帧不均匀的锁帧游戏重新对齐到均匀的时间，每帧最多晚一次刷新。需要开启自动去重")
        enabled: quick.generating
        opacity: enabled ? 1 : 0.5
        VSwitch { objectName: "home-pacing"; checked: app.pacingRepair; onToggled: on => app.pacingRepair = on }
    }
    // HUD protection (HudDialog.qml): the Windows tool tile as a row, with its words.
    VRow {
        visible: quick.shows("hud")
        objectName: "pushfps-hud-row"
        label: qsTr("HUD 保护")
        help: qsTr("自动找出小地图和 HUD，让它们不补帧；方框可以手动修正")
        hint: app.hudRegions.length > 0 ? qsTr("已应用：%1 个区域保持主机原画").arg(app.hudRegions.length) : qsTr("自动识别，可手动修正")
        hintBelow: true
        VButton { objectName: "pushfps-hud"; iconName: "scan"; text: qsTr("打开"); onClicked: app.openDialog("hud") }
    }
    VRow {
        visible: quick.shows("hud")
        label: qsTr("实时 HUD 锁定")
        hint: qsTr("每一帧自动找出 HUD，文字和准星在补出来的帧里保持清晰")
        enabled: quick.generating
        opacity: enabled ? 1 : 0.5
        VSwitch { objectName: "home-hud"; checked: app.hudLock; onToggled: on => app.hudLock = on }
    }
    // Crosshair Hold, as in the Windows build (the same rule, portable/core CrosshairHold.h): off unless switched on.
    VRow {
        id: crosshairRow
        visible: quick.shows("hud")
        objectName: "home-crosshair-row"
        label: qsTr("准星保持")
        help: qsTr("准星静止而背后的画面在动时，补帧会把准星当成背景一起移动：它从原位消失，在两侧各出现一个较淡的重影。开启后，PushFPS 在屏幕中央找出准星，补帧前先把它从画面里取出，补帧后再原样放回。准星刚出现或其线条正在收放时，需要先静止片刻才会被识别。")
        readonly property int holdState: app.running ? Number(app.stats.holdState || 0) : 0
        readonly property int holdPixels: app.running ? Number(app.stats.holdPixels || 0) : 0
        hint: !app.crosshairHold ? qsTr("已关闭：画面移动时准星可能闪烁或出现重影")
            : holdState === 1 ? (holdPixels > 0 ? qsTr("正在运行：已找到准星，补出的每一帧里都原样放回")
                                               : qsTr("正在运行：画面中央还没有找到准星（背后的画面动起来才会识别）"))
            : qsTr("已开启：有画面播放并且补帧开启后开始运行")
        hintBelow: true
        enabled: quick.generating
        opacity: enabled ? 1 : 0.5
        VSwitch { objectName: "home-crosshair"; checked: app.crosshairHold; onToggled: on => app.crosshairHold = on }
    }
    // Subtitle Hold, as in the Windows build (the same rule in the subtitle band): off unless switched on.
    VRow {
        visible: quick.shows("hud")
        objectName: "home-subtitle-row"
        label: qsTr("字幕保持")
        help: qsTr("字幕静止而背后的画面在动时（例如高速开车），补帧会把字幕当成背景一起拉动，文字会上下拖影、出现重影。开启后，PushFPS 在屏幕下方找出字幕，补帧前先把它从画面里取出，补帧后再原样放回。新的一行字幕出现时，需要先静止片刻才会被识别。不用字幕时不必开启。")
        readonly property int holdState: app.running ? Number(app.stats.subHoldState || 0) : 0
        readonly property int holdPixels: app.running ? Number(app.stats.subHoldPixels || 0) : 0
        hint: !app.subtitleHold ? qsTr("已关闭：画面快速移动时字幕可能拖影")
            : holdState === 1 ? (holdPixels > 0 ? qsTr("正在运行：已找到字幕，补出的每一帧里都原样放回")
                                               : qsTr("正在运行：屏幕下方还没有找到字幕（背后的画面动起来才会识别）"))
            : qsTr("已开启：有画面播放并且补帧开启后开始运行")
        hintBelow: true
        enabled: quick.generating
        opacity: enabled ? 1 : 0.5
        VSwitch { objectName: "home-subtitle"; checked: app.subtitleHold; onToggled: on => app.subtitleHold = on }
    }
    VRow {
        visible: quick.shows("fg")
        label: qsTr("去除重复帧")
        hint: qsTr("30 FPS 的游戏在 60 Hz 信号里每张画面会发两次；补帧只看真正的新画面")
        VSwitch { objectName: "home-dedup"; checked: app.removeDuplicates; onToggled: on => app.removeDuplicates = on }
    }
    VRow {
        visible: quick.shows("fg")
        label: qsTr("运动估计")
        hint: qsTr("精细：按 16 像素找运动，更准；快速：按 32 像素，占用更少")
        VSeg {
            objectName: "home-quality"
            options: [{ id: "0", label: qsTr("快速") }, { id: "1", label: qsTr("精细") }]
            current: String(app.quality)
            onPicked: id => app.quality = Number(id)
        }
    }
    // The latency readout over the picture, as the Windows build's (PushLatencyBadge.qml, shared).
    VRow {
        visible: quick.shows("latency")
        objectName: "pushfps-latency-overlay-row"
        label: qsTr("在画面上显示延迟")
        hint: qsTr("实测电脑处理延迟（采集卡画面到达到交给显示器）和帧率；采集卡、显示器和游戏的延迟另计") + "\n"
              + qsTr("可以用鼠标把它拖到画面上的任何位置；双击它，它就回到左上角")
        hintBelow: true
        VSwitch {
            objectName: "pushfps-latency-overlay"
            checked: (veyra.preferences || {}).pushLatencyOverlay === true
            onToggled: on => veyra.setPreference("pushLatencyOverlay", on)
        }
    }
}
