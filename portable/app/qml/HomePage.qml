// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The home page, laid out as the Windows build's new design: sources on the left, the picture in the
// middle with picture and sound under it, quick setup on the right. The picture plays here (the shell
// puts its one picture item into `videoViewport`). Under 900 pixels wide the page is one column, without
// a picture: a connection then shows the picture alone (the shell decides, Main.qml).
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "page-home"
    color: Theme.bgMid
    property bool sideCollapsed: false
    signal requestFullscreen()
    signal requestScreenshot()
    signal requestCheckSetup()

    readonly property bool threePane: width >= 900
    // Where the shell draws the picture on this page (null: the page has none).
    readonly property Item videoViewport: threePane ? viewport : null

    // The same numbers as the Windows home page.
    readonly property real paneMargin: 16
    readonly property real paneGap: 14
    readonly property real leftWidth: Math.round(Math.max(236, Math.min(290, width * 0.21)))
    readonly property real rightWidth: Math.round(Math.max(336, Math.min(410, width * 0.30)))
    readonly property real midX: paneMargin + leftWidth + paneGap
    readonly property real midWidth: Math.max(1, width - 2 * paneMargin - leftWidth - paneGap - (sideCollapsed ? 0 : rightWidth + paneGap))
    readonly property real actionHeight: 44
    readonly property real previewHeight: Math.round(Math.max(120, Math.min(midWidth * 9 / 16, height - 2 * paneMargin - actionHeight - 2 * paneGap - 60)))

    // Folded quick setup sections (preference pushQuickFolded, as in the Windows build).
    readonly property var folded: String((veyra.preferences || {}).pushQuickFolded || "").split(",").filter(s => s.length > 0)
    function isOpen(id) { return folded.indexOf(id) < 0 }
    function setOpen(id, open) {
        const next = folded.filter(s => s !== id)
        if (!open) next.push(id)
        veyra.setPreference("pushQuickFolded", next.join(","))
    }

    // ---- quick setup: one item, shown in the right pane or in the one column ----
    ColumnLayout {
        id: quick
        objectName: "home-quick"
        parent: root.threePane ? rightSlot : columnQuickSlot
        width: parent ? parent.width : 0
        spacing: 0
        // Console and game, as the Windows build's quick setup begins (its section "setup").
        PushSection {
            sectionId: "setup"
            title: qsTr("主机与游戏")
            first: true
            summary: setupProfiles.consoleLabel
            open: root.isOpen("setup")
            onToggled: open => root.setOpen("setup", open)
            ConsoleProfiles { id: setupProfiles }
        }
        PushSection {
            sectionId: "fg"
            title: qsTr("补帧")
            summary: app.targetMode === 0 ? qsTr("关") : app.targetMode === 1 ? "60 FPS" : app.targetMode === 2 ? "120 FPS" : app.targetMode === 4 ? "90 FPS" : qsTr("匹配显示器")
            open: root.isOpen("fg")
            onToggled: open => root.setOpen("fg", open)
            QuickSettings { part: "fg" }
        }
        // Picture, as the Windows build's quick setup has it: "Upscale to my screen" (the algorithm, the
        // size and the sharpening are in Professional mode's Quality tab, as on Windows).
        PushSection {
            sectionId: "picture"
            title: qsTr("画面")
            open: root.isOpen("picture")
            onToggled: open => root.setOpen("picture", open)
            UpscaleRow {}
        }
        // HUD and Latency and sound, as the Windows build's quick setup sections (their rows are written
        // once, in QuickSettings.qml).
        PushSection {
            sectionId: "hud"
            title: qsTr("HUD")
            open: root.isOpen("hud")
            onToggled: open => root.setOpen("hud", open)
            QuickSettings { part: "hud" }
        }
        PushSection {
            sectionId: "latency"
            title: qsTr("延迟与声音")
            open: root.isOpen("latency")
            onToggled: open => root.setOpen("latency", open)
            QuickSettings { part: "latency" }
        }
        PushSection {
            sectionId: "start"
            title: qsTr("连接与启动")
            open: root.isOpen("start")
            onToggled: open => root.setOpen("start", open)
            VRow {
                label: qsTr("打开时自动连接")
                hint: qsTr("打开 PushFPS 时直接连接上次使用的采集卡")
                VSwitch { objectName: "quick-autoconnect"; checked: app.autoConnect; onToggled: on => app.autoConnect = on }
            }
        }
    }

    // ---- picture and sound: one item, under the picture or in the one column ----
    ColumnLayout {
        id: tuning
        objectName: "home-tuning"
        parent: root.threePane ? midSlot : columnTuningSlot
        width: parent ? parent.width : 0
        spacing: root.paneGap
        VCard {
            Layout.fillWidth: true
            PanelTitle { text: qsTr("画面") }
            PictureSettings {}
        }
        VCard {
            Layout.fillWidth: true
            PanelTitle { text: qsTr("声音") }
            SoundSettings {}
        }
    }

    // ---- the three panes ----
    Item {
        id: desk
        anchors.fill: parent
        visible: root.threePane

        Flickable {
            id: leftFlick
            x: root.paneMargin; y: root.paneMargin
            width: root.leftWidth
            height: parent.height - 2 * root.paneMargin
            contentHeight: leftColumn.height
            interactive: contentHeight > height + 1
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: leftColumn
                width: leftFlick.width - 8
                spacing: 0
                Text {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    text: qsTr("片源")
                    color: Theme.t1
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsH3
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                }
                SourcesPanel { id: sourcesWide; Layout.fillWidth: true }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 18
                    text: qsTr("免费开源 · 基于 Veyra 构建")
                    color: Theme.t3
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsSmall
                    wrapMode: Text.WordWrap
                }
            }
        }

        // Middle: the picture.
        Rectangle {
            id: previewCard
            objectName: "home-desk-preview"
            x: root.midX; y: root.paneMargin
            width: root.midWidth
            height: root.previewHeight
            radius: 4
            color: "black"
            border.width: 1
            border.color: Theme.stroke
            Item {
                id: viewport
                objectName: "home-viewport"
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                PicturePlaceholder {
                    anchors.fill: parent
                    z: 1
                    onOpenFile: sourcesWide.openFile()
                }
            }
        }
        RowLayout {
            id: previewActions
            objectName: "home-desk-actions"
            x: root.midX
            y: root.paneMargin + root.previewHeight + root.paneGap
            width: root.midWidth
            height: root.actionHeight
            spacing: 8
            VDot { off: !app.running }
            Text {
                Layout.fillWidth: true
                text: app.running ? (app.sourceTitle.length > 0 ? app.sourceTitle : qsTr("已连接")) : qsTr("未连接")
                color: app.running ? Theme.t1 : Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsBody
                elide: Text.ElideMiddle
            }
            VButton {
                objectName: "home-desk-diagnose"
                visible: app.running
                icon: true
                iconName: "activity"
                tip: qsTr("检查我的设置")
                onClicked: root.requestCheckSetup()
            }
            VButton {
                objectName: "home-desk-screenshot"
                visible: app.running
                icon: true
                iconName: "camera"
                tip: qsTr("截图")
                onClicked: root.requestScreenshot()
            }
            VButton {
                objectName: "home-desk-stop"
                visible: app.running
                ghost: true
                text: qsTr("断开")
                onClicked: app.stop()
            }
            VButton {
                objectName: "home-desk-fullscreen"
                primary: app.running
                enabled: app.running
                iconName: "max"
                text: qsTr("全屏")
                onClicked: root.requestFullscreen()
            }
        }
        Flickable {
            id: midFlick
            x: root.midX
            y: previewActions.y + root.actionHeight + root.paneGap
            width: root.midWidth
            height: Math.max(0, desk.height - y - root.paneMargin)
            contentHeight: midSlot.height
            interactive: contentHeight > height + 1
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: VScrollBar {}
            Item { id: midSlot; width: midFlick.width - 10; height: tuning.parent === midSlot ? tuning.implicitHeight : 0 }
        }

        // Right: quick setup.
        Rectangle {
            id: rightPane
            objectName: "home-desk-settings"
            visible: !root.sideCollapsed
            x: desk.width - root.paneMargin - root.rightWidth
            y: root.paneMargin
            width: root.rightWidth
            height: desk.height - 2 * root.paneMargin
            radius: Theme.rCard
            color: Theme.card2
            border.width: 1
            border.color: Theme.stroke
            Text {
                id: rightTitle
                x: 14; y: 12
                text: qsTr("快速设置")
                color: Theme.t1
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsH3
                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
            }
            Flickable {
                id: rightFlick
                objectName: "home-desk-settings-list"
                // Room for a row's hover light, which reaches 7 px past the row (VRow.qml), inside this clipping
                // list: 7 px on the left (the rows start at 7 + 7 = 14, where they always did) and on the right 7 px
                // plus the scroll bar (10 px while the pointer is on it) plus 4 px between them (the 28 below).
                x: 7; y: rightTitle.y + rightTitle.height + 6
                width: parent.width - 11
                height: parent.height - y - 8
                contentHeight: rightSlot.height
                interactive: contentHeight > height + 1
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: VScrollBar {}
                Item { id: rightSlot; x: 7; width: rightFlick.width - 28; height: quick.parent === rightSlot ? quick.implicitHeight : 0 }
            }
        }
    }

    // ---- one column (a narrow window) ----
    Flickable {
        id: columnFlick
        anchors.fill: parent
        visible: !root.threePane
        contentHeight: column.height + 48
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: VScrollBar {}
        ColumnLayout {
            id: column
            width: Math.min(640, columnFlick.width - 32)
            x: (columnFlick.width - width) / 2
            y: 24
            spacing: 14
            SourcesPanel { Layout.fillWidth: true }
            VCard {
                Layout.fillWidth: true
                PanelTitle { text: qsTr("快速设置") }
                Item { id: columnQuickSlot; Layout.fillWidth: true; implicitHeight: quick.parent === columnQuickSlot ? quick.implicitHeight : 0 }
            }
            Item { id: columnTuningSlot; Layout.fillWidth: true; implicitHeight: tuning.parent === columnTuningSlot ? tuning.implicitHeight : 0 }
            Text {
                Layout.fillWidth: true
                text: qsTr("免费开源 · 基于 Veyra 构建")
                color: Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}
