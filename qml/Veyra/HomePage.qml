// 首页, rebuilt from the design (pages-a.js PAGES.home + pages.css .home).
//
// The design is: a large logo, a two-line greeting, a 4-up grid of source cards
// (164x124 each), a "continue last capture" row, and a row of recent chips.
// The logo in the dock returns here; there is no other home affordance.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

VPage {
    id: root
    signal requestPage(string page)
    // The first-run tour (PushTour.qml; the tour itself is OnboardingGuide in Main.qml): asked for with the
    // button under the cards, and what its steps on this page point at.
    signal requestOnboarding()
    // PushFPS redesign (the new home page): the big picture, and the Appearance panel.
    signal requestFullscreen()
    signal requestAppearance()
    // Three panes when there is room for them: sources | the picture | quick setup. Otherwise (legacy,
    // or a narrow window in the new design) one column, as the page always was.
    readonly property bool threePane: Theme.modern && width >= 900
    // (The page's own strip for moving the window, VPage: the three panes reach up into it, and the new
    // design moves the window by its title bar.)
    dragStripHeight: threePane ? 0 : 56
    // The index pinned over quick setup in the three panes: every section by its name, and AI picture
    // enhancement by its own (it is the first thing people look for, and stands in the Picture section).
    // A press opens the section if it was folded and brings it to the top of the list. Nothing is a
    // copy of a setting: these only scroll.
    readonly property var quickIndex: [
        { id: "setup", section: "setup", row: "", label: qsTr("主机与游戏") },
        { id: "fg", section: "fg", row: "", label: qsTr("补帧", "section") },
        { id: "enhance", section: "picture", row: "pushfps-enhance-row", label: qsTr("AI 画面增强") },
        { id: "picture", section: "picture", row: "pushfps-upscale-row,pushfps-fill-row,pushfps-crop-row", label: qsTr("画面") },
        { id: "hud", section: "hud", row: "", label: qsTr("HUD") },
        { id: "latency", section: "latency", row: "", label: qsTr("延迟与声音") },
        { id: "start", section: "start", row: "", label: qsTr("连接与启动") }
    ]
    property var quickWanted: null
    function showInQuick(entry) {
        const section = quickSetup.sectionItem(entry.section)
        if (!section) return
        root.showSide()
        const folded = !section.open
        if (folded) quickSetup.setSectionOpen(entry.section, true)
        root.quickWanted = entry
        // A section that was folded takes a moment to have its height again (PushSection: 260 ms).
        quickScroll.interval = folded ? Theme.d(300) + 1 : 1
        quickScroll.restart()
    }
    Timer {
        id: quickScroll
        onTriggered: {
            const entry = root.quickWanted
            const section = entry ? quickSetup.sectionItem(entry.section) : null
            if (!section) return
            let target = section
            for (const name of String(entry.row || "").split(",")) {
                const row = name.length > 0 ? quickSetup.rowItem(name) : null
                if (row) { target = row; break }
            }
            const y = target.mapToItem(rightFlick.contentItem, 0, 0).y - (target === section ? 0 : 6)
            // (What stands within a few pixels of the list's start is the start.)
            rightFlick.contentY = y < 24 ? 0 : Math.max(0, Math.min(y, rightFlick.contentHeight - rightFlick.height))
        }
    }
    // Picture and sound stands open under the picture the first time the three panes are shown.
    property bool tuningOpened: false
    onThreePaneChanged: if (threePane && !tuningOpened) { tuningOpened = true; tuning.open = true }
    readonly property real paneMargin: 16
    readonly property real paneGap: 14
    readonly property real leftWidth: Math.round(Math.max(236, Math.min(290, width * 0.21)))
    readonly property real rightWidth: Math.round(Math.max(336, Math.min(410, width * 0.30)))
    // (4.3.1) The settings side hidden with the title bar's button: the picture's column takes its
    // room. Quick setup is still the same item; whatever asks to be shown a row of it brings the
    // side back first (showSide).
    readonly property bool sideCollapsed: threePane && Theme.shot.pushSideCollapsed === true
    function showSide() { if (sideCollapsed) veyra.setPreference("pushSideCollapsed", false) }
    readonly property real midX: paneMargin + leftWidth + paneGap
    readonly property real midWidth: Math.max(1, width - 2 * paneMargin - leftWidth - paneGap - (sideCollapsed ? 0 : rightWidth + paneGap))
    readonly property real actionHeight: 44
    // The picture: 16:9 in the middle column, never taller than leaves room for its own row of actions
    // and a strip of picture and sound under it.
    readonly property real previewHeight: Math.round(Math.max(120, Math.min(midWidth * 9 / 16, height - 2 * paneMargin - actionHeight - 2 * paneGap - 60)))
    // Where the native picture window stands on this page (Main.qml videoRect): inside the preview
    // card's outline. No rectangle without the three panes: the legacy home page shows no picture.
    readonly property rect videoViewport: threePane ? Qt.rect(midX + 1, paneMargin + 1, midWidth - 2, previewHeight - 2) : Qt.rect(0, 0, 0, 0)
    readonly property Item onboardingEntry: tourEntry
    readonly property alias sourceViewport: content
    // Whether this PC offers what a step is about (a step for something that is not there is left out).
    function tourOffers(what) { return quickSetup.tourItem(what) !== null }
    function tourTarget(key) {
        const item = key === "capture" ? captureCard : quickSetup.tourItem(key)
        return item ? { item: item, viewport: root.threePane ? (key === "capture" ? leftFlick : key === "tools" ? midFlick : rightFlick) : content } : null
    }
    function showTourStep(key) { if (key !== "capture" && key !== "tools") root.showSide(); quickSetup.showTourStep(key) }
    // Opens 画面与声音 and scrolls it into view (the screenshot run's `--tab tuning`).
    function showTuning() {
        tuning.open = true
        tuningScroll.restart()
    }
    Timer {
        id: tuningScroll
        interval: 450
        onTriggered: {
            if (root.threePane) midFlick.contentY = 0
            else content.contentY = Math.max(0, Math.min(tuning.mapToItem(column, 0, 0).y - 24, content.contentHeight - content.height))
        }
    }
    // The screenshot run's `--scroll-to <objectName>` on this page (as SettingsPage.scrollToObject).
    function scrollToObject(name) {
        quickSetup.reveal(name)   // the row may stand in a folded section
        const find = (item) => {
            if (!item) return null
            if (item.objectName === name) return item
            for (let i = 0; i < item.children.length; ++i) { const r = find(item.children[i]); if (r) return r }
            return null
        }
        // The four groups are the page's contents wherever they stand; each has its own list in the three panes.
        for (const g of [[groupA, leftFlick], [groupB, rightFlick], [groupT, midFlick], [groupC, leftFlick]]) {
            const target = find(g[0])
            if (!target) continue
            if (g[0] === groupB) root.showSide()
            if (root.threePane) {
                const view = g[1]
                view.contentY = Math.max(0, Math.min(target.mapToItem(view.contentItem, 0, 0).y - 24, view.contentHeight - view.height))
            } else {
                content.contentY = Math.max(0, Math.min(target.mapToItem(column, 0, 0).y - 24, content.contentHeight - content.height))
            }
            return true
        }
        return false
    }
    // Veyra 2.1.9's startup intro hooks (Main.qml); PushFPS has no intro, so nothing is held.
    property bool introHold: false
    // Space kept free under a pinned dock (Main.qml), so it never covers the logo.
    property real topInset: 0
    readonly property Item wordmarkItem: root.sunset ? sunsetLogo : null
    // 设置 → 外观 → 日落之城 (pushSunset, unset = on): the logo filled with a neon sunset city.
    readonly property bool sunset: veyra.preferences.pushSunset !== false

    // PushFPS home, laid out for the console player: the capture card is the main
    // action, the other sources sit under it as smaller cards, then the quick setup,
    // then the lesser actions on one line. Every source and action of the design's
    // home is still here; the content scrolls when the window is short.
    Flickable {
        id: content
        visible: !root.threePane
        anchors.fill: parent
        anchors.bottomMargin: 44
        readonly property real columnWidthMax: 600
        readonly property real columnWidth: Math.min(columnWidthMax, width - 48)
        contentWidth: width
        contentHeight: Math.max(height, column.implicitHeight + 48 + root.topInset)
        interactive: contentHeight > height + 1
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: VScrollBar {}

        // Which sources this build includes; the cards themselves are a fixed list (see cardSub).
        QtObject {
            id: srcGrid
            function available(act) {
                if (act === "ps5") return !!veyra.ps5 && veyra.ps5.available === true
                if (act === "moonlight") return !!veyra.moonlight
                if (act === "xbox") return !!veyra.xbox
                return true
            }
            // Subtitles name what is really there: the device of the last capture
            // session and the saved PS5 host; generic wording when there is none.
            // Only the subtitle follows the live state. The cards themselves are a fixed
            // list: when the whole array was one binding, every Xbox / PC state change
            // (polling after sign-in) destroyed and rebuilt all six cards, and a click
            // landing during the rebuild was lost (field report 2026-10-02).
            function cardSub(act) {
                switch (act) {
                case "file": return qsTr("MP4 · MKV · 图片")
                case "capture": return veyra.hasCaptureSession ? veyra.captureSessionSummary.split(" · ")[0] : qsTr("HDMI 采集设备")
                case "ps5": return veyra.remotePlayHost.length > 0 ? qsTr("已保存主机 ") + veyra.remotePlayHost : qsTr("局域网串流")
                case "moonlight": return veyra.moonlight && veyra.moonlight.state.lastLabel ? veyra.moonlight.state.lastLabel : qsTr("Sunshine 主机")
                case "xbox": return veyra.xbox && veyra.xbox.state.lastLabel ? veyra.xbox.state.lastLabel : qsTr("账号登录 · 实验")
                case "frameinput": return qsTr("画面输入接口")
                default: return qsTr("窗口或显示器")
                }
            }
        }

        ColumnLayout {
            id: column
            x: Math.round((content.width - width) / 2)
            y: Math.max(24 + root.topInset, Math.round((content.height - implicitHeight) / 2))
            width: content.columnWidth
            spacing: 0

            // PushFPS redesign: the page's contents are four groups (sources; quick setup; picture and
            // sound; the lesser actions and recent files). Here, in the legacy design and in a window
            // too narrow for three panes, they stand under each other as they always did. In the new
            // design the sources stand left of the picture, quick setup right of it, picture and sound
            // under it (below).
            Item { id: slotA; Layout.fillWidth: true; implicitHeight: groupA.parent === slotA ? groupA.implicitHeight : 0 }
            Item { id: slotB; Layout.fillWidth: true; implicitHeight: groupB.parent === slotB ? groupB.implicitHeight : 0 }
            Item { id: slotT; Layout.fillWidth: true; implicitHeight: groupT.parent === slotT ? groupT.implicitHeight : 0 }
            Item { id: slotC; Layout.fillWidth: true; implicitHeight: groupC.parent === slotC ? groupC.implicitHeight : 0 }
        }
    }

    // The four groups. Each is declared once and stands where the design puts it.
    ColumnLayout {
        id: groupA
        parent: root.threePane ? paneSlotA : slotA
        width: parent ? parent.width : 0
        spacing: 0
            // Sunset City logo (scripts/brand/make_sunset.py; its glow is baked into the image, 70/1340
            // of the width on each side). The startup intro glides into exactly this rect.
            Image {
                id: sunsetLogo
                objectName: "home-sunset-logo"
                visible: root.sunset && !Theme.modern   // the new design has the wordmark in its title bar
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: -6
                Layout.bottomMargin: -22
                Layout.preferredWidth: Math.min(470, content.columnWidth)
                Layout.preferredHeight: Layout.preferredWidth * 550 / 1340
                source: "sunset-logo.png"
                sourceSize.width: 940
                fillMode: Image.PreserveAspectFit
                smooth: true
                opacity: root.introHold ? 0 : 1
            }
            Item {
                id: mark
                visible: !root.sunset && !Theme.modern   // (the new design has the mark in its title bar)
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: -40
                Layout.bottomMargin: -40
                implicitWidth: logoImg.implicitWidth + 80
                implicitHeight: logoImg.implicitHeight + 80
                property real breath: 0
                scale: 1 + 0.02 * breath
                // Only while the home page shows: running behind other pages it redrew the whole
                // window every vsync during playback (~300 presents/s on a 320 Hz screen), which
                // cost GPU time and kept OBS game capture on the UI instead of the video.
                SequentialAnimation on breath {
                    running: !Theme.reduced && mark.visible
                    loops: Animation.Infinite
                    NumberAnimation { to: 1; duration: Theme.d(2250); easing.type: Easing.InOutSine }
                    NumberAnimation { to: 0; duration: Theme.d(2250); easing.type: Easing.InOutSine }
                }
                Image {
                    id: logoImg
                    anchors.centerIn: parent
                    source: "logo.png"
                    sourceSize.width: 88
                    fillMode: Image.PreserveAspectFit
                }
                // A sibling MultiEffect, not layer.effect: Qt recreates a layer's effect item on
                // a screen DPI change while it walks the parent's children, and the walk then touched
                // the deleted item (crash moving the window to a 200 % monitor, field 2026-10-01).
                MultiEffect {
                    source: logoImg
                    anchors.fill: logoImg
                    shadowEnabled: true
                    shadowColor: "#FFFFFF"
                    shadowHorizontalOffset: 0
                    shadowVerticalOffset: 0
                    blurMax: 64
                    shadowBlur: 0.625 + 0.3125 * mark.breath
                    shadowOpacity: 0.35 + 0.2 * mark.breath
                }
            }


            ColumnLayout {
                id: hello
                visible: !Theme.modern
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 14
                spacing: 6
                PushWordmark {
                    visible: !root.sunset
                    Layout.alignment: Qt.AlignHCenter
                    pixelSize: Theme.fsH1 + 8
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("主机画面经采集卡接入，由电脑补帧到 60 或 120 FPS")
                    color: Theme.t2
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsBody
                }
            }

            Rectangle {
                id: resume
                objectName: "home-resume"
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: root.threePane ? 0 : 28
                // The last source actually used: capture card, PS5 or screen capture.
                readonly property var last: veyra.lastSource
                visible: last.kind !== undefined
                implicitWidth: root.threePane ? groupA.width : content.columnWidth
                implicitHeight: 54
                radius: 14
                border.width: 1
                border.color: Theme.stroke
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.accentSoft }
                    GradientStop { position: 1.0; color: Theme.ink(0.03) }
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 10
                    spacing: 14
                    Rectangle {
                        visible: !root.threePane   // (the narrow pane keeps the room for the words)
                        implicitWidth: 34; implicitHeight: 34; radius: 10
                        color: Theme.accentSoft
                        VIcon {
                            anchors.centerIn: parent
                            name: resume.last.kind === "screen" ? "monitor" : resume.last.kind === "capture" ? "video" : "gamepad"
                            color: Theme.accent
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: qsTr("继续上次")
                            color: Theme.t1
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsBody
                            font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                        }
                        Text {
                            Layout.fillWidth: true
                            text: resume.last.summary || ""
                            color: Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                    VButton {
                        text: qsTr("开始")
                        iconName: "play"
                        primary: true
                        onClicked: veyra.resumeLastSource()
                    }
                }
            }


            // The main action: a wide capture card card with its own button.
            Rectangle {
                id: captureCard
                objectName: "home-capture"
                Layout.fillWidth: true
                Layout.topMargin: Theme.modern ? (resume.visible ? 10 : 0) : resume.visible ? 12 : 28
                implicitHeight: root.threePane ? 126 : 76
                radius: Theme.rCard
                color: captureHover.hovered ? Theme.card2 : Theme.card
                border.width: 1
                border.color: captureHover.hovered ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45) : Theme.stroke2
                Behavior on color { ColorAnimation { duration: Theme.d(Theme.durFast) } }
                Behavior on border.color { ColorAnimation { duration: Theme.d(Theme.durNormal) } }
                scale: captureTap.pressed ? 0.985 : 1.0
                Behavior on scale { NumberAnimation { duration: Theme.d(500); easing.bezierCurve: Theme.spring } }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: root.threePane ? 12 : 16
                    anchors.rightMargin: root.threePane ? 12 : 16
                    anchors.bottomMargin: root.threePane ? 50 : 0
                    spacing: root.threePane ? 10 : 14
                    Rectangle {
                        implicitWidth: root.threePane ? 36 : 44; implicitHeight: implicitWidth; radius: 12
                        color: Theme.accentSoft
                        VIcon { anchors.centerIn: parent; name: "video"; size: root.threePane ? 17 : 20; color: Theme.accent }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        RowLayout {
                            spacing: 8
                            Text {
                                Layout.fillWidth: root.threePane
                                elide: Text.ElideRight
                                text: qsTr("采集卡")
                                color: Theme.t1
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fsH3
                                font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                            }
                            // PS5 手柄控制 is switched on for the capture card.
                            VTag {
                                visible: veyra.ps5Control.enabled === true
                                text: qsTr("PS5 手柄")
                                kind: "acc"
                            }
                        }
                        Text {
                            Layout.fillWidth: true
                            text: srcGrid.cardSub("capture")
                            color: Theme.t3
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsSmall
                            elide: Text.ElideRight
                        }
                    }
                    VButton {
                        objectName: "home-capture-connect"
                        visible: !root.threePane
                        text: qsTr("连接")
                        iconName: "play"
                        primary: true
                        onClicked: captureCard.open()
                    }
                }
                // (The new design's pane: the same button, as wide as the card, under the words.)
                VButton {
                    objectName: "home-capture-connect-wide"
                    visible: root.threePane
                    x: 12
                    y: parent.height - height - 12
                    width: parent.width - 24
                    text: qsTr("连接")
                    iconName: "play"
                    primary: true
                    onClicked: captureCard.open()
                }
                function open() {
                    veyra.logUi("ui-source", "home card act=capture")
                    veyra.openCaptureDialog()
                }
                HoverHandler { id: captureHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { id: captureTap; onTapped: captureCard.open() }
            }

            // The other sources: equal cards in one row (two rows when narrow).
            GridLayout {
                id: otherSources
                Layout.fillWidth: true
                Layout.topMargin: 10
                readonly property var shown: [
                    { glyph: "folder", title: qsTr("打开视频"), act: "file" },
                    { glyph: "monitor", title: qsTr("屏幕捕获"), act: "screen" },
                    { glyph: "gamepad", title: qsTr("PS5 串流"), act: "ps5" },
                    { glyph: "cast", title: qsTr("PC 串流"), act: "moonlight" },
                    { glyph: "gamepad", title: qsTr("Xbox 串流"), act: "xbox" },
                    { glyph: "import", title: qsTr("其他应用"), act: "frameinput" }
                ]
                readonly property int shownCount: shown.filter(c => srcGrid.available(c.act)).length
                // (The new design's left pane: one card to a line, so no card is ever squeezed.)
                columns: root.threePane ? 1 : width >= 140 * shownCount + 10 * (shownCount - 1) ? shownCount : Math.ceil(shownCount / 2)
                columnSpacing: 10
                rowSpacing: root.threePane ? 6 : 10
                Repeater {
                    model: otherSources.shown
                    delegate: Item {
                        id: slot
                        required property var modelData
                        required property int index
                        visible: srcGrid.available(modelData.act)
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: root.threePane ? 52 : 62
                        VRise { page: root; target: slot; d: 3 + slot.index }
                        Rectangle {
                            id: srcCard
                            width: parent.width
                            height: parent.height
                            radius: 12
                            color: cardHover.hovered ? Theme.card2 : Theme.card
                            border.width: 1
                            // (The new design: the edge lights up in the accent under the pointer.)
                            border.color: cardHover.hovered ? (Theme.modern ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.7) : Theme.stroke2) : Theme.stroke
                            Behavior on border.color { ColorAnimation { duration: Theme.d(160) } }
                            y: cardHover.hovered ? -2 : 0
                            Behavior on y { NumberAnimation { duration: Theme.d(500); easing.bezierCurve: Theme.spring } }
                            Behavior on color { ColorAnimation { duration: Theme.d(Theme.durFast) } }
                            scale: cardTap.pressed ? 0.97 : 1.0
                            Behavior on scale { NumberAnimation { duration: Theme.d(500); easing.bezierCurve: Theme.spring } }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 10
                                spacing: 10
                                Rectangle {
                                    implicitWidth: 32; implicitHeight: 32; radius: 9
                                    color: Theme.ink(0.06)
                                    VIcon { anchors.centerIn: parent; name: modelData.glyph; size: 16 }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.title
                                        color: Theme.t1
                                        font.family: Theme.fontUi
                                        font.pixelSize: Theme.fsBody
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: srcGrid.cardSub(modelData.act)
                                        color: Theme.t3
                                        font.family: Theme.fontUi
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                            HoverHandler { id: cardHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                id: cardTap
                                onTapped: {
                                    veyra.logUi("ui-source", "home card act=" + modelData.act)
                                    switch (modelData.act) {
                                    case "file": veyra.openFileDialog(); break
                                    case "ps5": veyra.openPs5Dialog(); break
                                    case "moonlight": veyra.openMoonlightDialog(); break
                                    case "xbox": veyra.openXboxDialog(); break
                                    case "screen": veyra.openScreenCaptureDialog(); break
                                    case "frameinput": veyra.navigate("frameinput"); break
                                    }
                                }
                            }
                        }
                    }
                }
            }

    }
    ColumnLayout {
        id: groupB
        parent: root.threePane ? paneSlotB : slotB
        width: parent ? parent.width : 0
        spacing: 0
            // PushFPS: pick the frame rate before connecting; the same settings as
            // Professional → 补帧, in their short form.
            PushQuickSetup {
                id: quickSetup
                objectName: "home-pushfps-quick"
                compact: true
                // (The new design shows the whole of quick setup here: the five rows the short form
                // leaves to Professional mode too. Legacy: the short form, as always.)
                shortForm: !Theme.modern
                // (The three panes: the tip stands in the empty picture, the title is pinned over the
                // list with the index of its sections, the tools stand under the picture, and the pane
                // is the card.)
                showSetupTip: !root.threePane
                showHeader: !root.threePane
                toolsHost: root.threePane ? toolsPlace : null
                color: root.threePane ? "transparent" : Theme.card2
                border.width: root.threePane ? 0 : 1
                Layout.fillWidth: true
                Layout.topMargin: root.threePane ? 0 : 22
            }

    }
    ColumnLayout {
        id: groupT
        parent: root.threePane ? paneSlotT : slotT
        width: parent ? parent.width : 0
        spacing: 0
            // PushFPS picture and sound: the everyday colour controls with presets and
            // the sound profile, folded away until wanted.
            VAccordion {
                id: tuning
                objectName: "home-picture-sound"
                Layout.fillWidth: true
                Layout.topMargin: root.threePane ? 0 : 12
                glyph: "sliders"
                title: qsTr("画面与声音")
                summary: {
                    const p = homePicture.presets.find(x => x.id === homePicture.currentPreset)
                    return (p ? p.label : qsTr("已自定义")) + " · " + homeSound.presets[homeSound.preset].label
                }
                enabledSwitch: false
                PushPicture { id: homePicture; Layout.fillWidth: true }
                Rectangle { Layout.fillWidth: true; Layout.topMargin: 8; Layout.bottomMargin: 2; implicitHeight: 1; color: Theme.stroke }
                PushSoundProfile { id: homeSound; Layout.fillWidth: true; compact: true }
            }

    }
    ColumnLayout {
        id: groupC
        parent: root.threePane ? paneSlotC : slotC
        width: parent ? parent.width : 0
        spacing: 0
            // The lesser actions on one line.
            // (One line in the one column; one under the other in the new design's left pane.)
            GridLayout {
                id: moreActions
                Layout.alignment: root.threePane ? Qt.AlignLeft : Qt.AlignHCenter
                Layout.topMargin: 14
                columns: root.threePane ? 1 : 5
                columnSpacing: 6
                rowSpacing: 2
                VButton {
                    objectName: "home-playlist"
                    Layout.maximumWidth: root.threePane ? groupC.width : Number.POSITIVE_INFINITY   // (a long label elides in the pane)
                    ghost: true
                    text: qsTr("播放列表 · 导入文件夹"); iconName: "list"
                    onClicked: veyra.showPlaylist()
                }
                Rectangle { visible: !root.threePane; implicitWidth: 1; implicitHeight: 14; color: Theme.stroke2 }
                VButton {
                    objectName: "home-network"
                    Layout.maximumWidth: root.threePane ? groupC.width : Number.POSITIVE_INFINITY
                    ghost: true
                    text: qsTr("播放地址…")
                    iconName: "cast"
                    onClicked: veyra.openNetworkDialog()
                }
                Rectangle { visible: !root.threePane; implicitWidth: 1; implicitHeight: 14; color: Theme.stroke2 }
                // The first-run tour, whenever it is wanted again.
                VButton {
                    id: tourEntry
                    objectName: "home-onboarding"
                    Layout.maximumWidth: root.threePane ? groupC.width : Number.POSITIVE_INFINITY
                    ghost: true
                    text: qsTr("查看新手引导")
                    iconName: "info"
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: text
                    Accessible.onPressAction: clicked()
                    Keys.onReturnPressed: clicked()
                    Keys.onEnterPressed: clicked()
                    Keys.onSpacePressed: clicked()
                    onClicked: root.requestOnboarding()
                }
            }

            Flow {
                id: recent
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 14
                Layout.preferredWidth: root.threePane ? groupC.width : content.columnWidth
                visible: veyra.recentFiles.length > 0
                spacing: 8
                Text {
                    height: 28
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("最近")
                    color: Theme.t3
                    font.family: Theme.fontUi
                    font.pixelSize: 12
                }
                Repeater {
                    model: veyra.recentFiles
                    delegate: Rectangle {
                        id: chip
                        required property var modelData
                        readonly property bool isImage: /\.(png|jpe?g|bmp|webp|tiff?)$/i.test(modelData.path)
                        width: chipRow.implicitWidth + 22
                        height: 28
                        radius: 99
                        opacity: modelData.exists ? 1 : 0.45
                        color: chipHover.hovered && modelData.exists ? Theme.ink(0.08) : Theme.ink(0.04)
                        border.width: 1
                        border.color: Theme.stroke
                        Row {
                            id: chipRow
                            anchors.centerIn: parent
                            spacing: 7
                            VIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                name: chip.isImage ? "image" : "film"
                                size: 14
                                color: chipText.color
                            }
                            Text {
                                id: chipText
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.label
                                color: chipHover.hovered && modelData.exists ? Theme.t1 : Theme.t2
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                            }
                        }
                        ToolTip.visible: chipHover.hovered && !modelData.exists
                        ToolTip.text: qsTr("文件已不存在")
                        HoverHandler { id: chipHover; cursorShape: modelData.exists ? Qt.PointingHandCursor : Qt.ArrowCursor }
                        TapHandler { onTapped: if (modelData.exists) veyra.openPath(modelData.path) }
                    }
                }
            }
    }

    // --- PushFPS redesign: sources | the picture | quick setup ---------------------------------
    // Nothing here is a new setting or a second copy of one: the panes hold the page's own groups.
    Item {
        id: desk
        objectName: "home-desk"
        anchors.fill: parent
        visible: root.threePane
        opacity: root.introHold ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.d(420); easing.type: Easing.OutCubic } }

        // Left: where the picture comes from.
        Rectangle {
            id: leftPane
            objectName: "home-desk-sources"
            x: root.paneMargin; y: root.paneMargin
            width: root.leftWidth
            height: desk.height - 2 * root.paneMargin
            radius: Theme.rCard
            color: Theme.card
            border.width: 1
            border.color: Theme.stroke
            Flickable {
                id: leftFlick
                anchors.fill: parent
                anchors.margins: 12
                anchors.rightMargin: 4
                contentWidth: width
                contentHeight: leftColumn.implicitHeight
                interactive: contentHeight > height + 1
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: VScrollBar {}
                ColumnLayout {
                    id: leftColumn
                    // (Room for the scroll bar, so it never lies over a card's edge. Always: a width that
                    // depends on whether the list scrolls changes the list's height, and that is a loop.)
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
                        elide: Text.ElideRight
                    }
                    Item { id: paneSlotA; Layout.fillWidth: true; implicitHeight: groupA.parent === paneSlotA ? groupA.implicitHeight : 0 }
                    Item { id: paneSlotC; Layout.fillWidth: true; implicitHeight: groupC.parent === paneSlotC ? groupC.implicitHeight : 0 }
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
        }

        // Middle: the picture. The native picture window stands inside this card's outline while a
        // source plays (videoViewport); with none, the card says so and offers the two usual ways in.
        Rectangle {
            id: previewCard
            objectName: "home-desk-preview"
            x: root.midX; y: root.paneMargin
            width: root.midWidth
            height: root.previewHeight
            radius: 4
            color: veyra.hasSource === true ? Theme.videoBlack : Theme.card
            border.width: 1
            border.color: previewHover.hovered && veyra.hasSource !== true ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6) : Theme.stroke
            Behavior on border.color { ColorAnimation { duration: Theme.d(160) } }
            HoverHandler { id: previewHover }
            ColumnLayout {
                objectName: "home-desk-empty"
                visible: veyra.hasSource !== true
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, 560)
                spacing: 10
                VIcon { Layout.alignment: Qt.AlignHCenter; name: "tv"; size: 30; color: Theme.t3 }
                Text {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("未打开片源")
                    color: Theme.t1
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsH3
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                    elide: Text.ElideRight
                }
                // The first-run tip of quick setup (what to set on the console before connecting), until a
                // capture card has been used once: here it has the room the list beside the picture has not.
                Text {
                    objectName: "home-desk-tip"
                    visible: previewCard.height >= 250 && !veyra.hasCaptureSession
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: quickSetup.setupTip
                    color: Theme.t3
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsHint
                    lineHeight: 1.1
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    spacing: 8
                    VButton {
                        objectName: "home-desk-connect"
                        primary: true
                        iconName: "play"
                        text: qsTr("连接")
                        onClicked: captureCard.open()
                    }
                    VButton {
                        objectName: "home-desk-open"
                        iconName: "folder"
                        text: qsTr("打开视频")
                        onClicked: veyra.openFileDialog()
                    }
                }
            }
        }
        // Under the picture: what plays, and the way to the big picture.
        RowLayout {
            id: previewActions
            objectName: "home-desk-actions"
            x: root.midX
            y: root.paneMargin + root.previewHeight + root.paneGap
            width: root.midWidth
            height: root.actionHeight
            spacing: 8
            VDot { off: veyra.hasSource !== true; warn: veyra.captureRecovering === true }
            Text {
                Layout.fillWidth: true
                text: veyra.hasSource === true ? (String(veyra.sourceTitle || "").length > 0 ? veyra.sourceTitle : qsTr("已连接")) : qsTr("未连接")
                color: veyra.hasSource === true ? Theme.t1 : Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsBody
                elide: Text.ElideMiddle
            }
            VButton {
                objectName: "home-desk-stop"
                visible: veyra.hasSource === true
                ghost: true
                text: qsTr("断开")
                onClicked: veyra.stopPlayback()
            }
            VButton {
                objectName: "home-desk-fullscreen"
                primary: veyra.hasSource === true
                enabled: veyra.hasSource === true
                iconName: "max"
                text: qsTr("全屏")
                onClicked: root.requestFullscreen()
            }
        }
        // Under that: picture and sound, with the picture right above it.
        Flickable {
            id: midFlick
            objectName: "home-desk-tuning"
            x: root.midX
            y: previewActions.y + root.actionHeight + root.paneGap
            width: root.midWidth
            height: Math.max(0, desk.height - y - root.paneMargin)
            contentWidth: width
            contentHeight: midColumn.height
            interactive: contentHeight > height + 1
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: VScrollBar {}
            Column {
                id: midColumn
                width: midFlick.width - 10   // (always: see the sources' list)
                spacing: root.paneGap
                // Quick setup's six tools (the same tiles: PushQuickSetup.toolsHost).
                Item { id: toolsPlace; objectName: "home-desk-tools"; width: parent.width; height: quickSetup.toolsHeight }
                Item { id: paneSlotT; width: parent.width; height: implicitHeight; implicitHeight: groupT.parent === paneSlotT ? groupT.implicitHeight : 0 }
            }
        }

        // Right: quick setup, every section of it, in one list. Its title and the index of what is in
        // it stay in place over the list.
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
            Column {
                id: rightHead
                x: 14; y: 12
                width: parent.width - 28
                spacing: 9
                Text {
                    width: parent.width
                    text: qsTr("快速设置")
                    color: Theme.t1
                    font.family: Theme.fontUi
                    font.pixelSize: Theme.fsH3
                    font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
                    elide: Text.ElideRight
                }
                Flow {
                    id: indexFlow
                    objectName: "home-desk-index"
                    width: parent.width
                    spacing: 6
                    Repeater {
                        model: root.quickIndex
                        delegate: Rectangle {
                            id: chip
                            required property var modelData
                            readonly property var section: quickSetup.sectionItem(modelData.section)
                            objectName: "home-desk-index-" + modelData.id
                            // (A section this PC shows nothing of has no entry.)
                            visible: section !== null && section.visible && !section.empty
                            width: Math.min(chipText.implicitWidth + 20, indexFlow.width)
                            height: 26
                            radius: 13
                            color: chipHover.hovered || chip.activeFocus ? Theme.accentSoft : Theme.ink(0.05)
                            border.width: 1
                            border.color: chipHover.hovered || chip.activeFocus ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.75) : Theme.stroke
                            Behavior on color { ColorAnimation { duration: Theme.d(160) } }
                            Behavior on border.color { ColorAnimation { duration: Theme.d(160) } }
                            activeFocusOnTab: true
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.label
                            Keys.onReturnPressed: root.showInQuick(modelData)
                            Keys.onSpacePressed: root.showInQuick(modelData)
                            Text {
                                id: chipText
                                anchors.centerIn: parent
                                width: Math.min(implicitWidth, chip.width - 20)
                                elide: Text.ElideRight
                                text: chip.modelData.label
                                color: chipHover.hovered ? Theme.t1 : Theme.t2
                                font.family: Theme.fontUi
                                font.pixelSize: 12
                            }
                            HoverHandler { id: chipHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: root.showInQuick(chip.modelData) }
                        }
                    }
                }
            }
            Rectangle { id: rightRule; x: 0; y: rightHead.y + rightHead.height + 11; width: parent.width; height: 1; color: Theme.stroke }
            Flickable {
                id: rightFlick
                objectName: "home-desk-settings-list"
                x: 0
                y: rightRule.y + 1
                width: parent.width
                height: parent.height - y - 6
                contentWidth: width
                contentHeight: paneSlotB.implicitHeight
                interactive: contentHeight > height + 1
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.vertical: VScrollBar {}
                Item { id: paneSlotB; width: rightFlick.width - 8; height: implicitHeight; implicitHeight: groupB.parent === paneSlotB ? groupB.implicitHeight : 0 }
            }
        }
    }

    // The painter's palette of the legacy design: the way to the Appearance panel, and so back to the
    // new design (the new design has it at the bottom of its navigation).
    Item {
        id: legacyPalette
        objectName: "home-appearance"
        visible: !Theme.modern
        x: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        width: 32; height: 32
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("外观")
        Keys.onReturnPressed: root.requestAppearance()
        Keys.onSpacePressed: root.requestAppearance()
        Rectangle {
            anchors.fill: parent
            radius: 16
            color: paletteHover.hovered ? Theme.card2 : "transparent"
            border.width: 1
            border.color: paletteHover.hovered || legacyPalette.activeFocus ? Theme.accent : Theme.stroke2
            Behavior on border.color { ColorAnimation { duration: Theme.d(160) } }
        }
        VIcon { anchors.centerIn: parent; name: "palette"; size: 16; color: paletteHover.hovered ? Theme.t1 : Theme.t2 }
        HoverHandler { id: paletteHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.requestAppearance() }
        ToolTip.visible: paletteHover.hovered
        ToolTip.delay: 300
        ToolTip.text: qsTr("外观")
    }

    // Free and open source, built on Veyra (since 3.1.3 PushFPS links no sponsor pages).
    Text {
        id: creditLine
        objectName: "home-credit"
        visible: !root.threePane   // the three panes have it at the foot of the sources
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        text: qsTr("免费开源 · 基于 Veyra 构建")
        color: Theme.t3
        font.family: Theme.fontUi
        font.pixelSize: Theme.fsSmall
    }

    // [data-in] entrance order from pages-a.js (the source cards rise from their delegate, --d 2-5).
    VRise { target: mark; d: 0 }
    VRise { target: hello; d: 1 }
    VRise { target: captureCard; d: 2 }
    VRise { target: resume; d: 2 }
    VRise { target: quickSetup; d: 8 }
    VRise { target: tuning; d: 9 }
    VRise { target: moreActions; d: 9 }
    VRise { target: recent; d: 10 }
}
