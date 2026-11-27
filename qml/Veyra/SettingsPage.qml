// 设置, rebuilt from the design (pages-b.js PAGES.set + pages.css .set).
//
// Design: a 220px nav column (外观 / 启动与窗口 / 播放 / 音频 / 性能 / 兼容 /
// PS5 串流 / 快捷键 / 组件与许可 / 关于 / 赞助作者; split 2026-10-05 because one page held
// almost every setting) and a large body. The design deliberately has NO
// capture-card section here: capture settings live in the capture dialog.
//
// Rows that would need engine support this build does not have say so, rather than
// being drawn as switches that would do nothing.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "PushOutputPresets.js" as OutputPresets
import "PushCompareClip.js" as CompareClip

VPage {
    id: root
    signal requestPage(string page)

    property string section: "look"
    property var anrState: ({ installed: false, folder: "" })
    property bool hdrAdvancedExpanded: false
    // The shortcut row waiting for a new key ("" = none).
    property string capturingKey: ""
    // PushFPS (screenshots): scroll so the row with this objectName is at the top.
    function scrollToObject(name) {
        const find = (item) => {
            if (!item) return null
            if (item.objectName === name) return item
            for (let i = 0; i < item.children.length; ++i) { const r = find(item.children[i]); if (r) return r }
            return null
        }
        const target = find(body)
        if (!target) return false
        const y = target.mapToItem(body, 0, 0).y
        settingsFlick.contentY = Math.max(0, Math.min(y - 12, settingsFlick.contentHeight - settingsFlick.height))
        return true
    }
    onSectionChanged: {
        capturingKey = ""
        settingsFlick.contentY = settingsFlick.originY
        if (section === "ps5") veyra.ps5Load()
        // M32: the section's groups redraw one after another (pages-b.js
        // render(true): 480 ms, i*40 ms, y 12).
        Qt.callLater(() => stagger(sectionColumns[section] || null))
    }
    readonly property var sectionColumns: ({ look: secLook, window: secWindow, play: secPlay, rec: secRec, audio: secAudio, perf: secPerf, compat: secCompat, ps5: secPs5, keys: secKeys, comp: secComp, about: secAbout })
    Component { id: staggerComp; VRise.Stagger {} }
    function stagger(col) {
        if (Theme.reduced || !col) return
        let i = 0
        for (let k = 0; k < col.children.length; ++k) {
            const c = col.children[k]
            if (!c.visible || c.motionDy === undefined) continue
            staggerComp.createObject(root, { target: c, delay: i * 40, span: 480, dy: 12 }).start()
            ++i
        }
    }

    // PushFPS: sections for features this build leaves out (PS5 streaming) are not listed.
    readonly property var sections: [
        { id: "look",   label: qsTr("外观"), icon: "palette" },
        { id: "window", label: qsTr("启动与窗口"), icon: "appwin" },
        { id: "play",   label: qsTr("播放", "section"), icon: "play" },
        { id: "rec",    label: qsTr("录制", "section"), icon: "video" },
        { id: "audio",  label: qsTr("音频"), icon: "vol" },
        { id: "perf",   label: qsTr("性能"), icon: "gauge" },
        { id: "compat", label: qsTr("兼容"), icon: "monitor" },
        { id: "ps5",   label: qsTr("PS5 串流"), icon: "gamepad" },
        { id: "keys",  label: qsTr("快捷键"), icon: "zap" },
        { id: "comp",  label: qsTr("组件与许可"), icon: "box" },
        { id: "about", label: qsTr("关于"), icon: "info" }
    ].filter(s => s.id !== "ps5" || (!!veyra.ps5 && veyra.ps5.available === true))

    // --- nav ---------------------------------------------------------------
    Rectangle {
        id: navCard
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.top: parent.top
        anchors.topMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: 220
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke

        Flickable {
            id: navScroll
            objectName: "settings-navigation"
            anchors.fill: parent
            anchors.margins: 10
            contentWidth: width
            contentHeight: navColumn.implicitHeight
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: VScrollBar { }
        }
        ColumnLayout {
            id: navColumn
            parent: navScroll.contentItem
            width: navScroll.width - (navScroll.contentHeight > navScroll.height ? 8 : 0)
            spacing: 2
            VH2 { text: qsTr("设置"); Layout.margins: 8 }
            Item { implicitHeight: 6 }
            Repeater {
                model: root.sections
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 36
                    radius: 9
                    color: root.section === modelData.id ? Theme.card3
                         : navHover.hovered ? Theme.ink(0.04) : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.d(200) } }
                    // .setnav button: icon, 10px, label.
                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10
                        VIcon {
                            name: modelData.icon
                            size: 15
                            color: modelData.tint !== undefined ? modelData.tint : navLabel.color
                        }
                        Text {
                            id: navLabel
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            elide: Text.ElideRight
                            text: modelData.label
                            color: root.section === modelData.id ? Theme.t1 : Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }
                    }
                    ToolTip.visible: navHover.hovered && navLabel.truncated
                    ToolTip.text: navLabel.text
                    ToolTip.delay: 500
                    HoverHandler { id: navHover; cursorShape: Qt.PointingHandCursor }
                    TapHandler { onTapped: root.section = modelData.id }
                }
            }
        }
    }

    // --- body --------------------------------------------------------------
    Rectangle {
        id: bodyCard
        anchors.left: parent.left
        anchors.leftMargin: 14 + 220 + 10
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.top: parent.top
        anchors.topMargin: 14
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke
        clip: true

        Flickable {
            id: settingsFlick
            anchors.fill: parent
            contentHeight: body.implicitHeight + 44
            clip: true
            ScrollBar.vertical: VScrollBar { }

            ColumnLayout {
                id: body
                x: 26
                y: 22
                // .setbody section { max-width: 720px }
                width: Math.min(parent.width - 52, 720)
                spacing: 12

                // --- 外观 ---
                ColumnLayout {
                    id: secLook
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "look"
                    VH1 { text: qsTr("外观") }
                    Text {
                        text: qsTr("界面随时可调，不影响播放和增强设置。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VH3 { text: qsTr("颜色"); Layout.topMargin: 6 }
                    VGroup {
                        // PushFPS redesign: the design itself. The palette button's panel has the same switch
                        // (PushAppearanceDialog.qml); both write preference pushDesign.
                        VRow {
                            objectName: "set-design-row"
                            label: qsTr("设计")
                            hint: qsTr("新版把导航、预览和全部快速设置放在同一个窗口里；旧版是 4.2 及更早版本的界面。两种设计的功能完全相同")
                            hintBelow: true
                            VSeg {
                                objectName: "set-design"
                                options: [{ id: "new", label: qsTr("新版") }, { id: "legacy", label: qsTr("旧版") }]
                                current: Theme.modern ? "new" : "legacy"
                                onPicked: id => veyra.setPreference("pushDesign", id)
                            }
                        }
                        // PushFPS appearance: six backgrounds, each drawn as it looks with the chosen accent, and
                        // seven accents. Every pair is checked for contrast (tst_push_appearance.qml).
                        VRow {
                            label: qsTr("主题")
                            hint: qsTr("界面的底色。纯黑适合 OLED 屏幕，浅色在明亮环境中更易读")
                            hintBelow: true
                            RowLayout {
                                id: themePicker
                                spacing: 12
                                readonly property var themes: [{ id: "default", tip: qsTr("石墨") }, { id: "black", tip: qsTr("纯黑") }, { id: "midnight", tip: qsTr("午夜蓝") },
                                                               { id: "plum", tip: qsTr("深紫") }, { id: "forest", tip: qsTr("墨绿") }, { id: "light", tip: qsTr("浅色") },
                                                               { id: "contrast", tip: qsTr("高对比度") }]
                                // The theme on screen (an unknown name shows the dark grey).
                                readonly property string shown: Theme.surfaces[Theme.mode] ? Theme.mode : "default"
                                Text {
                                    objectName: "set-theme-name"
                                    text: (themePicker.themes.find(t => t.id === themePicker.shown) || themePicker.themes[0]).tip
                                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12
                                }
                            Row {
                                objectName: "set-theme"
                                spacing: 8
                                Repeater {
                                    model: themePicker.themes
                                    delegate: Rectangle {
                                        id: themeTile
                                        required property var modelData
                                        readonly property var look: Theme.surfaceOf(modelData.id)   // (as this design draws it)
                                        readonly property bool lightLook: modelData.id === "light"
                                        readonly property bool on: themePicker.shown === modelData.id
                                        objectName: "set-theme-" + modelData.id
                                        width: 66; height: 44; radius: 9
                                        color: look.bgMid
                                        border.width: on ? 2 : 1
                                        border.color: on ? Theme.accent : Theme.stroke2
                                        // A card of that theme: two lines of text and a switch in the accent.
                                        Rectangle {
                                            x: 8; y: 8; width: parent.width - 16; height: parent.height - 16; radius: 5
                                            color: themeTile.look.card2
                                            // High contrast: a card is told from the background by its outline.
                                            border.width: themeTile.modelData.id === "contrast" ? 1 : 0
                                            border.color: "#FFFFFF"
                                            Rectangle { x: 6; y: 7; width: 20; height: 3; radius: 1.5; color: themeTile.lightLook ? "#15171A" : "#F3F3F5"; opacity: 0.85 }
                                            Rectangle { x: 6; y: 15; width: 13; height: 3; radius: 1.5; color: themeTile.lightLook ? "#15171A" : "#F3F3F5"; opacity: 0.4 }
                                            Rectangle {
                                                anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                                                width: 13; height: 8; radius: 4
                                                color: themeTile.lightLook ? Theme.accentPair.light : Theme.accentPair.dark
                                            }
                                        }
                                        scale: tileHover.hovered && !on ? 1.04 : 1
                                        Behavior on scale { NumberAnimation { duration: Theme.d(180); easing.type: Easing.OutCubic } }
                                        HoverHandler { id: tileHover; cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: veyra.setPreference("pushTheme", themeTile.modelData.id) }
                                        ToolTip.visible: tileHover.hovered
                                        ToolTip.text: modelData.tip
                                        Accessible.role: Accessible.RadioButton
                                        Accessible.name: modelData.tip
                                        Accessible.checked: on
                                    }
                                }
                            }
                            }
                        }
                        VRow {
                            label: qsTr("强调色")
                            hint: qsTr("开关、进度、选中状态")
                            // .swatch: one dot per accent, in the form it takes on this background; the chosen one ringed.
                            RowLayout {
                                id: accentPicker
                                spacing: 12
                                readonly property var accentList: [{ id: "orange", tip: qsTr("PushFPS 绿") }, { id: "white", tip: qsTr("白色光晕") }, { id: "ice", tip: qsTr("冰蓝") },
                                                                   { id: "blue", tip: qsTr("蓝色") }, { id: "violet", tip: qsTr("紫色") }, { id: "rose", tip: qsTr("玫红") },
                                                                   { id: "amber", tip: qsTr("琥珀") }]
                                readonly property string shown: Theme.accents[Theme.accentName] ? Theme.accentName : "orange"
                                Text {
                                    objectName: "set-accent-name"
                                    text: (accentPicker.accentList.find(a => a.id === accentPicker.shown) || accentPicker.accentList[0]).tip
                                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12
                                }
                            Row {
                                objectName: "set-accent"
                                spacing: 10
                                Repeater {
                                    model: accentPicker.accentList
                                    delegate: Rectangle {
                                        id: swatch
                                        required property var modelData
                                        readonly property bool on: accentPicker.shown === modelData.id
                                        objectName: "set-accent-" + modelData.id
                                        width: 24; height: 24; radius: 12
                                        color: Theme.light ? Theme.accents[modelData.id].light : Theme.accents[modelData.id].dark
                                        border.width: 2
                                        border.color: on ? Theme.t1 : "transparent"
                                        // A gap between the dot and its ring, so the ring reads on every colour.
                                        Rectangle {
                                            visible: swatch.on
                                            anchors.fill: parent; anchors.margins: 2
                                            radius: width / 2
                                            color: "transparent"
                                            border.width: 2
                                            border.color: Theme.card2
                                        }
                                        scale: on ? 1.12 : 1
                                        Behavior on scale { NumberAnimation { duration: Theme.d(400); easing.bezierCurve: Theme.spring } }
                                        HoverHandler { id: swHover; cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: veyra.setPreference("accent", swatch.modelData.id) }
                                        ToolTip.visible: swHover.hovered
                                        ToolTip.text: modelData.tip
                                        Accessible.role: Accessible.RadioButton
                                        Accessible.name: modelData.tip
                                        Accessible.checked: on
                                    }
                                }
                            }
                            }
                        }
                        // 日落 (the default) or the original black-grey 经典 palette.
                        // PushFPS: its own 主题 row below replaces Veyra's palette choice.
                        VRow {
                            visible: false
                            label: qsTr("界面配色")
                            hint: qsTr("日落：深靛紫底、日落渐变强调色；经典：原来的黑灰配色")
                            VSeg {
                                objectName: "set-palette"
                                options: [{ id: "dusk", label: qsTr("日落") }, { id: "classic", label: qsTr("经典") }]
                                current: veyra.preferences.palette === "classic" ? "classic" : "dusk"
                                onPicked: id => veyra.setPreference("palette", id)
                            }
                        }
                        VRow {
                            visible: !Theme.modern   // (the new design's background is one flat colour)
                            label: qsTr("背景渐变强度")
                            hint: Theme.dusk ? qsTr("日落天空的渐变强度，带抖动防色带") : qsTr("深黑底上的极轻渐变与抖动（防色带）")
                            VSeg {
                                objectName: "set-backdrop"
                                options: [{ id: "0", label: qsTr("无") }, { id: "1", label: qsTr("轻") }, { id: "2", label: qsTr("中") }]
                                current: String(veyra.preferences.backdrop !== undefined ? veyra.preferences.backdrop : 1)
                                onPicked: id => veyra.setPreference("backdrop", Number(id))
                            }
                        }
                    }
                    VH3 { text: qsTr("首页与启动"); Layout.topMargin: 6 }
                    VGroup {
                        // PushFPS Sunset City: the logo on the home page, the intro (PushIntro.qml) and the music.
                        VRow {
                            visible: !Theme.modern   // (the logo of the legacy home page; the new design has the wordmark in its title bar)
                            label: qsTr("日落之城标志")
                            hint: qsTr("首页的 PushFPS 标志里是霓虹日落下的城市；关闭后显示经典标志")
                            VSwitch {
                                objectName: "set-sunset"
                                checked: veyra.preferences.pushSunset !== false
                                onToggled: checked => veyra.setPreference("pushSunset", checked)
                            }
                        }
                        VRow {
                            label: qsTr("启动动画")
                            hint: Theme.modern ? qsTr("打开 PushFPS 时播放约 2 秒的开场动画，单击或按任意键可跳过")
                                               : qsTr("打开 PushFPS 时播放约 3 秒的日落之城开场，单击或按任意键可跳过")
                            VSwitch {
                                objectName: "set-intro"
                                checked: veyra.preferences.introAnimation !== false
                                onToggled: checked => veyra.setPreference("introAnimation", checked)
                            }
                        }
                        // This saved switch controls future launches. The corner button
                        // stops only the current launch and leaves this preference intact.
                        // The startup music: the file Veyra's runtime package installs (Main.qml musicOn).
                        VRow {
                            label: qsTr("启动音乐")
                            hint: typeof vyMusic !== "undefined" && vyMusic.available
                                  ? qsTr("每次启动播放一次，动画结束后继续；此开关会保存，角落按钮仅停止本次")
                                  : qsTr("未找到音乐文件：程序目录下的 media/startup-music.mp3")
                            VSwitch {
                                objectName: "set-intro-music"
                                checked: veyra.preferences.introMusic === true
                                onToggled: checked => {
                                    veyra.setPreference("introMusic", checked)
                                    if (!checked && typeof vyMusic !== "undefined") vyMusic.pause()
                                }
                            }
                        }
                    }
                    VH3 { text: qsTr("窗口与操作"); Layout.topMargin: 6 }
                    VGroup {
                        VRow {
                            label: qsTr("页面切换栏")
                            hint: qsTr("平时隐藏；鼠标碰到窗口顶部时弹下来")
                            VSeg {
                                objectName: "set-dock"
                                options: [{ id: "auto", label: qsTr("自动隐藏") }, { id: "always", label: qsTr("始终显示") }]
                                current: veyra.preferences.dockPinned === true ? "always" : "auto"
                                onPicked: id => veyra.setPreference("dockPinned", id === "always")
                            }
                        }
                        VRow {
                            label: qsTr("界面缩放")
                            hint: (veyra.preferences.uiScale || 0) !== veyra.uiScaleActive ? qsTr("重启后生效") : qsTr("自动 = 跟随 Windows 显示缩放")
                            VSeg {
                                objectName: "set-scale"
                                options: [{ id: "0", label: qsTr("自动") }, { id: "100", label: "100%" }, { id: "125", label: "125%" }, { id: "150", label: "150%" }]
                                current: String(veyra.preferences.uiScale || 0)
                                onPicked: id => veyra.setPreference("uiScale", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("减少动画")
                            hint: qsTr("关闭弹性与转场")
                            VSwitch {
                                checked: veyra.reducedMotion
                                onToggled: checked => veyra.reducedMotion = checked
                            }
                        }
                        // PushFPS: the on-screen readout (latency and frame rate over the picture) in a larger size.
                        VRow {
                            objectName: "set-readout-size-row"
                            label: qsTr("画面读数大小")
                            hint: qsTr("画面上的延迟和帧率读数。坐得远或用手柄玩时，大一些更容易看清")
                            VSeg {
                                objectName: "set-readout-size"
                                options: [{ id: "0", label: qsTr("标准") }, { id: "1", label: qsTr("大") }, { id: "2", label: qsTr("特大") }]
                                current: String(Math.max(0, Math.min(2, Number(veyra.preferences.pushLatencySize || 0))))
                                onPicked: id => veyra.setPreference("pushLatencySize", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("滑块键盘微调")
                            hint: qsTr("点一下滑块后用 ← → 调整，每次的幅度；Esc 或单击画面交还方向键给快进快退")
                            VSeg {
                                objectName: "set-slider-step"
                                options: [{ id: "1", label: "1" }, { id: "0.1", label: "0.1" }, { id: "0.01", label: "0.01" }]
                                current: String(veyra.preferences.sliderKeyStep !== undefined ? veyra.preferences.sliderKeyStep : 0.1)
                                onPicked: id => veyra.setPreference("sliderKeyStep", Number(id))
                            }
                        }
                    }
                    VH3 { text: qsTr("语言"); Layout.topMargin: 6 }
                    VGroup {
                        VRow {
                            label: qsTr("界面语言")
                            hint: qsTr("立即生效；日志保持中文，方便反馈问题")
                            VSelect {
                                objectName: "set-language"
                                implicitWidth: 190
                                // Each language is named in itself, so it can be found from any of them.
                                options: [
                                    { id: "auto", label: qsTr("跟随系统") },
                                    { id: "zh-CN", label: qsTr("简体中文") },
                                    { id: "zh-TW", label: qsTr("繁體中文") },
                                    { id: "en", label: "English" },
                                    { id: "ja", label: qsTr("日本語") },
                                    { id: "ko", label: "한국어" },
                                    // PushFPS: machine-translated (scripts/i18n/machine_translate.py).
                                    { id: "nl", label: "Nederlands" },
                                    { id: "de", label: "Deutsch" },
                                    { id: "es", label: "Español" },
                                    { id: "fr", label: "Français" },
                                    { id: "it", label: "Italiano" },
                                    { id: "pt-BR", label: "Português (Brasil)" },
                                    { id: "tr", label: "Türkçe" },
                                    { id: "pl", label: "Polski" },
                                    { id: "ru", label: "Русский" },
                                    { id: "ar", label: "العربية" }
                                ]
                                value: (options.find(o => o.id === (veyra.preferences.language || "auto")) || options[0]).label
                                onPicked: id => veyra.setPreference("language", id)
                            }
                        }
                    }
                }

                // --- 启动与窗口 ---
                ColumnLayout {
                    id: secWindow
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "window"
                    VH1 { text: qsTr("启动与窗口") }
                    Text {
                        text: qsTr("程序启动时的页面、窗口和恢复行为。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VGroup {
                        VRow {
                            label: qsTr("打开时的默认页面")
                            hint: qsTr("首页 = 选择片源的页面（点顶部 Logo 也能回到这里）")
                            VSeg {
                                objectName: "set-default-page"
                                options: [
                                    { id: "home", label: qsTr("首页") },
                                    { id: "min", label: qsTr("极简模式") },
                                    { id: "pro", label: qsTr("专业模式") },
                                    { id: "last", label: qsTr("上次") }
                                ]
                                current: veyra.defaultPage
                                onPicked: id => veyra.defaultPage = id
                            }
                        }
                        VRow {
                            label: qsTr("启动窗口大小")
                            hint: qsTr("下次启动生效；超过屏幕时按屏幕缩小并居中")
                            VSelect {
                                objectName: "set-window-size"
                                readonly property var sizes: [
                                    { id: "1280x800", label: "1280 × 800" },
                                    { id: "1600x1000", label: "1600 × 1000" },
                                    { id: "1920x1200", label: "1920 × 1200" },
                                    { id: "last", label: qsTr("记住上次大小") }
                                ]
                                options: sizes
                                value: (sizes.find(o => o.id === (veyra.preferences.windowSize || "1280x800")) || sizes[0]).label
                                onPicked: id => veyra.setPreference("windowSize", id)
                            }
                        }
                        VRow {
                            label: qsTr("启动后自动继续上次内容")
                            hint: qsTr("视频恢复上次进度；采集卡沿用上次设备与设置")
                            VSwitch {
                                objectName: "set-auto-resume"
                                checked: !!veyra.preferences.autoResume
                                onToggled: v => veyra.setPreference("autoResume", v)
                            }
                        }
                        // PushFPS: keep the player above other windows (Main.qml applyOnTop).
                        VRow {
                            label: qsTr("窗口置顶")
                            hint: qsTr("PushFPS 窗口保持在其他窗口上面，适合边玩边看攻略或聊天")
                            VSwitch {
                                objectName: "set-always-on-top"
                                checked: veyra.preferences.pushAlwaysOnTop === true
                                onToggled: v => veyra.setPreference("pushAlwaysOnTop", v)
                            }
                        }
                        VRow {
                            label: qsTr("记住播放位置")
                            hint: qsTr("重新打开同一个文件时从上次位置继续")
                            VSwitch {
                                objectName: "set-resume"
                                checked: veyra.preferences.rememberPosition !== false
                                onToggled: checked => veyra.setPreference("rememberPosition", checked)
                            }
                        }
                    }
                }

                // --- 播放 ---
                ColumnLayout {
                    id: secPlay
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "play"
                    VH1 { text: qsTr("播放", "section") }
                    Text {
                        text: qsTr("文件播放、字幕与截图的默认行为。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VH3 { text: qsTr("解码", "heading"); Layout.topMargin: 6 }
                    VGroup {
                        VRow {
                            label: qsTr("解码")
                            hint: (veyra.preferences.decode || "auto") === "hardware"
                                  ? qsTr("强制硬解 = 只用硬件解码，打不开时报错不回退；下次打开文件生效")
                                  : qsTr("自动 = 优先硬解，失败回退软解；下次打开文件生效")
                            VSeg {
                                objectName: "set-decode"
                                options: [{ id: "auto", label: qsTr("自动") }, { id: "hardware", label: qsTr("强制硬解") }, { id: "software", label: qsTr("软解") }]
                                current: veyra.preferences.decode || "auto"
                                onPicked: id => veyra.setPreference("decode", id)
                            }
                        }
                        VRow {
                            label: qsTr("自动去隔行")
                            hint: veyra.deinterlaceAvailable ? qsTr("只处理标记为隔行的视频，优先使用 GPU；软解使用兼容算法。下次打开视频生效。") : qsTr("当前软件包未包含去隔行组件")
                            VSwitch { enabled: veyra.deinterlaceAvailable; checked: veyra.preferences.deinterlace !== false; onToggled: checked => veyra.setPreference("deinterlace",checked) }
                        }
                        VRow {
                            objectName: "set-sharp-scaling-row"
                            label: qsTr("锐利缩放")
                            hint: veyra.preferences.pushSharpScaling !== false
                                  ? qsTr("画面与窗口大小不同时更清晰：放大不发软，缩小不闪烁。大小相同时逐像素一致")
                                  : qsTr("已关闭：按 4.1 之前的方式缩放")
                            VSwitch {
                                objectName: "set-sharp-scaling"
                                checked: veyra.preferences.pushSharpScaling !== false
                                onToggled: checked => veyra.setPreference("pushSharpScaling", checked)
                            }
                        }
                    }
                    VH3 { text: qsTr("HDR"); Layout.topMargin: 6 }
                    VGroup {
                        VRow {
                            label: qsTr("HDR 输出格式")
                            hint: qsTr("HDR10 默认；补帧固定 HDR10，需要 Windows HDR 开启")
                            VSeg { options: [{id:"0",label:"HDR10"},{id:"1",label:qsTr("scRGB 浮点")}]; current: String(veyra.hdrOutputMode); onPicked: id => veyra.hdrOutputMode = Number(id) }
                        }
                        VRow {
                            label: qsTr("HDR 亮度曲线") + " / " + qsTr("HDR 元数据")
                            VButton {
                                objectName: "set-hdr-advanced-toggle"
                                text: root.hdrAdvancedExpanded ? qsTr("收起") : qsTr("展开")
                                trailingIcon: root.hdrAdvancedExpanded ? "up" : "down"
                                onClicked: root.hdrAdvancedExpanded = !root.hdrAdvancedExpanded
                            }
                        }
                        ColumnLayout {
                            objectName: "set-hdr-advanced-body"
                            Layout.fillWidth: true
                            spacing: 0
                            visible: root.hdrAdvancedExpanded
                            // HDR 亮度曲线: static, so the highlights are protected.
                            // One-click presets (same ids as the list page).
                            Flow {
                                Layout.fillWidth: true
                                Layout.preferredHeight: implicitHeight
                                spacing: 6
                                Repeater {
                                    model: [{id:"standard",label:qsTr("标准")},
                                            {id:"darkLift",label:qsTr("暗场提亮")},
                                            {id:"highlightGuard",label:qsTr("高光保护")},
                                            {id:"darkAndBright",label:qsTr("暗亮双修")},
                                            {id:"brightRoom",label:qsTr("明亮环境")},
                                            {id:"darkRoom",label:qsTr("暗室观影")},
                                            {id:"punch",label:qsTr("强化冲击")}]
                                    delegate: VButton {
                                        required property var modelData
                                        text: modelData.label
                                        ghost: true
                                        primary: veyra.hdrTuningPreset === modelData.id
                                        onClicked: veyra.applyHdrTuningPreset(modelData.id)
                                    }
                                }
                            }
                            VRow {
                                label: qsTr("HDR 亮度曲线")
                                hint: qsTr("静态曲线：抬暗部、定中灰、压高光")
                                value: veyra.hdrCurve ? qsTr("已开启") : qsTr("已关闭")
                                VSwitch {
                                    objectName: "hdrcurve-enabled"
                                    checked: veyra.hdrCurve
                                    onToggled: checked => veyra.hdrCurve = checked
                                }
                            }
                            VRow {
                                label: qsTr("强度")
                                hint: qsTr("0% 关闭曲线；峰值保护与元数据独立")
                                value: veyra.hdrCurveStrength + "%"
                                VSlider {
                                    objectName: "hdrcurve-strength"
                                    implicitWidth: 120
                                    valueFromModel: true
                                    resettable: true; defaultValue: 100
                                    from: 0; to: 200
                                    value: veyra.hdrCurveStrength
                                    onMoved: value => veyra.hdrCurveStrength = Math.round(value)
                                }
                            }
                            VRow {
                                label: qsTr("显示设备峰值 (nit)")
                                hint: veyra.hdrDisplayInfo.valid
                                    ? qsTr("系统报告：峰值 %1、全屏 %2、黑位 %3 nit")
                                        .arg(veyra.hdrDisplayInfo.peakNits)
                                        .arg(veyra.hdrDisplayInfo.fullFrameNits)
                                        .arg(Number(veyra.hdrDisplayInfo.minNits).toFixed(3))
                                    : qsTr("0 = 自动使用系统报告值")
                                value: veyra.hdrDisplayPeakNits > 0
                                    ? (veyra.hdrDisplayPeakNits + " nit") : qsTr("自动")
                                VSlider {
                                    objectName: "hdrcurve-displaypeak"
                                    implicitWidth: 120
                                    valueFromModel: true
                                    resettable: true; defaultValue: 0
                                    from: 0; to: 4000
                                    value: veyra.hdrDisplayPeakNits
                                    onMoved: value => veyra.hdrDisplayPeakNits = Math.round(value)
                                }
                            }
                            Repeater {
                                model: [{key:"shadowLiftEv100",label:qsTr("暗部抬升 (0.01 EV)"),from:-200,to:200,def:0},
                                        {key:"shadowRangeNits",label:qsTr("抬升范围 (nit)"),from:1,to:100,def:10,hint:qsTr("只在这个亮度以下的暗部生效")},
                                        {key:"midGrayNits",label:qsTr("中灰 (nit)"),from:100,to:500,def:203,hint:qsTr("203 = 中性；调高整体变亮，峰值不变")},
                                        {key:"highlightStartNits",label:qsTr("高光起点 (nit)"),from:0,to:2000,def:0,hint:qsTr("0 = 不做高光滚降")},
                                        {key:"rollOffPercent",label:qsTr("滚降强度 (%)"),from:10,to:200,def:100},
                                        {key:"peakCapNits",label:qsTr("峰值上限 (nit)"),from:0,to:4000,def:0,hint:qsTr("0 = 不限制")}]
                                delegate: VRow {
                                    required property var modelData
                                    label: modelData.label
                                    hint: modelData.hint || ""
                                    value: String(veyra.hdrCurveParams[modelData.key] ?? "—")
                                    VSlider {
                                        objectName: "hdrcurve-" + modelData.key
                                        implicitWidth: 150
                                        valueFromModel: true
                                        resettable: true; defaultValue: modelData.def
                                        from: modelData.from; to: modelData.to
                                        value: veyra.hdrCurveParams[modelData.key] ?? modelData.def
                                        onMoved: value => veyra.setHdrCurveParameter(modelData.key, Math.round(value))
                                    }
                                }
                            }
                            // HDR 元数据: what the display's own tone mapping works from.
                            VRow {
                                label: qsTr("HDR 元数据")
                                hint: qsTr("HDR10 提示；系统或显示器可能忽略")
                                value: veyra.hdrMetadataEnabled ? qsTr("已启用（HDR10）") : qsTr("已关闭")
                                VSwitch {
                                    objectName: "hdrmeta-enabled"
                                    checked: veyra.hdrMetadataEnabled
                                    onToggled: checked => veyra.hdrMetadataEnabled = checked
                                }
                            }
                            Repeater {
                                model: [{key:"masteringPeakNits",label:qsTr("母版峰值 (nit)"),from:0,to:4000,def:0,hint:qsTr("0 = 跟随片源；实际效果取决于显示设备")},
                                        {key:"maxCllNits",label:qsTr("MaxCLL (nit)"),from:0,to:4000,def:0,hint:qsTr("0 = 跟随片源")},
                                        {key:"maxFallNits",label:qsTr("MaxFALL (nit)"),from:0,to:4000,def:0,hint:qsTr("0 = 跟随片源")}]
                                delegate: VRow {
                                    required property var modelData
                                    label: modelData.label
                                    hint: modelData.hint || ""
                                    value: String(veyra.hdrMetadataParams[modelData.key] ?? "—")
                                    VSlider {
                                        objectName: "hdrmeta-" + modelData.key
                                        implicitWidth: 150
                                        valueFromModel: true
                                        resettable: true; defaultValue: modelData.def
                                        from: modelData.from; to: modelData.to
                                        value: veyra.hdrMetadataParams[modelData.key] ?? modelData.def
                                        onMoved: value => veyra.setHdrMetadataParameter(modelData.key, Math.round(value))
                                    }
                                }
                            }

                        }
                    }
                    VH3 { text: qsTr("网络串流"); Layout.topMargin: 6 }
                    VGroup {
                        // Network / server playback (#48). Read when a source opens.
                        VRow {
                            label: qsTr("网络缓冲")
                            hint: qsTr("网络地址、直播与服务器视频先攒一段再播放；服务器速度低于视频码率时暂停缓冲并说明原因，不会报错黑屏。下次打开网络视频生效")
                            VSeg {
                                objectName: "set-network-buffer"
                                options: [{ id: "auto", label: qsTr("自动") }, { id: "off", label: qsTr("关闭", "off") },
                                          { id: "small", label: qsTr("小") }, { id: "medium", label: qsTr("中") }, { id: "large", label: qsTr("大") }]
                                current: veyra.preferences.networkBuffer || "auto"
                                onPicked: id => veyra.setPreference("networkBuffer", id)
                            }
                        }
                        VRow {
                            label: qsTr("起播预缓冲")
                            hint: qsTr("开始播放前先攒够的秒数；“跟随缓冲档”按上面的缓冲档位决定。下次打开网络视频生效")
                            VSelect {
                                objectName: "set-network-prebuffer"
                                implicitWidth: 170
                                options: [{ id: "-1", label: qsTr("跟随缓冲档") }, { id: "0", label: qsTr("不等待") },
                                          { id: "3", label: qsTr("%1 秒").arg(3) }, { id: "5", label: qsTr("%1 秒").arg(5) },
                                          { id: "10", label: qsTr("%1 秒").arg(10) }, { id: "20", label: qsTr("%1 秒").arg(20) },
                                          { id: "30", label: qsTr("%1 秒").arg(30) }]
                                value: (options.find(o => o.id === (veyra.preferences.networkPrebuffer === undefined ? "-1" : String(veyra.preferences.networkPrebuffer))) || options[0]).label
                                onPicked: id => veyra.setPreference("networkPrebuffer", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("断线自动重连")
                            hint: qsTr("网络中断后自动重连：点播从断点继续，直播重新连接；关闭后断线即停止。下次打开网络视频生效")
                            VSwitch {
                                objectName: "set-network-reconnect"
                                checked: veyra.preferences.networkAutoReconnect !== false
                                onToggled: checked => veyra.setPreference("networkAutoReconnect", checked)
                            }
                        }
                        VRow {
                            label: qsTr("损坏画面处理")
                            hint: (veyra.preferences.networkCorrupt || "conceal") === "keyframe"
                                  ? qsTr("等待关键帧 = 丢弃损坏的画面，停在上一帧直到下一个完好的关键帧；只对网络视频与 MPEG-TS 生效")
                                  : qsTr("保持画面 = 显示解码器修补后的画面，动作不中断；只对网络视频与 MPEG-TS 生效")
                            VSeg {
                                objectName: "set-network-corrupt"
                                options: [{ id: "conceal", label: qsTr("保持画面") }, { id: "keyframe", label: qsTr("等待关键帧") }]
                                current: veyra.preferences.networkCorrupt || "conceal"
                                onPicked: id => veyra.setPreference("networkCorrupt", id)
                            }
                        }
                    }
                    VH3 { text: qsTr("字幕与截图"); Layout.topMargin: 6 }
                    VGroup {
                        VRow {
                            label: qsTr("字幕默认字号")
                            hint: qsTr("双行不自动缩小；更多样式在字幕设置里")
                            VSeg {
                                objectName: "set-subsize"
                                readonly property int size: veyra.preferences.subtitleSize || 22
                                options: [{ id: "18", label: qsTr("小") }, { id: "22", label: qsTr("中") }, { id: "28", label: qsTr("大") }]
                                current: size <= 19 ? "18" : size >= 26 ? "28" : "22"
                                onPicked: id => veyra.setPreference("subtitleSize", Number(id))
                            }
                        }
                        VRow {
                            visible: (veyra.liveTranslation || null) !== null
                            label: qsTr("实时翻译")
                            VButton {
                                objectName: "set-live-translation"
                                text: qsTr("实时翻译设置…")
                                onClicked: root.requestPage("translation")
                            }
                        }
                        // PushFPS: read the text on the picture (PushReadTextDialog.qml).
                        VRow {
                            objectName: "set-readtext"
                            label: qsTr("读取画面上的文字")
                            hint: qsTr("取一帧画面，由 Windows 在这台电脑上识别其中的文字，可以复制或翻译。快捷键 %1").arg(String((veyra.shortcuts || ({})).readText || "") || qsTr("未设置"))
                            VButton { objectName: "set-readtext-open"; text: qsTr("打开"); onClicked: veyra.navigate("readtext") }
                        }
                        VRow {
                            label: qsTr("截图保存位置")
                            hint: veyra.screenshotDirectory + qsTr("（SDR 存 PNG，HDR 存 JPEG XR）")
                            RowLayout {
                                spacing: 6
                                VButton { text: qsTr("打开"); ghost: true; onClicked: veyra.openScreenshotDirectory() }
                                VButton { objectName: "set-shotdir"; text: qsTr("更改…"); onClicked: veyra.chooseScreenshotDirectory() }
                            }
                        }
                    }
                    VH3 { text: "NVIDIA Smooth Motion"; Layout.topMargin: 6 }
                    VGroup {
                        objectName: "set-smooth-motion"
                        VRow {
                            label: qsTr("Smooth Motion 开启方法")
                            hint: qsTr("NVIDIA 驱动级 AI 插帧，与软件内补帧二选一")
                            VButton {
                                text: smoothHelp.visible ? qsTr("收起") : qsTr("查看")
                                ghost: true
                                onClicked: smoothHelp.visible = !smoothHelp.visible
                            }
                        }
                        Text {
                            id: smoothHelp
                            visible: false
                            Layout.fillWidth: true
                            Layout.leftMargin: 12; Layout.rightMargin: 12; Layout.bottomMargin: 10
                            text: qsTr("只用驱动补帧时：在专业模式的补帧页关闭补帧，再到 NVIDIA App 打开 Smooth Motion（AI 插帧）。") + "\n"
                                  + qsTr("只用 DLSS / XeSS 补帧时：到 NVIDIA App 关闭 Smooth Motion。") + "\n"
                                  + qsTr("软件不检测、不拦截两者叠加，也不修改驱动配置；AMD FSR 补帧暂不提供。")
                            color: Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                            lineHeight: 1.3
                        }
                    }
                }
                // --- 录制 --- PushFPS: recordings, comparison clips, instant replay and the virtual camera have their own
                // page. They stood inside the playback page, in one card of sixty rows (cards inside a card).
                ColumnLayout {
                    id: secRec
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "rec"
                    VH1 { text: qsTr("录制", "section") }
                    Text {
                        text: qsTr("录制、对比片段、即时回放和虚拟摄像机的设置。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VH3 { text: qsTr("文件", "heading"); Layout.topMargin: 6 }
                    VGroup {
                        objectName: "set-recording"
                        VRow {
                            label: qsTr("录制保存位置")
                            hint: veyra.recordingDirectory + qsTr("（MP4，录下处理后的画面和正在播放的声音）")
                            RowLayout {
                                spacing: 6
                                VButton { text: qsTr("打开"); ghost: true; onClicked: veyra.openRecordingDirectory() }
                                VButton { objectName: "set-recdir"; text: qsTr("更改…"); onClicked: veyra.chooseRecordingDirectory() }
                            }
                        }
                        // PushFPS 4.0: the record buttons show the choices first (PushRecordDialog.qml).
                        VRow {
                            label: qsTr("每次录制前都询问")
                            hint: qsTr("点录制按钮时先选形状、大小和画质再开始。录制快捷键总是直接开始")
                            VSwitch {
                                objectName: "set-record-ask"
                                checked: veyra.preferences.pushRecordAsk !== false
                                onToggled: v => veyra.setPreference("pushRecordAsk", v)
                            }
                        }
                        // PushFPS 4.1: a second file, 1080 x 1920, beside every wide recording (Recording.cpp).
                        VRow {
                            objectName: "set-rec-vertical-row"
                            label: qsTr("同时录一份竖屏副本")
                            hint: veyra.preferences.pushRecordVerticalCopy === true
                                  ? qsTr("每次录制另存一份 1080 × 1920 的副本（文件名以 -vertical 结尾）。两个编码同时进行，更吃显卡")
                                  : qsTr("关闭：只录一个文件。适合同时发 TikTok、Shorts 和 Reels 的人")
                            VSwitch {
                                objectName: "set-rec-vertical"
                                checked: veyra.preferences.pushRecordVerticalCopy === true
                                onToggled: v => veyra.setPreference("pushRecordVerticalCopy", v)
                            }
                        }
                        VRow {
                            label: qsTr("自动分段")
                            hint: qsTr("长时间录制时每隔一段时间接着写入一个新文件；各段之间不丢画面也不丢声音，可以无缝接起来。单个文件更小，出问题时损失更少")
                            VSeg {
                                objectName: "set-recsplit"
                                options: [{ id: "0", label: qsTr("关") }, { id: "30", label: qsTr("%1 分钟").arg(30) }, { id: "60", label: qsTr("%1 分钟").arg(60) }, { id: "120", label: qsTr("%1 分钟").arg(120) }]
                                current: String([0, 30, 60, 120].indexOf(Number(veyra.preferences.recordSplitMinutes || 0)) >= 0 ? Number(veyra.preferences.recordSplitMinutes || 0) : 0)
                                onPicked: id => veyra.setPreference("recordSplitMinutes", Number(id))
                            }
                        }
                        // Split by size. Splitting by time is the row 自动分段 above (recordSplitMinutes). Both cut
                        // without a gap: one encoder runs through every part (engine/PushRecordParts.h).
                        VRow {
                            label: qsTr("按大小分段")
                            hint: qsTr("文件快到这个大小时，在关键帧处接着写入新文件；各段之间不丢画面也不丢声音。0 表示关闭，下次录制时生效")
                            RowLayout {
                                DialogHost.VTextField {
                                    objectName: "record-split-mib"
                                    implicitWidth: 88
                                    text: String(veyra.preferences.recordSplitMiB || 0)
                                    validator: IntValidator { bottom: 0; top: 1048576 }
                                    onEdited: veyra.setPreference("recordSplitMiB", Number(text))
                                }
                                Text { text: "MiB"; color: Theme.t3; font.pixelSize: Theme.fsSmall }
                            }
                        }
                    }
                    VH3 { text: qsTr("画质"); Layout.topMargin: 6 }
                    VGroup {
                        // PushFPS upload presets (PushOutputPresets.js): YouTube, Shorts, TikTok, Reels, X and
                        // editing, per resolution and frame rate. Every value below stays editable.
                        VRow {
                            id: recPresetRow
                            readonly property var preset: OutputPresets.recordPreset(String(veyra.preferences.recordProfile || ""))
                            readonly property var platform: preset ? OutputPresets.platformOf(OutputPresets.recordPlatforms, preset.id) : null
                            readonly property var changed: preset ? OutputPresets.differences(veyra.preferences, preset, false) : []
                            function platformName(pl) { return pl.id === "edit" ? qsTr("剪辑用") : pl.name }
                            function presetText(pl, pr) {
                                const size = OutputPresets.sizeLabel(pr.width, pr.height) || qsTr("画面原尺寸")
                                if (pl.id === "edit") return platformName(pl) + " · " + size + " · " + (pr.hevc ? qsTr("HEVC 恒定质量") : qsTr("H.264 恒定质量"))
                                return pl.name + " · " + OutputPresets.presetLabel(pr, size)
                            }
                            label: qsTr("上传预设")
                            help: qsTr("按平台推荐的分辨率、帧率、码率、编码和音频一次填好。之后每一项都可以自己改")
                            hint: !preset ? qsTr("手动设置")
                                : changed.length > 0 ? qsTr("基于 %1，已修改").arg(presetText(platform, preset))
                                : preset.hevc && platform.hdr ? qsTr("HDR 需要 NVIDIA 显卡和 HDR 输出（HEVC 10bit）")
                                : qsTr("使用中：%1").arg(presetText(platform, preset))
                            VSelect {
                                objectName: "set-recprofile"
                                implicitWidth: 240
                                title: qsTr("上传预设")
                                options: {
                                    const out = [{ id: "", label: qsTr("手动设置") }]
                                    for (const pl of OutputPresets.recordPlatforms)
                                        for (const pr of pl.presets) out.push({ id: pr.id, label: recPresetRow.presetText(pl, pr) })
                                    return out
                                }
                                value: recPresetRow.preset ? recPresetRow.presetText(recPresetRow.platform, recPresetRow.preset) : qsTr("手动设置")
                                onPicked: id => {
                                    const pr = OutputPresets.recordPreset(id)
                                    if (!pr) { veyra.setPreference("recordProfile", ""); return }
                                    const prefs = OutputPresets.recordPrefs(pr)
                                    for (const k in prefs) veyra.setPreference(k, prefs[k])
                                    // PushFPS 4.5.4: what is chosen here wins; the record window's "smaller files" share goes.
                                    veyra.setPreference("pushRecordBitrateScale", 100)
                                }
                            }
                        }
                        VRow {
                            label: qsTr("输出分辨率")
                            hint: qsTr("录制文件的尺寸，与直播分别设置。比画面大时会放大，不会更清晰")
                            VSelect {
                                objectName: "set-recsize"
                                implicitWidth: 180
                                readonly property int w: Number(veyra.preferences.recordWidth || 0)
                                readonly property int h: Number(veyra.preferences.recordHeight || 0)
                                function sizeText(sw, sh) { return sw > 0 ? (sh > sw ? sw + " × " + sh + qsTr("（竖屏）") : sw * 9 > sh * 20 ? sw + " × " + sh + "（21:9）" : sw + " × " + sh) : qsTr("画面原尺寸") }
                                options: OutputPresets.recordSizes.map(z => ({ id: z[0] + "x" + z[1], label: sizeText(z[0], z[1]) }))
                                value: sizeText(w, h)
                                onPicked: id => { const z = id.split("x"); veyra.setPreference("recordWidth", Number(z[0])); veyra.setPreference("recordHeight", Number(z[1])) }
                            }
                        }
                        VRow {
                            visible: Number(veyra.preferences.recordWidth || 0) > 0
                            label: qsTr("画面填充")
                            hint: qsTr("形状不同时：适应保留整个画面（有黑边），填满裁掉边缘，拉伸会变形。位置和裁剪在直播模式的布局里调整")
                            VSeg {
                                objectName: "set-recfit"
                                options: [{ id: "0", label: qsTr("适应") }, { id: "1", label: qsTr("填满") }, { id: "2", label: qsTr("拉伸") }]
                                current: String(Number(veyra.preferences.recordFit || 0))
                                onPicked: id => veyra.setPreference("recordFit", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("录制码率")
                            // PushFPS 4.5.4: the record window's "smaller files" (half the automatic rate) is said here too.
                            hint: Number(veyra.preferences.recordBitrateKbps || 0) <= 0 && veyra.recordingBitrateMbps <= 0
                                  && Number(veyra.preferences.pushRecordBitrateScale || 100) !== 100
                                ? qsTr("自动码率的一半：录制窗口里选了“小文件”。在这里选“自动”就会用完整的自动码率")
                                : qsTr("自动按画面尺寸和帧率选择；数字为 Mbps，越高越清晰、文件也越大")
                            VSeg {
                                objectName: "set-recbitrate"
                                readonly property int presetKbps: Number(veyra.preferences.recordBitrateKbps || 0)
                                options: {
                                    const base = [{ id: "0", label: qsTr("自动") }, { id: "8", label: "8" }, { id: "16", label: "16" }, { id: "20", label: "20" }, { id: "40", label: "40" }, { id: "80", label: "80" }]
                                    return presetKbps > 0 ? base.concat([{ id: "k", label: String(Math.round(presetKbps / 100) / 10) }]) : base
                                }
                                current: presetKbps > 0 ? "k" : String(veyra.recordingBitrateMbps)
                                onPicked: id => {
                                    if (id === "k") return
                                    veyra.setPreference("recordBitrateKbps", 0)
                                    veyra.setPreference("recordBitrateMbps", Number(id))
                                    // PushFPS 4.5.4: a rate chosen here is the rate recorded with, nothing hidden halves it.
                                    veyra.setPreference("pushRecordBitrateScale", 100)
                                }
                            }
                        }
                        VRow {
                            label: qsTr("录制帧率")
                            hint: qsTr("跟随显示可录到补帧后的画面；限速则按该帧率抽帧")
                            VSeg {
                                objectName: "set-recfps"
                                options: [{ id: "0", label: qsTr("跟随") }, { id: "30", label: "30" }, { id: "60", label: "60" }, { id: "120", label: "120" }]
                                current: String(veyra.recordingFpsLimit)
                                onPicked: id => veyra.setPreference("recordFps", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("录制编码")
                            hint: qsTr("HDR 画面固定使用 HEVC 10bit；宽或高超过 4096 时自动使用 HEVC")
                            VSeg {
                                objectName: "set-rechevc"
                                options: [{ id: "0", label: "H.264" }, { id: "1", label: "HEVC" }]
                                current: veyra.recordingHevc ? "1" : "0"
                                onPicked: id => veyra.setPreference("recordHevc", id === "1")
                            }
                        }
                        // PushFPS advanced recording: encoder controls for people who want them,
                        // like OBS's output settings. Off keeps the standard encoding above.
                        VRow {
                            label: qsTr("高级编码设置")
                            hint: qsTr("码率控制、编码预设、关键帧间隔、音频码率和封装格式，类似 OBS 的输出设置。关闭时使用标准设置")
                            VSwitch {
                                objectName: "set-recadvanced"
                                checked: !!veyra.preferences.recordAdvanced
                                onToggled: v => veyra.setPreference("recordAdvanced", v)
                            }
                        }
                        ColumnLayout {
                            id: recAdvanced
                            objectName: "set-recadvanced-group"
                            Layout.fillWidth: true
                            spacing: 0
                            visible: !!veyra.preferences.recordAdvanced
                            readonly property int rateControl: veyra.preferences.recordRateControl !== undefined ? veyra.preferences.recordRateControl : 1
                            VRow {
                                label: qsTr("码率控制")
                                help: qsTr("码率是视频每秒使用的数据量。码率控制决定编码器怎样分配这些数据：越多数据，画质越好，文件越大")
                                hint: qsTr("VBR：按画面复杂度分配码率（标准）。CBR：每秒码率固定，适合要上传的视频。恒定质量：画质固定，文件大小随画面变化")
                                VSeg {
                                    objectName: "set-recrate"
                                    options: [{ id: "1", label: "VBR" }, { id: "0", label: "CBR" }, { id: "2", label: qsTr("恒定质量") }]
                                    current: String(recAdvanced.rateControl)
                                    onPicked: id => veyra.setPreference("recordRateControl", Number(id))
                                }
                            }
                            VRow {
                                visible: recAdvanced.rateControl === 2
                                label: qsTr("质量等级")
                                hint: qsTr("1–51，越小画质越好、文件越大；标准为 22")
                                value: String(veyra.preferences.recordQuality !== undefined ? veyra.preferences.recordQuality : 22)
                                VSlider {
                                    objectName: "set-recquality"
                                    implicitWidth: 140
                                    valueFromModel: true
                                    resettable: true; defaultValue: 22
                                    from: 1; to: 51
                                    value: veyra.preferences.recordQuality !== undefined ? veyra.preferences.recordQuality : 22
                                    onMoved: value => veyra.setPreference("recordQuality", Math.round(value))
                                }
                            }
                            VRow {
                                label: qsTr("编码预设")
                                help: qsTr("编码器在每一帧上花多少功夫。越慢压缩得越好，同样的文件大小画质更高")
                                hint: qsTr("越靠右画质越好，但显卡编码负担越重；标准为 4。录制卡顿时调低")
                                VSeg {
                                    objectName: "set-recpreset"
                                    options: [{ id: "1", label: "1" }, { id: "2", label: "2" }, { id: "3", label: "3" }, { id: "4", label: "4" },
                                              { id: "5", label: "5" }, { id: "6", label: "6" }, { id: "7", label: "7" }]
                                    current: String(veyra.preferences.recordPreset !== undefined ? veyra.preferences.recordPreset : 4)
                                    onPicked: id => veyra.setPreference("recordPreset", Number(id))
                                }
                            }
                            VRow {
                                label: qsTr("关键帧间隔")
                                help: qsTr("关键帧是一张完整的画面，其余的帧只记录变化。播放器和剪辑软件只能从关键帧开始解码")
                                hint: qsTr("秒；越短越方便剪辑和拖动，文件稍大；标准为 2")
                                VSeg {
                                    objectName: "set-reckeyframe"
                                    options: [{ id: "1", label: "1" }, { id: "2", label: "2" }, { id: "4", label: "4" }, { id: "10", label: "10" }]
                                    current: String(veyra.preferences.recordKeyframe !== undefined ? veyra.preferences.recordKeyframe : 2)
                                    onPicked: id => veyra.setPreference("recordKeyframe", Number(id))
                                }
                            }
                            VRow {
                                label: qsTr("音频码率")
                                hint: qsTr("kbps，每条音轨；标准为 192")
                                VSeg {
                                    objectName: "set-recaudiokbps"
                                    options: [{ id: "128", label: "128" }, { id: "160", label: "160" }, { id: "192", label: "192" }, { id: "256", label: "256" }, { id: "320", label: "320" }]
                                    current: String(veyra.preferences.recordAudioKbps !== undefined ? veyra.preferences.recordAudioKbps : 192)
                                    onPicked: id => veyra.setPreference("recordAudioKbps", Number(id))
                                }
                            }
                            VRow {
                                label: qsTr("封装格式")
                                hint: qsTr("MP4 兼容性最好（标准）。MKV 适合剪辑软件；即时回放始终保存为 MP4")
                                VSeg {
                                    objectName: "set-reccontainer"
                                    options: [{ id: "0", label: "MP4" }, { id: "1", label: "MKV" }]
                                    current: String(veyra.preferences.recordContainer !== undefined ? veyra.preferences.recordContainer : 0)
                                    onPicked: id => veyra.setPreference("recordContainer", Number(id))
                                }
                            }
                            VRow {
                                label: qsTr("二次编码")
                                help: qsTr("编码器先快速分析一遍画面，再决定每一帧用多少码率。同样的码率下，快速运动和复杂画面更清晰")
                                hint: qsTr("仅 NVIDIA（NVENC）。四分之一分辨率的首遍负担小；完整分辨率最好但最重。恒定质量模式下不起作用；标准为关闭")
                                VSeg {
                                    objectName: "set-recmultipass"
                                    options: [{ id: "0", label: qsTr("关闭", "off") }, { id: "1", label: qsTr("四分之一") }, { id: "2", label: qsTr("完整") }]
                                    current: String(veyra.preferences.recordMultipass !== undefined ? veyra.preferences.recordMultipass : 0)
                                    onPicked: id => veyra.setPreference("recordMultipass", Number(id))
                                }
                            }
                            VRow {
                                label: qsTr("心理视觉调优")
                                help: qsTr("自适应量化：把码率从看不出差别的地方挪到眼睛更敏感的地方（平坦的天空、暗部、静止的细节），减少色块")
                                hint: qsTr("仅 NVIDIA（NVENC）：空间自适应量化，显卡支持时加上时间自适应量化；标准为关闭")
                                VSwitch {
                                    objectName: "set-recpsychoaq"
                                    checked: veyra.preferences.recordPsychoAq === true
                                    onToggled: v => veyra.setPreference("recordPsychoAq", v)
                                }
                            }
                            VRow {
                                label: qsTr("恢复标准设置")
                                hint: qsTr("显卡不支持某个组合时，录制会自动改用标准设置并提示")
                                VButton {
                                    objectName: "set-recadvanced-reset"
                                    ghost: true
                                    text: qsTr("恢复")
                                    onClicked: {
                                        veyra.setPreference("recordRateControl", 1); veyra.setPreference("recordQuality", 22)
                                        veyra.setPreference("recordPreset", 4); veyra.setPreference("recordKeyframe", 2)
                                        veyra.setPreference("recordAudioKbps", 192); veyra.setPreference("recordContainer", 0)
                                        veyra.setPreference("recordMultipass", 0); veyra.setPreference("recordPsychoAq", false)
                                    }
                                }
                            }
                        }
                    }
                    VH3 { text: qsTr("声音"); Layout.topMargin: 6 }
                    VGroup {
                        VRow {
                            label: qsTr("录制声音")
                            hint: qsTr("录入所选音频输入，不受音量、静音和本机音频监听开关影响；采集卡需选择音频输入")
                            VSwitch {
                                objectName: "set-recaudio"
                                checked: veyra.recordingAudio
                                onToggled: v => veyra.setPreference("recordAudio", v)
                            }
                        }
                        // PushFPS: the PC's sound (everything but PushFPS, PushAuxAudio.h), for window and
                        // screen captures whose sound plays in another app; the same switch as streaming
                        // mode's desktop audio strip and video playback mode.
                        VRow {
                            label: qsTr("录制电脑声音")
                            hint: veyra.recordingAudio ? qsTr("录入电脑上除 PushFPS 以外的所有声音，例如浏览器或播放器里正在播放的视频声音。捕获窗口或屏幕时需要打开")
                                                       : qsTr("先打开录制声音")
                            VSwitch {
                                objectName: "set-recdesktop"
                                enabled: veyra.recordingAudio
                                opacity: enabled ? 1 : 0.4
                                checked: veyra.preferences.mixDesktopOn === true
                                onToggled: v => veyra.setPreference("mixDesktopOn", v)
                            }
                        }
                        // Microphone (#101): mixed into the sound, or a track of its own.
                        VRow {
                            label: qsTr("录制麦克风")
                            hint: qsTr("同时录入麦克风；默认混进录制声音，也可以放在单独的音轨")
                            VSwitch {
                                objectName: "set-recmic"
                                checked: veyra.recordingMicrophone
                                onToggled: v => { veyra.setPreference("recordMic", v); if (v) veyra.refreshMicrophones() }
                            }
                        }
                        VRow {
                            visible: veyra.recordingMicrophone
                            onVisibleChanged: if (visible) veyra.refreshMicrophones()
                            label: qsTr("麦克风设备")
                            hint: qsTr("需要在 Windows 设置 → 隐私和安全性 → 麦克风 中允许桌面应用访问")
                            VSelect {
                                objectName: "set-recmic-device"
                                implicitWidth: 230
                                options: [{ id: "", label: qsTr("系统默认麦克风") }].concat(veyra.microphoneDevices)
                                value: (options.find(o => o.id === veyra.recordingMicrophoneId)
                                        || { label: qsTr("已选设备（未连接）") }).label
                                onPicked: id => veyra.setPreference("recordMicDevice", id)
                            }
                        }
                        VRow {
                            visible: veyra.recordingMicrophone
                            label: qsTr("麦克风单独音轨")
                            hint: qsTr("开启后麦克风在第 2 条音轨，剪辑时可单独调整；普通播放器通常只播放第 1 条")
                            VSwitch {
                                objectName: "set-recmic-track"
                                checked: veyra.recordingMicrophoneTrack
                                onToggled: v => veyra.setPreference("recordMicTrack", v)
                            }
                        }
                    }
                    VH3 { text: qsTr("对比片段"); Layout.topMargin: 6 }
                    VGroup {
                        // PushFPS comparison clip: the same choices as its dialog (PushCompareDialog.qml).
                        VRow {
                            label: qsTr("对比片段")
                            hint: {
                                const n = CompareClip.layout(veyra.preferences)
                                return n === 3 ? qsTr("整个画面：先是原帧率，一条线从左扫到右露出 PushFPS，停一会儿再扫回去，如此反复")
                                     : n === 2 ? qsTr("竖屏 9:16，适合 TikTok、Shorts 和 Reels：上面是原帧率，下面是 PushFPS，各占半屏（取画面中间部分）")
                                     : n === 1 ? qsTr("完整画面并排两份：左边是原帧率，右边是 PushFPS")
                                     : qsTr("同一个画面从中间分开：左半边是原帧率，右半边是 PushFPS")
                            }
                            RowLayout {
                                spacing: 8
                                VSeg {
                                    objectName: "set-compare-layout"
                                    options: [{ id: "0", label: qsTr("分屏") }, { id: "1", label: qsTr("并排") }, { id: "2", label: qsTr("竖屏") }, { id: "3", label: qsTr("滑动揭示") }]
                                    current: String(CompareClip.layout(veyra.preferences))
                                    onPicked: id => veyra.setPreference("pushCompareLayout", Number(id))
                                }
                                VSeg {
                                    objectName: "set-compare-length"
                                    options: CompareClip.lengths.map(n => ({ id: String(n), label: qsTr("%1 秒").arg(n) }))
                                    current: String(CompareClip.seconds(veyra.preferences))
                                    enabled: !CompareClip.untilStop(veyra.preferences)
                                    opacity: enabled ? 1 : 0.4
                                    onPicked: id => veyra.setPreference("pushCompareSeconds", Number(id))
                                }
                            }
                        }
                        VRow {
                            label: qsTr("录到我停止为止")
                            hint: qsTr("对比片段不设时长：再点一次“对比片段”或按录制快捷键停止")
                            VSwitch {
                                objectName: "set-compare-until-stop"
                                checked: CompareClip.untilStop(veyra.preferences)
                                onToggled: v => veyra.setPreference("pushCompareUntilStop", v)
                            }
                        }
                        // Split and Reveal clips fill the frame: the height they are recorded at.
                        VRow {
                            objectName: "set-compare-quality-row"
                            visible: CompareClip.takesQuality(CompareClip.layout(veyra.preferences))
                            label: qsTr("片段尺寸")
                            hint: {
                                const q = CompareClip.quality(veyra.preferences)
                                return q === 2160 ? qsTr("最高按 4K 录制，每一帧都录：需要较强的显卡。若有帧没录下，录完会提示，请选低一档")
                                     : q > 0 ? qsTr("最高按所选的高度录制（画面本身更小时保持原样，不放大），并用更高的码率")
                                     : qsTr("按画面原本的大小录制，最高 1440p")
                            }
                            VSeg {
                                objectName: "set-compare-quality"
                                options: CompareClip.qualities.map(n => ({ id: String(n), label: n > 0 ? CompareClip.qualityName(n) : qsTr("自动") }))
                                current: String(CompareClip.quality(veyra.preferences))
                                onPicked: id => veyra.setPreference("pushCompareQuality", Number(id))
                            }
                        }
                        // Every layout: how finely a clip is encoded.
                        VRow {
                            objectName: "set-compare-picture-row"
                            label: qsTr("画质")
                            hint: {
                                const p = CompareClip.picture(veyra.preferences)
                                return p === 2 ? qsTr("几乎看不出和原画的差别。文件很大（4K 每分钟可达数 GB），建议录到固态硬盘")
                                     : p === 1 ? qsTr("固定画质编码：细节清楚、画面干净，文件大小随画面内容变化")
                                     : qsTr("标准码率：文件较小，适合直接分享")
                            }
                            VSeg {
                                objectName: "set-compare-picture"
                                options: CompareClip.pictures.map(n => ({ id: String(n), label: n === 2 ? qsTr("近无损") : n === 1 ? qsTr("高") : qsTr("标准") }))
                                current: String(CompareClip.picture(veyra.preferences))
                                onPicked: id => veyra.setPreference("pushComparePicture", Number(id))
                            }
                        }
                        // Reveal: how often the line sweeps.
                        VRow {
                            objectName: "set-compare-reveal-row"
                            visible: CompareClip.layout(veyra.preferences) === 3
                            label: qsTr("扫过间隔")
                            hint: qsTr("线条每隔这么久扫过一次，在原帧率和 PushFPS 之间切换")
                            VSeg {
                                objectName: "set-compare-reveal"
                                options: CompareClip.revealIntervals.map(n => ({ id: String(n), label: qsTr("%1 秒").arg(n) }))
                                current: String(CompareClip.revealEvery(veyra.preferences))
                                onPicked: id => veyra.setPreference("pushCompareRevealSeconds", Number(id))
                            }
                        }
                    }
                    VH3 { text: qsTr("即时回放与分享"); Layout.topMargin: 6 }
                    // Instant replay (#103): the last few seconds or minutes in memory.
                    VGroup {
                        objectName: "set-replay-group"
                        VRow {
                            label: qsTr("即时回放")
                            hint: qsTr("在内存中保留最近一段画面和声音，按 %1 保存到录制保存位置；会多占用一路硬件编码").arg(veyra.shortcuts.replay || qsTr("快捷键"))
                            VSwitch {
                                objectName: "set-replay"
                                checked: veyra.replayEnabled
                                onToggled: v => veyra.setPreference("replayEnabled", v)
                            }
                        }
                        VRow {
                            visible: veyra.replayEnabled
                            label: qsTr("回放长度")
                            hint: {
                                const s = veyra.replayState || {}
                                // Live: what the buffer holds now and what the chosen length takes when full.
                                if (!s.active || !(s.seconds > 0))
                                    return s.fullMegabytes > 0 ? qsTr("存满后约占用 %1 MB 内存（上限 %2 MB），保存的片段在磁盘上大小相近；播放时开始缓存").arg(s.fullMegabytes).arg(s.capMegabytes)
                                                               : qsTr("播放时开始缓存；录制码率越高，占用内存越多")
                                if (s.memoryLimited) return qsTr("内存上限只够保留 %1 秒（约 %2 MB），可降低录制码率").arg(s.seconds).arg(s.megabytes)
                                return s.fullMegabytes > 0 ? qsTr("已缓存 %1 秒，占用内存约 %2 MB；存满后约 %3 MB，保存的片段在磁盘上大小相近").arg(s.seconds).arg(s.megabytes).arg(s.fullMegabytes)
                                                           : qsTr("已缓存 %1 秒，占用内存约 %2 MB").arg(s.seconds).arg(s.megabytes)
                            }
                            VSeg {
                                objectName: "set-replay-seconds"
                                options: [{ id: "15", label: qsTr("15 秒") }, { id: "30", label: qsTr("30 秒") }, { id: "60", label: qsTr("1 分钟") },
                                          { id: "120", label: qsTr("2 分钟") }, { id: "300", label: qsTr("5 分钟") }, { id: "600", label: qsTr("10 分钟") }]
                                current: String(veyra.replaySeconds)
                                onPicked: id => veyra.setPreference("replaySeconds", Number(id))
                            }
                        }
                        // PushFPS: where "Send to Discord" on a saved clip goes.
                        VRow {
                            objectName: "set-discord-webhook-row"
                            label: qsTr("Discord Webhook")
                            hint: String(veyra.preferences.pushDiscordWebhook || "").length > 0
                                  ? qsTr("已设置：保存的片段可一键发送到这个频道（10 MB 以内）")
                                  : qsTr("可选。在 Discord 频道 → 编辑频道 → 整合 → Webhook 中复制地址；不填时“分享到 Discord”会复制文件并打开 Discord")
                            hintBelow: true
                            DialogTextField {
                                objectName: "set-discord-webhook"
                                implicitWidth: 240
                                placeholder: "https://discord.com/api/webhooks/…"
                                text: String(veyra.preferences.pushDiscordWebhook || "")
                                onEdited: t => veyra.setPreference("pushDiscordWebhook", t)
                            }
                        }
                    }
                    VH3 { text: qsTr("虚拟摄像机"); Layout.topMargin: 6 }
                    VGroup {
                        objectName: "set-virtual-camera-group"
                        VRow {
                            label: qsTr("直播软件兼容组件")
                            hint: {
                                const c = veyra.virtualCameraState || {}
                                if(c.installing) return qsTr("正在安装或卸载，请完成 Windows 管理员确认")
                                if(c.setupError) return c.setupError
                                if(c.systemOutdated) return qsTr("系统级组件是旧版本，管理员运行的 OBS 等直播软件会看不到画面；请点击“更新组件”")
                                return c.systemInstalled ? qsTr("已安装系统级组件，支持管理员运行的直播软件")
                                                         : qsTr("当前仅注册到本用户；管理员运行的 OBS 等直播软件需要安装系统级组件")
                            }
                            VButton {
                                objectName: "set-virtual-camera-install"
                                primary: !!(veyra.virtualCameraState || {}).systemOutdated
                                text: (veyra.virtualCameraState || {}).systemOutdated ? qsTr("更新组件")
                                    : (veyra.virtualCameraState || {}).systemInstalled ? qsTr("修复组件") : qsTr("安装组件")
                                enabled: !(veyra.virtualCameraState || {}).installing
                                onClicked: veyra.installVirtualCamera()
                            }
                        }
                        VRow {
                            label: qsTr("虚拟摄像机")
                            hint: qsTr("在直播软件的视频采集设备中选择 PushFPS Virtual Camera；输出处理后的完整画面，不需要全屏")
                            VSwitch {
                                objectName: "set-virtual-camera-toggle"
                                checked: veyra.virtualCameraEnabled
                                onToggled: enabled => veyra.setVirtualCameraEnabled(enabled)
                            }
                        }
                        VRow {
                            label: qsTr("摄像机输出帧率")
                            hint: qsTr("自动：跟随直播软件为摄像机选的帧率（如 OBS 设 60 或 120），补帧 3X、4X 时按均匀时间挑帧；固定值为上限。SDR 视频输出，声音请单独采集；XeSS 显示端生成的帧不包含在内")
                            VSeg {
                                objectName: "set-virtual-camera-fps"
                                options: [{id: "0", label: qsTr("自动")}, {id: "30", label: "30"}, {id: "60", label: "60"}, {id: "120", label: "120"}]
                                current: String(veyra.virtualCameraFps)
                                onPicked: id => veyra.setPreference("virtualCameraFps", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("摄像机状态")
                            hint: {
                                const c = veyra.virtualCameraState || {}
                                if (c.error) return c.error
                                if (!c.active) return qsTr("已关闭；每次启动软件后需手动启用")
                                if (!(c.width > 0)) return qsTr("已启用，等待处理后的画面")
                                const size = qsTr("%1 × %2 · 已输出 %3 帧 · 跳过 %4 帧").arg(c.width).arg(c.height).arg(c.frames).arg(c.dropped)
                                           + (c.consumerFps > 0 ? qsTr(" · 直播软件请求 %1 帧/秒").arg(c.consumerFps) : "")
                                return c.processingWidth !== c.width || c.processingHeight !== c.height
                                     ? size + qsTr("（处理尺寸 %1 × %2）").arg(c.processingWidth).arg(c.processingHeight) : size
                            }
                            VButton { objectName: "set-virtual-camera-uninstall"; text: qsTr("卸载摄像机"); ghost: true; enabled: !(veyra.virtualCameraState || {}).installing; onClicked: veyra.uninstallVirtualCamera() }
                        }
                        // The consumer takes evenly spaced pictures: from a rate that is not a whole
                        // multiple of its own (90 against 60: 3X on a 30 fps game) they cannot be even.
                        VRow {
                            objectName: "set-virtual-camera-rhythm"
                            readonly property var cam: veyra.virtualCameraState || {}
                            readonly property real ratio: cam.presentedFps > 0 && cam.consumerFps > 0
                                ? Math.max(cam.presentedFps, cam.consumerFps) / Math.min(cam.presentedFps, cam.consumerFps) : 1
                            visible: !!cam.active && Math.abs(ratio - Math.round(ratio)) > 0.1
                            label: qsTr("直播画面节奏")
                            hintColor: Theme.warn
                            hint: qsTr("画面 %1 帧/秒不是直播软件 %2 帧/秒的整数倍，直播画面会一快一慢交替。请换成整数倍的补帧倍数，例如 30 帧游戏直播 60 帧时用 2X 或 4X")
                                  .arg(cam.presentedFps).arg(cam.consumerFps)
                        }
                    }
                    // PushFPS local control (ui/PushControl.h): a button box (Stream Deck) or a script on this PC
                    // starts and stops a recording, saves the replay, marks a moment, switches a scene. Off unless
                    // switched on; this PC only; a token for every request, which is copied here and never shown.
                    VH3 { text: qsTr("本机控制"); Layout.topMargin: 6 }
                    VGroup {
                        id: controlGroup
                        objectName: "set-control-group"
                        // (Not named "state": every item already has a property of that name.)
                        property var info: veyra.pushControlState ? veyra.pushControlState() : ({})
                        property bool copied: false
                        function refresh() { controlGroup.info = veyra.pushControlState ? veyra.pushControlState() : ({}) }
                        Connections {
                            target: veyra
                            ignoreUnknownSignals: true
                            function onPushControlChanged() { controlGroup.refresh() }
                        }
                        VRow {
                            objectName: "set-control-row"
                            label: qsTr("允许按钮盒和脚本控制")
                            help: qsTr("开启后，Stream Deck、脚本或直播软件可以开始和停止录制、保存即时回放、标记这一刻、截图、切换场景，并读取是否正在播放和录制。只在这台电脑上监听（127.0.0.1），每个请求都要带令牌；网页发来的请求一律拒绝。它不能读取文件、改设置，也接触不到串流密钥。")
                            hint: controlGroup.info.enabled !== true ? qsTr("已关闭：没有打开任何端口")
                                : controlGroup.info.listening === true ? qsTr("正在监听 127.0.0.1:%1（仅本机）").arg(controlGroup.info.port)
                                : qsTr("端口 %1 无法使用，可能被别的程序占用").arg(controlGroup.info.port)
                            hintColor: controlGroup.info.enabled === true && controlGroup.info.listening !== true ? Theme.warn : Theme.t3
                            VSwitch {
                                objectName: "set-control"
                                checked: controlGroup.info.enabled === true
                                onToggled: checked => { veyra.pushSetControlEnabled(checked); controlGroup.copied = false; controlGroup.refresh() }
                            }
                        }
                        VRow {
                            objectName: "set-control-token-row"
                            visible: controlGroup.info.enabled === true
                            label: qsTr("令牌")
                            hint: controlGroup.copied ? qsTr("已复制。把它当密码保管：有它就能控制录制")
                                                      : qsTr("每个请求都要带上它。只能复制，不会显示在屏幕上")
                            RowLayout {
                                spacing: 8
                                VButton {
                                    objectName: "set-control-token-new"
                                    ghost: true
                                    text: qsTr("换一个")
                                    tip: qsTr("生成新令牌；用旧令牌的设备会立刻失效")
                                    onClicked: { veyra.pushNewControlToken(); controlGroup.copied = false }
                                }
                                VButton {
                                    objectName: "set-control-token-copy"
                                    text: qsTr("复制令牌")
                                    onClicked: controlGroup.copied = veyra.pushCopyControlToken() === true
                                }
                            }
                        }
                        VRow {
                            objectName: "set-control-port-row"
                            visible: controlGroup.info.enabled === true
                            label: qsTr("端口")
                            hint: qsTr("默认 18766。被占用时换一个（1024 到 65535）")
                            VSeg {
                                objectName: "set-control-port"
                                options: [{ id: "18766", label: "18766" }, { id: "28766", label: "28766" }, { id: "38766", label: "38766" }]
                                current: String(controlGroup.info.port || 18766)
                                onPicked: id => { veyra.setPreference("pushControlPort", Number(id)); controlGroup.refresh() }
                            }
                        }
                        VRow {
                            objectName: "set-control-docs-row"
                            label: qsTr("怎么用")
                            hint: qsTr("每个请求的写法，以及 PowerShell、curl 和 Stream Deck 的例子")
                            VButton { objectName: "set-control-docs"; ghost: true; text: qsTr("打开说明"); trailingIcon: "external"; onClicked: veyra.pushOpenLink("controlDocs") }
                        }
                    }
                }

                // --- 音频 ---
                ColumnLayout {
                    id: secAudio
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "audio"
                    VH1 { text: qsTr("音频") }
                    Text {
                        text: qsTr("声音同步、音量与输出设备。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VGroup {
                        VRow {
                            label: qsTr("声音同步")
                            hint: veyra.audioSyncMode === 0 ? (veyra.audioSyncLive ? qsTr("软件估算补偿 ") + veyra.audioCompensationMs.toFixed(0) + " ms" : qsTr("自动估算 · 采集卡 / PS5 实时输入时生效"))
                                  : veyra.audioSyncMode === 1 ? qsTr("手动偏移 · 实时输入时生效") : qsTr("关闭补偿")
                            VSeg {
                                options: [{ id: "0", label: qsTr("自动估算") }, { id: "1", label: qsTr("手动") }, { id: "2", label: qsTr("关闭", "off") }]
                                current: String(veyra.audioSyncMode)
                                onPicked: id => veyra.audioSyncMode = Number(id)
                            }
                        }
                        VRow {
                            label: qsTr("音频偏移")
                            hint: qsTr("正值延后音频")
                            value: veyra.audioOffsetMs + " ms"
                            VSlider {
                                implicitWidth: 150
                                center: true
                                from: -250; to: 250; value: veyra.audioOffsetMs; enabledControl: veyra.audioSyncMode === 1
                                onMoved: veyra.audioOffsetMs = Math.round(value)
                            }
                        }
                        VRow {
                            label: qsTr("音量")
                            value: Math.round(veyra.volume * 100) + "%"
                            VSlider {
                                objectName: "settings-volume"
                                implicitWidth: 150
                                from: 0; to: veyra.maximumVolume; value: veyra.volume; inputScale: 100
                                valueFromModel: true; defaultValue: 1
                                onMoved: veyra.volume = value
                            }
                        }
                    }
                    VGroup {
                        VRow {
                            label: qsTr("输出设备与下混")
                            hint: veyra.audioOutputStatus
                            VButton { text: qsTr("音频设置…"); onClicked: veyra.openAudioDialog() }
                        }
                    }
                }

                // --- 性能 ---
                ColumnLayout {
                    id: secPerf
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "perf"
                    VH1 { text: qsTr("性能") }
                    Text {
                        text: qsTr("显卡调度、预热与资源监控。默认设置适合大多数机器。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VGroup {
                        VRow {
                            label: qsTr("GPU 优先级")
                            hint: veyra.gpuPriorityStatus + qsTr("；默认“高”，同时运行游戏或其他重负载程序变慢时可改为“普通”")
                            VSeg {
                                objectName: "set-gpu-priority"
                                options: [{ id: "normal", label: qsTr("普通") }, { id: "high", label: qsTr("高") }, { id: "realtime", label: qsTr("实时") }]
                                current: veyra.preferences.gpuPriority || "high"
                                onPicked: id => veyra.setPreference("gpuPriority", id)
                            }
                        }
                        VRow {
                            label: qsTr("启动时预热增强组件")
                            hint: !veyra.enhancementPrewarmAvailable
                                  ? qsTr("当前显卡不支持预热")
                                  : qsTr("默认关闭。开启后首页空闲时预先准备已启用的单层 NR 或超分，首次打开约快 2 秒；准备好后会一直占用约 0.8–1.8 GB 显存，直到打开视频")
                            VSwitch {
                                objectName: "set-prewarm-enhancement"
                                enabled: veyra.enhancementPrewarmAvailable
                                checked: veyra.preferences.prewarmEnhancement === true
                                onToggled: v => veyra.setPreference("prewarmEnhancement", v)
                            }
                        }
                        VRow {
                            label: qsTr("GPU 占用监控")
                            hint: qsTr("专业页「GPU 占用」只统计这一块显卡") + (veyra.gpuMonitorName ? qsTr("；当前：") + veyra.gpuMonitorName : "")
                            VSelect {
                                objectName: "set-monitor-gpu"
                                implicitWidth: 230
                                options: veyra.gpuMonitorChoices
                                value: (options.find(o => o.id === (veyra.preferences.monitorGpu || "")) || options[0] || { label: "—" }).label
                                onPicked: id => veyra.setPreference("monitorGpu", id)
                            }
                        }
                        VRow {
                            label: qsTr("同时用这块显卡运算")
                            hint: veyra.computeGpuHint
                            VSwitch {
                                objectName: "set-compute-gpu"
                                checked: veyra.preferences.computeOnMonitorGpu === true
                                onToggled: v => veyra.setPreference("computeOnMonitorGpu", v)
                            }
                        }
                        VRow {
                            label: qsTr("全屏显存增长保护")
                            hint: qsTr("检测异常增长时自动退出全屏；默认关闭")
                            VSwitch {
                                objectName: "set-fullscreen-memory-protection"
                                checked: !!veyra.preferences.fullscreenMemoryProtection
                                onToggled: v => veyra.setPreference("fullscreenMemoryProtection", v)
                            }
                        }
                    }
                }

                // --- 兼容 ---
                ColumnLayout {
                    id: secCompat
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "compat"
                    VH1 { text: qsTr("兼容") }
                    Text {
                        text: qsTr("界面绘制、OBS、监控软件兼容需重启；HDR 补帧兼容可在播放中切换。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                    VGroup {
                        VRow {
                            // 界面绘制 (veyra/ui/InterfaceRenderer.h, read by main.cpp at start).
                            // Software by default: no swapchain or D3D12 device of its own for
                            // overlays to hook, and no video memory. OBS 游戏采集兼容 and
                            // 监控软件兼容 still draw in software when GPU is chosen.
                            readonly property bool gpuChosen: veyra.preferences.uiRenderer === "gpu"
                            label: qsTr("界面绘制")
                            hint: gpuChosen
                                  ? (!veyra.softwareUiActive
                                     ? qsTr("显卡绘制：动画更顺，带阴影与模糊；占用显存和 GPU，可能与小飞机等监控软件冲突")
                                     : veyra.obsGameCaptureActive
                                       ? qsTr("OBS 游戏采集兼容开启时，界面始终软件绘制")
                                       : veyra.overlayCompatActive
                                         ? qsTr("本次检测到监控软件，仍用软件绘制（监控软件兼容：自动）")
                                         : qsTr("重启后生效"))
                                  : (veyra.softwareUiActive
                                     ? qsTr("软件绘制：兼容性最好，不占显存，不和视频抢 GPU；部分阴影与模糊简化")
                                     : qsTr("重启后生效"))
                            VSeg {
                                objectName: "set-ui-renderer"
                                options: [{ id: "software", label: qsTr("软件") }, { id: "gpu", label: "GPU" }]
                                current: veyra.preferences.uiRenderer === "gpu" ? "gpu" : "software"
                                onPicked: id => {
                                    // A restart changes the renderer unless a compatibility rule
                                    // keeps the interface in software anyway.
                                    const changes = id === "gpu"
                                        ? veyra.softwareUiActive && !veyra.obsGameCaptureActive
                                          && !((veyra.preferences.overlayCompat || "auto") === "auto" && veyra.presentationOverlayRunning())
                                        : !veyra.softwareUiActive
                                    if (veyra.setPreference("uiRenderer", id) && changes)
                                        obsRestart.open()
                                }
                            }
                        }
                        VRow {
                            label: qsTr("HDR 补帧闪黑兼容")
                            hint: qsTr("默认关闭。仅 NVIDIA HDR10 补帧时生效；4K 额外约 32 MiB 显存，可能增加 GPU 开销。切换时画面可能短暂停顿。")
                            VSwitch {
                                objectName: "set-hdr-fg-compatibility"
                                checked: !!veyra.preferences.hdrFgCompatibility
                                onToggled: v => veyra.setPreference("hdrFgCompatibility", v)
                            }
                        }
                        VRow {
                            label: qsTr("OBS 游戏采集兼容")
                            hint: (!!veyra.preferences.obsGameCapture !== veyra.obsGameCaptureActive)
                                  ? qsTr("重启后生效")
                                  : qsTr("仅采集视频；界面软件绘制，部分阴影与模糊简化")
                            VSwitch {
                                objectName: "set-obs-game-capture"
                                checked: !!veyra.preferences.obsGameCapture
                                onToggled: v => {
                                    if (veyra.setPreference("obsGameCapture", v)
                                            && v !== veyra.obsGameCaptureActive)
                                        obsRestart.open()
                                }
                            }
                        }
                        VRow {
                            // RivaTuner (MSI Afterburner's OSD), NVIDIA's in-game overlay and GamePP: see
                            // main.cpp. Auto draws the interface in software when one runs at start, so
                            // they only hook the video. With 界面绘制 = 软件 (the default) the interface
                            // is drawn in software anyway, and this choice changes nothing.
                            id: overlayCompatRow
                            readonly property bool automatic: (veyra.preferences.overlayCompat || "auto") === "auto"
                            readonly property bool gpuChosen: veyra.preferences.uiRenderer === "gpu"
                            label: qsTr("监控软件兼容")
                            hint: veyra.obsGameCaptureActive
                                  ? qsTr("OBS 游戏采集兼容已让界面软件绘制，小飞机 OSD 只显示在视频上")
                                  : !gpuChosen
                                  ? qsTr("只在“界面绘制”选 GPU 时起作用；软件绘制时小飞机 OSD 只显示在视频上")
                                  : (automatic !== veyra.overlayCompatActive && (veyra.overlayCompatActive || veyra.presentationOverlayRunning()))
                                    ? qsTr("重启后生效")
                                    : veyra.overlayCompatActive
                                      ? qsTr("监控兼容绘制已启用：OSD 只显示在视频上；部分阴影与模糊简化")
                                      : automatic
                                        ? qsTr("启动时检测到小飞机（RTSS）、NVIDIA 信息浮窗或游戏加加就改用软件绘制界面，它们只显示在视频上")
                                        : qsTr("界面始终用显卡绘制；小飞机 OSD 不在 PushFPS 里显示；开着 NVIDIA 信息浮窗或游戏加加时可能卡死")
                            VSeg {
                                objectName: "set-overlay-compat"
                                options: [{ id: "auto", label: qsTr("自动") }, { id: "off", label: qsTr("关闭") }]
                                current: veyra.preferences.overlayCompat || "auto"
                                onPicked: id => {
                                    if (veyra.setPreference("overlayCompat", id) && !veyra.obsGameCaptureActive
                                            && overlayCompatRow.gpuChosen
                                            && (id === "auto") !== veyra.overlayCompatActive
                                            && (veyra.overlayCompatActive || veyra.presentationOverlayRunning()))
                                        obsRestart.open()
                                }
                            }
                        }
                    }
                }

                // --- PS5 串流 --------------------------------------------
                ColumnLayout {
                    id: secPs5
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "ps5"
                    VH1 { text: qsTr("PS5 串流") }
                    Text {
                        text: qsTr("主机配对凭据加密保存在用户数据目录。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                    }
                    VGroup {
                        VRow {
                            label: qsTr("已保存主机")
                            hint: (veyra.ps5.host || "").length > 0 ? veyra.ps5.host + (veyra.remotePlayState.length > 0 ? " · " + veyra.remotePlayState : "") : qsTr("还没有配对的主机")
                            VButton { text: qsTr("管理"); onClicked: veyra.openPs5Dialog() }
                        }
                        VRow {
                            label: qsTr("画质")
                            VSelect {
                                objectName: "set-ps5-quality"
                                implicitWidth: 170
                                readonly property var qualities: [{ id: "0", label: "720p · 30 fps" }, { id: "1", label: "720p · 60 fps" }, { id: "2", label: "1080p · 30 fps" }, { id: "3", label: "1080p · 60 fps" }]
                                options: qualities
                                value: qualities[veyra.ps5.quality || 0].label
                                onPicked: id => veyra.ps5Set("quality", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("编码")
                            hint: qsTr("HDR 需要 PS5 实际输出 HDR 且 Windows HDR 开启")
                            VSeg {
                                objectName: "set-ps5-codec"
                                options: [{ id: "0", label: "H.264" }, { id: "1", label: "H.265" }, { id: "2", label: "H.265 HDR" }]
                                current: String(veyra.ps5.codec || 0)
                                onPicked: id => veyra.ps5Set("codec", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("请求码率")
                            hint: qsTr("发给 PS5 的带宽请求，主机不保证达到。有线网络下 30 Mbps 或更高会更清晰；画面卡顿就调低")
                            VSelect {
                                objectName: "set-ps5-bitrate"
                                implicitWidth: 130
                                readonly property var bitrates: [5, 10, 15, 20, 30, 50, 80, 100].map((m, i) => ({ id: String(i), label: m + " Mbps" }))
                                options: bitrates
                                value: bitrates[veyra.ps5.bitrate || 0].label
                                onPicked: id => veyra.ps5Set("bitrate", Number(id))
                            }
                        }
                        VRow {
                            label: qsTr("PSN 账号")
                            hint: qsTr("“获取 Account ID”会打开 PS5 串流窗口，并在索尼自己的页面登录后替你填入 Account ID。")
                            hintBelow: true
                            RowLayout {
                                spacing: 6
                                VButton { objectName: "set-ps5-account-lookup"; text: qsTr("获取 Account ID"); ghost: true; onClicked: { veyra.openPs5Dialog(); veyra.ps5OpenAccountChecker() } }
                                VButton { objectName: "set-ps5-pairing"; text: qsTr("配对设置"); ghost: true; onClicked: veyra.openPs5Dialog() }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: psNote.implicitHeight + 20
                        radius: 9
                        color: Qt.rgba(0.961, 0.784, 0.294, 0.06)
                        Text {
                            id: psNote
                            anchors.fill: parent
                            anchors.margins: 10
                            text: qsTr("这里和 PS5 串流窗口是同一份设置，下次连接时一起保存到该主机的配对档案。")
                            color: Theme.t2
                            font.family: Theme.fontUi
                            font.pixelSize: Theme.fsSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // --- 快捷键 ----------------------------------------------
                ColumnLayout {
                    id: secKeys
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "keys"
                    VH1 { text: qsTr("快捷键") }
                    Text {
                        text: qsTr("点击右侧按键，再按下新的组合键即可重新绑定；Esc 取消。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                    }
                    VGroup {
                        Repeater {
                            model: [
                                { id: "playPause", a: qsTr("播放 / 暂停") },
                                { id: "fullscreen", a: qsTr("全屏"), note: qsTr("F11 / Alt+Enter 也可以") },
                                { id: "lock", a: qsTr("锁定全屏控制条") },
                                { id: "hold", a: qsTr("按住查看原画") },
                                { id: "screenshot", a: qsTr("截图"), note: qsTr("游戏模式下在游戏里也能用") },
                                { id: "record", a: qsTr("开始 / 停止录制"), note: qsTr("游戏模式下在游戏里也能用") },
                                { id: "replay", a: qsTr("保存即时回放"), note: qsTr("需先在 设置 → 录制 开启即时回放；游戏模式下在游戏里也能用") },
                                { id: "marker", a: qsTr("录制时标记这一刻"), note: qsTr("时间记在录像旁边的文本文件里，剪辑时好找") },
                                { id: "readText", a: qsTr("读取画面上的文字"), note: qsTr("打开后立即读取；窗口开着时再按一次，读现在的画面") },
                                { id: "latencyOverlay", a: qsTr("显示 / 隐藏延迟读数"), note: qsTr("全屏时也能用，不用退出全屏") },
                                { id: "compareClip", a: qsTr("开始 / 停止对比片段"), note: qsTr("按 设置 → 录制 里对比片段的布局和长度录制；再按一次停止") },
                                { id: "fgToggle", a: qsTr("开启 / 关闭补帧") },
                                { id: "enhanceToggle", a: qsTr("开启 / 关闭 AI 画面增强") },
                                { id: "toggleMode", a: qsTr("切换极简 / 专业") },
                                { id: "gamePanel", a: qsTr("游戏模式：调整画面"), note: qsTr("仅在游戏模式中接管按键；支持 Ctrl、Alt、Shift 组合") },
                                { id: "gameExit", a: qsTr("游戏模式：退出鼠标穿透"), note: qsTr("退出全屏，恢复普通窗口操作") }
                            ]
                            delegate: VRow {
                                id: keyRow
                                required property var modelData
                                label: modelData.a
                                hint: modelData.note || ""
                                // PushFPS (4.5.3): a key can be taken off an action (the game mode keys always keep one).
                                readonly property bool clearable: keyRow.modelData.id !== "gamePanel" && keyRow.modelData.id !== "gameExit"
                                RowLayout {
                                    spacing: 6
                                    VButton {
                                        objectName: "key-clear-" + keyRow.modelData.id
                                        visible: keyRow.clearable && !!veyra.shortcuts[keyRow.modelData.id] && root.capturingKey !== keyRow.modelData.id
                                        ghost: true
                                        text: qsTr("清除")
                                        onClicked: veyra.clearShortcut(keyRow.modelData.id)
                                    }
                                    Rectangle {
                                        objectName: "key-" + keyRow.modelData.id
                                        readonly property bool capturing: root.capturingKey === keyRow.modelData.id
                                        implicitWidth: Math.max(64, keyText.implicitWidth + 20)
                                        implicitHeight: 26
                                        radius: 7
                                        color: capturing ? Theme.accentSoft : Theme.ink(0.06)
                                        border.width: 1
                                        border.color: capturing ? Theme.accent : Theme.stroke
                                        Text {
                                            id: keyText
                                            anchors.centerIn: parent
                                            text: parent.capturing ? qsTr("按下新按键…") : (veyra.shortcuts[keyRow.modelData.id] || qsTr("未设置"))
                                            color: Theme.t1
                                            font.family: Theme.fontMono
                                            font.pixelSize: Theme.fsSmall
                                        }
                                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                                        TapHandler { onTapped: { root.capturingKey = keyRow.modelData.id; keyCatcher.forceActiveFocus() } }
                                    }
                                }
                            }
                        }
                        VRow {
                            label: qsTr("恢复默认")
                            VButton { text: qsTr("全部还原"); ghost: true; onClicked: veyra.resetShortcuts() }
                        }
                    }
                    // PushFPS: the keyboard and mouse as a controller for the console (PushKbmDialog.qml).
                    VGroup {
                        VRow {
                            objectName: "set-kbm"
                            label: qsTr("鼠标和键盘")
                            hint: qsTr("用鼠标和键盘玩主机游戏：GTA 6、GTA V 和 Red Dead Redemption 2 的键位，每个键都可以改")
                            VButton { objectName: "set-kbm-open"; text: qsTr("按键设置"); onClicked: veyra.navigate("kbm") }
                        }
                    }
                    VGroup {
                        Repeater {
                            model: [
                                { a: qsTr("后退 / 前进 5 秒"), k: "← / →" },
                                { a: qsTr("播放 / 暂停（播放文件时）"), k: qsTr("单击画面") },
                                { a: qsTr("音量"), k: "↑ / ↓" },
                                { a: qsTr("打开文件 / 导出页"), k: "Ctrl+O / Ctrl+E" },
                                { a: qsTr("字幕开关 / 主轨 / 副轨"), k: "B / T / Y" },
                                { a: qsTr("字幕延时 50 ms（Shift = 1 s）"), k: "Z / X" },
                            ]
                            delegate: VRow {
                                required property var modelData
                                label: modelData.a
                                hint: qsTr("固定")
                                VTag { text: modelData.k }
                            }
                        }
                    }
                    // Captures the next key combination for the row being rebound.
                    Item {
                        id: keyCatcher
                        focus: false
                        Keys.onPressed: event => {
                            if (root.capturingKey === "") return
                            event.accepted = true
                            if (event.key === Qt.Key_Escape) { root.capturingKey = ""; return }
                            const modifierOnly = [Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt, Qt.Key_Meta].indexOf(event.key) >= 0
                            if (modifierOnly) return
                            const parts = []
                            if (event.modifiers & Qt.MetaModifier) parts.push("Meta")
                            if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
                            if (event.modifiers & Qt.AltModifier) parts.push("Alt")
                            if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
                            const names = { [Qt.Key_Space]: "Space", [Qt.Key_Tab]: "Tab", [Qt.Key_Return]: "Return", [Qt.Key_Enter]: "Enter",
                                            [Qt.Key_Backspace]: "Backspace", [Qt.Key_Delete]: "Del", [Qt.Key_Home]: "Home", [Qt.Key_End]: "End",
                                            [Qt.Key_PageUp]: "PgUp", [Qt.Key_PageDown]: "PgDown", [Qt.Key_Insert]: "Ins" }
                            let name = names[event.key]
                            if (!name && event.key >= Qt.Key_F1 && event.key <= Qt.Key_F24) name = "F" + (event.key - Qt.Key_F1 + 1)
                            if (!name && event.text && event.text.trim().length === 1) name = event.text.toUpperCase()
                            if (!name && event.key >= 0x20 && event.key < 0x7F) name = String.fromCharCode(event.key)
                            if (!name) return
                            parts.push(name)
                            if (veyra.setShortcut(root.capturingKey, parts.join("+"))) root.capturingKey = ""
                        }
                    }
                }

                // --- 组件与许可 ------------------------------------------
                ColumnLayout {
                    id: secComp
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "comp"
                    onVisibleChanged: if (visible) { veyra.refreshComponentList(); root.anrState = veyra.pushAnrState() }
                    VH1 { text: qsTr("组件与许可") }
                    // PushFPS: Apex Neural Rendering. ANR may not be redistributed, so PushFPS links to
                    // Apex's releases and installs the package the user downloaded on this PC.
                    VGroup {
                        objectName: "set-anr"
                        VRow {
                            label: "Apex Neural Rendering (ANR)"
                            hint: root.anrState.installed
                                  ? qsTr("已安装。ANR 补帧由 Apex Compute 制作，可在专业模式的补帧方式中选择")
                                  : qsTr("ANR 补帧由 Apex Compute 制作，支持 GTX 16 系列及更新的显卡。它不能随 PushFPS 一起分发：从 Apex 的 GitHub 下载 anr-fg 压缩包，再在这里选择它")
                            VTag {
                                objectName: "set-anr-status"
                                text: root.anrState.installed ? qsTr("已安装") : qsTr("未安装")
                                kind: root.anrState.installed ? "ok" : ""
                            }
                        }
                        VRow {
                            label: qsTr("获取与安装")
                            hint: String(root.anrState.folder || "")
                            RowLayout {
                                spacing: 6
                                VButton {
                                    objectName: "set-anr-download"
                                    text: qsTr("下载 ANR")
                                    iconName: "external"
                                    onClicked: veyra.pushOpenLink("anrReleases")
                                }
                                VButton {
                                    objectName: "set-anr-install"
                                    primary: !root.anrState.installed
                                    text: root.anrState.installed ? qsTr("更新…") : qsTr("从下载安装…")
                                    onClicked: { veyra.pushInstallAnr(); root.anrState = veyra.pushAnrState() }
                                }
                                VButton {
                                    objectName: "set-anr-folder"
                                    ghost: true
                                    text: qsTr("打开文件夹")
                                    iconName: "folder"
                                    onClicked: veyra.pushOpenAnrFolder()
                                }
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("运行组件均为实验运行时，非 NVIDIA 官方合作或认证。详细哈希见 release-runtime-manifest.json。")
                        color: Theme.t2
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsBody
                        wrapMode: Text.WordWrap
                    }
                    VGroup {
                        // Reported by the engine's component state rather than a
                        // hand-written list that could drift from the package.
                        Repeater {
                            model: veyra.componentList
                            delegate: VRow {
                                required property var modelData
                                label: modelData.name
                                hint: modelData.detail
                                VTag {
                                    text: modelData.experimental ? qsTr("实验") : (modelData.loaded ? qsTr("已加载") : qsTr("未加载"))
                                    kind: modelData.experimental ? "exp" : (modelData.loaded ? "ok" : "")
                                }
                            }
                        }
                    }
                }

                // --- 关于 ------------------------------------------------
                ColumnLayout {
                    id: secAbout
                    Layout.fillWidth: true
                    spacing: 12
                    visible: root.section === "about"
                    RowLayout {
                        spacing: 18
                        Image {
                            source: Theme.logoImage
                            sourceSize.width: 88
                            fillMode: Image.PreserveAspectFit
                        }
                        ColumnLayout {
                            spacing: 4
                            PushWordmark { objectName: "about-wordmark"; pixelSize: 28 }
                            Text {
                                objectName: "about-version"
                                text: qsTr("版本 %1").arg(veyra.version)
                                color: Theme.t2
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fsBody
                            }
                            Text {
                                objectName: "about-upstream-credit"
                                text: qsTr("基于 Veyra %1 构建（GPL-3.0，作者 Likely7）").arg(veyra.upstreamVersion || "")
                                color: Theme.t2
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fsBody
                            }
                            Text {
                                objectName: "about-anr-credit"
                                text: qsTr("ANR 补帧由 Apex Compute 的 Apex Neural Rendering 提供")
                                color: Theme.t2
                                font.family: Theme.fontUi
                                font.pixelSize: Theme.fsBody
                            }
                        }
                    }
                    // GPL-3.0 appropriate legal notices: the licence, no warranty, the right to
                    // redistribute, where to read the licence and where the source is.
                    Text {
                        objectName: "about-license-notice"
                        Layout.fillWidth: true
                        text: qsTr("PushFPS 是自由软件，是 Veyra 的修改版本，依据 GNU 通用公共许可证第 3 版（GPL-3.0）发布。你可以在该许可证的条款下使用、修改和再分发它。本软件不提供任何担保。")
                        color: Theme.t3
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsHint
                        lineHeight: 1.1
                        wrapMode: Text.WordWrap
                    }
                    // NVIDIA RTX SDKs License, supplement 7.1(b): an application that incorporates the
                    // DLSS / NGX SDK attributes it, with the NVIDIA marks, in its about box.
                    Text {
                        objectName: "about-sdk-attribution"
                        Layout.fillWidth: true
                        text: qsTr("PushFPS 使用 NVIDIA® DLSS SDK 与 NGX SDK、AMD FidelityFX™ SDK 和 Intel® XeSS SDK。NVIDIA、NVIDIA RTX™ 和 DLSS 是 NVIDIA Corporation 的商标；PushFPS 未获 NVIDIA、AMD 或 Intel 赞助或认可。各 SDK 的许可证随程序提供。")
                        color: Theme.t3
                        font.family: Theme.fontUi
                        font.pixelSize: Theme.fsHint
                        lineHeight: 1.1
                        wrapMode: Text.WordWrap
                    }
                    VGroup {
                        VRow {
                            objectName: "about-creator-row"
                            label: "PushFPS"
                            hint: qsTr("由 DosukaSOL 制作 · X：@DosukaSOL")
                            VButton {
                                objectName: "about-open-creator"
                                text: qsTr("在 X 上关注")
                                iconName: "external"
                                onClicked: veyra.pushOpenLink("creator")
                            }
                        }
                        VRow {
                            label: qsTr("许可证")
                            hint: "GNU GPL-3.0"
                            VButton {
                                objectName: "about-open-license"
                                text: qsTr("查看许可证")
                                onClicked: veyra.openLicense()
                            }
                        }
                        VRow {
                            label: "Veyra"
                            hint: qsTr("PushFPS 的基础，作者 Likely7")
                            VButton {
                                objectName: "about-open-veyra"
                                text: qsTr("访问页面")
                                iconName: "external"
                                onClicked: veyra.pushOpenLink("veyra")
                            }
                        }
                        VRow {
                            label: "Apex Neural Rendering (ANR)"
                            hint: qsTr("ANR 补帧，作者 Apex Compute")
                            VButton {
                                objectName: "about-open-anr"
                                text: qsTr("访问页面")
                                iconName: "external"
                                onClicked: veyra.pushOpenLink("anr")
                            }
                        }
                        VRow {
                            label: "PushFlow · RIFE"
                            hint: qsTr("PushFlow 补帧使用 RIFE 神经网络（MIT，作者 hzwer），经 ONNX Runtime 与 DirectML 运行")
                            VButton {
                                objectName: "about-open-rife"
                                text: qsTr("访问页面")
                                iconName: "external"
                                onClicked: veyra.pushOpenLink("rife")
                            }
                        }
                        VRow {
                            label: qsTr("源代码")
                            hint: "github.com/DosukaSOL/PushFPS"
                            VButton {
                                objectName: "about-open-source"
                                text: qsTr("打开源代码页")
                                iconName: "external"
                                onClicked: veyra.openSourcePage()
                            }
                        }
                    }
                    VGroup {
                        VRow {
                            label: qsTr("检查更新")
                            // PushFPS signed updates (UpdateVerify.h): one click when the release is signed.
                            hint: veyra.updates.installState === "downloading" ? qsTr("正在下载更新… %1%").arg(Math.round(veyra.updates.installProgress * 100))
                                  : veyra.updates.installState === "verifying" ? qsTr("正在验证签名…")
                                  : veyra.updates.installState === "installing" ? qsTr("签名和校验通过，正在启动安装程序；PushFPS 会自动关闭并重新打开")
                                  : veyra.updates.latestVersion.length > 0
                                  ? (veyra.updates.canInstall ? qsTr("发现新版本 %1，已签名，可以一键安装").arg(veyra.updates.latestVersion)
                                                              : qsTr("发现新版本 %1").arg(veyra.updates.latestVersion))
                                  : qsTr("启动时自动检查，有更新时提示")
                            // (PushFPS 4.2.2) A row lays out one control: the three buttons stood on top of each other ("View update" over "Install update").
                            RowLayout {
                            spacing: 8
                            VButton {
                                text: veyra.updates.checking ? qsTr("检查中…") : qsTr("检查")
                                enabled: !veyra.updates.checking
                                onClicked: veyra.updates.check(true)
                            }
                            VButton {
                                objectName: "about-install-update"
                                visible: veyra.updates.latestVersion.length > 0 && veyra.updates.canInstall
                                primary: true
                                enabled: ["downloading", "verifying", "installing"].indexOf(veyra.updates.installState) < 0
                                text: qsTr("安装更新")
                                onClicked: veyra.updates.installUpdate()
                            }
                            VButton {
                                visible: veyra.updates.latestVersion.length > 0
                                text: qsTr("查看更新")
                                onClicked: veyra.updates.openRelease()
                            }
                            }
                        }
                        VRow {
                            label: qsTr("反馈问题")
                            hint: qsTr("打开 GitHub Issues，诊断信息已自动复制")
                            VButton { text: qsTr("打开"); onClicked: veyra.openFeedbackPage() }
                        }
                        // PushFPS: its own community. (The row used to point at group QR codes in a README that has none.)
                        VRow {
                            objectName: "about-discord-row"
                            label: qsTr("社区")
                            hint: qsTr("PushFPS 的 Discord：提问、分享设置和片段")
                            VButton { objectName: "about-open-discord"; iconName: "external"; text: qsTr("加入 Discord"); onClicked: veyra.pushOpenLink("discord") }
                        }
                        VRow {
                            label: qsTr("诊断信息")
                            hint: qsTr("复制后附在反馈里")
                            VButton { text: qsTr("复制"); onClicked: veyra.copyDiagnostics() }
                        }
                        // PushFPS: what the session playing now really does, to copy (engine/PushPerfReport.h).
                        VRow {
                            objectName: "set-perfreport"
                            label: qsTr("性能报告")
                            hint: qsTr("复制这次播放的实际数据：显卡和驱动、设置、游戏帧率、补出的帧率和软件延迟。里面没有任何个人信息，可以附在反馈或硬件报告里")
                            VButton { objectName: "set-perfreport-copy"; text: qsTr("复制"); onClicked: veyra.pushCopyPerformanceReport() }
                        }
                        // PushFPS: Check my setup (PushDiagnoseDialog.qml).
                        VRow {
                            objectName: "set-diagnose"
                            label: qsTr("检查我的设置")
                            hint: qsTr("看看正在运行的画面里，是什么在影响清晰度、流畅度和声音，每一项都有一键更改")
                            VButton { objectName: "set-diagnose-open"; text: qsTr("检查"); onClicked: veyra.navigate("diagnose") }
                        }
                        // PushFPS: PushFlow's benchmark on this PC's graphics card (PushCardTestDialog.qml).
                        VRow {
                            objectName: "set-cardtest"
                            label: qsTr("测试我的显卡")
                            hint: qsTr("测出 PushFlow 在这块显卡上的速度，结果可以复制发给我们。大约一分钟，测试时不能有画面在播放")
                            VButton { objectName: "set-cardtest-open"; text: qsTr("测试"); onClicked: veyra.navigate("cardtest") }
                        }
                        VRow {
                            label: qsTr("日志")
                            hint: veyra.logFile
                            VButton { text: qsTr("打开目录"); ghost: true; onClicked: veyra.openLogFolder() }
                        }
                        VRow {
                            label: qsTr("用户数据")
                            hint: veyra.dataDirectory
                            VButton { text: qsTr("打开目录"); ghost: true; onClicked: veyra.openDataFolder() }
                        }
                    }
                    // The diagnostics the engine actually reports, shown verbatim.
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 220
                        radius: 9
                        color: Theme.card2
                        clip: true
                        Flickable {
                            anchors.fill: parent
                            anchors.margins: 10
                            contentHeight: diagText.implicitHeight
                            clip: true
                            ScrollBar.vertical: VScrollBar { }
                            Text {
                                id: diagText
                                width: parent.width
                                text: veyra.diagnosticsReport()
                                color: Theme.t2
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.fsSmall
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }

                Item { Layout.preferredHeight: 12 }
            }
        }
    }

    VConfirm {
        id: obsRestart
        objectName: "obs-restart-confirm"
        title: qsTr("需要重启软件")
        text: qsTr("设置已保存，重启后生效。现在重启吗？当前播放或串流将停止。")
        acceptText: qsTr("是")
        rejectText: qsTr("否")
        onAccepted: veyra.restartApplication()
    }

    // [data-in] entrance order from pages-b.js PAGES.set.
    VRise { target: navCard; d: 0 }
    VRise { target: bodyCard; d: 1 }
}
