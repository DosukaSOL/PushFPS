// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Game presets, as the Windows build's PushGamePresetsDialog.qml (its words, its object names and the shared
// library PushGamePresets.js): ready-made settings per game whose HUD areas become the HUD protection boxes
// (app.setHudRegions, FrameView protect), the player's own presets (preference pushGamePresets, kept as
// Windows keeps it) and share codes. What a preset sets here is what the preview has of the Windows one: the
// console, the target (targetMode) and the boxes. The preview has no game frame rate and no multiplier (it
// shows a picture at every refresh up to the target), so a preset's frame rate and adaptive multiplier are
// kept in it and shown, never applied. A Popup, as CropDialog.qml: the Windows dialog host is not in the preview.
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import "PushGamePresets.js" as Presets

Popup {
    id: games
    objectName: "pushfps-game-presets-dialog"
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(680, (parent ? parent.width : 680) - 40)
    height: Math.min(implicitHeight, (parent ? parent.height : 700) - 60)
    padding: 20
    leftPadding: 13
    rightPadding: 13
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var mine: {
        try {
            const list = JSON.parse(app.preferences.pushGamePresets || "[]")
            return Array.isArray(list) ? list.map(Presets.sanitize).filter(p => p !== null) : []
        } catch (e) { return [] }
    }
    readonly property var all: Presets.builtIn.map(Presets.sanitize).concat(games.mine.map(p => Object.assign({}, p, { mine: true })))
    // Which games the list shows: every preset, or one Nintendo console's games (the console chosen in the
    // quick setup picks the starting filter).
    property string consoleFilter: ""
    readonly property var listed: games.all.filter(p => games.consoleFilter === "" || p.console === games.consoleFilter)
    property string selectedName: "GTA VI"
    readonly property var selected: games.listed.find(p => p.name === games.selectedName) || games.listed[0] || games.all[0]
    function consoleName(id) { return id === "switch" ? "Switch" : id === "switch2" ? "Switch 2" : "" }
    property bool protectHud: true
    property string importText: ""
    property string message: ""
    property bool messageError: false
    // Tests set the screen's refresh rate.
    property real refreshHzOverride: 0
    readonly property real refreshHz: refreshHzOverride > 0 ? refreshHzOverride : Number(Screen.refreshRate || 0)

    // Built-in names and notes in the interface language (the library keeps English ids).
    function labelFor(p) {
        if (!p) return ""
        if (p.mine) return p.name + " · " + qsTr("我的")
        if (p.console === "switch" || p.console === "switch2") return p.name + " · " + games.consoleName(p.console)
        switch (p.id) {
        case "open-world-bl": return qsTr("开放世界 · 小地图在左下")
        case "open-world-tr": return qsTr("开放世界 · 小地图在右上")
        case "action-corners": return qsTr("动作 · HUD 在下方两角")
        case "racing": return qsTr("赛车 · 速度表在右下")
        case "shooter": return qsTr("射击 · HUD 在四个角")
        case "cinematic": return qsTr("电影感 · 不保护 HUD")
        }
        return p.name
    }
    // Switch and Switch 2 games: where the 30 FPS comes from, the adaptive multiplier when the game dips, and
    // whether its HUD areas are set or left to the automatic detection.
    function switchNote(p) {
        let text
        if (p.id === "ns-botw") text = qsTr("Switch 版 30 FPS（Switch 2 上也是）。Nintendo Switch 2 Edition 为 60 FPS，玩那个版本时请把游戏帧率选 60。")
        else if (p.id === "ns-animal-crossing") text = qsTr("Switch 游戏，30 FPS；Nintendo Switch 2 Edition 也是 30 FPS。")
        else if (p.id === "ns2-cyberpunk") text = qsTr("Switch 2 游戏：画质模式 30 FPS，性能模式最高 40 FPS。")
        else if (p.id === "ns2-elden-ring") text = qsTr("Switch 2 游戏：上限 40 FPS，底座模式下多在 30–35 FPS。")
        else text = p.console === "switch" ? qsTr("Switch 游戏，在 Switch 2 上也是 30 FPS（没有 Switch 2 更新）。")
                                           : qsTr("Switch 2 游戏，底座模式 30 FPS。")
        text += " " + (p.regions.length > 0 ? qsTr("HUD 位置按游戏推测，开始玩后请检查一下。")
                                            : qsTr("没有预设 HUD 区域：需要时用「调整 HUD 区域」自动识别。"))
        return text
    }
    function noteFor(p) {
        if (!p) return ""
        if (!p.mine && (p.console === "switch" || p.console === "switch2")) return games.switchNote(p)
        switch (p.mine ? "" : p.id) {
        // The Windows note without its last sentence: the preview has no adaptive multiplier to switch on.
        case "gta6": return qsTr("小地图在左下，生命和专注条在左上，武器、通缉等级和金钱在右上（来自 Rockstar 发售前公开的画面）。边缘位置是估计的，开始玩后请用「调整 HUD 区域」检查一下")
        case "open-world-bl": return qsTr("左下角的圆形或方形小地图")
        case "open-world-tr": return qsTr("右上角的小地图或指南针")
        case "action-corners": return qsTr("左下角血量，右下角武器和弹药")
        case "racing": return qsTr("右下角的速度、挡位和小地图")
        case "shooter": return qsTr("每个角都有小元素")
        case "cinematic": return qsTr("剧情游戏和电影：整个画面都补帧")
        }
        return p.note
    }
    function say(text, error) { games.message = text; games.messageError = error === true }
    function writeMine(list) {
        app.setPreference("pushGamePresets", JSON.stringify(list.slice(0, 100)))
        return true
    }

    // --- apply ---------------------------------------------------------------
    // The preview's targetMode: 0 off, 1 60 FPS, 2 120 FPS, 3 the screen's rate, 4 90 FPS.
    readonly property var targetModes: ({ "off": 0, "60": 1, "120": 2, "monitor": 3, "90": 4 })
    function apply(p) {
        if (!p) return
        if (p.console) app.setPreference("pushConsole", p.console)
        const refresh = games.refreshHz > 0 ? games.refreshHz : 60
        const target = Presets.resolveTarget(p.target, refresh)
        if (games.targetModes[target] !== undefined) app.targetMode = games.targetModes[target]
        const regions = games.protectHud ? p.regions : []
        if (!app.setHudRegions(regions, p.feather)) { games.say(qsTr("HUD 保护区域没有应用"), true); return }
        games.say(regions.length > 0 ? qsTr("已应用「%1」，%2 个 HUD 区域受保护。可用「HUD 保护」微调").arg(games.labelFor(p)).arg(regions.length)
                                     : qsTr("已应用「%1」").arg(games.labelFor(p)), false)
    }

    // --- your own ------------------------------------------------------------
    function currentAsPreset(name) {
        const regions = (app.hudRegions || []).map(r => ({ left: r.left, top: r.top, right: r.right, bottom: r.bottom, ellipse: r.ellipse === true }))
        const ids = ["off", "60", "120", "monitor", "90"]
        return Presets.sanitize({ name: name, fps: Number(app.preferences.pushGameFps || 30), target: ids[app.targetMode] || "auto",
                                  feather: Number(app.hudFeather), regions: regions, adaptive: false,
                                  console: String(app.preferences.pushConsole || ""),
                                  note: qsTr("我的预设") })
    }
    function saveMine(name) {
        const p = games.currentAsPreset(name)
        if (!p) { games.say(qsTr("请先输入名称"), true); return false }
        if (Presets.builtIn.some(b => b.name === p.name)) { games.say(qsTr("这个名称已被内置预设使用"), true); return false }
        const list = games.mine.filter(x => x.name !== p.name)
        list.push(p)
        list.sort((a, b) => a.name.localeCompare(b.name))
        if (!games.writeMine(list)) { games.say(qsTr("保存失败"), true); return false }
        if (games.consoleFilter !== "" && games.consoleFilter !== p.console) games.consoleFilter = ""
        games.selectedName = p.name
        games.say(qsTr("已保存「%1」（%2 个 HUD 区域）").arg(p.name).arg(p.regions.length), false)
        return true
    }
    function addImported(p) {
        if (!p) { games.say(qsTr("这不是有效的 PushFPS 预设代码"), true); return false }
        let name = p.name
        if (Presets.builtIn.some(b => b.name === name)) name = (name + " " + qsTr("（分享）")).slice(0, Presets.kMaxName)
        p.name = name
        const list = games.mine.filter(x => x.name !== name)
        list.push(p)
        list.sort((a, b) => a.name.localeCompare(b.name))
        if (!games.writeMine(list)) { games.say(qsTr("保存失败"), true); return false }
        if (games.consoleFilter !== "" && games.consoleFilter !== p.console) games.consoleFilter = ""
        games.selectedName = name
        games.say(qsTr("已添加「%1」").arg(name), false)
        return true
    }
    function deleteMine(name) {
        games.writeMine(games.mine.filter(x => x.name !== name))
        games.consoleFilter = ""
        games.selectedName = "GTA VI"
        games.say(qsTr("已删除「%1」").arg(name), false)
    }
    function shareCode(p) { return Presets.encode(p, Qt.btoa) }
    function targetLabel(p) {
        return p.target === "auto" ? qsTr("自动") : p.target === "off" ? qsTr("不补帧")
             : p.target === "monitor" ? qsTr("跟随显示器") : p.target + " FPS"
    }

    onAboutToShow: {
        games.message = ""; games.importText = ""; codeField.text = ""
        const c = String(app.preferences.pushConsole || "")
        games.consoleFilter = c === "switch" || c === "switch2" ? c : ""
        if (games.consoleFilter !== "" && !games.listed.some(p => p.name === games.selectedName) && games.listed.length > 0)
            games.selectedName = games.listed[0].name
    }

    FileDialog {
        id: saveFile
        title: qsTr("保存游戏预设")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "pushfps"
        nameFilters: ["PushFPS (*.pushfps)"]
        onAccepted: if (!app.writeTextFile(selectedFile, games.shareCode(games.selected))) games.say(qsTr("保存失败"), true)
    }
    FileDialog {
        id: openFile
        title: qsTr("打开游戏预设")
        nameFilters: ["PushFPS (*.pushfps *.txt *.json)"]
        onAccepted: {
            const t = app.readTextFile(selectedFile)
            games.addImported(t.length > 0 ? Presets.decode(t, Qt.atob) : null)
        }
    }

    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.45) }
    background: Rectangle {
        radius: Theme.rCard
        color: Theme.card
        border.width: 1
        border.color: Theme.stroke
    }
    // RowList keeps a row's hover light inside the list (its `room` comes out of this dialog's side padding).
    contentItem: RowList {
        ColumnLayout {
            id: body
            Layout.fillWidth: true
            spacing: 0
            Text {
                text: qsTr("游戏预设")
                color: Theme.t1
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsH3
                font.weight: Font.DemiBold
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 4
                Layout.bottomMargin: 10
                wrapMode: Text.WordWrap
                text: qsTr("GTA VI 等游戏的推荐设置，小地图和 HUD 保持清晰")
                color: Theme.t3
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsBody
            }

            VRow {
                label: qsTr("游戏主机")
                VSeg {
                    objectName: "pushfps-game-preset-console"
                    options: [{ id: "", label: qsTr("全部") }, { id: "switch", label: "Switch" }, { id: "switch2", label: "Switch 2" }]
                    current: games.consoleFilter
                    onPicked: id => {
                        games.consoleFilter = id
                        if (!games.listed.some(p => p.name === games.selectedName) && games.listed.length > 0) games.selectedName = games.listed[0].name
                    }
                }
            }
            VRow {
                label: qsTr("游戏")
                VSelect {
                    objectName: "pushfps-game-preset-select"
                    implicitWidth: 300
                    options: games.listed.map(p => ({ id: p.name, label: games.labelFor(p) }))
                    value: games.labelFor(games.selected)
                    onPicked: id => games.selectedName = id
                }
            }
            Text {
                Layout.fillWidth: true
                Layout.leftMargin: 14; Layout.rightMargin: 14
                visible: games.noteFor(games.selected).length > 0
                text: games.noteFor(games.selected)
                color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12; wrapMode: Text.WordWrap
            }
            // A small picture of where the protected areas are.
            Rectangle {
                objectName: "pushfps-game-preset-map"
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8; Layout.bottomMargin: 8
                implicitWidth: 320; implicitHeight: 180
                radius: 8
                color: Theme.ink(0.04)
                border.width: 1; border.color: Theme.stroke
                Repeater {
                    model: games.selected ? games.selected.regions : []
                    delegate: Rectangle {
                        required property var modelData
                        x: modelData.left * 320; y: modelData.top * 180
                        width: (modelData.right - modelData.left) * 320; height: (modelData.bottom - modelData.top) * 180
                        radius: modelData.ellipse ? Math.min(width, height) / 2 : 3
                        color: Qt.rgba(0.537, 0.961, 0.165, games.protectHud ? 0.22 : 0.06)
                        border.width: 1; border.color: Theme.accent
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: !games.selected || games.selected.regions.length === 0
                    text: qsTr("整个画面补帧")
                    color: Theme.t3; font.family: Theme.fontUi; font.pixelSize: 12
                }
            }
            VRow {
                label: qsTr("保护 HUD")
                hint: qsTr("绿色区域保持主机原画，不补帧，文字和小地图不会糊")
                VSwitch { objectName: "pushfps-game-preset-hud"; checked: games.protectHud; onToggled: checked => games.protectHud = checked }
            }
            VRow {
                label: qsTr("HUD 位置不对？")
                hint: qsTr("在你的游戏画面上自动识别 HUD，或手动拖动方框")
                VButton {
                    objectName: "pushfps-game-preset-edit-hud"
                    ghost: true
                    iconName: "scan"
                    text: qsTr("调整 HUD 区域")
                    onClicked: { games.close(); app.openDialog("hud") }
                }
            }
            VRow {
                label: qsTr("游戏帧率 · 目标")
                value: games.selected ? games.selected.fps + " → " + games.targetLabel(games.selected) : ""
            }

            Text {
                Layout.topMargin: 14
                Layout.bottomMargin: 4
                text: qsTr("分享")
                color: Theme.t2
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsBody
                font.weight: Font.DemiBold
            }
            VRow {
                label: qsTr("分享这个预设")
                hint: qsTr("复制代码发给朋友或贴到 Reddit / Discord；也可以保存为文件")
                RowLayout {
                    spacing: 6
                    VButton {
                        objectName: "pushfps-game-preset-copy"
                        text: qsTr("复制代码"); ghost: true; iconName: "copy"
                        onClicked: { if (app.copyText(games.shareCode(games.selected))) games.say(qsTr("代码已复制"), false) }
                    }
                    VButton {
                        objectName: "pushfps-game-preset-file"
                        text: qsTr("保存文件"); ghost: true
                        onClicked: { saveFile.selectedFile = games.selected.name + ".pushfps"; saveFile.open() }
                    }
                }
            }
            VRow {
                label: qsTr("添加别人的预设")
                // A VRow's slot does not lay out several controls: they go in a row here.
                RowLayout {
                    spacing: 6
                    DialogTextField {
                        id: codeField
                        objectName: "pushfps-game-preset-code"
                        implicitWidth: 200
                        placeholder: qsTr("粘贴 PUSHFPS1: 代码")
                        onTyped: t => games.importText = t
                        onEdited: t => games.importText = t
                    }
                    VButton {
                        objectName: "pushfps-game-preset-add"
                        text: qsTr("添加"); ghost: true
                        onClicked: games.addImported(Presets.decode(games.importText, Qt.atob))
                    }
                    VButton {
                        objectName: "pushfps-game-preset-open"
                        text: qsTr("打开文件"); ghost: true
                        onClicked: openFile.open()
                    }
                }
            }
            VRow {
                label: qsTr("保存当前设置为预设")
                RowLayout {
                    spacing: 6
                    DialogTextField {
                        id: ownName
                        objectName: "pushfps-game-preset-name"
                        implicitWidth: 180
                        placeholder: qsTr("名称")
                    }
                    VButton { objectName: "pushfps-game-preset-save"; text: qsTr("保存"); ghost: true; onClicked: games.saveMine(ownName.text) }
                }
            }
            VRow {
                visible: !!games.selected && games.selected.mine === true
                label: qsTr("删除「%1」").arg(games.selected ? games.selected.name : "")
                VButton { objectName: "pushfps-game-preset-delete"; text: qsTr("删除"); ghost: true; iconName: "trash"; onClicked: games.deleteMine(games.selected.name) }
            }
            Text {
                objectName: "pushfps-game-preset-message"
                Layout.fillWidth: true
                Layout.topMargin: 8
                visible: games.message.length > 0
                text: games.message
                color: games.messageError ? Theme.warn : Theme.accent
                font.family: Theme.fontUi; font.pixelSize: 12; wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 12
                spacing: 8
                Item { Layout.fillWidth: true }
                VButton { objectName: "pushfps-game-preset-close"; text: qsTr("关闭"); onClicked: games.close() }
                VButton { objectName: "pushfps-game-preset-apply"; text: qsTr("应用"); primary: true; onClicked: games.apply(games.selected) }
            }
        }
    }
}
