// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PS5 pairing form: an Account ID and host typed and then 配对并保存 clicked straight away (the button
// takes no focus, so no editingFinished) must reach ps5Pair and stay in the fields afterwards.
import QtQuick
import QtTest
import Veyra
import VeyraTest 1.0

Item {
    id: fixture
    width: 900; height: 1500
    QtObject {
        id: backend
        property var ps5: ({host:"",account:"",profile:"",busy:false,active:false,quality:3,codec:0,bitrate:2,decode:0,sampling:1,status:""})
        property var ps5Profiles: []
        property var ps5Control: ({available:false,enabled:false})
        property string pairedAccount: ""
        property string pairedHost: ""
        property string pairedPin: ""
        property int scans: 0
        property bool pairingStarts: false
        function ps5Load() {}
        function ps5Scan() { scans += 1 }
        property int signIns: 0
        property string finished: ""
        // As the bridge: Sony's page opened, an address is expected.
        function ps5OpenAccountChecker() { signIns += 1; const next=Object.assign({},ps5); next.signInWaiting=true; ps5=next }
        function ps5FinishSignIn(pasted) { finished = pasted; const next=Object.assign({},ps5); next.signInAsking=true; ps5=next }
        property var setCalls: []
        function ps5Set(key,value) { setCalls = setCalls.concat([[key,value]]); const next=Object.assign({},ps5);next[key]=String(value).trim();ps5=next;return true }
        // As the bridge: reads the form, then a status refresh re-emits the form.
        function ps5Pair(pin) {
            pairedAccount=ps5.account; pairedHost=ps5.host; pairedPin=pin
            // The bridge is busy from the moment a pairing really starts; a field to fix first leaves it idle.
            const next=Object.assign({},ps5); next.status="paired"; next.busy=pairingStarts; ps5=next
        }
    }
    Item {
        id: host
        anchors.fill: parent
        property string dialog: "ps5"
        property real motionScale: 1
        function close() {}
        function open(key) { dialog=key }
        // Something behind the dialog (a shown dialog is this item's last child), as the tool tiles are.
        Item { id: behind; anchors.fill: parent; HoverHandler { id: behindHover } }
    }
    Ps5SourceDialog { id: panel; property var veyra: backend; dialogHost: host }
    TestCase {
        name: "PushPs5Pairing"
        when: windowShown
        function typeInto(field, value) {
            const input = field.children[0]
            mouseClick(field)
            for (const ch of value) keyClick(ch)
            verify(input.text === value, input.text)
        }
        // PushFPS voice chat: the PC microphone switch is the form's "mic", off in a view-only stream, and
        // while a stream plays its hint says whether the PS5 hears the microphone.
        function test_pc_microphone_switch_follows_the_form_and_view_only() {
            const row = findChild(panel, "ps5-mic-row")
            const sw = findChild(panel, "ps5-mic")
            verify(row !== null && sw !== null)
            backend.ps5 = Object.assign({}, backend.ps5, {mic: false, viewOnly: false, active: false})
            verify(!sw.checked)
            verify(sw.enabled)
            backend.setCalls = []
            sw.toggled(true)
            compare(backend.setCalls.length, 1)
            compare(backend.setCalls[0][0], "mic")
            compare(backend.setCalls[0][1], true)
            backend.ps5 = Object.assign({}, backend.ps5, {mic: true, viewOnly: false})
            verify(sw.checked)
            backend.ps5 = Object.assign({}, backend.ps5, {viewOnly: true})
            verify(!sw.enabled, "a view-only stream sends nothing to the PS5")
            backend.ps5 = Object.assign({}, backend.ps5, {viewOnly: false, mic: true, active: true, micLive: true})
            verify(row.hint.indexOf(qsTr("PS5 正在收到这台电脑的麦克风。")) === 0, row.hint)
            backend.ps5 = Object.assign({}, backend.ps5, {micLive: false})
            verify(row.hint.indexOf(qsTr("已静音，或等待 PS5 接受麦克风。")) === 0, row.hint)
            backend.ps5 = Object.assign({}, backend.ps5, {mic: false, active: false, micLive: false})
        }
        function test_typed_values_reach_pairing_and_stay() {
            const account = findChild(panel, "ps5-account-id")
            const hostField = findChild(panel, "ps5-host")
            verify(account && hostField)
            typeInto(hostField, "192.168.1.20")
            typeInto(account, "1234567890")
            compare(backend.ps5.host, "192.168.1.20")
            compare(backend.ps5.account, "1234567890")
            // 配对并保存 takes no focus; the status refresh after it must not wipe the fields.
            typeInto(findChild(panel, "ps5-pair").parent.children[0], "12345678")
            mouseClick(findChild(panel, "ps5-pair"))
            compare(backend.pairedPin, "12345678")
            compare(backend.pairedAccount, "1234567890")
            compare(backend.pairedHost, "192.168.1.20")
            compare(account.text, "1234567890")
            compare(hostField.text, "192.168.1.20")
        }
        // The number is good for five minutes: when pairing did not start (an address or an account ID to fix
        // first) it stays in its field, and it is cleared once a pairing really runs.
        function test_the_number_stays_when_pairing_did_not_start() {
            const code = findChild(panel, "ps5-pair-code")
            verify(code)
            backend.pairingStarts = false
            backend.ps5 = Object.assign({}, backend.ps5, {busy: false})
            code.text = ""
            typeInto(code, "1234 5678")
            mouseClick(findChild(panel, "ps5-pair"))
            compare(backend.pairedPin, "1234 5678")
            compare(code.text, "1234 5678")
            backend.pairingStarts = true
            mouseClick(findChild(panel, "ps5-pair"))
            compare(code.text, "")
            backend.pairingStarts = false
            backend.ps5 = Object.assign({}, backend.ps5, {busy: false})
        }
        // A first visit (nothing paired, no address) looks for the PS5 by itself, as Search does; a saved
        // pairing, an address already there or a version without Remote Play does not.
        function test_a_first_visit_looks_for_the_ps5() {
            function reopen() { host.dialog = ""; host.dialog = "ps5" }
            backend.scans = 0
            backend.ps5 = Object.assign({}, backend.ps5, {host: "", busy: false, available: true})
            backend.ps5Profiles = []
            reopen()
            compare(backend.scans, 1)
            backend.ps5 = Object.assign({}, backend.ps5, {host: "192.168.1.20"})
            reopen()
            compare(backend.scans, 1)
            backend.ps5 = Object.assign({}, backend.ps5, {host: ""})
            backend.ps5Profiles = [{id: "a.dat", label: "192.168.1.20"}]
            reopen()
            compare(backend.scans, 1)
            backend.ps5Profiles = []
            backend.ps5 = Object.assign({}, backend.ps5, {busy: true})
            reopen()
            compare(backend.scans, 1)
            backend.ps5 = Object.assign({}, backend.ps5, {busy: false, available: false})
            reopen()
            compare(backend.scans, 1)
            backend.ps5 = Object.assign({}, backend.ps5, {available: true})
            reopen()
            compare(backend.scans, 2)
            wait(700)   // the dialog's own way in, before the next test clicks in it
        }
        // An answer that names a field to fix stands at the top with the fields, and is brought into view.
        function test_the_status_line_stands_first_and_is_shown() {
            const note = findChild(panel, "ps5-status")
            const hostField = findChild(panel, "ps5-host")
            verify(note && hostField)
            backend.ps5 = Object.assign({}, backend.ps5, {status: "fix this field", busy: false})
            tryVerify(function() { return note.visible }, 3000)
            verify(note.mapToItem(panel, 0, 0).y < hostField.mapToItem(panel, 0, 0).y)
            panel.bodyViewport.contentY = Math.max(0, panel.bodyViewport.contentHeight - panel.bodyViewport.height)
            panel.showStatus()
            compare(panel.bodyViewport.contentY, 0)
        }
        // Get Account ID opens Sony's own sign-in; the address the page lands on is pasted into a row that is
        // only there while it is expected, handed over once, and never left in its box.
        function test_sign_in_with_playstation_fills_the_account_id() {
            const row = findChild(panel, "ps5-signin-row")
            verify(row)
            backend.ps5 = Object.assign({}, backend.ps5, {signInWaiting: false, signInAsking: false})
            tryVerify(function() { return !row.visible }, 3000)
            mouseClick(findChild(panel, "ps5-account-lookup"))
            compare(backend.signIns, 1)
            tryVerify(function() { return row.visible }, 3000)
            const paste = findChild(panel, "ps5-signin-paste")
            paste.text = "https://remoteplay.dl.playstation.net/remoteplay/redirect?code=v3.abc"
            mouseClick(findChild(panel, "ps5-signin-finish"))
            compare(backend.finished, "https://remoteplay.dl.playstation.net/remoteplay/redirect?code=v3.abc")
            compare(paste.text, "")
            // While Sony is asked, neither button starts another exchange.
            verify(!findChild(panel, "ps5-signin-finish").enabled)
            verify(!findChild(panel, "ps5-account-lookup").enabled)
            backend.ps5 = Object.assign({}, backend.ps5, {signInWaiting: false, signInAsking: false, account: ""})
            tryVerify(function() { return !row.visible }, 3000)
        }

        // The way to the Account ID stands out until the box holds something shaped like one: a PSN name is not.
        function test_get_account_id_stands_out_until_an_id_is_there() {
            const lookup = findChild(panel, "ps5-account-lookup")
            verify(lookup)
            function account(text) { backend.ps5 = Object.assign({}, backend.ps5, {account: text}) }
            account("")
            verify(lookup.primary && !lookup.ghost)
            account("SomePlayers")
            verify(lookup.primary && !lookup.ghost)
            account("Player_One-99")
            verify(lookup.primary)
            account("1234567890123456789")
            verify(!lookup.primary && lookup.ghost)
            account(" 1234 5678 9012 3456 789 ")
            verify(!lookup.primary)
            account("AAAAAAAAAAA=")
            verify(!lookup.primary && lookup.ghost)
            account("AAAAAAAAAAA")
            verify(lookup.primary)
            account("")
        }
        // What lies behind a dialog is not under the pointer: a tool tile's tooltip came up over these fields.
        function test_the_page_behind_the_dialog_is_not_hovered() {
            tryVerify(function() { return panel.visible && panel.width > 100 }, 3000)
            mouseMove(fixture, 4, 4)
            tryVerify(function() { return behindHover.hovered }, 3000)
            mouseMove(panel, panel.width / 2, 30)
            tryVerify(function() { return !behindHover.hovered }, 3000)
            mouseMove(panel, panel.width / 2, panel.height - 20)
            wait(60)
            verify(!behindHover.hovered)
        }
    }
}
