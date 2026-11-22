// 专业模式 · 列表视图, rebuilt from the design (pages-pro.js + pages.css .pro).
//
// The design's grid: 1fr | 376px columns, rows auto | 1fr | auto, gap 10,
// padding 14. The inspector spans rows 2-3 on the right; the three meters sit
// under the video with room for the active rate-policy explanation.
//
// The chain order is fixed (SR → NR layers → protect → HDR → 补帧) and is NOT
// reorderable here: the user's reason was that dragging breaks the pipeline for
// beginners. Node mode is where free arrangement lives, and it is a separate page
// reached through the view toggle (which confirms first, per the design).
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import "EffectSupport.js" as Support
import "PushRecord.js" as Record
import "PushShell.js" as PushShell
import QtQuick.Shapes
import QtQuick.Effects

VPage {
    id: root
    signal requestPage(string page)
    signal requestDialog(string key)
    signal requestFullscreen()
    // Fullscreen quick-adjust (Home, Main.qml): only the inspector is shown, moved
    // to the left of the picture, cut out of the native video window.
    property bool overlay: false
    property bool onboardingActive: false
    function saveOnboardingView() {
        return { tab: tab, scroll: inspFlick.contentY, fgOpen: fgCard.open,
                 flowOpen: fgFlowGroup.expanded }
    }
    function restoreOnboardingView(view) {
        fgCard.open = view.fgOpen
        fgFlowGroup.expanded = view.flowOpen
        tab = view.tab
        inspFlick.contentY = view.scroll
    }
    function showOnboardingStep(step) {
        const changedTab = tab !== step.tab
        tab = step.tab
        if (step.target === "fg" || step.target === "dedup") fgCard.open = true
        if (step.target === "dedup") fgFlowGroup.expanded = true
        if (changedTab || step.target === "fg") inspFlick.contentY = inspFlick.originY
    }
    function onboardingTarget(key) {
        if (key === "fg") return { item: insp }
        if (key === "timing") return { item: meters }
        const items = { "sr": srCard.headerItem, "nr": nrStage.headerItem,
                        "hdr": hdrCard.headerItem, "dedup": dedupRow }
        return { item: items[key], viewport: inspFlick }
    }
    readonly property bool hdrMeterObserved: visible && !sideCollapsed && blockShown("display") && displayCol.hdrMeterObserved
    onHdrMeterObservedChanged: veyra.observeHdrCoverage(hdrMeterObserved)
    function overlayContains(x, y) {
        const hit = item => x >= item.x && x <= item.x + item.width && y >= item.y && y <= item.y + item.height
        return overlay && hit(insp)
    }
    // Leaving the page (or switching to the node editor) disarms region drawing;
    // showing it replays the GPU bars' staggered entrance (M27).
    onVisibleChanged: {
        if (!visible && !onboardingActive && veyra.protectionDrawShape.length > 0) veyra.beginProtectionDraw("")
        if (visible) armBars()
    }

    // M27 .bar u: width 0 -> value over .8s --spring-soft, bar i starting at
    // 120 + i*60 ms after the page shows; later readings move without the delay.
    property bool barsArmed: false
    property bool barsSettled: false
    function armBars() { barsSettled = false; barsArmed = false; barsArmTimer.restart() }
    Timer { id: barsArmTimer; interval: 16; onTriggered: { root.barsArmed = true; barsSettleTimer.restart() } }
    Timer { id: barsSettleTimer; interval: 1600; onTriggered: root.barsSettled = true }
    Component.onCompleted: { armBars(); followDesign() }   // (followDesign: the new design's "all" view, below)

    // M31: switching tabs redraws the tab's cards one after another
    // (pages-pro.js render(true): 460 ms, i*30 ms, y 10).
    Component { id: staggerComp; VRise.Stagger {} }
    function stagger(col, step, span, dy) {
        if (Theme.reduced || onboardingActive || !col) return
        let i = 0
        for (let k = 0; k < col.children.length; ++k) {
            const c = col.children[k]
            if (!c.visible || c.motionDy === undefined) continue
            staggerComp.createObject(root, { target: c, delay: i * step, span: span, dy: dy }).start()
            ++i
        }
    }
    onTabChanged: Qt.callLater(() => stagger(tab === "quality" || tab === "all" ? qualityCol : tab === "fg" ? fgCol
                                           : tab === "color" ? colourPanel.sectionColumn
                                           : tab === "audio" ? audioCol : displayCol, 30, 460, 10))

    // M30: the NR layer card that was just added pops in; the number is reset
    // shortly after so a later rebuild of the list does not pop it again.
    property int popLayer: -1
    property int popBeauty: -1
    Timer { id: popReset; interval: 900; onTriggered: { root.popLayer = -1; root.popBeauty = -1 } }
    function markPop(number) { popLayer = number; popReset.restart() }
    function markBeautyPop(number) { popBeauty = number; popReset.restart() }

    // Design's st.tab: quality / fg / color / audio / display.
    property string tab: "quality"
    // PushFPS redesign: "all" shows every tab's settings in one list, each under its name, so nothing
    // has to be looked for from tab to tab (the owner's instruction). The five tabs stay beside it, and
    // everything that sets `tab` to one of them still does. Only the new design offers it, and it is
    // where that design starts; with Legacy chosen the list goes back to a tab.
    readonly property bool allTabs: tab === "all"
    property bool allTabsOffered: false
    // (PushFPS 4.3.1) In the All tab each tab's block folds by its title and is remembered folded
    // (preference pushProFolded: the blocks' ids with commas, as quick setup's sections are kept). A
    // tab of its own always shows its block: there the fold has nothing to say.
    // (The list's rules are PushShell.js, tested without Qt.)
    readonly property string foldedText: String(Theme.shot.pushProFolded || "")
    function blockOpen(id) { return PushShell.blockOpen(root.foldedText, id) }
    function setBlockOpen(id, open) { veyra.setPreference("pushProFolded", PushShell.withBlock(root.foldedText, id, open)) }
    function blockShown(id) { return root.tab === id || (root.allTabs && root.blockOpen(id)) }
    function followDesign() {
        if (Theme.modern && !allTabsOffered) { allTabsOffered = true; if (tab === "quality") tab = "all" }
        else if (!Theme.modern) { allTabsOffered = false; if (tab === "all") tab = "quality" }
    }
    Connections { target: Theme; function onModernChanged() { root.followDesign() } }
    property int selectedEffect: -1
    // 处理顺序 "点击定位": switch to the chip's tab, open its card and scroll
    // the inspector so the card sits at the top.
    function locateNode(chip) {
        selectedEffect = chip.index
        let target = ""
        if (chip.type === "frame-generation") { tab = "fg"; target = "list-fg-group" }
        else if (chip.type === "color") { tab = "color"; target = "" }
        else {
            tab = "quality"
            if (chip.type === "sr") target = "list-sr"
            else if (chip.type === "aa") target = "list-anti-alias"
            else if (chip.type === "video-hdr") target = "list-video-hdr"
            else if (chip.type === "protection") { nrStage.open = true; target = "list-global-protection" }
            else if (chip.type === "beauty") {
                beautyStage.open = true
                let ordinal = 0
                for (const n of veyra.chain) { if (n.type === "beauty") ++ordinal; if (n.index === chip.index) break }
                target = "beauty-card-" + ordinal
            }
            else if (chip.type === "nr") {
                nrStage.open = true
                let ordinal = 0
                for (const n of veyra.chain) { if (n.type === "nr") ++ordinal; if (n.index === chip.index) break }
                target = "nr-card-" + ordinal
            }
        }
        Qt.callLater(() => {
            const find = item => {
                if (!item) return null
                if (target.length > 0 && item.objectName === target) return item
                for (let i = 0; i < item.children.length; ++i) { const r = find(item.children[i]); if (r) return r }
                return null
            }
            const card = target.length > 0 ? find(body) : null
            if (card && card.open !== undefined) card.open = true
            if (card && card.flash !== undefined) card.flash()
            const y = card ? card.mapToItem(body, 0, 0).y - 4 : 0
            inspFlick.contentY = Math.max(0, Math.min(y, Math.max(0, inspFlick.contentHeight - inspFlick.height)))
        })
    }
    onSelectedEffectChanged: {
        veyra.selectedNrLayer = selectedEffect
        veyra.selectedColourLayer = selectedEffect
    }

    // The NR layer card wraps the shared editor (NrLayerEditor.qml).
    // 显示同步 and 输出上限 (1.4.4 PresentationSettings), shared by the list 补帧
    // page and the node editor's output box (ProPage.PresentationRows).
    component PresentationRows: ProPresentationRows {}    // A performance orb: a ring filled to `fraction` (green under 70 %, amber
    // under 100 %, red above), the value in the middle, the label under it.
    // Shared with the node page as ProPage.PerfOrb.
    component PerfOrb: ProPerfOrb {}
    // The three orbs every performance readout shows: total processing time
    // against the source frame budget, GPU utilization, and whether the output
    // keeps up with its target rate.
    component PerfOrbs: ProPerfOrbs {}
    // The status light: 1.4.4's 当前状态 (正常 / 补帧调度降档 / 输出未达标 /
    // 输入帧率不足 …) as a glowing strip with the detail beside it.
    component StatusLight: ProStatusLight {}

    component StatusSummary: ProStatusSummary {}

    component NrLayerCard: VAccordion {
        id: card
        required property var layerData
        property int layerNumber: 1
        property int layerCount: 1
        property bool nrFirst: false
        signal edited(int nodeIndex, string key, double amount)
        signal enabledEdited(int nodeIndex, bool enabled)
        signal duplicateRequested(int nodeIndex)
        signal removeRequested(int nodeIndex)
        signal orderEdited(bool enabled)
        objectName: "nr-card-" + layerNumber
        Layout.fillWidth: true
        glyph: "wand"; hue: Theme.accent
        compactHeader: true
        popIn: layerNumber === root.popLayer
        title: qsTr("NR 层 ") + layerNumber
        // .lnum: the layer number where a stage card has its icon.
        headerLeadingActions: Rectangle {
            implicitWidth: 22; implicitHeight: 22; radius: 7
            color: Qt.rgba(1, 138 / 255, 61 / 255, 0.16)
            Text {
                anchors.centerIn: parent
                text: card.layerNumber
                color: Theme.accent
                font.family: Theme.fontMono; font.pixelSize: 11
                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
            }
        }
        headerActions: VButton {
            objectName: "nr-card-more-" + card.layerNumber
            icon: true; ghost: true; iconName: "dup"
            implicitWidth: 26; implicitHeight: 26
            tip: qsTr("复制 · 恢复默认 · 删除")
            onClicked: { nrLayerMenu.layer = card; nrLayerMenu.openAt(this, "down") }
        }
        // This layer's own GPU time (list mode keys a node by its index + 2).
        readonly property var timing: veyra.nodeTimings[String(layerData.index + 2)] || ({})
        readonly property string timingText: timing.state === "measured" ? timing.ms.toFixed(2) + " ms"
                                           : timing.state === "pending" ? qsTr("等待样本")
                                           : timing.state === "failed" ? qsTr("启用失败") : ""
        summary: nrEditor.sizeLabel + qsTr(" · 强度 ") + layerData.intensity.toFixed(2)
                 + (layerData.enabled ? (timingText.length > 0 ? " · " + timingText : "") : qsTr(" · 已旁路"))
        on: layerData.enabled
        onToggled: on => enabledEdited(layerData.index, on)
        NrLayerEditor {
            id: nrEditor
            Layout.fillWidth: true
            layerData: card.layerData
            layerCount: card.layerCount
            nrFirst: card.nrFirst
            onEdited: (i, k, v) => card.edited(i, k, v)
            onDuplicateRequested: i => card.duplicateRequested(i)
            onRemoveRequested: i => card.removeRequested(i)
            onOrderEdited: e => card.orderEdited(e)
        }
    }

    // A beauty layer: the shared editor (BeautyEditor.qml) under a numbered header.
    component BeautyLayerCard: VAccordion {
        id: card
        required property var layerData
        property int layerNumber: 1
        objectName: "beauty-card-" + layerNumber
        Layout.fillWidth: true
        glyph: "heart"; hue: "#FF8FBF"
        compactHeader: true
        popIn: layerNumber === root.popBeauty
        title: qsTr("美颜层 ") + layerNumber
        headerLeadingActions: Rectangle {
            implicitWidth: 22; implicitHeight: 22; radius: 7
            color: Qt.rgba(1, 143 / 255, 191 / 255, 0.16)
            Text {
                anchors.centerIn: parent
                text: card.layerNumber
                color: "#FF8FBF"
                font.family: Theme.fontMono; font.pixelSize: 11
                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
            }
        }
        headerActions: RowLayout {
            spacing: 2
            VButton {
                objectName: "beauty-card-more-" + card.layerNumber
                icon: true; ghost: true; iconName: "dup"
                implicitWidth: 26; implicitHeight: 26
                tip: qsTr("复制 · 移动 · 恢复默认 · 删除")
                onClicked: { beautyLayerMenu.layer = card; beautyLayerMenu.openAt(this, "down") }
            }
            VButton {
                objectName: "beauty-card-delete-" + card.layerNumber
                icon: true; ghost: true; iconName: "trash"
                implicitWidth: 26; implicitHeight: 26
                tip: qsTr("删除这一层")
                onClicked: veyra.removeEffect(card.layerData.index)
            }
        }
        readonly property var timing: veyra.nodeTimings[String(layerData.index + 2)] || ({})
        readonly property string timingText: timing.state === "measured" ? timing.ms.toFixed(2) + " ms"
                                           : timing.state === "pending" ? qsTr("等待样本") : ""
        summary: layerData.summary + (layerData.enabled && timingText.length > 0 ? " · " + timingText : "")
        on: layerData.enabled
        onToggled: on => veyra.setBeautyParameterAt(layerData.index, "enabled", on ? 1 : 0)
        BeautyEditor {
            Layout.fillWidth: true
            namePrefix: "list-beauty-" + card.layerNumber
            params: card.layerData
            showDetection: false
            onEdited: (key, amount) => veyra.setBeautyParameterAt(card.layerData.index, key, amount)
            onPresetPicked: preset => veyra.applyBeautyPreset(card.layerData.index, preset)
        }
    }

    // .addlayer: the dashed button at the bottom of a layered stage (NR, beauty).
    component AddLayerButton: ProAddLayerButton {}

    // Source and output belong together; the transport stays clear for seeking.
    Item {
        id: head
        objectName: "pro-header"
        visible: !root.overlay
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        readonly property bool compactActions: width < 850
        readonly property bool compact: width < 972 || width - headActions.implicitWidth < 320
        height: compact ? 72 : 32

        RowLayout {
            id: sourceInfo
            objectName: "pro-source-info"
            anchors.left: parent.left
            anchors.top: parent.top
            width: head.compact ? head.width : Math.max(0, head.width - actionViewport.width - 16)
            height: 32
            spacing: 8

            // pages-pro.js data-srcbtn: what is playing, with a chevron for the menu.
            VButton {
                id: sourceBtn
                objectName: "pro-source-button"
                iconName: veyra.sourceKind === "ps5" ? "gamepad" : veyra.sourceKind === "moonlight" ? "cast" : veyra.sourceKind === "xbox" ? "gamepad" : veyra.sourceKind === "screen" ? "monitor"
                        : veyra.sourceKind === "image" ? "image" : veyra.sourceKind === "file" ? "film" : "video"
                text: veyra.sourceTitle.length > 0 ? veyra.sourceTitle : qsTr("片源")
                tip: text
                maxTextWidth: Math.max(64, Math.min(260, sourceInfo.width - formatTag.implicitWidth
                                                  - (outputTag.visible ? outputTag.implicitWidth + 8 : 0) - 74))
                trailingIcon: "down"
                onClicked: sourceMenu.openAt(this, "down")
            }
            // "1080p60 · YUY2 · SDR": reported values only.
            VTag {
                id: formatTag
                objectName: "pro-format-tag"
                text: veyra.sourceFormatText.length > 0 ? veyra.sourceFormatText : qsTr("未打开")
            }
            VTag {
                id: outputTag
                objectName: "pro-output-summary"
                visible: veyra.outputSummary.length > 0
                text: qsTr("输出 ") + veyra.outputSummary.replace("x", "×")
            }
            Item { Layout.fillWidth: true }
        }

        Flickable {
            id: actionViewport
            objectName: "pro-header-actions"
            anchors.right: parent.right
            y: head.compact ? 40 : 0
            width: Math.min(head.width, headActions.implicitWidth)
            height: 32
            contentWidth: headActions.implicitWidth
            contentHeight: height
            flickableDirection: Flickable.HorizontalFlick
            interactive: contentWidth > width
            boundsBehavior: Flickable.StopAtBounds
            clip: interactive
            ScrollBar.horizontal: VScrollBar { }
        }
        RowLayout {
            id: headActions
            parent: actionViewport.contentItem
            width: implicitWidth
            height: actionViewport.height
            spacing: 8

            // View toggle. Choosing 节点 asks first, because switching rebuilds the
            // whole processing chain (design note: 切换需要确认，会重建处理链).
            VSeg {
                objectName: "pro-mode-switch"
                options: [{ id: "list", label: qsTr("列表"), icon: "list" }, { id: "node", label: qsTr("节点"), icon: "nodes" }]
                current: veyra.nodeMode === 1 ? "node" : "list"
                onPicked: id => {
                    const target = id === "node" ? 1 : 0
                    if (target === veyra.nodeMode) { if (target === 1) root.requestPage("node"); return }
                    switchDialog.targetMode = target
                    switchDialog.open()
                }
            }
            VButton {
                id: screenshotButton
                objectName: "pro-screenshot"
                iconName: "camera"
                text: head.compactActions ? "" : qsTr("截图")
                icon: head.compactActions
                tip: qsTr("截图")
                Accessible.name: screenshotButton.tip
                onClicked: veyra.takeScreenshot()
            }
            // Recording: the red dot pulses while the file is open, and a
            // three second countdown runs before the engine is asked to open
            // it. The countdown is an interface animation; the file itself is
            // owned by the engine. The header row is a layout, so this control
            // uses Layout.alignment instead of anchors.
            Rectangle {
                id: recordControl
                objectName: "pro-record-control"
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: recordRow.implicitWidth + 24
                Layout.preferredHeight: Theme.ctlHeight
                implicitWidth: recordRow.implicitWidth + 24
                implicitHeight: Theme.ctlHeight
                radius: Theme.rCtl
                color: recordMouse.containsMouse ? Theme.card3 : Theme.card2
                border.width: 1
                border.color: (veyra.recording || recordControl.countdown > 0) ? Theme.err : Theme.stroke2
                property int countdown: 0
                // Elapsed recording time, m:ss (h:mm:ss after an hour).
                readonly property string elapsed: {
                    const total = Math.max(0, Math.floor((veyra.recordingState && veyra.recordingState.seconds) || 0))
                    const h = Math.floor(total / 3600), m = Math.floor(total / 60) % 60, s = total % 60
                    const two = n => (n < 10 ? "0" : "") + n
                    return h > 0 ? h + ":" + two(m) + ":" + two(s) : two(m) + ":" + two(s)
                }
                readonly property bool saving: !veyra.recording && !!(veyra.recordingState && veyra.recordingState.finishing)
                Behavior on color { ColorAnimation { duration: Theme.d(Theme.durFast) } }
                Behavior on border.color { ColorAnimation { duration: Theme.d(Theme.durFast) } }

                Row {
                    id: recordRow
                    anchors.centerIn: parent
                    spacing: 7
                    Rectangle {
                        id: recordDot
                        width: 9; height: 9; radius: 4.5
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.err
                        SequentialAnimation on opacity {
                            running: veyra.recording || recordControl.countdown > 0
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.25; duration: veyra.recording ? 700 : 380; easing.type: Easing.InOutQuad }
                            NumberAnimation { to: 1.0; duration: veyra.recording ? 700 : 380; easing.type: Easing.InOutQuad }
                        }
                    }
                    Text {
                        id: recordLabel
                        width: head.compactActions ? Math.min(96, implicitWidth) : implicitWidth
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                        text: recordControl.countdown > 0 ? qsTr("录制 %1 秒").arg(recordControl.countdown)
                              : veyra.recording ? qsTr("停止 %1").arg(recordControl.elapsed)
                              : recordControl.saving ? qsTr("保存中…") : qsTr("录制")
                        color: Theme.t1
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        font.weight: Font.Medium
                    }
                }
                ToolTip.visible: recordMouse.containsMouse && recordLabel.truncated
                ToolTip.text: recordLabel.text
                MouseArea {
                    id: recordMouse
                    objectName: "pro-record-mouse"
                    anchors.fill: parent
                    enabled: veyra.hasSource && !recordControl.saving
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (veyra.recording) { veyra.stopRecording("pro-button"); recordControl.countdown = 0; recordTick.stop(); return }
                        if (recordControl.countdown > 0) { recordControl.countdown = 0; recordTick.stop(); return }
                        // PushFPS 4.0: the choices first (PushRecordDialog); its start button records at once.
                        // With "ask before every recording" off, the three-second countdown as before.
                        if (Record.asks(veyra.preferences)) { veyra.navigate("record"); return }
                        recordControl.countdown = 3
                        recordTick.restart()
                    }
                }
                // The countdown number pops on every tick.
                NumberAnimation {
                    id: recordPop
                    target: recordLabel
                    property: "scale"
                    from: 1.18
                    to: 1.0
                    duration: Theme.d(Theme.durFast)
                    easing: Theme.spring
                }
                Timer {
                    id: recordTick
                    interval: 1000
                    repeat: true
                    onTriggered: {
                        if (!veyra.hasSource) { recordControl.countdown = 0; stop(); return }
                        recordControl.countdown -= 1
                        recordPop.restart()
                        if (recordControl.countdown <= 0) {
                            recordControl.countdown = 0
                            recordTick.stop()
                            veyra.startRecording("pro-button")
                        }
                    }
                }
            }
            RowLayout {
                objectName: "pro-virtual-camera-control"
                Layout.alignment: Qt.AlignVCenter
                spacing: 6
                VIcon { visible: head.compactActions; name: "video"; size: 14 }
                Text {
                    visible: !head.compactActions
                    text: qsTr("虚拟摄像机")
                    color: veyra.virtualCameraEnabled ? Theme.accent : Theme.t2
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsBody
                }
                VSwitch {
                    objectName: "pro-virtual-camera-toggle"
                    checked: veyra.virtualCameraEnabled
                    onToggled: enabled => veyra.setVirtualCameraEnabled(enabled)
                }
                HoverHandler { id: cameraHover }
                ToolTip.visible: head.compactActions && cameraHover.hovered
                ToolTip.text: qsTr("虚拟摄像机")
            }
            RowLayout {
                objectName: "pro-replay-control"
                Layout.alignment: Qt.AlignVCenter
                spacing: 6
                VIcon { visible: head.compactActions; name: "clock"; size: 14 }
                Text {
                    visible: !head.compactActions
                    text: qsTr("即时回放")
                    color: Theme.t2
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsBody
                }
                VSwitch {
                    objectName: "pro-replay-toggle"
                    checked: veyra.replayEnabled
                    onToggled: checked => veyra.setPreference("replayEnabled", checked)
                }
                HoverHandler { id: replayHover }
                ToolTip.visible: head.compactActions && replayHover.hovered
                ToolTip.text: qsTr("即时回放")
            }
            // Instant replay (#103): while the buffer is on, the time it holds;
            // a click saves it, as the replay shortcut does.
            VButton {
                id: replayBtn
                objectName: "pro-replay-save"
                visible: veyra.replayEnabled
                readonly property var replay: veyra.replayState || ({})
                readonly property int held: Math.max(0, replay.seconds || 0)
                iconName: "clock"
                text: replay.saving ? qsTr("保存中…")
                      : qsTr("回放 %1").arg(Math.floor(held / 60) + ":" + (held % 60 < 10 ? "0" : "") + held % 60)
                tip: qsTr("保存最近的画面和声音（%1）").arg(veyra.shortcuts.replay || qsTr("未设置快捷键"))
                maxTextWidth: head.compactActions ? 84 : 0
                enabled: !replay.saving && held > 0
                onClicked: veyra.saveReplay("pro-button")
            }
            VButton {
                id: presetBtn
                iconName: "layers"
                trailingIcon: "down"
                text: qsTr("预设：") + veyra.currentPresetName
                tip: text
                maxTextWidth: head.compactActions ? 110 : 180
                onClicked: presetMenu.openAt(this, "down")
            }
            // PushFPS 4.1: the window as large as the screen, and back, in sight (PushMaximizeButton.qml).
            PushMaximizeButton { objectName: "pro-maximize" }
        }
    }

    // One geometry contract for this layout and Main's native video HWND.
    // These are available before the first layout pass when a source is opened.
    // (PushFPS 4.3.1) The settings side hidden with the title bar's button (the new design; never the
    // panel over the full-screen picture, which has its own way in and out, and never during the
    // tour, which points at the panel): the picture's column takes the panel's room.
    readonly property bool sideCollapsed: Theme.modern && !root.overlay && !root.onboardingActive
        && Theme.shot.pushSideCollapsed === true
    readonly property real videoColumnWidth: Math.max(1, root.width - 14 * 2 - (root.sideCollapsed ? 0 : root.inspectorWidth + 10))
    // PushFPS: the three meter cards show only while something plays (with nothing open they were a wall of
    // "Not measured"), and, since 2.2.0's layout, only when the picture keeps room beside them. The guided
    // tour shows them whatever plays.
    readonly property real metersHeight: Math.max(172, meter3Content.implicitHeight + 28, stageGrid.implicitHeight + 66)
    readonly property real pictureSpace: root.height - 14 * 2 - head.height - 10 - transport.height
    readonly property bool showMeters: root.onboardingActive || (veyra.hasSource && pictureSpace >= metersHeight + 10 + 80)
    readonly property bool metersShown: !root.overlay && root.showMeters
    readonly property real videoCardHeight: Math.max(transport.height + 1,
        root.height - 14 * 2 - head.height - 10 - (root.metersShown ? root.metersHeight + 10 : 0))
    readonly property rect videoViewport: Qt.rect(14, 14 + head.height + 10,
        root.videoColumnWidth, root.videoCardHeight - transport.height)

    // --- video + transport bar -------------------------------------------
    Rectangle {
        id: vwrap
        visible: !root.overlay
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.top: head.bottom
        anchors.topMargin: 10
        width: root.videoColumnWidth
        height: root.videoCardHeight
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke
        clip: true

        // The native video window sits over the top of this; the bar is its foot.
        Item {
            id: videoArea
            objectName: "videoArea"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height - transport.height
            // Drawing an NR protection region: the native video window lets the
            // pointer through, so this catches the drag; the outline itself is
            // drawn by the bridge on a layered child of the video window.
            MouseArea {
                id: protectionDraw
                objectName: "pro-protection-draw"
                anchors.fill: parent
                enabled: false // Main.qml owns mask input on every page and in fullscreen.
                visible: enabled
                cursorShape: Qt.CrossCursor
                preventStealing: true
                property point start
                // Fractions of the video host, whatever the page layout is.
                function hostFraction(x, y) {
                    const content = protectionDraw.Window.contentItem
                    let host = null
                    for (let i = 0; content && i < content.children.length; ++i)
                        if (content.children[i].objectName === "videoHost") host = content.children[i]
                    const p = protectionDraw.mapToItem(content, x, y)
                    if (!host || host.width <= 0 || host.height <= 0) return Qt.point(x / width, y / height)
                    return Qt.point((p.x - host.x) / host.width, (p.y - host.y) / host.height)
                }
                onPressed: mouse => { start = hostFraction(mouse.x, mouse.y) }
                onPositionChanged: mouse => {
                    const p = hostFraction(mouse.x, mouse.y)
                    veyra.updateProtectionDraw(start.x, start.y, p.x, p.y)
                }
                onReleased: mouse => {
                    const p = hostFraction(mouse.x, mouse.y)
                    veyra.commitProtectionDraw(start.x, start.y, p.x, p.y)
                }
            }
            // 分屏对比: drag the divider (the presenter draws it) across the picture.
            MouseArea {
                objectName: "pro-compare-drag"
                anchors.fill: parent
                enabled: veyra.compareMode === 2 && veyra.protectionDrawShape.length === 0
                visible: enabled
                cursorShape: Qt.SplitHCursor
                preventStealing: true
                function drag(mouse) { veyra.setCompareSplitAt(protectionDraw.hostFraction(mouse.x, mouse.y).x) }
                onPressed: mouse => drag(mouse)
                onPositionChanged: mouse => { if (pressed) drag(mouse) }
            }
            Shortcut {
                sequence: "Escape"
                enabled: root.visible && !root.onboardingActive && (!root.shell || !root.shell.fullscreen) &&
                         veyra.protectionDrawShape.length > 0
                onActivated: veyra.beginProtectionDraw("")
            }
            // Empty state: what to do next, not only a status line.
            ColumnLayout {
                objectName: "pro-empty-state"
                anchors.centerIn: parent
                visible: !veyra.hasSource
                spacing: 14
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 52; implicitHeight: 52; radius: 16
                    color: Theme.accentSoft
                    VIcon { anchors.centerIn: parent; name: "video"; size: 22; color: Theme.accent }
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: veyra.statusText
                    color: Theme.t2
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsH3
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    visible: veyra.liveOpeningText.length === 0
                    spacing: 8
                    VButton {
                        objectName: "pro-empty-capture"
                        text: qsTr("采集卡"); iconName: "video"; primary: true
                        onClicked: veyra.openCaptureDialog()
                    }
                    VButton {
                        objectName: "pro-empty-file"
                        text: qsTr("打开视频"); iconName: "folder"
                        onClicked: veyra.openFileDialog()
                    }
                }
            }
        }

        // Transport controls; source/output details are in the header.
        Item {
            id: transport
            objectName: "pro-transport"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            readonly property bool compact: width < 520
            height: compact ? 68 : 34
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            RowLayout {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.rightMargin: transport.compact ? 0 : playbackActions.width + 10
                height: 34
                spacing: 10
                Item {
                    implicitWidth: 28; implicitHeight: 28
                    VIcon {
                        anchors.centerIn: parent
                        name: (veyra.running && !veyra.paused) ? "pause" : "play"
                        color: Theme.t2
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: veyra.togglePlayPause() }
                }
                Text {
                    text: veyra.positionText
                    color: Theme.t2
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fsSmall
                }
                Item {
                    id: proSeekArea
                    objectName: "pro-seek"
                    Layout.fillWidth: true
                    implicitHeight: 14
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 3
                        radius: 9
                        color: Theme.ink(0.14)
                        Rectangle {
                            width: parent.width * proSeekArea.frac
                            height: parent.height
                            radius: 9
                            color: Theme.t1
                        }
                    }
                    // Press or drag anywhere on the rail: the picture follows the
                    // pointer (a seek every 120 ms) and lands on release.
                    property bool scrubbing: false
                    property real scrubFrac: 0
                    readonly property real frac: scrubbing ? scrubFrac : Math.max(0, Math.min(1, veyra.progress))
                    Rectangle {
                        width: 11; height: 11; radius: 5.5
                        color: Theme.t1
                        anchors.verticalCenter: parent.verticalCenter
                        x: proSeekArea.frac * proSeekArea.width - width / 2
                        visible: proSeekMouse.containsMouse || proSeekArea.scrubbing
                    }
                    Timer {
                        id: proScrubSeek
                        // Restarted on every move, so it fires only once the drag settles
                        // (see CineBar.qml for why overlapping seeks break playback).
                        interval: 250
                        onTriggered: if (proSeekArea.scrubbing) veyra.seekTo(veyra.timelineSeconds(proSeekArea.scrubFrac))
                    }
                    MouseArea {
                        id: proSeekMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: veyra.timelineSeekable
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true
                        function fracAt(x) { return Math.max(0, Math.min(1, x / Math.max(1, proSeekArea.width))) }
                        onPressed: mouse => { proSeekArea.scrubFrac = fracAt(mouse.x); proSeekArea.scrubbing = true }
                        onPositionChanged: mouse => {
                            if (!proSeekArea.scrubbing) return
                            proSeekArea.scrubFrac = fracAt(mouse.x)
                            proScrubSeek.restart()
                        }
                        onReleased: mouse => {
                            proScrubSeek.stop()
                            const target = veyra.timelineSeconds(fracAt(mouse.x))
                            proSeekArea.scrubbing = false
                            veyra.seekTo(target)
                        }
                        onCanceled: { proScrubSeek.stop(); proSeekArea.scrubbing = false }
                    }
                }
                Text {
                    text: veyra.durationText
                    visible: !veyra.liveTimeshift.available
                    color: Theme.t2
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fsSmall
                }
                VButton {
                    objectName: "pro-return-live"
                    visible: veyra.liveTimeshift.available
                    ghost: true
                    text: qsTr("回到直播")
                    onClicked: veyra.returnToLive()
                }
            }
            RowLayout {
                id: playbackActions
                objectName: "pro-transport-actions"
                anchors.right: parent.right
                y: transport.compact ? 34 : 0
                height: 34
                spacing: 10
                PlaybackRateButton { objectName: "pro-playback-rate" }
                RotateButton { objectName: "pro-rotate" }
                VTag {
                    objectName: "pro-fg-tag"
                    visible: veyra.hasSource
                    kind: veyra.fgEnabled ? "acc" : ""
                    text: veyra.fgEnabled ? veyra.fgMultiplier + "X" : qsTr("补帧关")
                }
                // Subtitle and audio-track menus (field request 2026-10-01), the same menus as the
                // 极简 pill's, beside the fullscreen button.
                component BarButton: Item {
                    id: barBtn
                    property string glyph: ""
                    property string tip: ""
                    signal tapped()
                    implicitWidth: 28; implicitHeight: 28
                    VIcon { anchors.centerIn: parent; name: barBtn.glyph; color: barHover.hovered ? Theme.t1 : Theme.t2 }
                    HoverHandler { id: barHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: barBtn.tapped() }
                    ToolTip.visible: barHover.hovered
                    ToolTip.text: barBtn.tip
                }
                BarButton {
                    id: proCcButton
                    objectName: "pro-subtitles"
                    glyph: "cc"
                    tip: qsTr("字幕")
                    visible: veyra.hasSource && !veyra.isCapture
                    onTapped: proCcMenu.openAt(proCcButton, "up")
                }
                BarButton {
                    id: proAudioButton
                    objectName: "pro-audio-tracks"
                    glyph: "music"
                    tip: qsTr("音轨")
                    visible: veyra.hasSource && !veyra.isCapture
                    onTapped: proAudioMenu.openAt(proAudioButton, "up")
                }
                // Fullscreen, at the picture's bottom-right corner where players put it.
                Item {
                    objectName: "pro-fullscreen"
                    implicitWidth: 28; implicitHeight: 28
                    VIcon { anchors.centerIn: parent; name: "max"; color: fullHover.hovered ? Theme.t1 : Theme.t2 }
                    HoverHandler { id: fullHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.requestFullscreen() }
                    ToolTip.visible: fullHover.hovered
                    ToolTip.text: qsTr("全屏（双击画面 / F11）· 全屏后按 %1 打开快速调节").arg(veyra.gameMode ? veyra.shortcuts.gamePanel : "Home")
                }
            }
        }
    }

    // --- meters (at least 172px, three cards) -----------------------------
    RowLayout {
        id: meters
        visible: root.metersShown
        anchors.left: vwrap.left
        anchors.top: vwrap.bottom
        anchors.topMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: vwrap.width
        spacing: 10

        // Card 1: signal chain. Reported values only.
        // A narrow picture column (wide inspector, small window) drops this card
        // first, so the stage timings beside it keep room to read.
        Rectangle {
            id: meter1
            visible: meters.width >= 850
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 192
            // Use pixel widths in the same range as the minimum. A ratio
            // below the minimum is clamped and would squeeze the other cards.
            Layout.preferredWidth: 210
            radius: Theme.rCard
            color: Theme.card
            border.width: 1
            border.color: Theme.stroke
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    VEyebrow { Layout.fillWidth: true; text: qsTr("信号") }
                    // 刷新显示器状态: Windows HDR, refresh rate and variable refresh are
                    // read on events only (startup, a display or monitor change, the
                    // 可变刷新 switch turned on); this asks the driver again.
                    Item {
                        implicitWidth: 22; implicitHeight: 22
                        VButton {
                            objectName: "pro-display-refresh"
                            anchors.fill: parent
                            visible: !veyra.displayChecking
                            icon: true; ghost: true; iconName: "refresh"; tip: qsTr("刷新显示器状态")
                            onClicked: veyra.recheckDisplay()
                        }
                        VSpinner { anchors.centerIn: parent; visible: veyra.displayChecking }
                    }
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 14
                    rowSpacing: 4
                    Text { text: qsTr("输入"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        Layout.fillWidth: true
                        text: veyra.sourceSummary.length > 0
                              ? veyra.sourceSummary + " · " + veyra.sourceFps.toFixed(2)
                              : "—"
                        color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                    Text { text: qsTr("输出"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        Layout.fillWidth: true
                        text: veyra.outputSummary.length > 0 ? veyra.outputSummary : "—"
                        color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                    Text { text: qsTr("光流"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        Layout.fillWidth: true
                        text: veyra.flowBackend.length > 0 ? veyra.flowBackend : qsTr("未执行")
                        color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                    Text { text: qsTr("色彩"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        Layout.fillWidth: true
                        text: veyra.colorStatus.length > 0 ? veyra.colorStatus : "—"
                        color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                    // The shown monitor: the video's while playing, otherwise the window's.
                    Text { text: qsTr("系统 HDR"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        objectName: "pro-display-hdr"
                        Layout.fillWidth: true
                        text: veyra.displayHdr > 0 ? qsTr("开") : veyra.displayHdr === 0 ? qsTr("关") : qsTr("未知")
                        color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                    }
                    Text { text: qsTr("可变刷新"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        objectName: "pro-display-vrr"
                        Layout.fillWidth: true
                        text: veyra.vrrSummary + (veyra.displayRefreshHz > 0 ? " · " + veyra.displayRefreshHz.toFixed(0) + " Hz" : "")
                        color: veyra.vrrState >= 4 ? Theme.ok : Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }
                    // Network / server playback (#48): buffer, download speed, reconnects.
                    Text { visible: veyra.networkActive; text: qsTr("网络"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                    Text {
                        objectName: "pro-network-summary"
                        visible: veyra.networkActive
                        Layout.fillWidth: true
                        text: veyra.networkSummary
                        color: veyra.networkHolding ? Theme.warn : Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                        horizontalAlignment: Text.AlignRight
                        // The card is narrow: the three figures wrap instead of losing the last one.
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }
                }
                Item { Layout.fillHeight: true }
            }
        }

        // Card 2: per-stage GPU cost. Only stages the engine actually timed are
        // listed; a stage with no measurement is drawn as unmeasured.
        Rectangle {
            id: meter2
            objectName: "pro-meter-stages"
            // Too narrow for label, bar and value side by side in two columns: the
            // bars and the budget note go, the values stay.
            readonly property bool narrow: width < 380
            clip: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 300
            radius: Theme.rCard
            color: Theme.card
            border.width: 1
            border.color: Theme.stroke
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 5
                RowLayout {
                    Layout.fillWidth: true
                    VEyebrow { text: qsTr("GPU 阶段耗时 · 最近一秒平均"); Layout.fillWidth: true }
                    Text {
                        visible: !meter2.narrow
                        text: qsTr("预算 ") + veyra.stageBudgetMs.toFixed(1) + qsTr(" ms / 源帧")
                        color: Theme.t3
                        font.family: Theme.fontMono
                        font.pixelSize: 11
                    }
                }
                // Switch to one column when two complete translated readings do not fit.
                GridLayout {
                    id: stageGrid
                    objectName: "pro-stage-grid"
                    // The label column fits the longest stage name in the interface
                    // language (52px held two Chinese characters; translations run longer).
                    FontMetrics { id: stageLabelMetrics; font.family: Theme.fontUi; font.pixelSize: Theme.fsSmall }
                    readonly property real stageLabelWidth: {
                        let w = 52
                        for (const r of veyra.stageTimings) w = Math.max(w, Math.ceil(stageLabelMetrics.advanceWidth(r.label || "")) + 6)
                        return w
                    }
                    // The value column fits "000.0 ms" and the unmeasured word in the interface
                    // language (a fixed 48px let the English "Not measured" run out of the card).
                    FontMetrics { id: stageValueMetrics; font.family: Theme.fontMono; font.pixelSize: 11 }
                    readonly property real stageValueWidth: Math.min(120, Math.ceil(Math.max(stageValueMetrics.advanceWidth("000.0 ms"), stageValueMetrics.advanceWidth(qsTr("未测量")))) + 2)
                    Layout.fillWidth: true
                    columns: meter2.width - 28 >= 2 * (stageLabelWidth + stageValueWidth + 16) + 16 ? 2 : 1
                    columnSpacing: 16
                    rowSpacing: 6
                    flow: GridLayout.TopToBottom
                    rows: Math.max(1, Math.ceil(veyra.stageTimings.length / columns))
                // The model is the row count, not the list: the list is a new array on
                // every snapshot, and a list model rebuilt the rows each time, so every
                // bar restarted from 0 (the 0 / 5.8 / 0 / 5.8 flicker in the field).
                Repeater {
                    model: veyra.stageTimings.length
                    delegate: RowLayout {
                        id: stageRow
                        required property int index
                        readonly property var modelData: veyra.stageTimings[index] || ({ label: "", ms: 0, fraction: 0, measured: false, color: "transparent" })
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        spacing: 8
                        Text {
                            Layout.preferredWidth: stageGrid.stageLabelWidth
                            Layout.minimumWidth: Math.min(stageGrid.stageLabelWidth, Math.max(0, stageRow.width - stageGrid.stageValueWidth - 16))
                            text: modelData.label
                            color: Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsSmall
                            elide: Text.ElideRight
                        }
                        Item {
                            Layout.fillWidth: true
                            visible: !meter2.narrow
                            implicitHeight: 5
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 5
                                radius: 9
                                color: Theme.ink(0.08)
                                Rectangle {
                                    objectName: "stage-bar-fill"
                                    width: root.barsArmed ? Math.round(parent.width * (root.barsSettled ? liveFraction.value : Math.max(0, Math.min(1, modelData.fraction)))) : 0
                                    height: parent.height
                                    radius: 9
                                    color: modelData.color
                                    VTelemetryValue {
                                        id: liveFraction
                                        targetValue: Math.max(0, Math.min(1, modelData.fraction))
                                        animate: root.barsSettled && stageRow.visible && !Theme.reduced
                                        duration: 800
                                    }
                                    Behavior on width { enabled: stageRow.visible && !root.barsSettled; SequentialAnimation {
                                        PauseAnimation { duration: root.barsSettled ? 0 : Theme.d(120 + stageRow.index * 60) }
                                        NumberAnimation { duration: Theme.d(800); easing.bezierCurve: Theme.springSoft } } }
                                }
                            }
                        }
                        Text {
                            Layout.preferredWidth: stageRow.parent ? stageRow.parent.stageValueWidth : 48
                            Layout.minimumWidth: Math.min(stageGrid.stageValueWidth, stageRow.width * 0.54)
                            elide: Text.ElideRight
                            text: modelData.measured ? modelData.ms.toFixed(1) + " ms" : qsTr("未测量")
                            HoverHandler { id: stageValueHover }
                            ToolTip.visible: stageValueHover.hovered && modelData.measured
                            ToolTip.text: qsTr("平均 ") + modelData.ms.toFixed(2) + " ms · P95 " + (modelData.p95 || 0).toFixed(2) + qsTr(" ms（最近一秒）")
                            color: modelData.measured ? Theme.t1 : Theme.t3
                            font.family: Theme.fontMono
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
                }
                Item { Layout.fillHeight: true }
            }
        }

        // Card 3: frame rate and cadence.
        Rectangle {
            id: meter3
            readonly property bool wide: width >= 380
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 230
            radius: Theme.rCard
            color: Theme.card
            border.width: 1
            border.color: Theme.stroke
            ColumnLayout {
                id: meter3Content
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 6
                        VEyebrow { text: qsTr("帧率与节奏") }

                        // The submit rate is the headline, as an overlay such as RTSS shows
                        // it: both count Present calls. Panel scanout is not measured, so
                        // there is no separate display rate (it only ever said 未测). The
                        // 1% low is the 99th-percentile present interval of the last 10 s.
                        RowLayout {
                            spacing: 6
                            Text {
                                objectName: "fps-submit"
                                text: veyra.submitFpsKnown ? veyra.submitFps.toFixed(1) : qsTr("未测")
                                color: veyra.submitFpsKnown ? Theme.ok : Theme.t3
                                font.family: Theme.fontMono
                                font.pixelSize: 22
                                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                            }
                            Text { text: veyra.submitFpsLabel + " fps"; color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11 }
                            Rectangle { implicitWidth: 1; implicitHeight: 12; color: Theme.stroke2 }
                            Text {
                                objectName: "fps-low1"
                                text: veyra.submitLowFpsKnown ? veyra.submitLowFps.toFixed(1) : "—"
                                color: Theme.t1
                                font.family: Theme.fontMono
                                font.pixelSize: 13
                                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                            }
                            Text { text: qsTr("1% low"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11 }
                        }
                        // Frame-time line (design .spark): submit interval over the last
                        // 12 s, the dashed line is the source frame budget.
                        Canvas {
                            id: spark
                            objectName: "fps-sparkline"
                            visible: meter3.wide
                            Layout.fillWidth: true
                            Layout.preferredHeight: 24
                            readonly property var points: veyra.frameTimes
                            onPointsChanged: requestPaint()
                            onWidthChanged: requestPaint()
                            onPaint: {
                                const g = getContext("2d"); g.reset()
                                const pts = points || []
                                const budget = veyra.stageBudgetMs
                                if (pts.length < 2) return
                                let lo = budget > 0 ? budget : pts[0], hi = lo
                                for (const v of pts) { lo = Math.min(lo, v); hi = Math.max(hi, v) }
                                const pad = Math.max(0.5, (hi - lo) * 0.2); lo -= pad; hi += pad
                                const y = v => height - 2 - (v - lo) / (hi - lo) * (height - 4)
                                if (budget > 0) {
                                    g.strokeStyle = Theme.ink(0.12); g.lineWidth = 1; g.setLineDash([3, 3])
                                    g.beginPath(); g.moveTo(0, y(budget)); g.lineTo(width, y(budget)); g.stroke(); g.setLineDash([])
                                }
                                const x = i => i / 47 * width + (48 - pts.length) / 47 * width
                                const grad = g.createLinearGradient(0, 0, 0, height)
                                grad.addColorStop(0, Qt.rgba(61 / 255, 220 / 255, 132 / 255, 0.25)); grad.addColorStop(1, Qt.rgba(61 / 255, 220 / 255, 132 / 255, 0))
                                g.beginPath(); pts.forEach((v, i) => i ? g.lineTo(x(i), y(v)) : g.moveTo(x(i), y(v)))
                                g.lineTo(x(pts.length - 1), height); g.lineTo(x(0), height); g.closePath(); g.fillStyle = grad; g.fill()
                                g.beginPath(); pts.forEach((v, i) => i ? g.lineTo(x(i), y(v)) : g.moveTo(x(i), y(v)))
                                g.strokeStyle = Theme.ok; g.lineWidth = 1.5; g.stroke()
                            }
                        }
                        GridLayout {
                            visible: meter3.wide
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 14
                            rowSpacing: 3
                            Text { text: qsTr("进程内待呈现"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                            Text {
                                Layout.fillWidth: true
                                text: veyra.queuedFramesKnown ? veyra.queuedFrames.toFixed(0) + qsTr(" 帧") : qsTr("未测")
                                color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                            }
                            Text { text: qsTr("跳过源帧"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12 }
                            Text {
                                Layout.fillWidth: true
                                text: String(veyra.skippedFrames)
                                color: Theme.t1; font.family: Theme.fontMono; font.pixelSize: 12
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                        PerfOrbs { visible: !meter3.wide; size: 38; spacing: 6 }
                    }
                    PerfOrbs { visible: meter3.wide; size: 50; Layout.alignment: Qt.AlignVCenter }
                }
                StatusSummary { Layout.fillWidth: true }
            }
        }
    }

    // --- inspector (376px by default, rows 2-3) ----------------------------
    // The panel can be widened by dragging the gap between it and the picture
    // (field request 2026-10-05); the width is remembered, a double click on the
    // gap returns to the design's 376px. It never goes narrower than that, and
    // the picture keeps at least 640px, where the meters under it still read.
    // PushFPS: 420 by default (the design's 376 squeezed every hint into a narrow
    // column); 376 stays the minimum and the splitter still goes either way.
    readonly property real inspectorDefault: Math.min(420, inspectorMax)
    readonly property real inspectorMax: Math.max(376, root.width - 14 * 2 - 10 - 640)
    property real inspectorDrag: -1
    readonly property real inspectorWidth: Math.max(376, Math.min(inspectorMax,
        inspectorDrag >= 0 ? inspectorDrag : Number(veyra.preferences.listInspectorWidth || root.inspectorDefault)))
    // PushFPS 4.2.1: the panel over the full-screen picture is as wide as its right edge is dragged
    // (a user's request; it was 376 and nothing else). 376 stays what it is until it is dragged, the
    // picture keeps at least two fifths of the window, and the width has its own preference: the
    // docked panel's width is a share of a window, this one lies over the picture.
    readonly property real overlayGrip: 8
    readonly property real overlayMax: Math.max(376, Math.min(root.width * 0.6, root.width - 16 * 2 - overlayGrip - 240))
    property real overlayDrag: -1
    readonly property real overlayWidth: Math.max(376, Math.min(overlayMax,
        overlayDrag >= 0 ? overlayDrag : Number(veyra.preferences.pushOverlayPanelWidth || 376)))
    MouseArea {
        id: inspectorSplit
        objectName: "pro-inspector-split"
        visible: !root.overlay && !root.sideCollapsed
        x: insp.x - 10
        y: insp.y
        width: 10
        height: insp.height
        cursorShape: Qt.SplitHCursor
        hoverEnabled: true
        preventStealing: true
        property real pressX: 0
        property real pressWidth: 0
        onPressed: mouse => {
            pressX = mapToItem(root, mouse.x, 0).x
            pressWidth = root.inspectorWidth
            root.inspectorDrag = pressWidth
        }
        onPositionChanged: mouse => {
            if (!pressed) return
            root.inspectorDrag = Math.max(376, Math.min(root.inspectorMax, pressWidth - (mapToItem(root, mouse.x, 0).x - pressX)))
        }
        onReleased: {
            const width = Math.round(root.inspectorWidth)
            root.inspectorDrag = -1
            veyra.setPreference("listInspectorWidth", width)
        }
        onCanceled: root.inspectorDrag = -1
        onDoubleClicked: veyra.setPreference("listInspectorWidth", root.inspectorDefault)
        // A hairline handle while the pointer is on the gap.
        Rectangle {
            anchors.centerIn: parent
            width: 3
            height: 36
            radius: 1.5
            color: inspectorSplit.pressed ? Theme.accent : Theme.ink(0.28)
            opacity: inspectorSplit.containsMouse || inspectorSplit.pressed ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.d(150) } }
        }
        // Beside the pointer: the attached tip sat at the top of this full-height strip.
        ToolTip {
            visible: inspectorSplit.containsMouse && !inspectorSplit.pressed
            delay: 600
            x: -implicitWidth - 6
            y: inspectorSplit.mouseY + 16
            text: qsTr("拖动调整参数面板宽度 · 双击还原")
        }
    }
    Rectangle {
        id: insp
        objectName: "pro-inspector"
        visible: !root.sideCollapsed
        readonly property bool videoCover: root.overlay
        property real coverRadius: radius
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.top: head.bottom
        anchors.topMargin: 10
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: root.overlay ? root.overlayWidth + root.overlayGrip : root.inspectorWidth
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke
        clip: true
        // Quick-adjust: the same panel on the left edge of the fullscreen picture.
        states: State {
            name: "overlay"
            when: root.overlay
            AnchorChanges { target: insp; anchors.right: undefined; anchors.left: root.left; anchors.top: root.top }
            PropertyChanges { target: insp; anchors.leftMargin: 16; anchors.topMargin: 16; anchors.bottomMargin: 16 }
        }
        // A click on the panel's own background is not a click on the picture.
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: wheel => wheel.accepted = true }
        // PushFPS 4.2.1: the overlay panel's right edge is a grip. It stands inside the panel (a strip
        // of its own beside the rows, so the scroll bar keeps its place): what is drawn outside a
        // cover is behind the picture.
        MouseArea {
            id: overlaySplit
            objectName: "pro-overlay-split"
            visible: root.overlay
            z: 10
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.overlayGrip
            cursorShape: Qt.SplitHCursor
            hoverEnabled: true
            preventStealing: true
            property real pressX: 0
            property real pressWidth: 0
            // A double click puts the width back. Its second press is a press too and its release a
            // release, so that release must not write the width the press started from.
            property bool restored: false
            onPressed: mouse => {
                restored = false
                pressX = mapToItem(root, mouse.x, 0).x
                pressWidth = root.overlayWidth
                root.overlayDrag = pressWidth
            }
            onPositionChanged: mouse => {
                if (!pressed || restored) return
                root.overlayDrag = Math.max(376, Math.min(root.overlayMax, pressWidth + (mapToItem(root, mouse.x, 0).x - pressX)))
            }
            onReleased: {
                if (restored) { restored = false; root.overlayDrag = -1; return }
                const width = Math.round(root.overlayWidth)
                root.overlayDrag = -1
                veyra.setPreference("pushOverlayPanelWidth", width)
            }
            onCanceled: { restored = false; root.overlayDrag = -1 }
            onDoubleClicked: {
                restored = true
                root.overlayDrag = -1
                veyra.setPreference("pushOverlayPanelWidth", 376)
            }
            Rectangle {
                anchors.centerIn: parent
                width: 3
                height: 36
                radius: 1.5
                color: overlaySplit.pressed ? Theme.accent : Theme.ink(0.28)
                opacity: overlaySplit.containsMouse || overlaySplit.pressed ? 1 : 0.55
                Behavior on opacity { NumberAnimation { duration: Theme.d(150) } }
            }
            ToolTip {
                visible: overlaySplit.containsMouse && !overlaySplit.pressed
                delay: 600
                x: overlaySplit.width + 6
                y: overlaySplit.mouseY + 16
                text: qsTr("拖动调整参数面板宽度 · 双击还原")
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: root.tab === "color" ? insp.border.width : 0
            anchors.rightMargin: (root.tab === "color" ? insp.border.width : 0) + (root.overlay ? root.overlayGrip : 0)
            spacing: 0

            // .insp-tabs: five tabs, equal width, 30px, radius 9.
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 8
                spacing: 2
                Repeater {
                    model: (Theme.modern ? [{ id: "all", label: qsTr("全部") }] : []).concat([
                        { id: "quality", label: qsTr("画质") },
                        { id: "fg", label: qsTr("补帧") },
                        { id: "color", label: qsTr("色彩") },
                        { id: "audio", label: qsTr("声音") },
                        { id: "display", label: qsTr("显示") }
                    ])
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitWidth: tabLabel.implicitWidth + 10
                        implicitHeight: Math.max(30, tabLabel.implicitHeight + 8)
                        radius: 9
                        objectName: "pro-tab-" + modelData.id
                        color: root.tab === modelData.id ? Theme.card3
                             : tabHover.hovered ? Theme.ink(0.04) : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.d(200) } }
                        // (The new design: the edge lights up in the accent under the pointer.)
                        border.width: Theme.modern ? 1 : 0
                        border.color: Theme.modern && tabHover.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.7) : "transparent"
                        Behavior on border.color { ColorAnimation { duration: Theme.d(160) } }
                        Text {
                            id: tabLabel
                            width: Math.max(1, parent.width - 6)
                            anchors.centerIn: parent
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                            text: modelData.label
                            color: root.tab === modelData.id ? Theme.t1 : Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsBody
                            font.weight: Font.Medium
                        }
                        HoverHandler { id: tabHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: root.tab = modelData.id }
                    }
                }
            }

            // .chainbar: the processing order, chips joined by arrows. Fixed order:
            // 补帧 is always last and RTX Video HDR sits immediately before it.
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                implicitHeight: chainCol.implicitHeight + 20
                objectName: "pro-processing-order"
                // The design draws the chain bar on the 画质 tab only.
                // Leave the controls a usable scroll area in short windows.
                visible: (root.tab === "quality" || root.allTabs) && insp.height >= 240
                radius: 12
                color: Theme.ink(0.03)
                border.width: 1
                border.color: Theme.stroke
                ColumnLayout {
                    id: chainCol
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 7
                    RowLayout {
                        Layout.fillWidth: true
                        VEyebrow { text: qsTr("处理顺序"); Layout.fillWidth: true; Layout.minimumWidth: 0; wrapMode: Text.Wrap }
                        Text { text: qsTr("点击定位"); color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 11 }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 2
                        // .c.io: the chain starts at the input, which is not a node.
                        Rectangle {
                            width: inChip.implicitWidth + 18
                            height: 24
                            radius: 99
                            color: "transparent"
                            border.width: 1
                            border.color: Theme.stroke
                            Text {
                                id: inChip
                                anchors.centerIn: parent
                                text: qsTr("输入")
                                color: Theme.t3
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                            }
                        }
                        Repeater {
                            model: veyra.chain
                            delegate: Row {
                                required property var modelData
                                spacing: 2
                                Text { text: "›"; color: Theme.t3; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                                // PushFPS: a stage that is on is tinted green; one that is off is
                                // a quiet outline (struck-through labels read as errors).
                                Rectangle {
                                    width: chipLabel.implicitWidth + 18
                                    height: 24
                                    radius: 99
                                    color: modelData.enabled ? Theme.accentSoft : "transparent"
                                    border.width: 1
                                    border.color: modelData.enabled ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35) : Theme.stroke
                                    Behavior on color { ColorAnimation { duration: Theme.d(200) } }
                                    Text {
                                        id: chipLabel
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: modelData.enabled ? Theme.t1 : Theme.t3
                                        font.family: Theme.fontUi
                                        font.pixelSize: 12
                                    }
                                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                                    TapHandler { gesturePolicy: TapHandler.WithinBounds; onTapped: root.locateNode(modelData) }
                                }
                            }
                        }
                    }
                }
            }

            // Body. Each tab shows the controls the engine actually has; where the
            // engine has no control yet, the panel says so instead of offering one
            // that would do nothing.
            Flickable {
                id: inspFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.topMargin: 8
                contentHeight: body.implicitHeight
                clip: true
                ScrollBar.vertical: VScrollBar {
                    id: inspectorScroll
                    objectName: "pro-inspector-scrollbar"
                    wide: root.tab === "color"
                }

                ColumnLayout {
                    id: body
                    // CSS thin scrollbars consume 10px on the reference host;
                    // reserve their gutter instead of drawing over the colour card.
                    width: parent.width - 20 - (root.tab === "color" && inspectorScroll.size < 1 ? inspectorScroll.implicitWidth : 0)
                    x: 10
                    spacing: 6

                    // --- 画质 -------------------------------------------------
                    PushFoldHead { objectName: "pro-all-quality"; visible: root.allTabs; text: qsTr("画质"); open: root.blockOpen("quality"); onToggled: open => root.setBlockOpen("quality", open) }
                    ColumnLayout {
                        id: qualityCol
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.blockShown("quality")
                        // (The new design: quick setup's Picture card, with AI picture enhancement, comes first.)
                        Item {
                            id: pictureFirst
                            objectName: "list-pushfps-picture-first"
                            visible: Theme.modern
                            Layout.fillWidth: true
                            implicitHeight: pictureCard.implicitHeight
                            property real motionDy: 0
                            transform: Translate { y: pictureFirst.motionDy }
                        }

                        // The design builds the inspector from accordion cards, not
                        // flat rows: an icon, a title, a live summary line, a switch
                        // and a chevron, with nested groups inside.
                        // Spatial anti-aliasing (#53) runs at source size right after the
                        // input conversion, in front of SR, so jaggies are never enlarged.
                        VAccordion {
                            objectName: "list-anti-alias"
                            Layout.fillWidth: true
                            glyph: "type"
                            hue: "#7FD4FF"
                            title: qsTr("抗锯齿")
                            summary: veyra.antiAliasEnabled ? veyra.antiAliasParams.summary : qsTr("已关闭")
                            on: veyra.antiAliasEnabled
                            onToggled: on => veyra.antiAliasEnabled = on
                            AntiAliasEditor {
                                Layout.fillWidth: true
                                namePrefix: "list-aa"
                                params: veyra.antiAliasParams
                                onEdited: (key, amount) => veyra.setAntiAliasParameter(key, amount)
                            }
                        }

                        VAccordion {
                            id: srCard
                            objectName: "list-sr"
                            Layout.fillWidth: true
                            glyph: "sparkles"
                            hue: "#4F7BFF"
                            title: qsTr("超分辨率")
                            switchAvailable: Support.available(veyra, "sr")
                            summary: !switchAvailable ? Support.reason(veyra, "sr") : (veyra.srEnabled ? (veyra.srBackendChoices?.find(o => o.id === veyra.srAlgorithmOf(veyra.videoSrQuality))?.label || qsTr("RTX 视频超分")) : qsTr("已关闭"))
                                     + " · " + veyra.srTargetLabel
                                     + (supersampleShown ? "→" + (veyra.srOutput === 1 ? qsTr("片源尺寸") : ["", "", "2K", "4K", "5K", "6K", "7K"][veyra.srOutput]) : "")
                                     + (dualShown ? "→" + (veyra.srPrescale >= 150 ? (veyra.srPrescale - 100) + "%" : ["", "720p", "900p", "1080p", "1440p"][veyra.srPrescale]) + "→" + ["", "2K", "4K", "8K", "5K", "6K", "7K"][veyra.dualSrTargetIndex] : "")
                                     + (rtxVideoSr ? " · " + veyra.srQualityLabel(veyra.videoSrQuality, false) + (dualShown ? " / " + secondLabel : "")
                                        : dualShown ? " · " + qsTr("第二次 %1").arg(secondLabel) : "")
                            // Base target, then the supersample shrink (RTX Video SR), or the double SR
                            // reduction and second target while double SR is on (any algorithm), then the
                            // tier and the second algorithm. Each kept value is hidden while its mode is off.
                            readonly property bool rtxVideoSr: veyra.srAlgorithmOf(veyra.videoSrQuality).startsWith("rtx")
                            readonly property bool supersampleShown: !veyra.dualSrEnabled && veyra.srOutput > 0 && rtxVideoSr
                            readonly property bool dualShown: veyra.dualSrEnabled && veyra.srPrescale > 0
                            // "高码率 超高" for an RTX family, the algorithm name for DLSS SR / FSR.
                            readonly property string secondLabel: veyra.srQualityLabel(veyra.dualSrQuality, true)
                                || veyra.srBackendChoices?.find(o => o.id === veyra.srAlgorithmOf(veyra.dualSrQuality))?.label || ""
                            on: veyra.srEnabled
                            open: true
                            onToggled: on => veyra.srEnabled = on

                            VRow {
                                label: qsTr("超分算法")
                                VSelect {
                                    objectName: "list-sr-backend"
                                    implicitWidth: 210 // the RTX Video SR family names need more than the default 150
                                    options: veyra.srBackendChoices || []
                                    value: options.find(o => o.id === veyra.srAlgorithmOf(veyra.videoSrQuality))?.label || ""
                                    // Switching between RTX families keeps the tier.
                                    onPicked: id => veyra.videoSrQuality = veyra.srQualityFor(id, veyra.srTierOf(veyra.videoSrQuality))
                                }
                            }
                            VRow {
                                label: qsTr("DLSS 模型")
                                hint: qsTr("K 保留原有模型；M、L 为 DLSS 4.5。L 针对较低输入分辨率，实际画质与耗时取决于片源。")
                                // Also the model of a DLSS SR second pass under double SR.
                                visible: veyra.videoSrQuality === 0 || (veyra.dualSrEnabled && veyra.dualSrQuality === 0)
                                enabled: Support.available(veyra, "sr0")
                                VSelect {
                                    objectName: "list-dlss-sr-model"
                                    options: veyra.dlssSrModelChoices || []
                                    value: options.find(o => o.id === String(veyra.dlssSrModel))?.label || ""
                                    onPicked: id => veyra.dlssSrModel = Number(id)
                                }
                            }
                            SrStageEditor { Layout.fillWidth: true; namePrefix: "list" }


                        }

                        // RTX Video HDR: SDR -> HDR before frame generation. Runs
                        // only on an HDR display path; the status says why not.
                        VAccordion {
                            id: hdrCard
                            objectName: "list-video-hdr"
                            Layout.fillWidth: true
                            glyph: "sun"
                            hue: "#E58BD9"
                            title: "RTX Video HDR"
                            switchAvailable: Support.available(veyra, "video-hdr")
                            summary: !switchAvailable ? Support.reason(veyra, "video-hdr") : veyra.videoHdr ? (veyra.videoHdrStatus.length > 0 ? veyra.videoHdrStatus : qsTr("已开启")) : qsTr("已关闭")
                            on: veyra.videoHdr
                            onToggled: on => veyra.videoHdr = on
                            // RTX Video HDR is an SDR -> HDR stage: an HDR source
                            // keeps the native HDR path (and the HDR brightness
                            // management), so there is no "convert HDR sources" switch.
                            Repeater {
                                model: [{key:"contrast",label:qsTr("对比度"),from:0,to:200,def:125},
                                        {key:"saturation",label:qsTr("饱和度"),from:0,to:200,def:75},
                                        {key:"middleGray",label:qsTr("中灰"),from:10,to:100,def:44},
                                        {key:"peakNits",label:qsTr("峰值亮度 (nit)"),from:400,to:2000,def:1000}]
                                delegate: VRow {
                                    required property var modelData
                                    enabled: Support.available(veyra, "video-hdr")
                                    opacity: enabled ? 1 : 0.4
                                    label: modelData.label
                                    // modelData.scale turns an integer parameter into a
                                    // readable number (exposure is 1/100 EV); the unit
                                    // lives in the label, so every value stays short
                                    // enough for the rows to line up.
                                    value: modelData.scale
                                        ? (Number(veyra.videoHdrParams[modelData.key] ?? modelData.def) / modelData.scale).toFixed(2)
                                        : String(veyra.videoHdrParams[modelData.key] ?? "—")
                                    VSlider {
                                        objectName: "list-hdr-" + modelData.key
                                        implicitWidth: 120
                                        valueFromModel: true
                                        resettable: true; defaultValue: modelData.def
                                        from: modelData.from; to: modelData.to
                                        value: veyra.videoHdrParams[modelData.key] ?? modelData.def
                                        onMoved: value => veyra.setVideoHdrParameter(modelData.key, Math.round(value))
                                    }
                                }
                            }
                        }

                        // PushFPS 4.2.1: quick setup's Picture section (the look of AI picture enhancement,
                        // its size and fine-tuning, the wide-screen fill, the crop) stands here, over the
                        // enhancement's own card, and no longer in the Frame Gen tab: a user asked for the
                        // enhancement's settings to be in one place.
                        // (The redesign: in the new design this card is the first thing in the list, so AI
                        // picture enhancement is where the eye lands; in Legacy it stands here, as in 4.2.1.
                        // The card is one item drawn in one of two places: pictureFirst above, pictureHere.)
                        Item {
                            id: pictureHere
                            visible: !Theme.modern
                            Layout.fillWidth: true
                            implicitHeight: pictureCard.implicitHeight
                            // (The tab's cards come in one after another, VRise.Stagger: the place takes
                            // the card's part in that, as the card did when it stood here itself.)
                            property real motionDy: 0
                            transform: Translate { y: pictureHere.motionDy }
                        }
                        PushQuickSetup {
                            id: pictureCard
                            objectName: "list-pushfps-picture"
                            parent: Theme.modern ? pictureFirst : pictureHere
                            width: parent ? parent.width : 0
                            compactPro: true
                            scope: "picture"
                        }

                        // .stage: one NR card, its layers inside (pages-pro.js nrStage):
                        // "N 层依次处理 · 每层参数独立", a bar per layer lit when it
                        // runs, the add button at the bottom of the stage.
                        VAccordion {
                            id: nrStage
                            objectName: "list-nr-stage"
                            Layout.fillWidth: true
                            glyph: "wand"
                            hue: Theme.accent
                            title: qsTr("NR 画面增强")
                            summary: veyra.nrLayers.length > 0 ? veyra.nrLayers.length + qsTr(" 层依次处理 · 每层参数独立") : qsTr("未添加 NR 层")
                            // Master switch (field request 2026-10-02): every layer off
                            // at once; on again brings back the layers that were on.
                            enabledSwitch: veyra.nrLayers.length > 0
                            switchAvailable: Support.available(veyra, "nr")
                            switchObjectName: "list-nr-master"
                            on: veyra.nrAnyEnabled
                            bypassed: veyra.nrLayers.length > 0 && !veyra.nrAnyEnabled
                            onToggled: on => veyra.setAllNrEnabled(on)
                            open: true
                            headerActions: [
                                VTag { text: qsTr("实验"); kind: "exp" },
                                Row {
                                    objectName: "list-nr-count"
                                    spacing: 3
                                    Repeater {
                                        model: veyra.nrLayers
                                        delegate: Rectangle {
                                            required property var modelData
                                            width: 14; height: 4; radius: 3
                                            color: modelData.enabled ? Theme.accent : Theme.ink(0.18)
                                            Behavior on color { ColorAnimation { duration: Theme.d(200) } }
                                        }
                                    }
                                }
                            ]
                            Repeater {
                                // Numeric count preserves delegates during slider edits.
                                model: veyra.nrLayers.length
                                delegate: NrLayerCard {
                                    id: layerCard
                                    required property int index
                                    Layout.topMargin: 4
                                    layerData: veyra.nrLayers[index]
                                    layerNumber: index + 1
                                    layerCount: veyra.nrLayers.length
                                    nrFirst: veyra.lowLatency
                                    onEdited: (nodeIndex,key,amount) => veyra.setNrLayerParameter(nodeIndex,key,amount)
                                    onEnabledEdited: (nodeIndex,enabled) => veyra.setEffectEnabled(nodeIndex,enabled)
                                    onDuplicateRequested: nodeIndex => { root.markPop(layerNumber + 1); root.selectedEffect = veyra.duplicateNrLayer(nodeIndex) }
                                    onRemoveRequested: nodeIndex => layerCard.bye(() => veyra.removeEffect(nodeIndex))
                                    onOrderEdited: enabled => veyra.lowLatency = enabled
                                }
                            }

                            // .addlayer: dashed, inside the stage; list mode adds and
                            // toggles layers, never reorders them.
                            AddLayerButton {
                                objectName: "list-nr-add"
                                full: veyra.nrLayers.length >= 4
                                label: qsTr("+  添加 NR 层")
                                note: full ? qsTr("（最多 4 层）") : qsTr("（加在 NR ") + veyra.nrLayers.length + qsTr(" 之后 · 最多 4 层）")
                                onAdd: { root.markPop(veyra.nrLayers.length + 1); root.selectedEffect = veyra.addEffect("nr") }
                            }

                            // Global NR options sit inside the NR stage, beside its layers
                            // (field request 2026-11-07): each runs once over the whole stack.
                            // Stage-5 output stabiliser. Global, not per-layer: it
                            // runs once after the whole NR stack. Default off, and
                            // off means the pass is never even dispatched.
                            VAccordion {
                                objectName: "list-output-stabiliser"
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                visible: veyra.nodeMode === 0
                                title: qsTr("输出稳定器 · 抗闪烁")
                                glyph: "shield"; hue: Theme.accent
                                summary: veyra.nrHoldStrength > 0
                                         ? qsTr("已开启 · 强度 ") + Math.round(veyra.nrHoldStrength * 100) + "%"
                                         : qsTr("默认关闭 · 只稳住没变化的画面，动的地方不拖影")
                                on: veyra.nrHoldStrength > 0
                                onToggled: on => veyra.nrHoldStrength = on ? 0.8 : 0
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("比较源画面与上一帧：没有明显变化的像素沿用上一帧输出，明显变化的位置直接用新结果。") +
                                          qsTr("不重投影、不用光流，所以运动区域不会拖影。放在最后一层 NR 之后、超分与补帧之前。")
                                    wrapMode: Text.WordWrap
                                    color: Theme.t3
                                    font.family: Theme.fontUi; font.pixelSize: 11
                                }
                                VRow {
                                    label: qsTr("稳定强度")
                                    hint: qsTr("越高越稳，过高会显得发闷")
                                    visible: veyra.nrHoldStrength > 0
                                    value: Math.round(veyra.nrHoldStrength * 100) + "%"
                                    VSlider {
                                        objectName: "list-hold-strength"
                                        implicitWidth: 110
                                        from: 0.1; to: 1.0; inputScale: 100
                                        value: veyra.nrHoldStrength
                                        onMoved: value => veyra.nrHoldStrength = value
                                    }
                                }
                                VRow {
                                    label: qsTr("变化容差")
                                    hint: qsTr("越小越敏感，越大稳得越多")
                                    visible: veyra.nrHoldStrength > 0
                                    value: Number(veyra.nrHoldTolerance).toFixed(3)
                                    VSlider {
                                        objectName: "list-hold-tolerance"
                                        implicitWidth: 110
                                        inputDecimals: 3
                                        from: 0.005; to: 0.10
                                        value: veyra.nrHoldTolerance
                                        onMoved: value => veyra.nrHoldTolerance = value
                                    }
                                }
                            }

                            // NR inference reuse (src/pipeline/NrReuse.h). Global like the
                            // stabiliser and realtime preview only; default off, and off
                            // builds no reuse pass at all.
                            VAccordion {
                                objectName: "list-nr-reuse"
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                visible: veyra.nodeMode === 0
                                title: qsTr("NR 自动推理复用")
                                glyph: "zap"; hue: Theme.accent
                                summary: veyra.nrReuseMode === 0
                                         ? qsTr("默认关闭 · 跟不上时让部分帧沿用上一帧的 NR 结果")
                                         : veyra.nrReuseStatus.length > 0 ? veyra.nrReuseStatus
                                         : veyra.nrReuseMode === 1 ? qsTr("已开启 · 自动")
                                         : qsTr("已开启 · 每 ") + veyra.nrReuseMode + qsTr(" 帧复用 1 帧")
                                on: veyra.nrReuseMode !== 0
                                onToggled: on => veyra.nrReuseMode = on ? 1 : 0
                                Text {
                                    Layout.fillWidth: true
                                    text: qsTr("只在实时预览生效：被复用的帧不跑 NR 推理，改用上一帧的 NR 变化按光流对齐后叠加，下一帧一定重新推理；对不齐的地方显示原画面。") +
                                          qsTr("导出、暂停单帧和图片照常逐帧完整推理。目前支持英伟达 NR、SDR 和列表模式。")
                                    wrapMode: Text.WordWrap
                                    color: Theme.t3
                                    font.family: Theme.fontUi; font.pixelSize: 11
                                }
                                VRow {
                                    label: qsTr("复用方式")
                                    hint: qsTr("自动：只在跟不上时复用，最多每 2 帧复用 1 帧；手动：固定比例")
                                    visible: veyra.nrReuseMode !== 0
                                    VSelect {
                                        objectName: "list-nr-reuse-mode"
                                        implicitWidth: 210
                                        options: [
                                            { id: "1", label: qsTr("自动") },
                                            { id: "2", label: qsTr("手动 1/2 · 每 2 帧复用 1 帧") },
                                            { id: "3", label: qsTr("手动 1/3 · 每 3 帧复用 1 帧") },
                                            { id: "4", label: qsTr("手动 1/4 · 每 4 帧复用 1 帧") }
                                        ]
                                        value: (options.find(o => o.id === String(veyra.nrReuseMode)) || options[0]).label
                                        onPicked: id => veyra.nrReuseMode = Number(id)
                                    }
                                }
                            }

                            VAccordion {
                                objectName: "list-global-protection"
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                visible: veyra.nodeMode===0
                                title: qsTr("NR 全局保护区域")
                                glyph: "shield"; hue: Theme.accent
                                summary: qsTr("区域内排除所有 NR 层")
                                on: veyra.protectionEnabled
                                onToggled: on=>veyra.protectionEnabled=on
                                MaskEditor { namePrefix: "list-protection" }
                            }
                        }

                        VAccordion {
                            objectName: "list-effect-exclusion"
                            Layout.fillWidth: true
                            visible: veyra.nodeMode===0
                            title: qsTr("全效果剔除")
                            glyph: "shield"; hue: Theme.accent
                            summary: qsTr("区域内恢复原始画面与帧率")
                            on: veyra.exclusionState.enabled===true
                            onToggled: on=>veyra.setMaskEnabled(true,on)
                            MaskEditor { allEffects: true; namePrefix: "list-exclusion" }
                        }

                        // Beauty: face restore and skin smoothing, after the whole NR group
                        // (and its protection). Layers run in order; one face detection
                        // serves all of them.
                        VAccordion {
                            id: beautyStage
                            objectName: "list-beauty-stage"
                            Layout.fillWidth: true
                            glyph: "heart"
                            hue: "#FF8FBF"
                            title: qsTr("美颜")
                            summary: veyra.beautyLayers.length === 0 ? qsTr("未添加美颜层 · 人脸原画保护与磨皮")
                                     : !veyra.beautyAnyEnabled ? qsTr("已关闭")
                                     : veyra.beautyStatus.length > 0 ? veyra.beautyStatus
                                     : veyra.beautyLayers.length + qsTr(" 层依次处理")
                            enabledSwitch: veyra.beautyLayers.length > 0
                            switchObjectName: "list-beauty-master"
                            on: veyra.beautyAnyEnabled
                            bypassed: veyra.beautyLayers.length > 0 && !veyra.beautyAnyEnabled
                            onToggled: on => veyra.setAllBeautyEnabled(on)
                            headerActions: VTag { text: qsTr("实验"); kind: "exp" }
                            Repeater {
                                model: veyra.beautyLayers.length
                                delegate: BeautyLayerCard {
                                    id: beautyCard
                                    required property int index
                                    Layout.topMargin: 4
                                    layerData: veyra.beautyLayers[index]
                                    layerNumber: index + 1
                                }
                            }
                            AddLayerButton {
                                objectName: "list-beauty-add"
                                full: veyra.beautyLayers.length >= 6
                                label: qsTr("+  添加美颜层")
                                note: full ? qsTr("（最多 6 层）") : qsTr("（新层默认关闭 · 最多 6 层）")
                                onAdd: { root.markBeautyPop(veyra.beautyLayers.length + 1); root.selectedEffect = veyra.addEffect("beauty") }
                            }
                            // The detection every layer shares, drawn once for the group.
                            BeautyEditor {
                                id: beautyDetectionPanel
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                visible: veyra.beautyLayers.length > 0
                                namePrefix: "list-beauty-detection"
                                // Only the detection rows: no layer's values here.
                                params: ({})
                                detection: veyra.beautyDetection
                                status: veyra.beautyStatus
                                detectionOnly: true
                                onDetectionEdited: (key, amount) => veyra.setBeautyDetection(key, amount)
                            }
                        }

                        // A refused edit explains itself here rather than in a modal.
                        Text {
                            Layout.fillWidth: true
                            visible: !veyra.chainValid
                            text: veyra.chainError
                            color: Theme.err
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsSmall
                            wrapMode: Text.WordWrap
                        }
                    }

                    // --- 补帧 -------------------------------------------------
                    // pages-pro.js fg tab: one 补帧 card with its switch ("始终在所有
                    // 画质效果之后"), the optical-flow and presentation groups nested
                    // as sub-panels. Backend choices share the node editor's
                    // bridge contract; FSR 4 requests never silently become 3.1.
                    PushFoldHead { objectName: "pro-all-fg"; visible: root.allTabs; text: qsTr("补帧"); Layout.topMargin: 6; open: root.blockOpen("fg"); onToggled: open => root.setBlockOpen("fg", open) }
                    ColumnLayout {
                        id: fgCol
                        Layout.fillWidth: true
                        spacing: 6
                        visible: root.blockShown("fg")
                        objectName: "list-fg-group"

                        // PushFPS: target frame rate, AI enhancement and low latency in
                        // three controls; the detailed frame generation card follows.
                        PushQuickSetup {
                            objectName: "list-pushfps-quick"
                            Layout.fillWidth: true
                            compactPro: true
                            scope: "rest"
                            onRequestTab: tab => root.tab = tab
                        }

                        VAccordion {
                            id: fgCard
                            objectName: "list-fg-card"
                            readonly property var backendOption: veyra.fgBackendChoices.find(o => o.id === veyra.fgBackendName)
                            instantExpansion: root.onboardingActive
                            Layout.fillWidth: true
                            glyph: "layers"
                            hue: "#3DDC84"
                            title: qsTr("补帧")
                            summary: qsTr("始终在所有画质效果之后") + (veyra.fgEnabled
                                     ? " · " + veyra.fgMultiplier + "X " + Support.fgBackendShortLabel(backendOption)
                                     : qsTr(" · 已关闭"))
                            switchObjectName: "list-fg-enabled"
                            switchAvailable: Support.available(veyra, "frame-generation")
                            on: veyra.fgEnabled
                            onToggled: on => veyra.fgEnabled = on
                            open: true

                            VRow {
                                label: qsTr("补帧方式")
                                hint: veyra.fgProviderText
                                VSelect {
                                    id: fgBackendSelect
                                    objectName: "list-fg-backend"
                                    options: veyra.fgBackendChoices
                                    value: (fgCard.backendOption || options[0] || {}).label || ""
                                    displayValue: Support.fgBackendShortLabel(selectedOption) || value
                                    onPicked: id => veyra.fgBackendName = id
                                }
                            }
                            VRow {
                                id: fgMotionRow
                                objectName: "list-fg-motion-row"
                                label: qsTr("补帧运动来源")
                                // (PushFPS 4.3.1) Shown for every method. NVIDIA VFG, AMD FRC and PushFlow work out
                                // the motion themselves from the two pictures (engine::motionUsesFlow) and take no
                                // motion source: with VFG the row was not shown at all, which read as "the optical
                                // flow choice is gone" (users' reports), and with the other two it was a choice that
                                // changed nothing. It now stays, greyed out, and says why.
                                readonly property bool ownMotion: ["vfg", "amdfrc", "pushflow"].indexOf(veyra.fgBackendName) >= 0
                                hint: ownMotion ? qsTr("当前补帧方式自行估算运动，不使用此选项") : qsTr("XeSS 默认零运动；FSR 仍有自身的光流计算")
                                VSeg {
                                    objectName: "list-fg-motion"
                                    options: [{id:"0",label:qsTr("零运动"),disabled:fgMotionRow.ownMotion},{id:"1",label:qsTr("光流"),disabled:fgMotionRow.ownMotion}]
                                    current: fgMotionRow.ownMotion ? "" : String(veyra.fgMotionSource)
                                    onPicked: id => { if (!fgMotionRow.ownMotion) veyra.fgMotionSource = Number(id) }
                                }
                            }
                            VRow {
                                label: qsTr("倍率")
                                // 档位与作者原版一致（只有数字档）；自动选择由下方独立开关控制。
                                hint: veyra.fgDisplayHint.length > 0 ? veyra.fgDisplayHint : veyra.fgMaxMultiplier + qsTr("X 为上限")
                                hintColor: veyra.fgDisplayHint.length > 0 ? Theme.warn : Theme.t3
                                // (PushFPS 4.2.2) One of the two is shown; in one layout they can never stand on top of each other.
                                RowLayout {
                                spacing: 0
                                VSeg {
                                    visible: veyra.fgBackendName !== "vfg"
                                    options: veyra.fgMultiplierChoices
                                    current: String(veyra.fgMultiplier)
                                    onPicked: id => veyra.fgMultiplier = parseInt(id)
                                }
                                VSelect {
                                    objectName: "list-vfg-multiplier"
                                    visible: veyra.fgBackendName === "vfg"
                                    options: veyra.fgMultiplierChoices
                                    value: String(veyra.fgMultiplier) + "X"
                                    onPicked: id => veyra.fgMultiplier = Number(id)
                                }
                                }
                            }
                            // PushFPS adaptive multiplier (EnhancementSettings::fgAdaptive): the multiplier
                            // above becomes the ceiling. XeSS paces itself; 2X-only backends have nothing to adapt.
                            VRow {
                                objectName: "list-fg-adaptive-row"
                                label: qsTr("自适应倍率")
                                readonly property bool possible: !veyra.presentationOwned &&
                                    (veyra.fgMultiplierChoices || []).some(o => parseInt(o.id) > 2)
                                visible: possible
                                hint: qsTr("倍率作为上限：按测得的游戏帧率，每对画面选择最接近目标帧率（输出上限或显示器刷新率）的倍率。需要开启自动去重")
                                VSwitch {
                                    objectName: "list-fg-adaptive"
                                    checked: veyra.fgAdaptive
                                    onToggled: checked => {
                                        veyra.fgAdaptive = checked
                                        if (checked && Number(veyra.contentRate) === 0) veyra.contentRate = 1
                                    }
                                }
                            }
                            // PushFPS measurements: ground-truth quality (any source with CPU frames) and the
                            // Remote Play latency test (PS5 streaming).
                            VRow {
                                objectName: "list-fg-tests-row"
                                label: qsTr("测量")
                                hint: qsTr("画质测试：扣下真实帧再比较生成的画面。延迟测试：PS5 串流时自动按键计时")
                                RowLayout {
                                    spacing: 6
                                    VButton { objectName: "list-fg-quality-test"; text: qsTr("画质测试"); enabled: veyra.hasSource && veyra.fgEnabled; onClicked: veyra.navigate("qualitytest") }
                                    VButton { objectName: "list-fg-latency-test"; text: qsTr("延迟测试"); visible: veyra.sourceKind === "ps5"; onClicked: veyra.navigate("latencytest") }
                                }
                            }
                            VRow {
                                objectName: "list-fg-pacing-row"
                                label: qsTr("帧节奏修复")
                                visible: !veyra.presentationOwned
                                hint: qsTr("把出帧不均匀的锁帧游戏重新对齐到均匀的时间，每帧最多晚一次刷新。需要开启自动去重")
                                VSwitch {
                                    objectName: "list-fg-pacing"
                                    checked: veyra.pacingRepair
                                    onToggled: checked => {
                                        veyra.pacingRepair = checked
                                        if (checked && Number(veyra.contentRate) === 0) veyra.contentRate = 1
                                    }
                                }
                            }
                            VRow {
                                objectName: "list-fg-hudlock-row"
                                label: qsTr("实时 HUD 锁定")
                                visible: !veyra.presentationOwned
                                hint: qsTr("每一帧找出静止且有边缘的像素（文字、准星、血条），补出来的帧在这些位置用真实帧的像素")
                                VSwitch {
                                    objectName: "list-fg-hudlock"
                                    checked: veyra.hudLock
                                    onToggled: checked => veyra.hudLock = checked
                                }
                            }
                            // PushFPS still rule (preference pushHudStill, off unless switched on): here beside Live HUD
                            // Lock, since the quick setup above leaves both to this list in Professional mode.
                            VRow {
                                objectName: "list-fg-hudstill-row"
                                label: qsTr("保持 HUD 清晰")
                                visible: !veyra.presentationOwned
                                hint: veyra.preferences.pushHudStill === true ? qsTr("已开启：镜头移动时画面可能变糊，物体边缘可能破碎；出现这种情况请关闭") : qsTr("已关闭：HUD 交给补帧处理")
                                VSwitch {
                                    objectName: "list-fg-hudstill"
                                    checked: veyra.preferences.pushHudStill === true
                                    onToggled: checked => veyra.setPreference("pushHudStill", checked)
                                }
                            }
                            VRow {
                                label: qsTr("DLSS 性能模式")
                                visible: veyra.fgBackendName === "dlss"
                                enabled: veyra.dlssgLiteAvailable
                                opacity: enabled ? 1 : 0.5
                                hint: veyra.dlssgLiteHint
                                VSwitch {
                                    objectName: "list-dlssg-lite"
                                    checked: veyra.dlssgLite
                                    onToggled: checked => veyra.dlssgLite = checked
                                }
                            }
                            VRow {
                                label: qsTr("VFG 质量")
                                visible: veyra.fgBackendName === "vfg"
                                hint: qsTr("高质量更耗时；混合档：每对原帧的中间帧用前一档，其余用后一档")
                                VSelect {
                                    objectName: "list-vfg-quality"
                                    options: [{id:"0",label:qsTr("低")},{id:"1",label:qsTr("中")},{id:"2",label:qsTr("高")},{id:"3",label:qsTr("高+中")},{id:"4",label:qsTr("高+低")},{id:"5",label:qsTr("中+低")}]
                                    value: options[veyra.vfgQuality].label
                                    onPicked: id => veyra.vfgQuality = Number(id)
                                }
                            }
                            VRow {
                                label: qsTr("PushFlow 档位")
                                visible: veyra.fgBackendName === "pushflow"
                                hint: qsTr("快速：轻量模型，最高 540p 估算运动；均衡：轻量模型，最高 720p；高质量：完整模型，最高 1080p，最耗显卡。画面都按原分辨率重建")
                                VSelect {
                                    objectName: "list-pushflow-tier"
                                    options: [{id:"0",label:qsTr("快速")},{id:"1",label:qsTr("均衡")},{id:"2",label:qsTr("高质量")}]
                                    value: options[Math.min(veyra.vfgQuality, 2)].label
                                    onPicked: id => veyra.vfgQuality = Number(id)
                                }
                            }
                            // The motion size apart from the tier (3.2.7): PushFlowMotionRow.qml.
                            PushFlowMotionRow { visible: veyra.fgBackendName === "pushflow" }
                            // PushFlow 2 experiments, each off unless switched on here (PushFlow2Row.qml).
                            PushFlow2Row {
                                objectName: "list-pushflow2-fusion-row"
                                visible: veyra.fgBackendName === "pushflow"
                                part: "fusion"
                                stateKey: "pushFlowEncoderCache"
                                describe: function(st) {
                                    return qsTr("正在使用。启动时测得：完整模型每帧 %1 ms，拆分后 %2 ms；已复用 %3 次，重新编码 %4 次")
                                        .arg(Number(st.pushFlowWholeMs || 0).toFixed(1)).arg(Number(st.pushFlowSplitMs || 0).toFixed(1))
                                        .arg(Math.round(Number(st.pushFlowFeaturesKept || 0))).arg(Math.round(Number(st.pushFlowFeaturesMade || 0)))
                                }
                                title: qsTr("PushFlow 2：每个真实帧只编码一次（实验）")
                                explanation: qsTr("PushFlow 的模型每生成一帧，都要把前后两个真实帧各编码一遍。开启后，每个真实帧只编码一次，结果留给下一对画面和同一对里的其他时刻使用，所以 3× 和 4× 省得更多。画面应当和不开启时一样：启动时会在你的显卡上和完整模型对比，不一致就不使用。能省多少取决于你的显卡")
                            }
                            PushFlow2Row {
                                objectName: "list-pushflow2-guard-row"
                                visible: veyra.fgBackendName === "pushflow"
                                part: "guard"
                                stateKey: "pushFlowGuard"
                                describe: function(st) {
                                    return qsTr("正在使用。已检查 %1 个生成帧：%2 帧局部改用真实帧，%3 帧整帧改用真实帧")
                                        .arg(Math.round(Number(st.pushFlowGuardFrames || 0))).arg(Math.round(Number(st.pushFlowGuardMended || 0)))
                                        .arg(Math.round(Number(st.pushFlowGuardHeld || 0)))
                                }
                                title: qsTr("PushFlow 2：Guard，补回被漏掉的物体（实验）")
                                explanation: qsTr("物体在两个真实帧之间移动得太快时，PushFlow 跟不上，生成帧里这个物体会整个消失，看起来像闪烁。Guard 找出这些位置，在那里改用时间上更近的真实帧：物体不再消失，而是按游戏本身的帧率移动。它不会凭空生成画面；一帧里有三分之一以上的位置有问题时，整帧改用真实帧，并且如实计数。这条规则只在测试图上验证过，在真实游戏里是否更好看还不知道，所以需要你来判断")
                            }
                            PushFlow2Row {
                                objectName: "list-pushflow2-predict-row"
                                visible: veyra.fgBackendName === "pushflow"
                                part: "predict"
                                stateKey: "pushFlowPredict"
                                describe: function(st) {
                                    if (st.pushFlowPredictPaused === true) return qsTr("已让位：画质测试或自动优化正在测量普通的中间帧，结束后恢复")
                                    return qsTr("正在使用。已生成 %1 个预测帧；运动的画面中有 %2 % 因不可信而保持不动；%3 个预测帧因下一个真实帧已到而没有显示")
                                        .arg(Math.round(Number(st.pushFlowPredictFrames || 0))).arg(Number(st.pushFlowPredictHeldPercent || 0).toFixed(0))
                                        .arg(Math.round(Number(st.pushFlowPredictSuperseded || 0)))
                                }
                                title: qsTr("PushFlow 2：Predict，不等下一帧（实验）")
                                explanation: qsTr("普通补帧要等下一个真实帧到达，才能在两帧之间生成画面，所以真实画面会晚大约一帧才显示。Predict 不等：最新的真实帧处理完就显示，它之后的画面由已经到达的两帧预测出来。延迟更低，但预测可能出错：物体突然改变方向或新物体出现时，预测帧会和实际不同，直到下一个真实帧纠正。不可信的位置保持最新真实帧不动，不会凭空生成画面。只用于采集卡、串流等实时画面，不用于视频文件和导出。延迟降低多少、画面是否可以接受，取决于你的显卡和游戏，由你来判断")
                            }
                            PushFlow2Row {
                                objectName: "list-pushflow2-dual-row"
                                visible: veyra.fgBackendName === "pushflow"
                                part: "dual"
                                stateKey: "pushFlowDual"
                                describe: function(st) {
                                    return qsTr("正在使用。已交给录制和直播：%1 个真实帧，%2 个真实的中间帧")
                                        .arg(Math.round(Number(st.pushFlowDualReal || 0))).arg(Math.round(Number(st.pushFlowDualBetween || 0)))
                                }
                                title: qsTr("PushFlow 2：Dual，录制和直播得到真实的帧（实验）")
                                explanation: qsTr("开启 Predict 后，屏幕上显示的是预测帧。录制、即时回放和直播记录的是屏幕上的画面，所以也会包含这些预测帧。Dual 让它们改用真实的画面：真实帧本身，以及同一对真实帧之间真实的中间帧（Predict 已经算出了它，几乎不增加开销），时间上比屏幕晚一帧。你看到的画面和延迟都不变。只在 Predict 正在运行并且倍率是 2X 时起作用；对比短片仍然记录屏幕上的画面；录下的声音会比画面早一帧（30 FPS 时约 33 毫秒）")
                            }
                            PushFlow2Row {
                                objectName: "list-pushflow2-response-row"
                                visible: veyra.fgBackendName === "pushflow"
                                part: "response"
                                stateKey: "pushFlowResponse"
                                describe: function(st) {
                                    if (st.pushFlowResponseEnough !== true)
                                        return qsTr("正在观察。已看到 %1 个真实帧，其中 %2 个右摇杆被推动；至少需要 90 个才下结论")
                                            .arg(Math.round(Number(st.pushFlowResponseFrames || 0))).arg(Math.round(Number(st.pushFlowResponseActive || 0)))
                                    if (st.pushFlowResponseFollows === true)
                                        return qsTr("画面跟随右摇杆。推满时，画面每秒横向移动自身宽度的 %1 %、纵向移动自身高度的 %2 %；画面比摇杆晚 %3 毫秒；摇杆能解释这些运动的 %4 %")
                                            .arg((Math.abs(Number(st.pushFlowResponseGainX || 0)) * 100).toFixed(0)).arg((Math.abs(Number(st.pushFlowResponseGainY || 0)) * 100).toFixed(0))
                                            .arg(Number(st.pushFlowResponseDelayMs || 0).toFixed(0)).arg((Number(st.pushFlowResponseExplained || 0) * 100).toFixed(0))
                                    return qsTr("画面的运动和右摇杆对不上（摇杆只能解释 %1 %）。在这个游戏或这个场景里，Response 可能帮不上忙")
                                        .arg((Number(st.pushFlowResponseExplained || 0) * 100).toFixed(0))
                                }
                                title: qsTr("PushFlow 2：Response，先测量（实验）")
                                explanation: qsTr("Response 的设想是：在主机自己的画面到达之前，先根据右摇杆把画面转动一点。第一步只测量，不改动画面：画面自身的运动是否跟随右摇杆、有多强、晚多少。它只观察两样已经在 PushFPS 里的东西：PushFPS 自己发给主机的右摇杆（目前只有 PS5 Remote Play；接在主机上的手柄看不到），以及 Predict 运行时 PushFlow 算出的画面整体运动。不读取任何按键，不写入磁盘，不发送到任何地方；关闭开关或更换画面来源时全部清除。玩几分钟、多转动视角，这里就会显示结果，性能报告里也有")
                            }
                            PushFlow2Row {
                                objectName: "list-pushflow2-memory-row"
                                visible: veyra.fgBackendName === "pushflow"
                                part: "memory"
                                stateKey: "pushFlowMemory"
                                describe: function(st) {
                                    const pairs = Math.round(Number(st.pushFlowMemoryPairs || 0)), apart = Number(st.pushFlowMemoryApart || 0)
                                    if (pairs < 60) return qsTr("正在测量。已测 %1 对真实帧，至少需要 60 对").arg(pairs)
                                    if (apart < 1.0) return qsTr("相邻的真实帧对齐后几乎一样（平均相差 %1，满值 255）。这个画面来源很干净，Memory 没有什么可以补回的").arg(apart.toFixed(2))
                                    return qsTr("相邻的真实帧对齐后平均相差 %1（满值 255）。把对齐的两帧取平均，大约能去掉其中三成：这是 Memory 能改善多少的估计，画面本身还没有改动").arg(apart.toFixed(2))
                                }
                                title: qsTr("PushFlow 2：Memory，先测量（实验）")
                                explanation: qsTr("Memory 的设想是：把之前的真实帧里出现过的细节，按运动对齐后带回当前画面，让纹理更稳定、压缩噪点更少。只有相邻的真实帧之间确实有差别时，这才有意义，所以第一步只测量，不改动画面：在运动可信的位置，相邻两个真实帧对齐之后还相差多少。采集卡送来的画面没有压缩，差别通常很小；Remote Play 等串流画面压缩过，差别会大一些。需要 Predict 正在运行；结果也会写进性能报告")
                            }
                            // Half precision is tested in a separate process first (PushFlowBench --fp16-probe):
                            // the switch only turns on after that test passed on this graphics card.
                            VRow {
                                id: pushFlowHalfRow
                                objectName: "list-pushflow-half-row"
                                label: qsTr("PushFlow 半精度")
                                visible: veyra.fgBackendName === "pushflow"
                                property bool testing: false
                                readonly property var probe: {
                                    try { return JSON.parse(veyra.preferences.pushFlowHalfProbe || "{}") } catch (e) { return ({}) }
                                }
                                help: qsTr("半精度（fp16）通常让 PushFlow 的网络快一倍左右，但是否可用取决于显卡、驱动和 DirectML。测试在单独的进程里运行，出错也不会影响 PushFPS")
                                hint: testing ? qsTr("正在测试，约需半分钟…")
                                    : pushFlowHalfRow.probe.ok === true ? qsTr("已通过测试：每帧 %1 ms，全精度 %2 ms").arg(Number(pushFlowHalfRow.probe.halfMs || 0).toFixed(1)).arg(Number(pushFlowHalfRow.probe.fullMs || 0).toFixed(1))
                                    : pushFlowHalfRow.probe.reason === "retest" ? qsTr("显卡、驱动或 PushFPS 版本变了：请重新测试后再开启。现在使用全精度")
                                    : pushFlowHalfRow.probe.reason ? qsTr("上次测试没有通过，使用全精度")
                                    : qsTr("先测试这块显卡；通过后才能开启。请在没有播放时测试")
                                hintBelow: true
                                RowLayout {
                                    spacing: 8
                                    VButton {
                                        objectName: "list-pushflow-half-test"
                                        text: pushFlowHalfRow.testing ? qsTr("测试中…") : qsTr("测试")
                                        enabled: !pushFlowHalfRow.testing
                                        onClicked: if (veyra.pushTestPushFlowHalf()) pushFlowHalfRow.testing = true
                                    }
                                    VSwitch {
                                        objectName: "list-pushflow-half"
                                        enabled: pushFlowHalfRow.probe.ok === true && !pushFlowHalfRow.testing
                                        opacity: enabled ? 1 : 0.4
                                        checked: veyra.preferences.pushFlowHalf === true
                                        onToggled: v => veyra.setPreference("pushFlowHalf", v)
                                    }
                                    Timer {
                                        interval: 500; repeat: true
                                        running: pushFlowHalfRow.testing
                                        onTriggered: if (!veyra.pushFlowHalfTesting()) pushFlowHalfRow.testing = false
                                    }
                                }
                            }
                            VRow {
                                label: qsTr("VFG 自动调控")
                                visible: veyra.fgBackendName === "vfg"
                                enabled: veyra.vfgQuality !== 0 && veyra.fgMultiplier === 2
                                opacity: enabled ? 1 : 0.5
                                hint: qsTr("实时采集 2X 时，当前档来不及赶上屏幕刷新，这一对帧就改用下一档，不丢帧也不加延迟。多占约 2 GB 显存，显存不够时不生效")
                                VSwitch {
                                    objectName: "list-vfg-auto-regulate"
                                    checked: veyra.vfgAutoRegulate
                                    onToggled: checked => veyra.vfgAutoRegulate = checked
                                }
                            }
                            VRow {
                                label: qsTr("严格补帧节奏")
                                hint: qsTr("帧同步 · 默认关闭")
                                VSwitch {
                                    checked: veyra.fgStrict
                                    onToggled: checked => veyra.fgStrict = checked
                                }
                            }
                            VRow {
                                objectName: "list-fg-auto-multiplier"
                                label: qsTr("自动选择倍率")
                                visible: veyra.fgAutoMultiplierAvailable
                                hint: veyra.fgAutoMultiplierEnabled
                                      ? qsTr("按源帧率和输出上限自动选档；源帧率超过上限时关闭补帧")
                                      : qsTr("关闭时按手动选的倍率 · 默认关闭")
                                hintBelow: veyra.fgAutoMultiplierEnabled
                                VSwitch {
                                    checked: veyra.fgAutoMultiplierEnabled
                                    onToggled: checked => veyra.fgAutoMultiplierEnabled = checked
                                }
                            }
                            VSubGroup {
                                id: fgFlowGroup
                                objectName: "list-fg-flow-group"
                                instantExpansion: root.onboardingActive
                                Layout.fillWidth: true
                                Layout.topMargin: 6
                                label: qsTr("光流 · 运动估算")
                                count: veyra.opticalFlowChoice === 1 ? 5 : 4
                                expanded: true
                                VRow {
                                    label: qsTr("光流算法")
                                    hint: qsTr("补帧与 NR 共用 · 输入后计算一次")
                                    VSelect {
                                        objectName: "list-flow-choice"
                                        options: [Support.option(veyra,"0","NVIDIA NVOF","flow0"),Support.option(veyra,"1","AMD FidelityFX","flow1")]
                                        value: options[veyra.opticalFlowChoice]?.label ?? (veyra.opticalFlowChoice === 2 ? qsTr("GPU DIS 已移除，请重选") : qsTr("未知配置"))
                                        onPicked: id => veyra.setOpticalFlowChoice(Number(id))
                                    }
                                }
                                VRow {
                                    label: qsTr("边缘神盾")
                                    hint: qsTr("DLSS、FSR、ANR 补帧，及运动来源选“光流”的 XeSS 生效：修人物边缘果冻，背景不再被人物拖着晃")
                                    VSwitch {
                                        objectName: "list-edge-aegis"
                                        checked: veyra.edgeAegis
                                        onToggled: checked => veyra.edgeAegis = checked
                                    }
                                }
                                VRow {
                                    label: qsTr("AMD 性能档")
                                    hint: qsTr("光流宽高减半，更省但更糙")
                                    visible: veyra.opticalFlowChoice === 1
                                    VSwitch {
                                        objectName: "list-amd-half"
                                        checked: veyra.amdFlowHalf
                                        onToggled: checked => veyra.amdFlowHalf = checked
                                    }
                                }
                                VRow {
                                    label: qsTr("运动估算质量")
                                    VSeg {
                                        objectName: "list-flow-quality"
                                        options: [{ id: "0", label: qsTr("性能") }, { id: "1", label: qsTr("平衡") }, { id: "2", label: qsTr("质量") }]
                                        current: String(veyra.flowQuality)
                                        onPicked: id => veyra.flowQuality = Number(id)
                                    }
                                }
                                DedupModeRow { id: dedupRow; backend: veyra; namePrefix: "list" }
                            }
                            NvidiaLowLatencyEditor { namePrefix: "list" }
                            VRow {
                                visible: !veyra.nvidiaLowLatencyAvailable
                                label: qsTr("低延迟队列")
                                hint: qsTr("减少处理排队")
                                VSwitch {
                                    objectName: "list-low-queue"
                                    checked: veyra.fgLowQueue
                                    onToggled: checked => veyra.fgLowQueue = checked
                                }
                            }
                        }
                    }

                    // --- 色彩 -------------------------------------------------
                    // The same real-instance editor is used by the node inspector.
                    PushFoldHead { objectName: "pro-all-color"; visible: root.allTabs; text: qsTr("色彩"); Layout.topMargin: 6; open: root.blockOpen("color"); onToggled: open => root.setBlockOpen("color", open) }
                    ColourPanel {
                        id: colourPanel
                        Layout.fillWidth: true
                        visible: root.blockShown("color")
                        api: veyra
                    }

                    // --- 声音 -------------------------------------------------
                    PushFoldHead { objectName: "pro-all-audio"; visible: root.allTabs; text: qsTr("声音"); Layout.topMargin: 6; open: root.blockOpen("audio"); onToggled: open => root.setBlockOpen("audio", open) }
                    ProAudioPanel {
                        id: audioCol
                        Layout.fillWidth: true
                        visible: root.blockShown("audio")
                    }

                    // --- 显示 -------------------------------------------------
                    PushFoldHead { objectName: "pro-all-display"; visible: root.allTabs; text: qsTr("显示"); Layout.topMargin: 6; open: root.blockOpen("display"); onToggled: open => root.setBlockOpen("display", open) }
                    ProDisplayPanel {
                        id: displayCol
                        Layout.fillWidth: true
                        visible: root.blockShown("display")
                    }

                    Item { Layout.preferredHeight: 12 }
                }
            }

            // .insp-foot
            RowLayout {
                objectName: "pro-inspector-footer"
                Layout.fillWidth: true
                Layout.margins: insp.height < 240 ? 6 : 10
                spacing: 8
                VDot { Layout.alignment: Qt.AlignTop; Layout.topMargin: 5; off: !veyra.running; warn: veyra.captureRecovering; err: veyra.failed }
                Text {
                    id: inspectorStatus
                    objectName: "pro-inspector-status"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: veyra.statusText
                    color: Theme.t2
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsSmall
                    wrapMode: Text.Wrap
                    maximumLineCount: insp.height < 240 ? 1 : 4
                    elide: Text.ElideRight
                    HoverHandler { id: inspectorStatusHover }
                    ToolTip.visible: inspectorStatusHover.hovered && inspectorStatus.truncated
                    ToolTip.text: inspectorStatus.text
                }
                VButton {
                    objectName: "pro-reset-current-page"
                    Layout.alignment: Qt.AlignTop
                    ghost: true
                    text: qsTr("重置本页")
                    onClicked: veyra.resetCurrentPage()
                }
            }
        }
    }

    // Test hook (--menu, motion probe "menu"): open a header menu from its button.
    function openTestMenu(name) {
        if (name === "source") sourceMenu.openAt(sourceBtn, "down")
        else if (name === "preset") presetMenu.openAt(presetBtn, "down")
    }
    readonly property real testMenuScale: sourceMenu.motionScale

    // The pill's 字幕 / 音轨 menus, for the video bar's buttons.
    VMenu {
        id: proCcMenu
        title: qsTr("字幕")
        items: [{ label: qsTr("关闭", "off"), checked: veyra.subtitlePrimary < 0, track: -1 }]
            .concat(veyra.subtitleTracks.map(t => ({ label: t.label, note: t.note, track: t.index,
                                                     disabled: !t.usable, checked: t.index === veyra.subtitlePrimary })))
            .concat([{ label: qsTr("加载外部字幕…"), icon: "import", act: "load" },
                     { sep: true },
                     { label: qsTr("字幕设置…"), note: qsTr("字体、字号、描边、位置、延时"), icon: "type", act: "dlg" },
                     { label: qsTr("实时翻译设置…"), icon: "type", act: "translation" }])
        onPicked: (i, o) => {
            if (o.act === "load") veyra.loadSubtitleDialog()
            else if (o.act === "dlg") root.requestDialog("subtitle")
            else if (o.act === "translation") root.requestDialog("translation")
            else if (o.track !== undefined) veyra.subtitlePrimary = o.track
        }
    }
    VMenu {
        id: proAudioMenu
        title: qsTr("音轨")
        readonly property var mk: t => ({ label: t.label, index: t.index,
                                          note: t.channels > 0 ? (t.channels + qsTr(" 声道")) : "",
                                          checked: t.index === veyra.selectedAudioTrack })
        items: veyra.audioTracks.length > 0
               ? veyra.audioTracks.map(mk).concat([{ sep: true },
                     { label: qsTr("音频设置…"), note: qsTr("输出设备、音画同步、偏移"), icon: "music", act: "dlg" }])
               : [{ label: qsTr("片源没有音轨或尚未打开"), disabled: true }]
        onPicked: (i, o) => {
            if (o.act === "dlg") return root.requestDialog("audio")
            if (o.index !== undefined) veyra.selectedAudioTrack = o.index
        }
    }
    // pages-pro.js data-srcbtn: the open source first (checked), then the ways in.
    VMenu {
        id: sourceMenu
        title: qsTr("片源")
        items: (veyra.sourceName.length > 0
                ? [{ label: (veyra.isCapture ? qsTr("采集卡 · ") : "") + veyra.sourceName, note: veyra.sourceSummary,
                     checked: true, icon: "video", act: "" }] : [])
            .concat([{ label: qsTr("打开文件…"), icon: "folder", act: "file" },
                { label: qsTr("播放地址…"), icon: "cast", act: "network" },
                     { label: qsTr("PS5 串流…"), icon: "gamepad", act: "ps5" },
                     { label: qsTr("PC 串流…"), icon: "cast", act: "moonlight" },
                     { label: qsTr("Xbox 串流…"), icon: "gamepad", act: "xbox" },
                     { label: qsTr("屏幕捕获…"), icon: "monitor", act: "screen" },
                     { sep: true },
                     { label: qsTr("采集卡设置…"), icon: "settings", act: "capture" }])
        onPicked: (i, o) => {
            if (o.act === "file") veyra.openFileDialog()
            else if (o.act === "network") veyra.openNetworkDialog()
            else if (o.act === "ps5") veyra.openPs5Dialog()
            else if (o.act === "moonlight") veyra.openMoonlightDialog()
            else if (o.act === "xbox") veyra.openXboxDialog()
            else if (o.act === "screen") veyra.openScreenCaptureDialog()
            else if (o.act === "capture") veyra.openCaptureDialog()
        }
    }
    // The design puts 另存为 / 管理 in the professional page's preset menu.
    // presets.js VY.presetMenu(app, anchor, 'list').
    VMenu {
        id: presetMenu
        title: veyra.nodeMode === 1 ? qsTr("节点预设") : qsTr("列表预设")
        items: veyra.presets.filter(p => p.nodeMode === (veyra.nodeMode === 1))
            .map(p => ({ label: p.name, note: p.note, checked: p.name === veyra.currentPresetName,
                         tag: p.builtin ? qsTr("内置") : "", preset: p.index }))
            .concat([{ sep: true },
                     { label: qsTr("把当前设置另存为预设…"), icon: "plus", act: "save" },
                     { label: qsTr("管理预设…"), note: qsTr("重命名 · 复制 · 删除"), icon: "settings", act: "manage" }])
        onPicked: (i, o) => {
            if (o.act) root.requestDialog(o.act)
            else veyra.applyPresetIndex(o.preset)
        }
    }

    // pages-pro.js layer "更多": copy as a new layer, back to defaults, remove.
    VMenu {
        id: nrLayerMenu
        objectName: "nr-layer-menu"
        property Item layer: null
        title: layer ? qsTr("NR 层 ") + layer.layerNumber : ""
        items: layer ? [
            { label: qsTr("复制为新层"), icon: "dup", act: "dup", disabled: veyra.nrLayers.length >= 4 },
            { label: qsTr("恢复默认"), icon: "reset", act: "reset" },
            { label: qsTr("删除这一层"), icon: "trash", act: "del", disabled: veyra.nrLayers.length <= 1 }
        ] : []
        onPicked: (i, o) => {
            const card = layer
            if (!card) return
            const nodeIndex = card.layerData.index
            if (o.act === "dup") { root.markPop(card.layerNumber + 1); root.selectedEffect = veyra.duplicateNrLayer(nodeIndex) }
            else if (o.act === "reset") veyra.resetNrLayer(nodeIndex)
            else if (o.act === "del") card.bye(() => veyra.removeEffect(nodeIndex))
        }
    }

    // A beauty layer's "更多": copy, move within the group, back to defaults, remove.
    VMenu {
        id: beautyLayerMenu
        objectName: "beauty-layer-menu"
        property Item layer: null
        title: layer ? qsTr("美颜层 ") + layer.layerNumber : ""
        items: layer ? [
            { label: qsTr("复制为新层"), icon: "dup", act: "dup", disabled: veyra.beautyLayers.length >= 6 },
            { label: qsTr("上移"), icon: "left", act: "up", disabled: layer.layerNumber <= 1 },
            { label: qsTr("下移"), icon: "right", act: "down", disabled: layer.layerNumber >= veyra.beautyLayers.length },
            { label: qsTr("恢复默认"), icon: "reset", act: "reset" },
            { label: qsTr("删除这一层"), icon: "trash", act: "del" }
        ] : []
        onPicked: (i, o) => {
            const card = layer
            if (!card) return
            const nodeIndex = card.layerData.index
            if (o.act === "dup") { root.markBeautyPop(card.layerNumber + 1); root.selectedEffect = veyra.duplicateBeautyLayer(nodeIndex) }
            else if (o.act === "up") veyra.moveBeautyLayer(nodeIndex, -1)
            else if (o.act === "down") veyra.moveBeautyLayer(nodeIndex, 1)
            else if (o.act === "reset") veyra.resetBeautyLayer(nodeIndex)
            // Let the model remove the card only after acceptance; a fade also
            // hides a retained layer when the engine refuses deletion.
            else if (o.act === "del") veyra.removeEffect(nodeIndex)
        }
    }

    // presets.js DIALOGS.toNode / toList: the two modes side by side, then what
    // switching means. Independent configurations; the node canvas starts empty
    // the first time and keeps its chain afterwards.
    component ModeCard: Rectangle {
        id: mc
        property string glyph: ""
        property string heading: ""
        property string detail: ""
        property string tag: ""
        property bool active: false
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: modeCol.implicitHeight + 24
        radius: 12
        color: active ? Qt.rgba(1, 138 / 255, 61 / 255, 0.08) : Theme.card2
        border.width: 1
        border.color: active ? Theme.accent : Theme.stroke
        ColumnLayout {
            id: modeCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            spacing: 6
            Rectangle {
                implicitWidth: 28; implicitHeight: 28; radius: 8
                color: Theme.ink(0.06)
                VIcon { anchors.centerIn: parent; name: mc.glyph; size: 14 }
            }
            Text {
                text: mc.heading
                color: Theme.t1
                font.family: Theme.fontUi; font.pixelSize: 13
                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
            }
            Text {
                Layout.fillWidth: true
                text: mc.detail
                color: Theme.t3
                font.family: Theme.fontUi; font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
            VTag { text: mc.tag; kind: mc.active ? "acc" : "" }
        }
    }
    component Bullet: RowLayout {
        id: bl
        property string text: ""
        Layout.fillWidth: true
        spacing: 8
        Rectangle { Layout.alignment: Qt.AlignTop; Layout.topMargin: 7; implicitWidth: 4; implicitHeight: 4; radius: 2; color: Theme.t3 }
        Text {
            Layout.fillWidth: true
            text: bl.text
            color: Theme.t2
            font.family: Theme.fontUi; font.pixelSize: 12
            wrapMode: Text.WordWrap
            lineHeight: 1.25
        }
    }

    VConfirm {
        id: switchDialog
        property int targetMode: 1
        objectName: "node-switch-dialog"
        cardWidth: targetMode === 1 ? 560 : 480
        glyph: targetMode === 1 ? "nodes" : "list"
        title: targetMode === 1 ? qsTr("切换到节点模式？") : qsTr("切换回列表模式？")
        acceptText: targetMode === 1 ? qsTr("切换到节点模式") : qsTr("切换到列表模式")
        acceptIcon: targetMode === 1 ? "nodes" : "list"
        RowLayout {
            objectName: "node-switch-cards"
            visible: switchDialog.targetMode === 1
            Layout.fillWidth: true
            spacing: 8
            ModeCard {
                glyph: "list"
                heading: qsTr("列表模式")
                detail: qsTr("固定顺序：超分 → NR → 保护 → HDR → 补帧")
                tag: qsTr("当前 · 设置会原样保留")
            }
            VIcon { name: "right"; color: Theme.t3 }
            ModeCard {
                glyph: "nodes"
                heading: qsTr("节点模式")
                detail: qsTr("自由排列节点，按连线顺序运行")
                tag: qsTr("首次进入为空白画布，之后保留上次的节点链")
                active: true
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            Bullet { visible: switchDialog.targetMode === 1; text: qsTr("两种模式的设置、预设完全分开保存，切回列表时恢复列表原来的设置。") }
            Bullet { visible: switchDialog.targetMode === 1; text: qsTr("切换会重建处理链，画面可能停顿几百毫秒。") }
            Bullet { visible: switchDialog.targetMode === 1; text: qsTr("节点模式按连线顺序真实执行；离线导出仅支持列表模式。") }
            Bullet { visible: switchDialog.targetMode === 0; text: qsTr("回到列表模式，恢复列表之前的设置和预设。") }
            Bullet { visible: switchDialog.targetMode === 0; text: qsTr("节点链会保留，下次切回节点模式时继续使用。") }
            Bullet { visible: switchDialog.targetMode === 0; text: qsTr("切换会重建处理链，画面可能停顿几百毫秒。") }
        }
        onAccepted: {
            veyra.nodeMode = targetMode
            if (veyra.nodeMode === targetMode) root.requestPage(targetMode === 1 ? "node" : "pro")
        }
    }

    // [data-in] entrance order from pages-pro.js.
    VRise { id: headRise; target: head; d: 0 }
    readonly property alias probeRise: headRise
    VRise { target: vwrap; d: 1 }
    VRise { target: meter1; d: 2 }
    VRise { target: meter2; d: 3 }
    VRise { target: meter3; d: 4 }
    VRise { target: insp; d: 2 }
}
