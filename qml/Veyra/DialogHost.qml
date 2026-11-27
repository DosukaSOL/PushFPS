// Dialog routing and scrim. Each dialog owns its own editable content.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: host
    // Compatibility type used by the professional and node views.
    component VTextField: DialogTextField {}
    // (4.5.1) Dialogs nobody has opened yet are not made at start: less to load and to keep evaluating while the
    // picture plays. Made the first time their key is asked for and kept from then on (their fade-out and what
    // they hold stay as they were). Only dialogs that do nothing while closed and that nothing outside reaches.
    component LazyDialog: Loader {
        required property string dialogKey
        active: false
        readonly property bool asked: host.dialog === dialogKey
        onAskedChanged: if (asked) active = true
        Component.onCompleted: if (asked) active = true
    }
    anchors.fill: parent
    z: 100

    // Active dialog: network/capture/ps5/moonlight/xbox/screen/subtitle/audio/save/manage.
    property string dialog: ""
    // The game scene the layout editor opens on (StudioPage.requestLayout).
    property string layoutScene: ""
    property bool onboardingActive: false
    readonly property alias captureSettings: captureDialog
    readonly property bool captureGuideOpen: captureGuide.visible
    onDialogChanged: {
        if (dialog !== "capture") {
            legacyCapturePicker.close()
            captureGuide.close()
        }
    }

    function open(key) {
        if (key === "save") saveDialog.resetForOpen()
        if (key === "manage") {
            manageDialog.mode = veyra.nodeMode === 1 ? "node" : "list"
            manageDialog.feedback = ""
        }
        veyra.logUi("ui-dialog", "open " + key + " (was " + (host.dialog || "none") + ")")
        host.dialog = key
    }
    function close() { host.dialog = "" }
    // PushFPS: the read-text key pressed with its dialog open reads the picture as it is now.
    function readTextAgain() { readTextDialog.start() }

    signal startPs5()
    signal startMoonlight()
    signal startXbox()
    signal startScreen()
    signal startScreenCrop()
    // PushFPS redesign: the Appearance panel asks for Settings -> Appearance.
    signal requestAppearanceSettings()

    CaptureGuideDialog { id: captureGuide }

    CaptureLegacyPicker {
        id: legacyCapturePicker
        formats: visible ? veyra.captureFormats : []
        selectedKey: veyra.captureFormatKey
        onSelected: key => { if (!veyra.captureQueryBusy) veyra.captureFormatKey = key }
    }
    Connections {
        target: veyra
        function onCaptureChanged() { if (veyra.captureQueryBusy) legacyCapturePicker.close() }
    }

    // Scrim (M16): the design dims the page behind the dialog, opacity .2s linear.
    // The native video window cannot be dimmed from QML; only the dialog panel is cut
    // out of it (videoCover below), so the picture beside the dialog stays undimmed.
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        opacity: host.dialog !== "" && !host.onboardingActive ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.d(200) } }
        TapHandler { onTapped: host.close() }
        // PushFPS: the dimmed page is not hovered either (no tooltips or lit edges from behind a dialog).
        // It takes no button, so a tap still closes the dialog.
        MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; hoverEnabled: true }
    }
    // For the motion probe: the open dialog's scale.
    property real motionScale: 1

    // --- shared chrome ----------------------------------------------------

    NetworkSourceDialog {
        id: networkDialog
        dialogHost: host
    }

    // --- 采集卡 -----------------------------------------------------------
    // The 1.4.4 capture panel, in the new design: device details and formats are
    // queried asynchronously, only an explicitly chosen audio input is opened,
    // and the choice is remembered for "继续上次采集". Resolution, frame rate and
    // pixel format are three views of that one saved format key.
    CaptureSourceDialog {
        id: captureDialog
        dialogHost: host
        onboardingActive: host.onboardingActive
        legacyPicker: legacyCapturePicker
        onGuideRequested: captureGuide.open()
    }

    // --- PS5 串流 ---------------------------------------------------------
    // The 1.4.4 Remote Play panel: saved pairings are encrypted in the user's
    // %LOCALAPPDATA%\Veyra\remoteplay and shared with 1.4.4.
    Ps5SourceDialog {
        id: ps5Dialog
        dialogHost: host    }

    // --- PC 串流（Sunshine / GameStream） ----------------------------------
    // Hosts, pairing, the host's apps and the stream-core settings. The effect chain is not here: once
    // connected, the picture goes through the software's own pages like any other source.
    PcSourceDialog {
        id: mlDialog
        dialogHost: host
    }

    // --- Xbox 串流（非官方） ---------------------------------------------------
    // Sign in with the Xbox (Microsoft) account on Microsoft's own page via a device code, pick a console,
    // start. The software never sees the password; only a refresh token is kept, encrypted for this user.
    XboxSourceDialog {
        id: xbDialog
        dialogHost: host    }

    // --- 屏幕捕获 ---------------------------------------------------------
    ScreenSourceDialog {
        id: screenDialog
        dialogHost: host
    }

    // --- 字幕设置 ---------------------------------------------------------
    SubtitleSettingsDialog {
        id: subtitleDialog
        dialogHost: host
    }
    LiveTranslationDialog {
        dialogHost: host
    }

    // --- 音频设置 ---------------------------------------------------------
    AudioSettingsDialog {
        dialogHost: host
    }

    // --- 另存为预设 -------------------------------------------------------
    // The design lists every part that will be saved before asking for a name, so
    // the user can see what they are about to store.
    SavePresetDialog {
        id: saveDialog
        dialogHost: host
    }

    // --- 管理预设 ---------------------------------------------------------
    ManagePresetsDialog {
        id: manageDialog
        dialogHost: host
    }

    // --- PushFPS: Optimise for my PC, game presets ----------------------------
    PushOptimizeDialog {
        id: optimizeDialog
        dialogHost: host
    }
    LazyDialog { dialogKey: "games"; sourceComponent: Component { PushGamePresetsDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "hud"; sourceComponent: Component { PushHudDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "latencyguide"; sourceComponent: Component { PushLatencyGuideDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "crash"; sourceComponent: Component { PushCrashDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "diagnose"; sourceComponent: Component { PushDiagnoseDialog { dialogHost: host } } }
    // PushFPS redesign: the palette button's panel (key "appearance").
    LazyDialog { dialogKey: "appearance"; sourceComponent: Component { PushAppearanceDialog { dialogHost: host; onRequestSettings: host.requestAppearanceSettings() } } }
    // PushFPS: read the text on the picture (key "readtext").
    PushReadTextDialog {
        id: readTextDialog
        dialogHost: host
    }
    LazyDialog { dialogKey: "cardtest"; sourceComponent: Component { PushCardTestDialog { dialogHost: host } } }
    PushStreamDialog {
        id: streamDialog
        dialogHost: host
    }
    LazyDialog { dialogKey: "compare"; sourceComponent: Component { PushCompareDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "record"; sourceComponent: Component { PushRecordDialog { dialogHost: host } } }
    // PushFPS: the keyboard and mouse as a controller (key "kbm").
    LazyDialog { dialogKey: "kbm"; sourceComponent: Component { PushKbmDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "micfilters"; sourceComponent: Component { PushMicFiltersDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "frameinput"; sourceComponent: Component { PushFrameInputDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "qualitytest"; sourceComponent: Component { PushQualityTestDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "latencytest"; sourceComponent: Component { PushLatencyTestDialog { dialogHost: host } } }
    LazyDialog { dialogKey: "layout"; sourceComponent: Component { PushLayoutDialog { dialogHost: host; sceneId: host.layoutScene } } }
    LazyDialog { dialogKey: "crop"; sourceComponent: Component { PushCropDialog { dialogHost: host } } }

    // --- small building blocks the design's dialogs use --------------------

}
