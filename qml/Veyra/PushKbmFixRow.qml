// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
//
// Mouse and keyboard switched on while nothing can take them yet: the one step that lets them reach the
// console, as a row with one button (PushKbm.js fixFor says which). The row above it (quick setup's
// "Mouse and keyboard", the window's switch) says what is missing in words; this row does it. A capture
// card only brings the picture and the sound in, so with one the keys go to the PS5 through Remote Play:
// "Control the PS5 through Remote Play" switched on here when a PS5 is paired, PS5 streaming opened to
// pair it when none is.
import QtQuick
import "PushKbm.js" as Kbm

VRow {
    id: fixRow
    property bool switchedOn: false
    property var kbmState: ({})
    property string buttonName: "pushfps-kbm-fix"
    readonly property bool paired: veyra.ps5Profiles !== undefined && veyra.ps5Profiles !== null && veyra.ps5Profiles.length > 0
    readonly property string fix: fixRow.switchedOn && fixRow.kbmState && fixRow.kbmState.available !== true && fixRow.kbmState.captured !== true
                                  ? Kbm.fixFor(fixRow.kbmState.waiting, fixRow.paired) : ""
    visible: fixRow.fix !== ""
    label: fixRow.fix === "control-on" ? qsTr("用远程游玩控制 PS5")
         : fixRow.fix === "pair" || fixRow.fix === "ps5" ? qsTr("PS5 串流")
         : fixRow.fix === "xbox" ? qsTr("Xbox 串流") : qsTr("PS5 手柄控制")
    VButton {
        objectName: fixRow.buttonName
        text: fixRow.fix === "control-on" ? qsTr("开启")
            : fixRow.fix === "pair" ? qsTr("去配对")
            : fixRow.fix === "reconnect" ? qsTr("重连") : qsTr("打开")
        onClicked: {
            const fix = fixRow.fix
            veyra.logUi("push-kbm", "fix " + fix + " for " + String(fixRow.kbmState.waiting || ""))
            if (fix === "control-on") veyra.ps5ControlSet("enabled", true)
            else if (fix === "pair" || fix === "ps5") veyra.navigate("ps5")
            else if (fix === "reconnect") veyra.ps5ControlRestart()
            else if (fix === "capture") veyra.navigate("capture")
            else if (fix === "xbox") veyra.navigate("xbox")
        }
    }
}
