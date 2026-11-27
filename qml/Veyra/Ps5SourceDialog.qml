// ps5 dialog: source/settings logic is owned by this component.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

DialogLayer {
    id: ps5Dialog
    objectName: "ps5-dialog"
    key: "ps5"
    glyph: "gamepad"
    title: qsTr("PS5 串流")
    sub: qsTr("局域网 Remote Play · 凭据加密保存在本机")
    dialogWidth: 680
    actions: [
        { label: veyra.ps5.active ? qsTr("断开") : qsTr("取消") },
        { label: veyra.ps5.active ? qsTr("应用设置并重连") : qsTr("连接"), primary: true }
    ]
    onActionTriggered: label => {
        if (label === qsTr("断开")) { veyra.ps5Cancel(); return }
        if (label === qsTr("取消")) { if (veyra.ps5.busy) veyra.ps5Cancel(); else dialogHost.close(); return }
        if (veyra.ps5Connect()) { dialogHost.close(); dialogHost.startPs5() }
        else ps5Dialog.showStatus()
    }
    // PushFPS: whether the Account ID box holds something shaped like an ID (digits, or the 12 characters
    // ending in =). Only for how loud the Get Account ID button is; pairing itself decides what is valid.
    readonly property bool accountLooksRight: {
        const typed = String(veyra.ps5.account || "").replace(/\s/g, "")
        return /^[0-9]{6,20}$/.test(typed) || /^[A-Za-z0-9+\/]{11}=$/.test(typed)
    }
    // PushFPS: an answer that names a field to fix is at the top, with the fields: bring it into view.
    function showStatus() { ps5Dialog.bodyViewport.contentY = 0 }
    Connections {
        target: dialogHost
        function onDialogChanged() {
            if (dialogHost.dialog !== "ps5") return
            veyra.ps5Load()
            // PushFPS: with nothing paired and no address yet, look for the PS5 straight away (what Search
            // does), so the address is filled in before the first field is touched.
            if (veyra.ps5.available === true && veyra.ps5Profiles.length === 0 && (veyra.ps5.host || "").length === 0 && veyra.ps5.busy !== true)
                veyra.ps5Scan()
        }
    }
    readonly property var qualities: [{ id: "0", label: "720p · 30 fps" }, { id: "1", label: "720p · 60 fps" }, { id: "2", label: "1080p · 30 fps" }, { id: "3", label: "1080p · 60 fps" }]
    readonly property var bitrates: [5, 10, 15, 20, 30, 50, 80, 100].map((m, i) => ({ id: String(i), label: m + " Mbps" }))

    // PushFPS: what to do next, and what went wrong, stands first. At the bottom it was under the last row of a
    // long dialog: people pressed Connect, read the line there and never saw the fields it was about.
    DialogNote { objectName: "ps5-status"; visible: (veyra.ps5.status || "").length > 0; text: (veyra.ps5.busy ? qsTr("处理中 · ") : "") + (veyra.ps5.status || "") }
    DialogSection { text: qsTr("主机") }
    DialogGroup {
        VRow {
            visible: veyra.ps5Profiles.length > 0
            label: qsTr("已保存主机")
            VSelect {
                objectName: "ps5-profile"
                implicitWidth: 220
                value: { const p = veyra.ps5Profiles.find(x => x.id === veyra.ps5.profile); return p ? p.label : qsTr("请选择") }
                options: veyra.ps5Profiles
                onPicked: id => veyra.ps5SelectProfile(id)
            }
        }
        VRow {
            label: qsTr("主机地址")
            hint: qsTr("PS5 设置 → 网络 → 连接状态；找不到时可手填 IP")
            RowLayout {
                spacing: 6
                DialogTextField {
                    objectName: "ps5-host"; implicitWidth: 170; placeholder: "192.168.1.x"; text: veyra.ps5.host || ""
                    onEdited: text => veyra.ps5Set("host", text)
                    // PushFPS: also as typed. The buttons take no focus, so editingFinished alone never
                    // ran before 配对并保存, and the next refresh of the form wiped what was typed.
                    onTyped: text => { if (text === text.trim()) veyra.ps5Set("host", text) }
                }
                VSpinner { visible: veyra.ps5.busy === true; Layout.alignment: Qt.AlignVCenter }
                VButton { text: qsTr("查找"); ghost: true; enabled: !veyra.ps5.busy; onClicked: veyra.ps5Scan() }
                VButton { text: qsTr("唤醒"); ghost: true; enabled: !veyra.ps5.busy && veyra.ps5.profile.length > 0; onClicked: veyra.ps5Wake() }
            }
        }
    }
    DialogSection { text: qsTr("配对（首次）") }
    DialogGroup {
        VRow {
            label: "PSN Account ID"
            // PushFPS: in plain words, and the button stands out until the box holds something shaped like an ID.
            // People typed their PSN name here with the old hint right under the box and never pressed the button.
            // Since 4.4.6 the button opens Sony's own sign-in page (never a lookup website) and the row under this
            // one takes back the address the page lands on.
            hint: qsTr("点“获取 Account ID”，在索尼自己的页面登录，PushFPS 会替你填入。不是 PSN 昵称。不要在任何地方粘贴 NPSSO 码：它就是你的 PSN 登录。")
            hintBelow: true
            RowLayout {
                spacing: 6
                DialogTextField {
                    objectName: "ps5-account-id"; implicitWidth: 240; text: veyra.ps5.account || ""
                    placeholder: qsTr("数字 ID，不是昵称")
                    onEdited: text => veyra.ps5Set("account", text)
                    onTyped: text => { if (text === text.trim()) veyra.ps5Set("account", text) }
                }
                VButton {
                    objectName: "ps5-account-lookup"; text: qsTr("获取 Account ID")
                    primary: !ps5Dialog.accountLooksRight; ghost: ps5Dialog.accountLooksRight
                    enabled: veyra.ps5.signInAsking !== true
                    onClicked: veyra.ps5OpenAccountChecker()
                }
            }
        }
        VRow {
            objectName: "ps5-signin-row"
            visible: veyra.ps5.signInWaiting === true || veyra.ps5.signInAsking === true
            label: qsTr("登录后的地址")
            hint: qsTr("登录后浏览器停下的页面以 remoteplay.dl.playstation.net 开头，可能是空白或报错。把地址栏里的完整地址粘贴到这里。")
            hintBelow: true
            RowLayout {
                spacing: 6
                DialogTextField { id: signInPaste; objectName: "ps5-signin-paste"; implicitWidth: 240; placeholder: "https://remoteplay.dl.playstation.net/…" }
                VButton {
                    objectName: "ps5-signin-finish"; text: qsTr("填入"); primary: true
                    enabled: veyra.ps5.signInAsking !== true
                    // The pasted address holds a one-time code: it leaves the box as soon as it is handed over.
                    onClicked: { const pasted = signInPaste.text; signInPaste.text = ""; veyra.ps5FinishSignIn(pasted); ps5Dialog.showStatus() }
                }
            }
        }
        VRow {
            label: qsTr("8 位配对码")
            hint: qsTr("PS5 设置 → 系统 → 远程游玩 → 配对设备（旧版系统软件叫“关联设备”）")
            RowLayout {
                spacing: 6
                // PushFPS: the number stays in its field when pairing did not start (an address or an account ID
                // to fix first): it is good for five minutes, and typing it again cost people the pairing.
                DialogTextField { id: pairPin; objectName: "ps5-pair-code"; implicitWidth: 120; placeholder: "1234 5678" }
                VButton { objectName: "ps5-pair"; text: qsTr("配对并保存"); enabled: !veyra.ps5.busy; onClicked: { veyra.ps5Pair(pairPin.text); if (veyra.ps5.busy === true) pairPin.text = ""; else ps5Dialog.showStatus() } }
            }
        }
        VRow {
            label: qsTr("删除配对")
            hint: qsTr("只删除本机保存的这台主机；以后需要重新配对")
            VButton { text: qsTr("删除"); ghost: true; enabled: !veyra.ps5.busy && veyra.ps5.profile.length > 0; onClicked: veyra.ps5Forget() }
        }
    }

    DialogSection { text: qsTr("串流（重连生效）") }
    DialogGroup {
        VRow {
            label: qsTr("分辨率与帧率")
            VSelect { implicitWidth: 170; value: ps5Dialog.qualities[veyra.ps5.quality || 0].label; options: ps5Dialog.qualities; onPicked: id => veyra.ps5Set("quality", Number(id)) }
        }
        VRow {
            label: qsTr("编码")
            hint: qsTr("H.265 在相同码率下画面更清晰（推荐）；HDR 需要 PS5 实际输出 HDR 且 Windows HDR 开启")
            VSeg {
                options: [{ id: "0", label: "H.264" }, { id: "1", label: "H.265" }, { id: "2", label: "H.265 HDR" }]
                current: String(veyra.ps5.codec || 0)
                onPicked: id => veyra.ps5Set("codec", Number(id))
            }
        }
        VRow {
            label: qsTr("请求码率")
            hint: qsTr("发给 PS5 的带宽请求，主机不保证达到。有线网络下 30 Mbps 或更高会更清晰；画面卡顿就调低")
            VSelect { implicitWidth: 130; value: ps5Dialog.bitrates[veyra.ps5.bitrate || 0].label; options: ps5Dialog.bitrates; onPicked: id => veyra.ps5Set("bitrate", Number(id)) }
        }
        VRow {
            label: qsTr("解码")
            VSeg {
                options: [{ id: "0", label: qsTr("自动") }, { id: "1", label: qsTr("CPU 软件") }, { id: "2", label: qsTr("D3D12VA 硬件") }]
                current: String(veyra.ps5.decode || 0)
                onPicked: id => veyra.ps5Set("decode", Number(id))
            }
        }
        VRow {
            label: qsTr("采样")
            hint: qsTr("精细：按位置还原色度，放大用双三次；不是 AI 超分")
            VSeg {
                options: [{ id: "0", label: qsTr("兼容") }, { id: "1", label: qsTr("精细") }]
                current: String(veyra.ps5.sampling === undefined ? 1 : veyra.ps5.sampling)
                onPicked: id => veyra.ps5Set("sampling", Number(id))
            }
        }
        VRow {
            label: qsTr("仅观看")
            hint: qsTr("不向 PS5 转发电脑手柄输入")
            VSwitch { checked: veyra.ps5.viewOnly === true; onToggled: checked => veyra.ps5Set("viewOnly", checked) }
        }
        // PushFPS voice chat: this PC's microphone in the PS5's voice chat (party chat, game chat).
        VRow {
            objectName: "ps5-mic-row"
            label: qsTr("电脑麦克风")
            hint: veyra.ps5.mic === true && veyra.ps5.active === true
                  ? (veyra.ps5.micLive === true ? qsTr("PS5 正在收到这台电脑的麦克风。") : qsTr("已静音，或等待 PS5 接受麦克风。"))
                    + " " + ((veyra.shortcuts || {}).micMute ? qsTr("按 %1 静音或打开。").arg(veyra.shortcuts.micMute) : qsTr("可在 设置 → 快捷键 设定静音键。"))
                  : qsTr("把这台电脑的麦克风接入 PS5 的语音聊天（派对、游戏内聊天）。用 设置 → 录制 里选的麦克风；下次连接时生效。")
            VSwitch {
                objectName: "ps5-mic"
                enabled: veyra.ps5.viewOnly !== true
                opacity: enabled ? 1 : 0.35
                checked: veyra.ps5.mic === true
                onToggled: checked => veyra.ps5Set("mic", checked)
            }
        }
        // The mirror of 仅观看: only the pad, the picture from a capture card.
        VRow {
            visible: veyra.ps5Control.available === true
            label: qsTr("画面用采集卡")
            hint: veyra.ps5Control.enabled === true
                  ? qsTr("已在「采集卡」里打开 PS5 手柄控制：连接采集卡时只用远程游玩传手柄")
                  : qsTr("PS5 的 HDMI 接了采集卡时，可以只用远程游玩传手柄、画面用采集卡原画（可 4K/HDR），也方便再串流给掌机")
            VButton { objectName: "ps5-open-capture"; text: qsTr("采集卡设置"); ghost: true; onClicked: dialogHost.open("capture") }
        }
        VRow {
            label: qsTr("陀螺仪")
            hint: veyra.ps5.controller ? (veyra.ps5.gyro ? qsTr("手柄支持陀螺仪") : qsTr("手柄没有陀螺仪")) : qsTr("连接串流及电脑手柄后可校准")
            VButton { text: veyra.ps5.calibrating ? qsTr("校准中…") : qsTr("校准"); ghost: true; onClicked: veyra.ps5Calibrate() }
        }
    }
    DialogSection { text: qsTr("登录 PIN") }
    DialogGroup {
        VRow {
            label: qsTr("PS5 提示时填写")
            hint: qsTr("不是 8 位配对码")
            RowLayout {
                spacing: 6
                DialogTextField { id: loginPin; implicitWidth: 120 }
                VButton { text: qsTr("提交"); ghost: true; onClicked: { veyra.ps5SendLoginPin(loginPin.text); loginPin.text = "" } }
            }
        }
    }
}
