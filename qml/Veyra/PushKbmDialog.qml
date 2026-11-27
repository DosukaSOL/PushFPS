// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// The keyboard and mouse as a controller (dialog key "kbm"): the switch, the layout (GTA 6, GTA 6 Classic
// / GTA V, Red Dead Redemption 2, or the player's own), how the mouse turns the camera, the key that gives
// the keyboard and mouse back, and every control's two inputs. Everything is one preference, "pushKbm"
// (PushKbm.js reads and writes it; bridge/PushKbm.cpp gives it to the program, which keeps every value in
// its range). The console only ever sees a controller: PS5 Remote Play, a capture card with "Control the PS5
// with Remote Play", or Xbox streaming.
import QtQuick
import QtQuick.Layouts
import "PushKbm.js" as Kbm

DialogLayer {
    id: kbm
    objectName: "pushfps-kbm"
    key: "kbm"
    glyph: "gamepad"
    title: qsTr("鼠标和键盘")
    sub: qsTr("用这台电脑的鼠标和键盘玩主机游戏：主机看到的是一个手柄，手柄也可以同时用")
    dialogWidth: 600

    readonly property var defaults: veyra.pushKbmDefaults()
    readonly property var cfg: Kbm.config(String(veyra.preferences.pushKbm || ""), kbm.defaults)
    readonly property var kbmState: veyra.pushKbm || ({})
    readonly property string pad: Kbm.padOf(kbm.cfg, String(veyra.preferences.pushConsole || ""), String(kbm.kbmState.target || ""))
    readonly property var allControls: veyra.pushKbmControls()
    readonly property var controls: Kbm.controlsFor(kbm.allControls, kbm.pad)
    readonly property string releaseName: veyra.pushKbmInputName(kbm.cfg.release)
    property string note: ""

    function save(next) { if (next) veyra.setPreference("pushKbm", Kbm.text(next)) }
    function set(key, value) { kbm.save(Kbm.changed(kbm.cfg, key, value)) }
    function controlIndex(id) { return kbm.allControls.indexOf(id) }

    function controlLabel(id) {
        const xbox = kbm.pad === "xbox"
        switch (id) {
        case "cross": return xbox ? "A" : "✕"
        case "circle": return xbox ? "B" : "○"
        case "square": return xbox ? "X" : "□"
        case "triangle": return xbox ? "Y" : "△"
        case "up": return qsTr("方向键 %1").arg("↑")
        case "down": return qsTr("方向键 %1").arg("↓")
        case "left": return qsTr("方向键 %1").arg("←")
        case "right": return qsTr("方向键 %1").arg("→")
        case "l1": return xbox ? "LB" : "L1"
        case "r1": return xbox ? "RB" : "R1"
        case "l2": return xbox ? "LT" : "L2"
        case "r2": return xbox ? "RT" : "R2"
        case "l3": return xbox ? "LS" : "L3"
        case "r3": return xbox ? "RS" : "R3"
        case "options": return xbox ? "Menu" : "Options"
        case "view": return xbox ? "View" : qsTr("触控板")
        case "create": return "Create"
        case "home": return xbox ? qsTr("Xbox 键") : qsTr("PS 键")
        case "moveUp": return qsTr("移动 %1").arg("↑")
        case "moveDown": return qsTr("移动 %1").arg("↓")
        case "moveLeft": return qsTr("移动 %1").arg("←")
        case "moveRight": return qsTr("移动 %1").arg("→")
        case "walk": return qsTr("慢走（按住）")
        }
        return id
    }
    function inputName(code) { return Number(code) > 0 ? veyra.pushKbmInputName(Number(code)) : "—" }
    readonly property var slotOptions: [{ id: "key", label: qsTr("按一个键…") }]
        .concat(Kbm.mouseCodes.map(c => ({ id: String(c), label: veyra.pushKbmInputName(c) })))
        .concat([{ id: "0", label: qsTr("无") }])
    function pick(controlId, slot, id) {
        kbm.note = ""
        if (id === "key") { veyra.pushKbmListen(kbm.controlIndex(controlId), slot); return }
        const next = Kbm.withBinding(kbm.cfg, controlId, slot, Number(id))
        if (next === null) kbm.note = qsTr("%1 是交还键，不能再按手柄上的键").arg(kbm.releaseName)
        else kbm.save(next)
    }
    function presetLabel(id) {
        return id === "gta6" ? qsTr("GTA 6（默认布局）") : id === "gta6classic" ? qsTr("GTA 6 经典布局 · GTA V")
             : id === "rdr2" ? "Red Dead Redemption 2" : qsTr("自定义")
    }
    readonly property string statusText: {
        switch (Kbm.status(kbm.kbmState, kbm.cfg.on)) {
        case "off": return qsTr("已关闭")
        case "using": return qsTr("正在使用：主机收到的是鼠标和键盘 · 按 %1 交还").arg(kbm.releaseName)
        case "ready": return qsTr("可以开始：点击画面或按 %1").arg(kbm.releaseName)
        }
        return qsTr("用于 PS5 远程游玩、开着“用远程游玩控制 PS5”的采集卡，以及 Xbox 串流")
    }

    onShownChanged: if (!shown) { veyra.pushKbmStopListening(); kbm.note = "" }
    Connections {
        target: veyra
        function onPushKbmBinding(control, slot, code) {
            kbm.note = ""
            if (control === kbm.allControls.length) { kbm.save(Kbm.withRelease(kbm.cfg, code)); return }
            const id = kbm.allControls[control]
            if (id === undefined) return
            const next = Kbm.withBinding(kbm.cfg, id, slot, code)
            if (next === null) kbm.note = qsTr("%1 是交还键，不能再按手柄上的键").arg(kbm.releaseName)
            else kbm.save(next)
        }
    }

    DialogGroup {
        VRow {
            objectName: "pushfps-kbm-on-row"
            label: qsTr("使用鼠标和键盘")
            hint: kbm.statusText
            hintBelow: true
            VSwitch {
                objectName: "pushfps-kbm-on"
                checked: kbm.cfg.on
                onToggled: v => kbm.set("on", v)
            }
        }
        VRow {
            objectName: "pushfps-kbm-preset-row"
            label: qsTr("按键布局")
            help: qsTr("按 Rockstar 电脑版游戏的键位（WASD 移动，Shift 跑，空格跳，F / E 互动，Q 掩护，Tab 武器，C 向后看，鼠标瞄准和射击），每个键按下手柄上做同样事情的键。GTA 6 默认布局用 L3 跑（再按一次冲刺）。开车更适合用手柄：两者可以同时用。")
            hint: qsTr("每个键在下面都可以改")
            hintBelow: true
            VSelect {
                objectName: "pushfps-kbm-preset"
                implicitWidth: 230
                value: kbm.presetLabel(kbm.cfg.preset)
                options: Kbm.presets.map(id => ({ id: id, label: kbm.presetLabel(id) }))
                onPicked: id => kbm.save(Kbm.withPreset(kbm.cfg, String(id), veyra.pushKbmPresetMap(String(id))))
            }
        }
        VRow {
            objectName: "pushfps-kbm-pad-row"
            label: qsTr("按钮名称")
            hint: qsTr("只改这里显示的名称，按键位置两种主机都一样")
            hintBelow: true
            VSeg {
                objectName: "pushfps-kbm-pad"
                options: [{ id: "ps", label: "PlayStation" }, { id: "xbox", label: "Xbox" }]
                current: kbm.pad
                onPicked: id => kbm.set("pad", String(id))
            }
        }
    }

    DialogGroup {
        VRow {
            label: qsTr("鼠标灵敏度")
            help: qsTr("鼠标移动多快时视角转到最快。游戏里的手柄灵敏度也会影响转动速度")
            hint: String(kbm.cfg.sens)
            VSlider {
                objectName: "pushfps-kbm-sens"
                implicitWidth: 170
                valueFromModel: true
                resettable: true; defaultValue: Number(kbm.defaults.sens)
                from: 1; to: 100; stepSize: 1
                value: kbm.cfg.sens
                onMoved: value => kbm.set("sens", Math.round(value))
            }
        }
        VRow {
            label: qsTr("上下灵敏度")
            hint: qsTr("左右的 %1%").arg(kbm.cfg.vert)
            VSlider {
                objectName: "pushfps-kbm-vert"
                implicitWidth: 170
                valueFromModel: true
                resettable: true; defaultValue: Number(kbm.defaults.vert)
                from: 25; to: 400; stepSize: 5
                value: kbm.cfg.vert
                onMoved: value => kbm.set("vert", Math.round(value / 5) * 5)
            }
        }
        VRow {
            label: qsTr("摇杆起点")
            help: qsTr("游戏会忽略推得很轻的摇杆。鼠标一动，摇杆就至少推到这里，小幅移动也能转动视角。视角在鼠标不动时还在漂，就调低；小幅移动没有反应，就调高")
            hint: kbm.cfg.dz + "%"
            VSlider {
                objectName: "pushfps-kbm-dz"
                implicitWidth: 170
                valueFromModel: true
                resettable: true; defaultValue: Number(kbm.defaults.dz)
                from: 0; to: 50; stepSize: 1
                value: kbm.cfg.dz
                onMoved: value => kbm.set("dz", Math.round(value))
            }
        }
        VRow {
            label: qsTr("响应曲线")
            help: qsTr("100 为直线。高于 100：小幅移动更细腻，快速移动照样转得快")
            hint: String(kbm.cfg.curve)
            VSlider {
                objectName: "pushfps-kbm-curve"
                implicitWidth: 170
                valueFromModel: true
                resettable: true; defaultValue: Number(kbm.defaults.curve)
                from: 50; to: 200; stepSize: 5
                value: kbm.cfg.curve
                onMoved: value => kbm.set("curve", Math.round(value / 5) * 5)
            }
        }
        VRow {
            label: qsTr("上下反转")
            VSwitch {
                objectName: "pushfps-kbm-invert"
                checked: kbm.cfg.invert
                onToggled: v => kbm.set("invert", v)
            }
        }
    }

    DialogGroup {
        VRow {
            objectName: "pushfps-kbm-release-row"
            label: qsTr("交还键")
            help: qsTr("开始和结束：点击画面或按这个键，鼠标和键盘就交给主机；再按一次（或 Alt+Tab）就交还给电脑。交给主机时，没被用到的 F1–F12 仍然是 PushFPS 的快捷键")
            hint: kbm.kbmState.listenControl === kbm.allControls.length ? qsTr("请按一个键…") : ""
            hintBelow: true
            RowLayout {
                spacing: 8
                Text {
                    text: kbm.releaseName
                    color: Theme.t1; font.family: Theme.fontUi; font.pixelSize: 13; font.weight: Font.Medium
                }
                VButton {
                    objectName: "pushfps-kbm-release"
                    text: kbm.kbmState.listenControl === kbm.allControls.length ? qsTr("取消") : qsTr("更改")
                    onClicked: kbm.kbmState.listenControl === kbm.allControls.length ? veyra.pushKbmStopListening()
                                                                                   : veyra.pushKbmListen(kbm.allControls.length, 0)
                }
            }
        }
        Text {
            objectName: "pushfps-kbm-note"
            visible: kbm.note.length > 0
            Layout.fillWidth: true
            text: kbm.note
            color: Theme.warn; font.family: Theme.fontUi; font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: kbm.controls
            delegate: VRow {
                id: controlRow
                required property string modelData
                readonly property var inputs: kbm.cfg.map[modelData] || [0, 0]
                readonly property int controlIdx: kbm.controlIndex(modelData)
                objectName: "pushfps-kbm-row-" + modelData
                label: kbm.controlLabel(modelData)
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: 2
                        delegate: VSelect {
                            required property int index
                            readonly property bool listening: kbm.kbmState.listenControl === controlRow.controlIdx && kbm.kbmState.listenSlot === index
                            objectName: "pushfps-kbm-" + controlRow.modelData + "-" + index
                            implicitWidth: 150
                            title: controlRow.label
                            value: listening ? qsTr("请按一个键…") : kbm.inputName(controlRow.inputs[index])
                            options: kbm.slotOptions
                            onPicked: id => kbm.pick(controlRow.modelData, index, String(id))
                        }
                    }
                }
            }
        }
    }

    actions: [{ label: qsTr("恢复布局默认") }, { label: qsTr("完成"), primary: true }]
    onActionTriggered: label => {
        if (label === qsTr("恢复布局默认")) {
            const id = Kbm.presets.indexOf(kbm.cfg.preset) >= 0 ? kbm.cfg.preset : "gta6"
            kbm.save(Kbm.withPreset(kbm.cfg, id, veyra.pushKbmPresetMap(id)))
            kbm.note = ""
        } else kbm.dialogHost.close()
    }
}
