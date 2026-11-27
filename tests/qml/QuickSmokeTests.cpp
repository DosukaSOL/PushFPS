// G1.6: component smoke tests through Qt Quick Test (tests/qml/quick/tst_*.qml).
// The test source and Veyra module are resolved from the executable's staging
// directory. This prevents a staging run from silently reading the source tree.
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QtQml/qqml.h>
#include <QtQuickTest/quicktest.h>
#include <QGuiApplication>
#include <QTest>
#include <QWindow>
#include <QScreen>
#include <QPixmap>

// Quick Test's keyClick targets its QQuickView host. Multi-window ownership
// tests need events delivered to the actual focused QML Window instead.
class ActiveWindowKeys : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool focused(QObject* window) { return QGuiApplication::focusWindow() == window; }
    Q_INVOKABLE bool savePresented(QObject* object,const QString& path) {
        auto* window=qobject_cast<QWindow*>(object);
        if(!window||!window->screen())return false;
        // Read already presented pixels, without triggering a fresh QML render.
        const auto image=window->screen()->grabWindow(window->winId());
        return !image.isNull()&&image.save(path);
    }
    Q_INVOKABLE bool press(int key) {
        auto* window = QGuiApplication::focusWindow();
        if (!window) return false;
        QTest::keyClick(window, Qt::Key(key));
        QCoreApplication::processEvents();
        return true;
    }
};

class Setup : public QObject {
    Q_OBJECT
public slots:
    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("testWindowKeys"), new ActiveWindowKeys(engine));
        engine->rootContext()->setContextProperty(QStringLiteral("exportTestOutputDirectory"),qEnvironmentVariable("VEYRA_EXPORT_TEST_ARTIFACTS"));
        engine->addImportPath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("qml")));
        // The registered inline NR card has the engine's creation context,
        // not the fixture item's lexical context. Supply its explicit UI-only
        // snapshot here. The separate real-player probes test the C++ bridge.
        QQmlComponent snapshot(engine);
        snapshot.setData(R"qml(import QtQml
QtObject {
    property var effectCapabilities: ({nr2:{available:true},nr3:{available:true},nr4:{available:true}})
    property var nrRuntimeChoices: [{id:"2",label:"RTX 20–50 · SF 310.9.16"},{id:"3",label:"RTX 50 · NVIDIA original"},{id:"4",label:"RX 7000 · RedDuke (experimental)"}]
    property var nrLayers: [{index:1}]
    property int nrMotionSource: 1
    property string nrAutoStatus: ""
    property var nodeTimings: ({})
    property var preferences: ({exportStopsPlayback:false})
    // UpdateChecker as quick setup reads it (4.5.2: installing the optional extras afterwards).
    property QtObject updates: QtObject {
        property bool canInstallExtras: false
        property string installState: "idle"
        property real installProgress: 0
        property int extrasAsked: 0
        function installExtras() { extrasAsked++ }
    }
    property QtObject liveTranslation: QtObject {
        property bool supported: true
        property bool enabled: false
        property int provider: 0
        property string sourceLanguage: ""
        property string targetLanguage: "en"
        property bool showOriginal: true
        property int holdMs: 1500
        property bool modelInstalled: false
        property bool downloading: false
        property real downloadProgress: 0
        property real downloadBytes: 1142252715
        property string apiBase: ""
        property string apiModel: ""
        property string status: ""
        function downloadModel() { downloading = true }
        function cancelModelDownload() { downloading = false }
        // PushFPS: one text translated apart from the captions (tst_push_read_text.qml).
        property string onceText: ""
        property string onceStatus: ""
        property bool onceBusy: false
        property bool onceCut: false
        property bool onceOnline: provider !== 0
        readonly property int onceReady: provider === 2 ? (sourceLanguage === "" ? 3 : 0)
            : provider === 0 ? (downloading ? 4 : modelInstalled ? 0 : 1) : (apiBase !== "" && apiModel !== "" ? 0 : 2)
        property var onceAsked: []
        property int onceClears: 0
        function translateOnce(text) {
            if (onceReady !== 0 || String(text).length === 0) return false
            onceAsked = onceAsked.concat([String(text)])
            onceText = "[" + targetLanguage + "] " + text
            return true
        }
        function clearOnce() { onceClears++; onceText = ""; onceStatus = ""; onceCut = false }
        function onceDownloadModel() { downloading = true }
    }
    property real playbackRate: 1
    readonly property real maximumVolume: 3
    property real duration: 100
    property bool hasSource: true
    property bool isCapture: false
    property bool openingSource: false
    property int sourceWidth: 1000
    property int sourceHeight: 500
    property string statusText: ""
    property var appliedCrop: ({})
    // Off by default, as before these existed: only the low latency test turns them on.
    property bool nvidiaLowLatencyAvailable: false
    property int nvidiaLowLatency: 0
    property bool fgLowQueue: false
    property int vrrState: 0
    property bool vrrAmd: false
    property bool running: false
    property string presentationStatus: ""
    // PushFPS quick setup (tst_push_quick_setup.qml): frame generation, output cap,
    // NR and presets as the bridge exposes them. Frame generation follows the
    // multiplier the way setFgMultiplier does (above 1 = on).
    property bool fgEnabled: false
    property int fgMultiplier: 1
    property var fgMultiplierChoices: [{id:"1"},{id:"2"},{id:"3"},{id:"4"}]
    // PushFPS adaptive multiplier (QmlPlayerBridge::fgAdaptive) and duplicate removal (contentRate).
    property bool fgAdaptive: false
    property bool pacingRepair: false
    property bool hudLock: false
    property int contentRate: 0
    property string version: ""
    property var nvidiaVrrProfile: null
    property int outputRateMode: 0
    property int displaySync: 2
    property bool variableRefresh: false
    property real outputCustomFps: 0
    property bool presentationOwned: false
    property real displayRefreshHz: 240
    property bool nrAnyEnabled: false
    property bool hasCaptureSession: false
    property var presets: []
    property string currentPresetName: "自定义"
    property int lastAppliedPreset: -1
    onFgMultiplierChanged: fgEnabled = fgMultiplier > 1
    onFgEnabledChanged: if (!fgEnabled && fgMultiplier > 1) fgMultiplier = 1
    function addEffect(type) { if (type === "nr") nrAnyEnabled = true }
    function setAllNrEnabled(on) { nrAnyEnabled = on }
    property int compareMode: 0
    function setNrLayerParameter(index, key, value) {
        if (!nrLayers.some(l => l.index === index)) return false
        nrLayers = nrLayers.map(l => { if (l.index !== index) return l; const c = Object.assign({}, l); c[key] = value; return c })
        return true
    }
    function applyPresetIndex(index) { lastAppliedPreset = index; return true }
    // PushFPS picture editor, sound profile and recording card (tst_push_picture_sound.qml).
    property var colourState: ({index: 0, enabled: false, exposure: 0, contrast: 0, highlights: 0, shadows: 0,
        saturation: 0, vibrance: 0, texture: 0, temperature: 0,
        curves: [[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}],[{x:0,y:0},{x:1,y:1}]]})
    property bool colorEnabled: false
    onColorEnabledChanged: { const s = Object.assign({}, colourState); s.enabled = colorEnabled; colourState = s }
    property bool colourCanUndo: false
    property int colourUndoCount: 0
    function setColourParameter(name, value) { const s = Object.assign({}, colourState); s[name] = value; colourState = s; colourCanUndo = true; return true }
    function setColourCurve(channel, points) {
        const s = Object.assign({}, colourState); const c = s.curves.slice(); c[channel] = points.map(p => ({x: p.x, y: p.y}))
        s.curves = c; colourState = s; return true
    }
    function colourUndo() { colourUndoCount++; return true }
    function soundResponseDb(preset, eq, hz) { return preset === 0 ? 0 : 3 }
    property var recordingState: ({saved: 0, active: false, path: ""})
    // `recording` (below) declares recordingChanged() as well.
    property var replayState: ({saved: 0, active: false, path: ""})
    signal replayChanged()
    // PushFPS optimiser, game presets, latency readout and sharing (src/ui/bridge/PushFps.cpp).
    property string fgBackendName: "dlss"
    property int fgMotionSource: 1
    property int flowQuality: 1
    property int opticalFlowChoice: 0
    property bool amdFlowHalf: false
    property var fgBackendChoices: [{id: "dlss", label: "DLSS"}]
    property string fgProviderText: ""
    property bool amdNrGpu: false
    property int vfgQuality: 1
    property bool srEnabled: false
    property string captureDeviceLabel: ""
    property var exclusionState: ({enabled: false, feather: 6, regions: []})
    property var liveStats: ({running: false})
    // Sound sync as the real bridge has it: 0 automatic, 1 manual, 2 off; whether something live plays;
    // and the delay the sound has now.
    property int audioSyncMode: 0
    property bool audioSyncLive: false
    property real audioCompensationMs: 0
    property var benchmarkModel: null
    property int benchmarkWindows: 0
    property var lastRegions: null
    property string lastCopiedText: ""
    property string lastCopiedFile: ""
    property int discordOpened: 0
    property string lastSharedFile: ""
    property string importFileText: ""
    property string lastExportText: ""
    signal pushShareFinished(bool ok, string message)
)qml"
    // MSVC limits one string literal to about 16 KB (C2026); adjacent literals are joined.
    R"qml(    function pushLiveStats() { return liveStats }
    property int halfTests: 0
    function pushTestPushFlowHalf() { halfTests++; return true }
    function pushFlowHalfTesting() { return false }
    function pushBenchmarkBegin() { benchmarkWindows++ }
    function pushBenchmarkEnd() { return benchmarkModel ? benchmarkModel() : ({valid: false}) }
    // PushFPS ground-truth quality test (tst_push_optimize.qml). qualityModel(backend) returns the status
    // the test ends with; null: the source can't be tested.
    property var qualityModel: null
    property int qualityStarts: 0
    property int qualityCancels: 0
    property var qualityState: ({running: false, done: false, failed: false})
    function pushQualityBegin(seconds) {
        qualityStarts++
        const r = qualityModel ? qualityModel(String(fgBackendName || "")) : null
        qualityState = r ? Object.assign({running: false, done: true, failed: false, samples: 20}, r)
                         : ({running: false, done: false, failed: true, error: "unsupported source"})
        return true
    }
    function pushQualityCancel() { qualityCancels++ }
    function pushQualityStatus() { return qualityState }
    function pushSetExclusionRegions(regions, feather) {
        lastRegions = regions
        exclusionState = ({enabled: regions.length > 0, feather: feather, regions: regions})
        return true
    }
    function pushCopyText(text) { lastCopiedText = text; return true }
    function pushCopyFile(path) { lastCopiedFile = path; return true }
    function pushOpenDiscord() { discordOpened++; return true }
    function pushShareToDiscord(path) { lastSharedFile = path; return true }
    property bool anrInstalled: false
    property int anrInstallCalls: 0
    property string lastOpenedLink: ""
    function pushAnrState() { return { installed: anrInstalled, folder: "C:/PushFPS/runtime/experimental/anr", name: "Apex Neural Rendering (ANR)", maker: "Apex Compute", url: "anr-url" } }
    function pushInstallAnr() { anrInstallCalls++; anrInstalled = true; return true }
    function pushOpenAnrFolder() { return true }
    function pushLinkUrl(key) { return ["repo", "issues", "veyra", "anr", "anrReleases", "apex", "discord", "controlDocs"].indexOf(key) >= 0 ? "link-" + key : "" }
    function pushOpenLink(key) { lastOpenedLink = key; return pushLinkUrl(key).length > 0 }
    function pushImportTextFile(title) { return importFileText }
    function pushExportTextFile(title, name, text) { lastExportText = text; return true }
    // PushFPS HUD detection, comparison clips and the wordmark (tst_push_hud.qml).
    property string appName: "PushFPS"
    property bool recording: false
    property var hudState: ({running: false, done: false, failed: false, taken: 0, target: 0, regions: [], preview: "", tooStill: false, motion: 0})
    property int hudStarts: 0
    property int hudLastSamples: 0
    property int hudCancels: 0
    property int compareStarts: 0
    property int compareSeconds: 0
    property int compareLayout: -1
    property int recordingStops: 0
    function pushStartHudDetection(samples) {
        if (!hasSource) return false
        hudStarts++; hudLastSamples = samples
        hudState = ({running: true, done: false, failed: false, taken: 0, target: samples, regions: [], preview: "", tooStill: false, motion: 0})
        return true
    }
    function pushCancelHudDetection() { hudCancels++; hudState = ({running: false, done: false, failed: false, taken: 0, target: 0, regions: [], preview: ""}) }
    function pushHudDetection() { return hudState }
    function pushStartCompareClip(seconds, layout) { compareStarts++; compareSeconds = seconds; compareLayout = layout === undefined ? 0 : layout; return true }
    // What the bridge calls the original's side (engine/PushCompareName.h, tested there): here the rule in small.
    function pushCompareSourceName() {
        if (["file", "url", "screen", "frameinput"].indexOf(String(sourceKind)) >= 0) return "原画"
        const names = { ps5: "PS5", ps5pro: "PS5 Pro", xbox: "Xbox Series X|S", "switch": "Switch", switch2: "Switch 2" }
        const chosen = preferences.pushConsole === undefined ? "" : String(preferences.pushConsole)
        return names[chosen] !== undefined ? names[chosen] : "主机原画"
    }
    // PushFPS going live (src/ui/bridge/PushStream.cpp, tst_push_stream.qml).
    property var streamKeys: ({})
    property var streamState: ({active: false, live: false, outputs: []})
    property int streamStarts: 0
    property int streamStops: 0
    function pushStreamPlatforms() {
        return [
            {id: "twitch", name: "Twitch", servers: [{name: "Auto", url: "twitch-auto"}], maxVideoKbps: 6000, maxAudioKbps: 320, needsRtmps: false, keyPage: "dashboard.twitch.tv"},
            {id: "youtube", name: "YouTube", servers: [{name: "Primary (RTMPS)", url: "youtube-primary"}], maxVideoKbps: 51000, maxAudioKbps: 160, needsRtmps: false, keyPage: "studio.youtube.com"},
            {id: "kick", name: "Kick", servers: [{name: "Default", url: "kick-default"}], maxVideoKbps: 8000, maxAudioKbps: 160, needsRtmps: true, keyPage: "kick.com"},
            {id: "tiktok", name: "TikTok", servers: [], maxVideoKbps: 6000, maxAudioKbps: 160, needsRtmps: false, keyPage: "tiktok.com"},
            {id: "custom", name: "Custom", servers: [], maxVideoKbps: 50000, maxAudioKbps: 320, needsRtmps: false, keyPage: ""}
        ]
    }
    function pushSetStreamKey(id, key) {
        if (String(key).trim().length === 0) return false
        const k = Object.assign({}, streamKeys); k[id] = true; streamKeys = k; return true
    }
    function pushHasStreamKey(id) { return streamKeys[id] === true }
    function pushForgetStreamKey(id) { const k = Object.assign({}, streamKeys); delete k[id]; streamKeys = k }
    function pushStartStream() {
        let list = []
        try { list = JSON.parse(String(preferences.streamTargets || "[]")) } catch (e) {}
        const on = list.filter(t => t.enabled !== false && streamKeys[t.id] === true)
        if (!hasSource || on.length === 0) return false
        streamStarts++
        streamState = ({active: true, live: true, seconds: 5, outputs: on.map(t => ({name: t.name || t.platform, state: "live", kbps: 6100, framesDropped: 0, reconnects: 0, error: ""}))})
        return true
    }
    function pushStopStream() { streamStops++; streamState = ({active: false, live: false, outputs: []}) }
    function pushStreamStatus() { return streamState }
    // PushFPS streaming mode (tst_push_studio.qml): mixer meters, recording and monitor controls.
    property var studioLevels: ({gamePeakDb: -100, gameRmsDb: -100, micPeakDb: -100, micRmsDb: -100, micOpen: false, micDevice: "", micError: "",
                                  desktopPeakDb: -100, desktopRmsDb: -100, desktopOpen: false, desktopError: "", auxPeakDb: -100, auxRmsDb: -100, auxOpen: false, auxDevice: "", auxError: ""})
    property bool studioMeters: false
    property int studioMeterCalls: 0
    function pushStudioLevels() { return studioLevels }
    function pushSetStudioMeters(on) { studioMeters = on; studioMeterCalls++ }
    property int microphoneRefreshes: 0
    function refreshMicrophones() { microphoneRefreshes++ }
    property var microphoneDevices: []
    readonly property bool recordingMicrophone: preferences.recordMic === true
    readonly property string recordingMicrophoneId: String(preferences.recordMicDevice || "")
    readonly property bool recordingAudio: preferences.recordAudio !== false
    property string currentPage: ""
    property real volume: 1
    property bool muted: false
    property int recordingToggles: 0
    function toggleRecording(source) { recordingToggles++; recording = !recording }
    // PushFPS record dialog (PushRecordDialog): a recording started from it, and from where.
    property int recordingStarts: 0
    property string recordingSource: ""
    function startRecording(source) { recordingStarts++; recordingSource = source === undefined ? "" : source; recording = true }
    readonly property bool replayEnabled: preferences.replayEnabled === true
    property int replaySeconds: 60
    property int replaySaves: 0
    function saveReplay(source) { replaySaves++ }
    // PushFPS clip markers (tst_push_studio.qml, tst_push_watch.qml).
    signal pushControlScene(int number)
    signal pushControlChanged()
    property var controlState: ({enabled: false, listening: false, port: 18766, hasToken: false, error: ""})
    property int controlTokenCopies: 0
    property int controlTokensMade: 0
    function pushControlState() { return controlState }
    function pushSetControlEnabled(on) {
        controlState = Object.assign({}, controlState, {enabled: on, listening: on, hasToken: controlState.hasToken || on})
        pushControlChanged()
        return true
    }
    function pushCopyControlToken() { if (!controlState.hasToken) return false; controlTokenCopies++; return true }
    function pushNewControlToken() { controlTokensMade++; return true }
)qml"
    R"qml(    // PushFPS reading the text on the picture (tst_push_read_text.qml). `readTextAnswer` is what a reading gives.
    // No `signal pushReadTextChanged()` here: the property brings its own change signal, and declaring it
    // again is an error that stops every test at start.
    property var pushReadText: ({phase: "idle"})
    property var readTextAnswer: ({phase: "done", error: "", text: "New game\nOptions", count: 2, language: "en-US",
        lines: [{text: "New game", x: 0.1, y: 0.1, w: 0.2, h: 0.05}, {text: "Options", x: 0.1, y: 0.2, w: 0.2, h: 0.05}]})
    property var readTextStarts: []
    property int readTextPrepared: 0
    property int readTextCleared: 0
    property int languageSettingsOpened: 0
    function pushReadTextPrepare() {
        readTextPrepared++
        pushReadText = Object.assign({}, pushReadText, {languagesKnown: true,
            languages: [{tag: "en-US", name: "English (United States)"}, {tag: "ja", name: "Japanese"}]})
    }
    function pushReadTextStart(tag, region) {
        if (!hasSource) { pushReadText = Object.assign({}, pushReadText, {phase: "failed", error: "no-picture", text: "", count: 0}); return false }
        readTextStarts = readTextStarts.concat([{tag: String(tag), region: Number(region)}])
        pushReadText = Object.assign({}, pushReadText, readTextAnswer, {region: Number(region)})
        return readTextAnswer.phase !== "failed"
    }
    function pushReadTextClear() {
        readTextCleared++
        pushReadText = {phase: "idle", languages: pushReadText.languages, languagesKnown: pushReadText.languagesKnown}
    }
    function pushOpenLanguageSettings() { languageSettingsOpened++; return true }
    property int markersAdded: 0
    function pushAddMarker(source) {
        if (!recording) return false
        markersAdded++
        const copy = Object.assign({}, recordingState); copy.markers = Number(copy.markers || 0) + 1; recordingState = copy
        return true
    }
    property int screenshots: 0
    function takeScreenshot() { screenshots++ }
    property string sourceTitle: ""
    property string sourceKind: ""
    // PushFPS Check my setup (PushDiagnoseDialog.qml): what the capture dialog knows of the card.
    property var captureSharperFormat: []
    property int captureAudioChoice: -2
    property int captureAudioSuggestion: -1
    // PushFPS streaming mode scenes and video playback mode (tst_push_studio.qml, tst_push_watch.qml).
    property var lastScene: null
    property int sceneCalls: 0
    function pushSetStreamScene(scene) { lastScene = scene; sceneCalls++; return true }
    property bool cardMute: false
    function pushSetCardMute(muted) { cardMute = muted === true }
    property string pickedImage: ""
    function pushPickImage() { return pickedImage }
    // Streaming mode sources (webcam, window, video file).
    property var cameras: [{id: "cam1", name: "USB Webcam"}]
    property var windowList: [{id: "4242", name: "Chat - Browser"}]
    property string pickedMedia: ""
    property var lastSources: []
    property int sourceCalls: 0
    property var sourceStatus: []
    function pushCameras() { return cameras }
    function pushWindows() { return windowList }
    function pushPickMedia() { return pickedMedia }
    function pushSetSceneSources(list) { lastSources = list; sourceCalls++ }
    function pushSourceStatus() { return sourceStatus }
    function pushSourcePreview(id) { return "" }
    // The live preview's frame counter: 0 keeps the page from asking for a picture (no provider here).
    property int sourceSerial: 0
    function pushSourceSerial(id) { return sourceSerial }
    // PushFPS Frame Input API (tst_push_frame_input.qml).
    property var frameInputSenders: []
    property string frameInputStarted: ""
    function pushFrameInputSenders() { return frameInputSenders }
    function pushStartFrameInput(channel) {
        if (!frameInputSenders.some(s => s.channel === channel && s.alive)) return false
        frameInputStarted = channel; return true
    }
    property var screenOptions: ({kind: 0, passthrough: false, method: 0, fps: 0, cursor: true, left: 0, top: 0, right: 0, bottom: 0, fill: false})
    property var screenTargets: [{id: "w1", label: "Browser - YouTube"}]
    property string screenTargetId: "w1"
    property string screenTargetLabel: "Browser - YouTube"
    function setScreenOption(key, value) { const o = Object.assign({}, screenOptions); o[key] = value; screenOptions = o; return true }
    property int screenStarts: 0
    function startScreenCapture() { screenStarts++; hasSource = true; return true }
    property int targetRefreshes: 0
    function refreshCaptureTargets() { targetRefreshes++ }
    property int fileDialogs: 0
    function openFileDialog() { fileDialogs++ }
    property int playbackStops: 0
    function stopPlayback() { playbackStops++; hasSource = false }
    property bool videoHdr: false
    function stopRecording(source) { recordingStops++; recording = false }
    // PushFPS super-resolution editor (SrStageEditor): ids as QmlPlayerBridge::srBackendChoices.
    property int videoSrQuality: 2
    property int dualSrQuality: 2
    property bool dualSrEnabled: false
    property int dualSrTargetIndex: 2
    property int srTargetIndex: 2
    property int srMotionSource: 1
    property int srPrescale: 0
    property int srOutput: 0
    property string srStageSummary: ""
    property string srStageNote: ""
    property var srBackendChoices: [{id: "0", label: "DLSS SR"}, {id: "rtx", label: "RTX"}, {id: "5", label: "FSR 3.1"},
                                    {id: "6", label: "NVIDIA Image Scaling (NIS)"}]
    function srAlgorithmOf(q) { return q >= 1 && q <= 4 ? "rtx" : String(q) }
    function srTierOf(q) { return q >= 1 && q <= 4 ? q : 0 }
    function srQualityFor(algorithm, tier) { return algorithm === "rtx" ? (tier >= 1 && tier <= 4 ? tier : 2) : Number(algorithm) }
    function srTierChoices(q) { return [{id: "1", label: "1"}, {id: "2", label: "2"}, {id: "3", label: "3"}, {id: "4", label: "4"}] }
    signal navigate(string page)
    property string lastOpenedPath: ""
    property var lastExportFiles: []
    property bool recordingFolderOpened: false
    function openPath(path) { lastOpenedPath = path }
    function addExportFiles(paths) { lastExportFiles = paths }
    function openRecordingDirectory() { recordingFolderOpened = true }
    signal settingsChanged()
    signal chainChanged()
    signal snapshotChanged()
    function nrAutoSelectionReason(index) { return "Auto is unavailable in this four-layer UI fixture" }
    function effectAvailability(id) { return effectCapabilities[id] || {available:true, reason:""} }
    function resetNrLayer(index) {}
    function setPreference(key, value) {
        const copy = Object.assign({}, preferences)
        copy[key] = value
        preferences = copy
        return true
    }
    function logUi(category, text) {}
    function observeHdrCoverage(on) {}
    function applyScreenCrop(left, top, right, bottom) {
        appliedCrop = {left, top, right, bottom}
        return true
    }
)qml"
    // A third piece (see the note above on the size of one literal): 3.2.2.
    R"qml(    // PushFPS wide-screen fill (tst_push_quick_setup.qml): Veyra's aspect choice; the ambient fill is a preference.
    property int aspectMode: 0
    // PushFPS crash notice and graphics card test (tst_push_tools.qml; bridge/PushTools.cpp).
    property var crashReport: ({present: false})
    property int crashDismissed: 0
    function pushCrashReport() { return crashReport }
    function pushDismissCrashReport() { crashDismissed++; crashReport = ({present: false}) }
    property int logFolderOpened: 0
    function openLogFolder() { logFolderOpened++ }
    property var cardTestState: ({running: false, done: false})
    property var cardTestNext: ({running: true, done: false})
    property int cardTestStarts: 0
    property int cardTestCancels: 0
    function pushStartCardTest() { cardTestStarts++; cardTestState = cardTestNext; return cardTestNext.running === true }
    function pushCancelCardTest() { cardTestCancels++; cardTestState = ({running: false, done: true, ok: false, reason: "cancelled"}) }
    function pushCardTestStatus() { return cardTestState }
    property int performanceReportCopies: 0
    function pushPerformanceReport() { return "report: pushfps-performance" }
    function pushCopyPerformanceReport() { performanceReportCopies++; lastCopiedText = pushPerformanceReport(); return true }
)qml"
    // A fifth piece: the redesign.
    R"qml(    // PushFPS redesign (tst_push_design.qml): what the home page reads beyond the above.
    property bool captureRecovering: false
    property string captureSessionSummary: ""
    property var lastSource: ({})
    property var moonlight: null
    property var xbox: null
    property var ps5Control: ({enabled: false})
    // PushFPS mouse and keyboard (tst_push_kbm.qml). The maps are remoteplay/PushKbm.h's presets.
    property var pushKbm: ({built: true, on: false, available: false, captured: false, target: "", releaseName: "F12", waiting: "", waitingText: "", listenControl: -1, listenSlot: -1})
    // PushFPS Discord status (bridge/PushDiscord.cpp): the connection's state.
    property var pushDiscord: ({on: false, state: "off", showing: false, picture: false})
    property var ps5Profiles: []
    property var ps5ControlCalls: []
    property int ps5ControlRestarts: 0
    function ps5ControlSet(key, value) { ps5ControlCalls = ps5ControlCalls.concat([[key, value]]) }
    function ps5ControlRestart() { ps5ControlRestarts++ }
    property int kbmCaptures: 0
    property var kbmListened: []
    signal pushKbmBinding(int control, int slot, int code)
    function pushKbmCapture(on) { kbmCaptures++ }
    function pushKbmListen(control, slot) { kbmListened = [control, slot]; pushKbm = Object.assign({}, pushKbm, {listenControl: control, listenSlot: slot}) }
    function pushKbmStopListening() { pushKbm = Object.assign({}, pushKbm, {listenControl: -1, listenSlot: -1}) }
    function pushKbmInputName(code) {
        const names = {32: "Space", 27: "Esc", 160: "Left Shift", 162: "Left Ctrl", 164: "Left Alt", 123: "F12", 9: "Tab",
                       257: "Left mouse button", 258: "Right mouse button", 259: "Middle mouse button", 260: "Mouse back", 261: "Mouse forward",
                       272: "Wheel up", 273: "Wheel down"}
        return names[code] || (code >= 65 && code <= 90 ? String.fromCharCode(code) : "Key " + code)
    }
    function pushKbmControls() {
        return ["cross", "circle", "square", "triangle", "up", "down", "left", "right", "l1", "r1", "l2", "r2", "l3", "r3",
                "options", "view", "create", "home", "moveUp", "moveDown", "moveLeft", "moveRight", "walk"]
    }
    function pushKbmPresetMap(id) {
        const c = ch => ch.charCodeAt(0)
        const base = {up: [38, 0], down: [40, 0], left: [37, 0], right: [39, 0], l1: [9, 0], r1: [c("Q"), 0], l2: [258, 0], r2: [257, 0],
                      r3: [c("C"), 259], create: [0, 0], home: [0, 0], moveUp: [c("W"), 0], moveDown: [c("S"), 0], moveLeft: [c("A"), 0],
                      moveRight: [c("D"), 0], walk: [164, 0], options: [27, c("P")], view: [c("M"), 0]}
        if (id === "gta6") return Object.assign(base, {cross: [32, 0], circle: [162, 0], square: [c("R"), 0], triangle: [c("F"), c("E")], l3: [160, 0]})
        if (id === "gta6classic") return Object.assign(base, {cross: [160, 0], circle: [c("R"), 0], square: [32, 0], triangle: [c("F"), c("E")], l3: [162, 0]})
        if (id === "rdr2") return Object.assign(base, {cross: [160, 0], circle: [c("R"), c("F")], square: [32, 0], triangle: [c("E"), 0], l3: [162, 0],
                                                       up: [c("H"), 38], left: [c("J"), 37], right: [c("B"), 39], options: [27, c("M")], view: [c("V"), 0]})
        return ({})
    }
    function pushKbmDefaults() { return {on: false, preset: "gta6", sens: 50, vert: 100, dz: 15, curve: 100, invert: false, release: 123, map: pushKbmPresetMap("gta6")} }
    property var recentFiles: []
    property string remotePlayHost: ""
    property int captureDialogs: 0
    function openCaptureDialog() { captureDialogs++ }
    function openMoonlightDialog() {}
    function openNetworkDialog() {}
    function openPs5Dialog() {}
    function ps5OpenAccountChecker() {}
    function ps5FinishSignIn(pasted) {}
    function openScreenCaptureDialog() {}
    function openXboxDialog() {}
    function resumeLastSource() {}
    function showPlaylist() {}
})qml", QUrl());
        auto* model = snapshot.create(engine->rootContext());
        if (!model) qFatal("Cannot create explicit component snapshot: %s", qPrintable(snapshot.errorString()));
        model->setParent(engine);
        engine->rootContext()->setContextProperty(QStringLiteral("veyra"), model);
        qmlRegisterType(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("qml/Veyra/NrLayerEditor.qml"))), "VeyraTest", 1, 0, "NrLayerEditor");
        // ProPage is private to the production shell's directory, not exported
        // by Veyra/qmldir. Register it only here to test its real inline controls.
        qmlRegisterType(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("qml/Veyra/ProPage.qml"))), "VeyraTest", 1, 0, "ProPage");
        // The home page, likewise (tst_push_design.qml: its three panes in the new design).
        qmlRegisterType(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("qml/Veyra/HomePage.qml"))), "VeyraTest", 1, 0, "HomePage");
        qmlRegisterType(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("qml/Veyra/Ps5SourceDialog.qml"))), "VeyraTest", 1, 0, "Ps5SourceDialog");
        qmlRegisterType(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("qml/Veyra/ExportPage.qml"))), "VeyraTest", 1, 0, "ExportPage");
        qmlRegisterType(QUrl::fromLocalFile(QDir(QCoreApplication::applicationDirPath()).filePath(
            QStringLiteral("qml/Veyra/NvidiaLowLatencyEditor.qml"))), "VeyraTest", 1, 0, "NvidiaLowLatencyEditor");
    }
};

int main(int argc, char** argv) {
    const QString applicationDirectory = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
    const QString sourceDirectory = QDir(applicationDirectory).filePath(QStringLiteral("qml-tests"));
    if (!QFileInfo::exists(QDir(sourceDirectory).filePath(QStringLiteral("tst_components.qml")))) {
        fprintf(stderr, "missing staged Qt Quick Test source directory: %s\n", qPrintable(sourceDirectory));
        return 2;
    }
    const QByteArray sourceDirectoryBytes = sourceDirectory.toLocal8Bit();
    Setup setup;
    return quick_test_main_with_setup(argc, argv, "veyra_qml_quick_tests", sourceDirectoryBytes.constData(), &setup);
}

#include "QuickSmokeTests.moc"
