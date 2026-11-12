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
    // Console games send 30 FPS content inside a 60 Hz signal. The multiplier is
    // chosen from this; the output cap then holds the result at the target, so a
    // 40 or 60 FPS game still lands on the chosen rate.
    readonly property int baseFps: 30

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
    function multiplierFor(fps) {
        const offered = (veyra.fgMultiplierChoices || []).map(o => parseInt(o.id))
            .filter(m => m >= 2).sort((a, b) => a - b)
        if (offered.length === 0) return 0
        let pick = offered[0]
        for (let i = 0; i < offered.length; ++i)
            if (quick.baseFps * offered[i] <= fps + 0.5) pick = offered[i]
        return pick
    }

    function applyTarget(id) {
        if (id === "off") { veyra.fgEnabled = false; return }
        const fps = targetFps(id)
        const m = multiplierFor(fps)
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

    readonly property string targetHint: {
        if (!quick.fgPossible) return Support.status(veyra, "frame-generation").reason || ""
        if (quick.currentTarget === "") return qsTr("已在专业模式中手动设置")
        if (quick.currentTarget === "off") return qsTr("主机游戏保持原有帧率")
        const m = veyra.fgMultiplier
        const reach = quick.baseFps * m
        const want = quick.targetFps(quick.currentTarget)
        if (reach + 0.5 < want)
            return qsTr("当前补帧方式最高 %1×，30 FPS 游戏约为 %2 FPS").arg(m).arg(reach)
        return qsTr("30 FPS 游戏将以 %1 FPS 运行（%2× 补帧）").arg(Math.round(Math.min(reach, want))).arg(m)
    }

    Text {
        visible: !quick.compact
        Layout.fillWidth: true
        text: qsTr("为主机游戏选好帧率和画面，其他细节在专业模式里")
        color: Theme.t3
        font.family: Theme.fontUi
        font.pixelSize: Theme.fsBody
        wrapMode: Text.WordWrap
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
            options: [{ id: "off", label: qsTr("关闭", "off") }, { id: "60", label: "60 FPS" },
                      { id: "120", label: "120 FPS" }, { id: "monitor", label: qsTr("跟随显示器") }]
            current: quick.currentTarget
            onPicked: id => quick.applyTarget(id)
        }
    }

    VRow {
        objectName: "pushfps-enhance-row"
        label: qsTr("AI 画面增强")
        hint: quick.nrPossible ? qsTr("关闭时只做补帧。开启后每帧都经过 NVIDIA 神经渲染（DLSS 5 NR）增强，画面更清晰，但更耗 GPU")
                               : (Support.status(veyra, "nr").reason || "")
        hintBelow: true
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
        hintBelow: true
        VSwitch {
            objectName: "pushfps-low-latency"
            enabled: veyra.nvidiaLowLatencyAvailable === true
            opacity: enabled ? 1 : 0.4
            checked: veyra.nvidiaLowLatencyAvailable === true && (veyra.nvidiaLowLatency ?? 0) > 0
            onToggled: checked => veyra.nvidiaLowLatency = checked ? 1 : 0
        }
    }
}
