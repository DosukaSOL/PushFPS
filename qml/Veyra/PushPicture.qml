// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
// PushFPS picture editor: the everyday controls of the colour grade (Professional
// → 色彩) in one short card, with presets. It writes the same colour node through
// the bridge's colour API, so Professional shows the same values, undo works, and
// live recordings and exports carry the result.
//
// Gamma is applied through the RGB tone curve (the grade works in a log domain,
// where a display gamma around white is a straight line; black stays anchored).
// A curve drawn by hand in Professional shows as "custom" here.
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: pic
    spacing: 0

    readonly property var st: veyra.colourState || ({})
    readonly property bool available: (st.index ?? -1) >= 0
    // Parameters the presets own; anything else in the grade is left alone.
    readonly property var keys: ["exposure", "contrast", "highlights", "shadows", "saturation", "vibrance", "texture", "temperature"]
    readonly property var presets: [
        { id: "natural", label: qsTr("自然"), values: {}, gamma: 1.0 },
        { id: "vivid", label: qsTr("鲜艳"), values: { contrast: 12, highlights: -10, shadows: 8, saturation: 8, vibrance: 30, texture: 15 }, gamma: 1.0 },
        { id: "cinematic", label: qsTr("电影感"), values: { contrast: 20, highlights: -25, shadows: -5, saturation: -12, vibrance: 5, texture: 5, temperature: 6 }, gamma: 0.95 },
        { id: "bright", label: qsTr("明亮"), values: { exposure: 0.25, highlights: -15, shadows: 30, vibrance: 10 }, gamma: 1.15 }
    ]

    // --- gamma on the tone curve -------------------------------------------
    readonly property var gammaXs: [0, 0.1, 0.25, 0.4, 0.55, 0.7, 0.85, 1.0]
    function decodeLog(x) { return 0.18 * Math.pow(2, (x - 0.5) * 10) }
    function encodeLog(l) { return Math.max(0, Math.min(1, Math.log2(Math.max(l, 1e-5) / 0.18) / 10 + 0.5)) }
    function gammaPoints(g) {
        return gammaXs.map(x => ({ x: x, y: x === 0 ? 0 : Math.round(encodeLog(Math.pow(decodeLog(x), 1 / g)) * 10000) / 10000 }))
    }
    // 1.0 for the identity curve, the gamma for a curve this card wrote, -1 for any other curve.
    readonly property real gamma: {
        const pts = (st.curves && st.curves[0]) || []
        if (pts.length === 2 && pts[0].x === 0 && pts[0].y === 0 && pts[1].x === 1 && pts[1].y === 1) return 1.0
        if (pts.length !== gammaXs.length) return pts.length === 0 ? 1.0 : -1
        const l = Math.log2(decodeLog(0.4)), lp = Math.log2(decodeLog(pts[3].y))
        if (Math.abs(lp) < 1e-6) return -1
        const g = Math.round(l / lp * 100) / 100
        if (!(g >= 0.5 && g <= 2.0)) return -1
        const ref = gammaPoints(g)
        for (let i = 0; i < ref.length; ++i)
            if (Math.abs(ref[i].x - pts[i].x) > 1e-4 || Math.abs(ref[i].y - pts[i].y) > 0.003) return -1
        return g
    }
    function setGamma(g) {
        ensureOn()
        g = Math.round(Math.max(0.5, Math.min(1.5, g)) * 100) / 100
        veyra.setColourCurve(0, Math.abs(g - 1) < 0.005 ? [{ x: 0, y: 0 }, { x: 1, y: 1 }] : gammaPoints(g))
    }

    function ensureOn() { if (pic.available && !pic.st.enabled) veyra.colorEnabled = true }
    function setParam(key, v) { ensureOn(); veyra.setColourParameter(key, v) }
    function applyPreset(p) {
        ensureOn()
        for (const k of pic.keys) veyra.setColourParameter(k, p.values[k] ?? 0)
        setGamma(p.gamma)
    }
    readonly property string currentPreset: {
        if (!available) return ""
        for (const p of presets) {
            let same = Math.abs((gamma < 0 ? -9 : gamma) - p.gamma) < 0.011
            for (const k of keys) same = same && Math.abs((st[k] ?? 0) - (p.values[k] ?? 0)) < 0.011
            if (same) return p.id
        }
        return "custom"
    }

    VRow {
        objectName: "pushfps-picture-preset-row"
        label: qsTr("画面风格")
        hint: !pic.available ? qsTr("当前处理链没有调色节点") : pic.currentPreset === "custom" ? qsTr("已自定义") : ""
        VSeg {
            objectName: "pushfps-picture-preset"
            enabled: pic.available
            opacity: enabled ? 1 : 0.4
            options: pic.presets.map(p => ({ id: p.id, label: p.label }))
            current: pic.currentPreset
            onPicked: id => pic.applyPreset(pic.presets.find(p => p.id === id))
        }
    }

    // Inline components do not see this file's ids: everything comes in through properties.
    component PicSlider: VRow {
        id: row
        required property string key
        property real from: -100
        property real to: 100
        property real step: 1
        property real current: 0
        property real neutral: 0
        property bool usable: true
        signal edited(real v)
        objectName: "pushfps-picture-" + key
        value: (current > 0 ? "+" : "") + Math.round(current)
        VSlider {
            objectName: "pushfps-picture-slider-" + row.key
            implicitWidth: 150
            enabledControl: row.usable
            from: row.from; to: row.to; stepSize: row.step
            // Fill from the middle when the neutral value sits in the middle of the range.
            center: row.neutral > row.from && Math.abs((row.neutral - row.from) - (row.to - row.neutral)) < 1e-6
            valueFromModel: true
            value: row.current
            resettable: true; defaultValue: row.neutral
            onMoved: v => row.edited(v)
        }
    }

    PicSlider {
        key: "exposure"; label: qsTr("亮度"); from: -1; to: 1; step: 0.05
        usable: pic.available; current: pic.st.exposure ?? 0
        value: (current > 0 ? "+" : "") + current.toFixed(2) + " EV"
        onEdited: v => pic.setParam("exposure", v)
    }
    PicSlider {
        key: "contrast"; label: qsTr("对比度"); from: -50; to: 50
        usable: pic.available; current: pic.st.contrast ?? 0
        onEdited: v => pic.setParam("contrast", v)
    }
    PicSlider {
        key: "gamma"; label: qsTr("伽马")
        hint: pic.gamma < 0 ? qsTr("专业模式中设置了自定义曲线；拖动会替换它") : qsTr("中间调亮度；黑色和白色保持不变")
        from: 0.5; to: 1.5; step: 0.01; neutral: 1
        usable: pic.available; current: pic.gamma < 0 ? 1 : pic.gamma
        value: pic.gamma < 0 ? qsTr("自定义") : current.toFixed(2)
        onEdited: v => pic.setGamma(v)
    }
    PicSlider {
        key: "saturation"; label: qsTr("饱和度"); from: -50; to: 50
        usable: pic.available; current: pic.st.saturation ?? 0
        onEdited: v => pic.setParam("saturation", v)
    }
    PicSlider {
        key: "texture"; label: qsTr("锐度"); from: 0; to: 100
        hint: qsTr("加强细节和边缘（纹理）")
        usable: pic.available; current: pic.st.texture ?? 0
        onEdited: v => pic.setParam("texture", v)
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.topMargin: 6
        spacing: 6
        VButton {
            objectName: "pushfps-picture-more"
            ghost: true
            text: qsTr("更多调色选项")
                        onClicked: veyra.navigate("pro")
        }
        Item { Layout.fillWidth: true }
        VButton {
            objectName: "pushfps-picture-undo"
            ghost: true
            enabled: veyra.colourCanUndo === true
            text: qsTr("撤销")
            onClicked: veyra.colourUndo()
        }
        VButton {
            objectName: "pushfps-picture-reset"
            ghost: true
            enabled: pic.available && pic.currentPreset !== "natural"
            text: qsTr("重置")
            onClicked: pic.applyPreset(pic.presets[0])
        }
    }
}
