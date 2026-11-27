// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// The keyboard and mouse dialog (PushKbmDialog, PushKbm.js) against the fixture bridge: the switch, the
// layouts, the button names of either console, an input moved from one control to another, the key that
// gives everything back, and the status line.
import QtQuick
import QtTest
import Veyra
Item {
    id: root
    width: 900; height: 1100
    Item { id: host; anchors.fill: parent; property string dialog: ""; property real motionScale: 1; function close() { dialog = "" } }
    PushKbmDialog { id: kbm; dialogHost: host }

    TestCase {
        name: "PushKbm"
        when: windowShown
        function init() {
            host.dialog = ""
            veyra.preferences = ({})
            veyra.pushKbm = ({built: true, on: false, available: false, captured: false, target: "", releaseName: "F12", listenControl: -1, listenSlot: -1})
            veyra.kbmListened = []
            host.dialog = "kbm"
            verify(kbm.shown)
        }
        function control(name) { const c = findChild(kbm, name); verify(c !== null, name); return c }
        function saved() { return JSON.parse(veyra.preferences.pushKbm) }

        function test_off_until_switched_on_with_gta_6_keys() {
            verify(kbm.cfg.on === false, "nothing saved: off")
            compare(kbm.cfg.preset, "gta6")
            compare(kbm.statusText, qsTr("已关闭"))
            compare(control("pushfps-kbm-preset").value, qsTr("GTA 6（默认布局）"))
            tryVerify(function() { return control("pushfps-kbm-row-cross").visible }, 3000, "the dialog is on screen")
            compare(control("pushfps-kbm-row-cross").label, "✕")
            compare(control("pushfps-kbm-cross-0").value, "Space", "Space jumps on Cross in GTA 6")
            control("pushfps-kbm-on").toggled(true)
            verify(saved().on === true, "switched on")
            compare(saved().map.cross[0], 32, "and saved with the layout's keys")
        }
        function test_layouts_replace_every_key() {
            control("pushfps-kbm-preset").picked("rdr2")
            compare(saved().preset, "rdr2")
            compare(saved().map.cross[0], 160, "Red Dead: Shift runs on Cross")
            compare(saved().map.up, [72, 38], "H whistles on the D-pad")
            control("pushfps-kbm-preset").picked("gta6classic")
            compare(saved().map.square[0], 32, "Classic / GTA V: Space jumps on Square")
        }
        function test_xbox_names_and_no_create_row() {
            compare(kbm.pad, "ps")
            verify(findChild(kbm, "pushfps-kbm-row-create") !== null, "a PS5 has Create")
            control("pushfps-kbm-pad").picked("xbox")
            compare(kbm.pad, "xbox")
            tryVerify(function() { const r = findChild(kbm, "pushfps-kbm-row-cross"); return r !== null && r.label === "A" }, 3000)
            compare(control("pushfps-kbm-row-l2").label, "LT")
            tryVerify(function() { return findChild(kbm, "pushfps-kbm-row-create") === null }, 3000, "an Xbox has no Create button of its own")
            veyra.preferences = ({})
            veyra.pushKbm = Object.assign({}, veyra.pushKbm, {target: "xbox"})
            compare(kbm.pad, "xbox", "an Xbox stream shows Xbox names by itself")
        }
        function test_an_input_moves_to_the_control_it_is_given_to() {
            control("pushfps-kbm-circle-1").picked("257")
            compare(saved().map.circle, [162, 257])
            compare(saved().map.r2, [0, 0], "the left button no longer fires")
            compare(saved().preset, "custom")
            compare(control("pushfps-kbm-preset").value, qsTr("自定义"))
        }
        function test_a_key_is_listened_for_and_the_release_key_refused() {
            control("pushfps-kbm-triangle-1").picked("key")
            compare(veyra.kbmListened, [3, 1], "the program listens for the triangle's second input")
            tryVerify(function() { return control("pushfps-kbm-triangle-1").value === qsTr("请按一个键…") }, 3000)
            veyra.pushKbmStopListening()
            veyra.pushKbmBinding(3, 1, 71)
            compare(saved().map.triangle, [70, 71], "G was pressed")
            veyra.pushKbmBinding(0, 0, 123)
            tryVerify(function() { return control("pushfps-kbm-note").visible }, 3000, "F12 gives the keyboard back and is not bound")
            compare(saved().map.cross[0], 32, "nothing changed")
        }
        function test_another_release_key() {
            control("pushfps-kbm-release").clicked()
            compare(veyra.kbmListened, [kbm.allControls.length, 0])
            veyra.pushKbmStopListening()
            veyra.pushKbmBinding(kbm.allControls.length, 0, 32)
            compare(saved().release, 32)
            compare(saved().map.cross[0], 0, "Space left the control that had it")
        }
        function test_reset_and_status() {
            control("pushfps-kbm-circle-1").picked("257")
            kbm.actionTriggered(qsTr("恢复布局默认"))
            compare(saved().map.r2[0], 257, "back to the layout's keys")
            compare(saved().preset, "gta6", "a custom layout goes back to GTA 6")
            control("pushfps-kbm-on").toggled(true)
            compare(kbm.statusText, qsTr("用于 PS5 远程游玩、开着“用远程游玩控制 PS5”的采集卡，以及 Xbox 串流"))
            veyra.pushKbm = Object.assign({}, veyra.pushKbm, {available: true})
            compare(kbm.statusText, qsTr("可以开始：点击画面或按 %1").arg("F12"))
            veyra.pushKbm = Object.assign({}, veyra.pushKbm, {captured: true})
            compare(kbm.statusText, qsTr("正在使用：主机收到的是鼠标和键盘 · 按 %1 交还").arg("F12"))
        }
    }
}
