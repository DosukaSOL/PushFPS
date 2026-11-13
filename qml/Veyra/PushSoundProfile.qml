// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS sound profiles (veyra::sink::SoundProcessor): a preset, the custom
// 5-band EQ and a live preview of the filter curve. Stored in the soundPreset
// and soundEq preferences; the bridge applies them to every audio renderer,
// before the recording tap, so recordings keep the profile too.
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: sound
    property bool compact: false
    spacing: 0

    readonly property var presets: [
        { id: "0", label: qsTr("关闭", "off"), hint: qsTr("原声输出") },
        { id: "1", label: qsTr("低音增强"), hint: qsTr("更饱满的低频，适合动作场面") },
        { id: "2", label: qsTr("人声清晰"), hint: qsTr("对白和语音更清楚，去掉低频轰鸣") },
        { id: "3", label: qsTr("脚步声"), hint: qsTr("突出脚步和枪声等中高频，适合竞技游戏") },
        { id: "4", label: qsTr("夜间模式"), hint: qsTr("压缩响亮和安静之间的差距，深夜不吵人") },
        { id: "5", label: qsTr("影院"), hint: qsTr("更宽的低频和高频，轻度压缩") },
        { id: "6", label: qsTr("自定义均衡器"), hint: qsTr("五段均衡器，每段 ±12 dB") }
    ]
    readonly property int preset: {
        const n = Number(veyra.preferences.soundPreset || 0)
        return n >= 0 && n < presets.length ? n : 0
    }
    readonly property var bandLabels: ["60 Hz", "230 Hz", "910 Hz", "3.6 kHz", "14 kHz"]
    readonly property var eq: {
        const parts = String(veyra.preferences.soundEq || "").split(",").map(Number)
        return parts.length === 5 && parts.every(v => isFinite(v)) ? parts : [0, 0, 0, 0, 0]
    }
    function setPreset(id) { veyra.setPreference("soundPreset", Number(id)) }
    function setBand(band, db) {
        const next = sound.eq.slice()
        next[band] = Math.max(-12, Math.min(12, Math.round(db * 2) / 2))
        veyra.setPreference("soundEq", next.join(","))
    }

    VRow {
        objectName: "pushfps-sound-row"
        label: qsTr("声音配置")
        hint: sound.presets[sound.preset].hint + (sound.compact ? "" : qsTr("；录制也会保留。杜比 / DTS 直通不受影响"))
        VSelect {
            objectName: "pushfps-sound"
            implicitWidth: 170
            options: sound.presets.map(p => ({ id: p.id, label: p.label }))
            value: sound.presets[sound.preset].label
            onPicked: id => sound.setPreset(id)
        }
    }

    // The filters' response from 20 Hz to 20 kHz, ±12 dB.
    Canvas {
        id: curve
        objectName: "pushfps-sound-curve"
        visible: sound.preset !== 0
        Layout.fillWidth: true
        Layout.topMargin: 4
        Layout.bottomMargin: 6
        implicitHeight: 64
        property var points: {
            const out = []
            for (let i = 0; i <= 60; ++i) {
                const hz = 20 * Math.pow(1000, i / 60)
                out.push(veyra.soundResponseDb(sound.preset, sound.eq, hz))
            }
            return out
        }
        onPointsChanged: requestPaint()
        onWidthChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height, mid = h / 2
            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.08)
            ctx.lineWidth = 1
            ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(w, mid); ctx.stroke()
            for (const hz of [100, 1000, 10000]) {
                const x = Math.log(hz / 20) / Math.log(1000) * w
                ctx.beginPath(); ctx.moveTo(x, 4); ctx.lineTo(x, h - 4); ctx.stroke()
            }
            ctx.strokeStyle = Theme.accent
            ctx.lineWidth = 2
            ctx.beginPath()
            for (let i = 0; i < points.length; ++i) {
                const x = i / (points.length - 1) * w
                const y = mid - Math.max(-12, Math.min(12, points[i])) / 12 * (mid - 4)
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
            }
            ctx.stroke()
        }
        Text { anchors.left: parent.left; anchors.bottom: parent.bottom; text: "20 Hz"; color: Theme.t3; font.pixelSize: 10; font.family: Theme.fontUi }
        Text { anchors.right: parent.right; anchors.bottom: parent.bottom; text: "20 kHz"; color: Theme.t3; font.pixelSize: 10; font.family: Theme.fontUi }
        Text { anchors.right: parent.right; anchors.top: parent.top; text: "+12 dB"; color: Theme.t3; font.pixelSize: 10; font.family: Theme.fontUi }
    }

    Repeater {
        model: sound.preset === 6 ? 5 : 0
        delegate: VRow {
            required property int index
            objectName: "pushfps-eq-band-" + index
            label: sound.bandLabels[index]
            value: (sound.eq[index] > 0 ? "+" : "") + sound.eq[index].toFixed(1) + " dB"
            VSlider {
                objectName: "pushfps-eq-slider-" + index
                implicitWidth: 150
                center: true
                from: -12; to: 12; stepSize: 0.5
                valueFromModel: true
                value: sound.eq[index]
                resettable: true; defaultValue: 0
                live: false
                onMoved: v => sound.setBand(index, v)
            }
        }
    }
    VButton {
        objectName: "pushfps-eq-reset"
        visible: sound.preset === 6 && sound.eq.some(v => v !== 0)
        Layout.alignment: Qt.AlignRight
        Layout.topMargin: 4
        ghost: true
        text: qsTr("均衡器归零")
        onClicked: veyra.setPreference("soundEq", "0,0,0,0,0")
    }
}
