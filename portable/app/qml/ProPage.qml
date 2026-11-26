// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Professional mode: the picture large on the left, every setting of the preview in the panel on the
// right (Quality, Frame Gen, Colour, Audio, Display, the Windows build's tabs), and the engine's
// figures in cards under the picture. The panel starts on All, where each block stands under a title that folds it (preference
// pushProFolded, as in the Windows build); the tabs show one block alone.
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    objectName: "page-pro"
    color: Theme.bgMid
    property bool sideCollapsed: false
    signal requestFullscreen()
    signal requestScreenshot()
    signal requestCheckSetup()
    signal requestSources()

    readonly property Item videoViewport: viewport
    readonly property real margin: 16
    readonly property real gap: 14
    // Wide enough for the tabs in this language (they are scrolled only when that would pass 45 % of the page).
    readonly property real panelWidth: Math.round(Math.min(Math.max(340, Math.min(420, width * 0.32), tabs.implicitWidth + 24), width * 0.45))
    readonly property real actionHeight: 44

    // The blocks, in the order All shows them.
    readonly property var blocks: [
        { id: "quality", label: qsTr("画质") },
        { id: "fg", label: qsTr("补帧") },
        { id: "color", label: qsTr("色彩") },
        { id: "audio", label: qsTr("声音") },
        { id: "display", label: qsTr("显示") }
    ]
    property string tab: "all"
    readonly property var folded: String((veyra.preferences || {}).pushProFolded || "").split(",").filter(s => s.length > 0)
    function blockShown(id) { return tab === id || (tab === "all" && folded.indexOf(id) < 0) }
    function setFolded(id, fold) {
        const next = folded.filter(s => s !== id)
        if (fold) next.push(id)
        veyra.setPreference("pushProFolded", next.join(","))
    }

    // A block of the panel: under a title that folds it in All, alone under its tab. (An inline component
    // does not see this file's ids: the tab and whether the block is shown are handed in.)
    component Block: ColumnLayout {
        id: block
        property string blockId: ""
        property string title: ""
        property string tab: "all"
        property bool shown: true
        signal foldToggled(bool open)
        default property alias rows: body.data
        Layout.fillWidth: true
        spacing: 0
        visible: tab === "all" || tab === blockId
        PushFoldHead {
            objectName: "pro-all-" + block.blockId
            visible: block.tab === "all"
            text: block.title
            open: block.shown
            onToggled: open => block.foldToggled(open)
        }
        ColumnLayout {
            id: body
            objectName: "pro-block-" + block.blockId
            visible: block.shown
            Layout.fillWidth: true
            Layout.leftMargin: block.tab === "all" ? 4 : 0
            spacing: 0
        }
    }

    // ---- the picture ----
    readonly property real videoWidth: Math.max(1, width - 2 * margin - (sideCollapsed ? 0 : panelWidth + gap))
    // The engine's figures in cards under the picture (LiveFigures.qml), as Windows has its meters: while
    // something plays, and only when the picture keeps some height.
    readonly property real metersHeight: Math.max(172, meters.implicitHeight)
    readonly property real pictureSpace: height - 2 * margin - actionHeight - gap
    readonly property bool metersShown: app.running && pictureSpace >= metersHeight + gap + 120
    Rectangle {
        id: videoCard
        objectName: "pro-video"
        x: root.margin; y: root.margin
        width: root.videoWidth
        height: Math.max(80, root.pictureSpace - (root.metersShown ? root.metersHeight + root.gap : 0))
        radius: 4
        color: "black"
        border.width: 1
        border.color: Theme.stroke
        Item {
            id: viewport
            objectName: "pro-viewport"
            anchors.fill: parent
            anchors.margins: 1
            clip: true
            PicturePlaceholder {
                anchors.fill: parent
                z: 1
                offerSources: false
            }
        }
    }
    RowLayout {
        objectName: "pro-actions"
        x: root.margin
        y: videoCard.y + videoCard.height + root.gap
        width: root.videoWidth
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
            objectName: "pro-sources"
            visible: !app.running
            text: qsTr("片源")
            iconName: "video"
            onClicked: root.requestSources()
        }
        VButton {
            objectName: "pro-connect"
            visible: !app.running
            primary: true
            text: qsTr("连接")
            enabled: app.videoDevices.length > 0
            onClicked: app.startCamera()
        }
        VButton {
            objectName: "pro-diagnose"
            visible: app.running
            icon: true
            iconName: "activity"
            tip: qsTr("检查我的设置")
            onClicked: root.requestCheckSetup()
        }
        VButton {
            objectName: "pro-screenshot"
            visible: app.running
            icon: true
            iconName: "camera"
            tip: qsTr("截图")
            onClicked: root.requestScreenshot()
        }
        VButton {
            objectName: "pro-stop"
            visible: app.running
            ghost: true
            text: qsTr("断开")
            onClicked: app.stop()
        }
        VButton {
            objectName: "pro-fullscreen"
            visible: app.running
            primary: true
            iconName: "max"
            text: qsTr("全屏")
            onClicked: root.requestFullscreen()
        }
    }

    LiveFigures {
        id: meters
        objectName: "pro-meters"
        visible: root.metersShown
        x: root.margin
        y: videoCard.y + videoCard.height + root.gap + root.actionHeight + root.gap
        width: root.videoWidth
        height: root.metersHeight
    }

    // ---- the panel ----
    Rectangle {
        id: panel
        objectName: "pro-inspector"
        visible: !root.sideCollapsed
        x: root.width - root.margin - root.panelWidth
        y: root.margin
        width: root.panelWidth
        height: root.height - 2 * root.margin
        radius: Theme.rCard
        color: Theme.card2
        border.width: 1
        border.color: Theme.stroke

        Flickable {
            id: tabFlick
            x: 12; y: 12
            width: parent.width - 24
            height: tabs.implicitHeight
            contentWidth: tabs.implicitWidth
            interactive: contentWidth > width
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            VSeg {
                id: tabs
                objectName: "pro-tabs"
                options: [{ id: "all", label: qsTr("全部") }].concat(root.blocks)
                current: root.tab
                onPicked: id => root.tab = id
            }
        }
        Flickable {
            id: listFlick
            objectName: "pro-list"
            // Room for a row's hover light, which reaches 7 px past the row (VRow.qml), inside this clipping
            // list: 7 px on the left (the rows start at 7 + 7 = 14, where they always did) and on the right 7 px
            // plus the scroll bar (10 px while the pointer is on it) plus 4 px between them (the 28 below).
            x: 7; y: tabFlick.y + tabFlick.height + 10
            width: parent.width - 11
            height: parent.height - y - 8
            contentHeight: list.implicitHeight + 8
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: VScrollBar {}
            ColumnLayout {
                id: list
                x: 7
                width: listFlick.width - 28
                spacing: 4

                // Quality, as the Windows build's tab: "Upscale to my screen" and the Super resolution card.
                Block {
                    blockId: "quality"; title: qsTr("画质"); tab: root.tab; shown: root.blockShown("quality"); onFoldToggled: open => root.setFolded("quality", !open)
                    UpscaleRow {}
                    CropRow {}
                    SuperResolutionCard { Layout.topMargin: 6; Layout.bottomMargin: 6 }
                }
                Block { blockId: "fg"; title: qsTr("补帧"); tab: root.tab; shown: root.blockShown("fg"); onFoldToggled: open => root.setFolded("fg", !open); ConsoleProfiles {} QuickSettings {} }
                Block { blockId: "color"; title: qsTr("色彩"); tab: root.tab; shown: root.blockShown("color"); onFoldToggled: open => root.setFolded("color", !open); PictureSettings { showShape: false } }
                Block { blockId: "audio"; title: qsTr("声音"); tab: root.tab; shown: root.blockShown("audio"); onFoldToggled: open => root.setFolded("audio", !open); SoundSettings {} }
                // Display, as the Windows build's tab: how the picture fills the window, under its names.
                Block {
                    blockId: "display"; title: qsTr("显示"); tab: root.tab; shown: root.blockShown("display"); onFoldToggled: open => root.setFolded("display", !open)
                    VRow {
                        label: qsTr("画面比例")
                        hint: app.fillMode === 1 ? qsTr("填满窗口，超出部分裁掉") : ""
                        VSelect {
                            objectName: "display-aspect"
                            // The preview's three ways (FrameView::fill) under the Windows build's names.
                            options: [{ id: "0", label: qsTr("适应") }, { id: "1", label: qsTr("填充裁切") }, { id: "2", label: qsTr("拉伸铺满") }]
                            value: (options.find(o => o.id === String(app.fillMode)) || options[0]).label
                            onPicked: id => app.fillMode = Number(id)
                        }
                    }
                    // The live comparison, as the Windows build's Display tab (ProDisplayPanel.qml): the
                    // original is the capture before the picture controls and NVIDIA Image Scaling.
                    VRow {
                        label: qsTr("原画 / 增强对比")
                        readonly property string sides: app.preferences.pushCompareSwap === true ? qsTr("在画面上按住左键拖动分割线：左边增强、右边原画。")
                                                                                                   : qsTr("在画面上按住左键拖动分割线：左边原画、右边增强。")
                        hint: (app.compareMode === 2 ? sides : "") + (app.compareMode !== 0 ? qsTr("对比时补帧暂停，关闭对比后恢复") : "")
                        hintBelow: true
                        VSeg {
                            objectName: "display-compare"
                            options: [{ id: "0", label: qsTr("关闭", "off") }, { id: "2", label: qsTr("分屏") }, { id: "1", label: qsTr("只看原画") }]
                            current: String(app.compareMode)
                            onPicked: id => app.compareMode = Number(id)
                        }
                    }
                    VRow {
                        objectName: "display-compare-swap-row"
                        label: qsTr("交换两侧")
                        visible: app.compareMode === 2 || app.preferences.pushCompareSwap === true
                        VSwitch {
                            objectName: "display-compare-swap"
                            checked: app.preferences.pushCompareSwap === true
                            onToggled: on => app.setPreference("pushCompareSwap", on)
                        }
                    }
                }
            }
        }
    }
}
