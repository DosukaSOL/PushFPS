import QtQuick
import QtQuick.Layouts
import "EffectSupport.js" as Support

ColumnLayout {
    id: editor
    property string namePrefix: "list"
    spacing: 6
    // Tiers and the supersample shrink belong to the RTX Video SR families
    // (standard / high bitrate / streaming); double SR takes every SR algorithm.
    readonly property bool srAvailable: Support.available(veyra,"sr")
    readonly property bool supported: veyra.srAlgorithmOf(veyra.videoSrQuality).startsWith("rtx") && srAvailable
    readonly property bool secondRtx: veyra.srAlgorithmOf(veyra.dualSrQuality).startsWith("rtx")
    readonly property bool manual: veyra.srPrescale >= 150 && veyra.srPrescale <= 190
    readonly property var targets: [{id:"1",label:"2K"},{id:"2",label:"4K"},{id:"4",label:"5K"},{id:"5",label:"6K"},{id:"6",label:"7K"},{id:"3",label:"8K"}]
    // PushFPS: NVIDIA Image Scaling (SR choice 6) is a single, spatial pass.
    readonly property bool nis: veyra.videoSrQuality === 6
    readonly property int nisSharpness: {
        const v = Number((veyra.preferences || {}).nisSharpness)
        return isFinite(v) && v >= 0 && v <= 100 ? Math.round(v) : 50
    }
    VRow {
        label: qsTr("目标尺寸")
        hint: qsTr("基础超分的尺寸；设置缩回时为中间尺寸。")
        VSeg { objectName: editor.namePrefix+"-sr-target"; options: editor.targets; current: String(veyra.srTargetIndex); onPicked: id => veyra.srTargetIndex=Number(id) }
    }
    VRow {
        objectName: editor.namePrefix + "-nis-sharpness-row"
        visible: editor.nis
        label: qsTr("NIS 锐化")
        hint: qsTr("0 只放大不锐化；50 为 NVIDIA 默认。一次最多放大 2 倍，更大的倍数由普通缩放补足。")
        value: editor.nisSharpness + "%"
        VSlider {
            objectName: editor.namePrefix + "-nis-sharpness"
            implicitWidth: 150
            from: 0; to: 100; stepSize: 1
            valueFromModel: true
            value: editor.nisSharpness
            resettable: true; defaultValue: 50
            live: false
            onMoved: v => veyra.setPreference("nisSharpness", Math.round(Math.max(0, Math.min(100, v))))
        }
    }
    VRow {
        label: qsTr("超分运动来源")
        hint: qsTr("零运动会限制时序重建；RTX 视频超分不使用此输入")
        VSeg { options:[{id:"0",label:qsTr("零运动")},{id:"1",label:qsTr("光流")}]; current:String(veyra.srMotionSource); onPicked:id=>veyra.srMotionSource=Number(id) }
    }
    VRow {
        label: qsTr("档位")
        hint: qsTr("越高越清晰、越耗时；流媒体只有中和超高两档。")
        enabled: editor.supported; opacity: enabled ? 1 : 0.4
        VSeg {
            objectName: editor.namePrefix+"-sr-quality"
            options: veyra.srTierChoices(veyra.videoSrQuality)
            current: String(veyra.srTierOf(veyra.videoSrQuality))
            onPicked: id => veyra.videoSrQuality = veyra.srQualityFor(veyra.srAlgorithmOf(veyra.videoSrQuality), Number(id))
        }
    }
    // Supersampling: a single RTX Video SR pass at the target, shrunk into this size.
    VRow {
        label: qsTr("超分后缩回到")
        hint: !editor.supported ? qsTr("超分后缩回目前使用 RTX 视频超分")
            : veyra.dualSrEnabled ? qsTr("双重超分开启时不使用；关闭双重超分后恢复。")
            : qsTr("超采样：先超到目标尺寸再精确缩回，更自然、略软；显存更高，降噪和补帧按缩回后尺寸运行。")
        enabled: editor.supported && !veyra.dualSrEnabled
        opacity: enabled ? 1 : 0.4
        VSelect {
            objectName: editor.namePrefix+"-sr-output"
            options: [{id:"0",label:qsTr("不缩回")},{id:"1",label:qsTr("片源尺寸")},{id:"2",label:"2K"},{id:"3",label:"4K"},{id:"4",label:"5K"},{id:"5",label:"6K"},{id:"6",label:"7K"}]
            value: options.find(o=>o.id===String(veyra.srOutput))?.label||qsTr("不缩回")
            onPicked: id => veyra.srOutput=Number(id)
        }
    }
    VRow {
        label: qsTr("双重超分")
        hint: editor.nis ? qsTr("NVIDIA Image Scaling 只能单次超分")
                         : qsTr("先完成基础超分，再将结果降低分辨率并执行第二次超分；两次执行独立，会增加耗时和显存。")
        enabled: editor.srAvailable && !editor.nis
        opacity: enabled ? 1 : 0.4
        VSwitch { objectName: editor.namePrefix+"-dual-sr-toggle"; checked: veyra.dualSrEnabled; onToggled: checked => veyra.dualSrEnabled=checked }
    }
    ColumnLayout {
        objectName: editor.namePrefix+"-dual-sr-panel"
        visible: veyra.dualSrEnabled
        enabled: editor.srAvailable
        Layout.fillWidth: true
        spacing: 6
        VRow {
            label: qsTr("再次降低到")
            hint: qsTr("降低的是基础超分后的画面，不是原始输入；选项必须小于基础结果和第二次超分目标。")
            VSelect {
                objectName: editor.namePrefix+"-sr-prescale"
                options: [{id:"1",label:"720p"},{id:"2",label:"900p"},{id:"3",label:"1080p"},{id:"4",label:"1440p"},{id:"manual",label:qsTr("手动")}]
                value: editor.manual ? qsTr("手动") : (options.find(o=>o.id===String(veyra.srPrescale))?.label||"1080p")
                onPicked: id => veyra.srPrescale=id==="manual"?175:Number(id)
            }
        }
        VRow {
            visible: editor.manual
            label: qsTr("二次处理比例")
            value: Math.round(percent.displayedValue)+"%"
            VSlider {
                id: percent; objectName: editor.namePrefix+"-sr-prescale-percent"
                valueFromModel: true; from: 50; to: 90; stepSize: 5; live: false
                value: editor.manual?veyra.srPrescale-100:75
                onMoved: value => veyra.srPrescale=100+Math.round(value)
            }
        }
        VRow {
            label: qsTr("第二次超分到")
            VSeg { objectName: editor.namePrefix+"-dual-sr-target"; options: editor.targets; current: String(veyra.dualSrTargetIndex); onPicked: id=>veyra.dualSrTargetIndex=Number(id) }
        }
        VRow {
            label: qsTr("第二次超分算法")
            hint: qsTr("独立于基础超分，可以选任意一种超分；DLSS SR 两次共用上面的 DLSS 模型。")
            VSelect {
                objectName: editor.namePrefix+"-dual-sr-algorithm"
                implicitWidth: 210
                // NVIDIA Image Scaling runs as a single pass only.
                options: (veyra.srBackendChoices || []).filter(o => o.id !== "6")
                value: options.find(o => o.id === veyra.srAlgorithmOf(veyra.dualSrQuality))?.label || ""
                // Switching between RTX families keeps the tier; DLSS SR and FSR have none.
                onPicked: id => veyra.dualSrQuality = veyra.srQualityFor(id, veyra.srTierOf(veyra.dualSrQuality))
            }
        }
        VRow {
            visible: editor.secondRtx
            label: qsTr("第二次超分档位")
            VSeg {
                objectName: editor.namePrefix+"-dual-sr-quality"
                options: veyra.srTierChoices(veyra.dualSrQuality)
                current: String(veyra.srTierOf(veyra.dualSrQuality))
                onPicked: id => veyra.dualSrQuality = veyra.srQualityFor(veyra.srAlgorithmOf(veyra.dualSrQuality), Number(id))
            }
        }
    }
    // Running sizes of double SR or supersampling, and why a choice did not apply.
    Text { text: veyra.srStageSummary; visible:text.length>0; color: Theme.t2; font.family: Theme.fontUi; font.pixelSize:11; wrapMode:Text.WordWrap; Layout.fillWidth:true }
    Text { text: veyra.srStageNote; visible:text.length>0; color:Theme.t2; font.pixelSize:11; wrapMode:Text.WordWrap; Layout.fillWidth:true }
}
