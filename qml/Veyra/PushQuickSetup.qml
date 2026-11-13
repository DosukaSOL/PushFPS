import QtQuick
import QtQuick.Layouts
import "EffectSupport.js" as Support

// PushFPS quick setup: the choices most console players make, above the detailed
// controls. Every control here only sets existing settings (frame generation on/off,
// multiplier, output cap, NR layers, NVIDIA low latency), so Professional mode always
// shows exactly what was chosen and nothing here adds engine state.
VGroup {
    id: quick
    objectName: "pushfps-quick-setup"
    // Home shows the short form: no low-latency row and no intro line.
    property bool compact: false
    // Home only: the first-run setup tip, until a capture card has been used once.
    property bool showSetupTip: false
    // Console games send their frames inside a 60 Hz signal: 30 FPS for most games
    // (GTA VI), 40 in some 120 Hz modes, 60 in performance modes. The player picks it
    // (设置 key pushGameFps); the multiplier is chosen from it, and the output cap holds
    // the result at the target.
    readonly property int baseFps: {
        const n = Number(veyra.preferences.pushGameFps || 30)
        return n === 40 || n === 60 ? n : 30
    }
    function setGameFps(id) {
        const n = parseInt(id)
        if (!veyra.setPreference("pushGameFps", n)) return
        // Keep the chosen target: the multiplier is worked out again for the new rate.
        // A target the game already reaches (60 FPS for a 60 FPS game) has nothing to
        // generate, so frame generation goes off instead of doubling past it.
        const t = quick.currentTarget
        if (t === "60" || t === "120" || t === "monitor") {
            if (quick.targetFps(t) <= n + 0.5) veyra.fgEnabled = false
            else quick.applyTarget(t, n)
        }
    }

    readonly property bool fgPossible: Support.available(veyra, "frame-generation")
    readonly property bool nrPossible: Support.available(veyra, "nr")

    // Which quick choice matches the current settings; "" when Professional mode
    // set something else (a custom cap, or no cap at all).
    readonly property string currentTarget: {
        if (!veyra.fgEnabled) return "off"
        if (veyra.outputRateMode === 1) return "monitor"
        if (veyra.outputRateMode === 2 && Math.abs(veyra.outputCustomFps - 60) < 0.5) return "60"
        if (veyra.outputRateMode === 2 && Math.abs(veyra.outputCustomFps - 120) < 0.5) return "120"
        return ""
    }

    function targetFps(id) {
        if (id === "60") return 60
        if (id === "120") return 120
        if (id === "monitor") return veyra.displayRefreshHz > 0 ? veyra.displayRefreshHz : 60
        return 0
    }

    // The largest multiplier the current backend offers whose output stays at or
    // under the target; the smallest offered one when even that overshoots.
    function multiplierFor(fps, base) {
        const gameFps = base || quick.baseFps
        const offered = (veyra.fgMultiplierChoices || []).map(o => parseInt(o.id))
            .filter(m => m >= 2).sort((a, b) => a - b)
        if (offered.length === 0) return 0
        let pick = offered[0]
        for (let i = 0; i < offered.length; ++i)
            if (gameFps * offered[i] <= fps + 0.5) pick = offered[i]
        return pick
    }

    // base: the game's frame rate; defaults to the saved one (a profile passes its own,
    // since the preference it just wrote may not have reached the binding yet).
    function applyTarget(id, base) {
        const gameFps = base || quick.baseFps
        if (id === "off") { veyra.fgEnabled = false; return }
        const fps = targetFps(id)
        if (fps <= gameFps + 0.5) { veyra.fgEnabled = false; return }
        const m = multiplierFor(fps, gameFps)
        if (m < 2) return
        veyra.fgMultiplier = m
        // XeSS paces its own presentation (presentationOwned); the cap is then
        // greyed in Professional mode too, and the multiplier alone decides.
        if (!veyra.presentationOwned) {
            if (id === "monitor") veyra.outputRateMode = 1
            else { veyra.outputCustomFps = fps; veyra.outputRateMode = 2 }
        }
    }

    function setEnhancement(on) {
        if (on) {
            if (veyra.nrLayers.length === 0) veyra.addEffect("nr")
            else veyra.setAllNrEnabled(true)
        } else if (veyra.nrLayers.length > 0) {
            veyra.setAllNrEnabled(false)
        }
    }

    // --- console and game profiles ---------------------------------------------
    // A console profile remembers the quick choices per console: switching console
    // keeps what was set for the old one and brings back the new one's. A game
    // profile is a named snapshot (console, game frame rate, target, enhancement, low
    // latency) plus, when the current Professional settings are a saved preset, that
    // preset's name, which is applied first. Stored as JSON in the pushProfiles
    // preference (validated by the bridge); nothing here adds engine state.
    readonly property var consoles: [
        { id: "ps5", label: "PlayStation 5" }, { id: "ps5pro", label: "PS5 Pro" },
        { id: "xbox", label: "Xbox Series X|S" }, { id: "switch2", label: "Nintendo Switch 2" },
        { id: "other", label: qsTr("其他主机") }
    ]
    readonly property string consoleId: {
        const c = String(veyra.preferences.pushConsole || "ps5")
        return consoles.some(x => x.id === c) ? c : "ps5"
    }
    readonly property var profiles: {
        try {
            const p = JSON.parse(veyra.preferences.pushProfiles || "{}")
            return p && typeof p === "object" && !Array.isArray(p) ? p : ({})
        } catch (e) { return ({}) }
    }
    readonly property var games: Array.isArray(profiles.games) ? profiles.games.filter(g => g && typeof g.name === "string") : []
    readonly property string activeGame: typeof profiles.game === "string" && games.some(g => g.name === profiles.game) ? profiles.game : ""
    readonly property int maxGames: 50

    function snapshot() {
        return { fps: quick.baseFps, target: quick.currentTarget, enhance: veyra.nrAnyEnabled === true,
                 lowLatency: veyra.nvidiaLowLatencyAvailable === true && (veyra.nvidiaLowLatency ?? 0) > 0 }
    }
    function writeProfiles(p) {
        p.v = 1
        return veyra.setPreference("pushProfiles", JSON.stringify(p))
    }
    function copyProfiles() { return JSON.parse(JSON.stringify(quick.profiles)) }
    function applySnapshot(s) {
        if (!s) return
        const fps = s.fps === 40 || s.fps === 60 ? s.fps : 30
        veyra.setPreference("pushGameFps", fps)
        if (quick.fgPossible && ["off", "60", "120", "monitor"].indexOf(s.target) >= 0)
            quick.applyTarget(s.target, fps)
        if (quick.nrPossible && typeof s.enhance === "boolean" && s.enhance !== (veyra.nrAnyEnabled === true))
            quick.setEnhancement(s.enhance)
        if (veyra.nvidiaLowLatencyAvailable === true && typeof s.lowLatency === "boolean")
            veyra.nvidiaLowLatency = s.lowLatency ? 1 : 0
    }
    function switchConsole(id) {
        if (id === quick.consoleId) return
        const p = copyProfiles()
        if (!p.consoles || typeof p.consoles !== "object") p.consoles = {}
        p.consoles[quick.consoleId] = snapshot()
        p.game = ""
        if (!writeProfiles(p)) return
        if (!veyra.setPreference("pushConsole", id)) return
        applySnapshot(p.consoles[id])
    }
    function saveGame(name) {
        name = String(name || "").trim().slice(0, 40)
        if (name.length === 0) return false
        const p = copyProfiles()
        const list = Array.isArray(p.games) ? p.games.filter(g => g && g.name !== name) : []
        if (list.length >= quick.maxGames) return false
        const presetName = veyra.currentPresetName || ""
        const g = snapshot()
        g.name = name
        g.console = quick.consoleId
        g.preset = (veyra.presets || []).some(x => x.name === presetName) ? presetName : ""
        list.push(g)
        list.sort((a, b) => a.name.localeCompare(b.name))
        p.games = list
        p.game = name
        return writeProfiles(p)
    }
    function applyGame(name) {
        const g = quick.games.find(x => x.name === name)
        const p = copyProfiles()
        p.game = g ? name : ""
        if (!g) { writeProfiles(p); return }
        if (g.preset) {
            const preset = (veyra.presets || []).find(x => x.name === g.preset)
            if (preset) veyra.applyPresetIndex(preset.index)
        }
        if (quick.consoles.some(c => c.id === g.console)) veyra.setPreference("pushConsole", g.console)
        applySnapshot(g)
        writeProfiles(p)
    }
    function deleteGame(name) {
        const p = copyProfiles()
        p.games = (Array.isArray(p.games) ? p.games : []).filter(g => g && g.name !== name)
        if (p.game === name) p.game = ""
        writeProfiles(p)
    }
    readonly property string setupTip: {
        switch (quick.consoleId) {
        case "xbox": return qsTr("Xbox 请在 设置 → 常规 → 电视和显示选项 中选择 60 Hz。游戏没有 HDCP，可以直接采集。")
        case "switch2": return qsTr("Switch 2 请放在底座上，输出 60 Hz。游戏没有 HDCP，可以直接采集。")
        case "other": return qsTr("主机请输出 60 Hz，并关闭 HDCP（如有此选项），否则采集画面可能为黑屏。")
        default: return qsTr("主机请输出 60 Hz。PlayStation 需在 设置 → 系统 → HDMI 中关闭 HDCP，否则采集画面为黑屏。")
        }
    }

    readonly property string targetHint: {
        if (!quick.fgPossible) return Support.status(veyra, "frame-generation").reason || ""
        if (quick.currentTarget === "") return qsTr("已在专业模式中手动设置")
        if (quick.currentTarget === "off") return qsTr("主机游戏保持原有帧率")
        const m = veyra.fgMultiplier
        const reach = quick.baseFps * m
        const want = quick.targetFps(quick.currentTarget)
        if (reach + 0.5 < want)
            return qsTr("当前补帧方式最高 %1×，%2 FPS 游戏约为 %3 FPS").arg(m).arg(quick.baseFps).arg(reach)
        return qsTr("%1 FPS 游戏将以 %2 FPS 运行（%3× 补帧）").arg(quick.baseFps)
            .arg(Math.round(Math.min(reach, want))).arg(m)
    }

    // Header: what this card is, and the one-line explanation in the full form.
    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: 12
        Layout.bottomMargin: 2
        spacing: 3
        Text {
            text: qsTr("快速设置")
            color: Theme.t1
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsH3
            font.weight: Font.DemiBold; font.variableAxes: Theme.axesDemiBold
        }
        Text {
            visible: !quick.compact
            Layout.fillWidth: true
            text: qsTr("为主机游戏选好帧率和画面，其他细节在专业模式里")
            color: Theme.t3
            font.family: Theme.fontUi
            font.pixelSize: Theme.fsHint
            wrapMode: Text.WordWrap
        }
    }

    // First-run tip, as a quiet info strip rather than a paragraph above the rows.
    Rectangle {
        objectName: "pushfps-setup-tip"
        visible: quick.showSetupTip && !veyra.hasCaptureSession
        Layout.fillWidth: true
        Layout.topMargin: 8
        Layout.bottomMargin: 2
        implicitHeight: tipRow.implicitHeight + 16
        radius: 9
        color: Qt.rgba(1, 1, 1, 0.035)
        border.width: 1
        border.color: Theme.stroke
        RowLayout {
            id: tipRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 9
            VIcon { Layout.alignment: Qt.AlignTop; name: "info"; size: 14; color: Theme.accent }
            Text {
                Layout.fillWidth: true
                text: quick.setupTip
                color: Theme.t2
                font.family: Theme.fontUi
                font.pixelSize: Theme.fsHint
                lineHeight: 1.1
                wrapMode: Text.WordWrap
            }
        }
    }

    VRow {
        objectName: "pushfps-console-row"
        label: qsTr("游戏主机")
        VSelect {
            objectName: "pushfps-console"
            implicitWidth: 190
            options: quick.consoles
            value: (quick.consoles.find(c => c.id === quick.consoleId) || quick.consoles[0]).label
            onPicked: id => quick.switchConsole(id)
        }
    }

    VRow {
        id: gameRow
        objectName: "pushfps-game-row"
        label: qsTr("游戏配置")
        hint: quick.activeGame.length > 0 ? "" : qsTr("把当前设置存为某个游戏的配置，之后一键切换")
        property bool naming: false
        RowLayout {
            spacing: 4
            VSelect {
                objectName: "pushfps-game"
                visible: !gameRow.naming
                implicitWidth: 150
                options: [{ id: "", label: qsTr("无") }].concat(quick.games.map(g => ({ id: g.name, label: g.name })))
                value: quick.activeGame.length > 0 ? quick.activeGame : qsTr("无")
                onPicked: id => quick.applyGame(id)
            }
            DialogTextField {
                id: gameName
                objectName: "pushfps-game-name"
                visible: gameRow.naming
                implicitWidth: 150
                placeholder: qsTr("游戏名称")
            }
            VButton {
                objectName: "pushfps-game-save"
                icon: true
                iconName: gameRow.naming ? "check" : "plus"
                tip: gameRow.naming ? qsTr("保存") : (quick.activeGame.length > 0 ? qsTr("更新或另存为游戏配置") : qsTr("存为游戏配置"))
                enabled: gameRow.naming ? gameName.text.trim().length > 0 : quick.games.length < quick.maxGames || quick.activeGame.length > 0
                onClicked: {
                    if (gameRow.naming) { if (quick.saveGame(gameName.text)) gameRow.naming = false; return }
                    gameName.text = quick.activeGame
                    gameRow.naming = true
                }
            }
            VButton {
                objectName: "pushfps-game-delete"
                icon: true
                iconName: gameRow.naming ? "x" : "trash"
                tip: gameRow.naming ? qsTr("取消") : qsTr("删除此游戏配置")
                visible: gameRow.naming || quick.activeGame.length > 0
                onClicked: {
                    if (gameRow.naming) { gameRow.naming = false; return }
                    quick.deleteGame(quick.activeGame)
                }
            }
        }
    }

    VRow {
        objectName: "pushfps-game-fps-row"
        label: qsTr("游戏帧率")
        hint: qsTr("大多数主机游戏为 30 FPS；性能模式选 60")
        VSeg {
            objectName: "pushfps-game-fps"
            enabled: quick.fgPossible
            opacity: enabled ? 1 : 0.4
            options: [{ id: "30", label: "30" }, { id: "40", label: "40" }, { id: "60", label: "60" }]
            current: String(quick.baseFps)
            onPicked: id => quick.setGameFps(id)
        }
    }

    VRow {
        objectName: "pushfps-target-row"
        label: qsTr("目标帧率")
        hint: quick.targetHint
        hintBelow: true
        hintColor: quick.fgPossible ? Theme.t3 : Theme.warn
        VSeg {
            objectName: "pushfps-target"
            enabled: quick.fgPossible
            opacity: enabled ? 1 : 0.4
            // 60 FPS is only a target when the game runs below it.
            options: quick.baseFps < 60
                ? [{ id: "off", label: qsTr("关闭", "off") }, { id: "60", label: "60 FPS" },
                   { id: "120", label: "120 FPS" }, { id: "monitor", label: qsTr("匹配显示器") }]
                : [{ id: "off", label: qsTr("关闭", "off") },
                   { id: "120", label: "120 FPS" }, { id: "monitor", label: qsTr("匹配显示器") }]
            current: quick.currentTarget
            onPicked: id => quick.applyTarget(id)
        }
    }

    VRow {
        objectName: "pushfps-enhance-row"
        label: qsTr("AI 画面增强")
        hint: quick.nrPossible ? qsTr("关闭时只做补帧。开启后每帧都经过 NVIDIA 神经渲染（DLSS 5 NR）增强，画面更清晰，但更耗 GPU")
                               : (Support.status(veyra, "nr").reason || "")
        VSwitch {
            objectName: "pushfps-enhance"
            enabled: quick.nrPossible
            opacity: enabled ? 1 : 0.4
            checked: veyra.nrAnyEnabled
            onToggled: checked => quick.setEnhancement(checked)
        }
    }

    VRow {
        objectName: "pushfps-low-latency-row"
        visible: !quick.compact
        label: qsTr("英伟达低延迟")
        hint: veyra.nvidiaLowLatencyAvailable === true ? qsTr("减少处理排队，降低输入延迟") : qsTr("需要 NVIDIA 显卡")
        VSwitch {
            objectName: "pushfps-low-latency"
            enabled: veyra.nvidiaLowLatencyAvailable === true
            opacity: enabled ? 1 : 0.4
            checked: veyra.nvidiaLowLatencyAvailable === true && (veyra.nvidiaLowLatency ?? 0) > 0
            onToggled: checked => veyra.nvidiaLowLatency = checked ? 1 : 0
        }
    }
}
