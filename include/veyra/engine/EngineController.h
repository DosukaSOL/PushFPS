#pragma once
#include <windows.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <condition_variable>
#include <functional>
#include <memory>
#include <optional>
#include "veyra/engine/EnhancementSettings.h"
#include "veyra/engine/EffectChain.h"
#include "veyra/engine/PresentationSettings.h"
#include "veyra/diagnostics/FrameMetrics.h"
#include "veyra/engine/PreviewView.h"
#include "veyra/engine/EngineWatchdog.h"
#include "veyra/sink/CaptureAudioSession.h"
#include "veyra/remoteplay/SessionInbox.h"
#ifdef VEYRA_ENABLE_MOONLIGHT
#include "veyra/source/MoonlightSessionSource.h"
#endif
#ifdef VEYRA_ENABLE_XBOX
#include "veyra/source/XboxSessionSource.h"
#endif
#include "veyra/media/AudioTrack.h"
#include "veyra/media/NetworkSettings.h"
#include "veyra/sink/VideoEncoder.h"
#include "veyra/engine/ExportOptions.h"
#include "veyra/engine/LiveRecorder.h"
#include "veyra/engine/PushHudSampler.h"
#include "veyra/engine/PushQualityProbe.h"
#include "veyra/engine/PushInputLatencyProbe.h"
#include "veyra/engine/VirtualCamera.h"
#include "veyra/engine/DisplayStateService.h"
namespace veyra::source { struct RemotePlayConnectDesc; class RemotePlaySessionSource; struct MoonlightConnectDesc; class MoonlightSessionSource; struct XboxConnectDesc; class XboxSessionSource; }
namespace veyra::remoteplay { struct ControllerState;struct ControllerFeedback; }
namespace veyra::sink { struct RgbaImage; }
namespace veyra::gfx { class D3D12DeviceContext; class CommandSlotRing; }
namespace veyra::engine {
class FrameFlowWindow;
struct PreviewGpuSession;
struct PlayerOptions { bool nr=false,sr=false,fg=false,realtime=true; uint32_t fgMultiplier=2; EnhancementSettings settings;
    // 「自动」抑制态由 settings.fgAutoMultiplier + 源帧率/输出上限在 EngineController
    // 的每帧循环里判定（previewFgMultiplier=1），不在这里另存状态：PlayerOptions 会在
    // 每次 applySettings 之后由 from() 重建，放在这里的字段会丢。
    std::optional<ChainRuntimeOrder> nodeOrder; // in-process only; not export shared memory
    int audioStreamIndex=-1; // export selection; -1 selects the container default
    double exportStartSeconds=0.0; // export-only trim; first complete frame at/after this time
    double exportEndSeconds=0.0; // export-only trim; 0 means source end
    sink::ExportRateControl exportRateControl=sink::ExportRateControl::Cq;
    ExportMediaOptions exportMedia;
    std::wstring exportTemporaryPath; // owned, uniquely reserved by this attempt
    bool captureReplayForTest=false; // file-backed live scheduler test; never enabled by UI
    bool captureCpuUnpack=false; // N1 diagnostic: legacy per-pixel CPU unpack
    bool softwareDecode=false; // user setting: files skip D3D12VA and decode on the CPU
    bool deinterlace=true; // automatic file deinterlace, preserves progressive bypass
    bool hardwareDecodeOnly=false; // user setting "强制硬解": files fail instead of falling back to software
    media::NetworkSettings network; // buffering / reconnect / damaged-picture choices for network sources and servers
    // Separate audio address for a DASH source (Bilibili): the video container has
    // no audio track, so the audio pipeline opens this instead.
    std::wstring audioPath;
    EnhancementSettings snapshot()const{auto s=settings;s.nr=nr;s.sr=sr;s.multiplier=fg?fgMultiplier:1;s.nrPolicy=realtime?(settings.nrPolicy==pipeline::NrSizePolicy::Native?pipeline::NrSizePolicy::Realtime:settings.nrPolicy):pipeline::NrSizePolicy::Native;return s;}
    static PlayerOptions from(EnhancementSettings s,std::optional<ChainRuntimeOrder> order=std::nullopt,const PlayerOptions* keep=nullptr){
        PlayerOptions o;o.nr=s.nr;o.sr=s.sr;o.fg=s.multiplier>1;o.fgMultiplier=std::max(2u,s.multiplier);o.realtime=s.nrPolicy!=pipeline::NrSizePolicy::Native;o.settings=s;o.nodeOrder=order;
        // Per-open state that EnhancementSettings does not carry: a rebuild would
        // otherwise drop the DASH audio address and the site's request headers.
        if(keep){o.audioPath=keep->audioPath;o.network.requestHeaders=keep->network.requestHeaders;o.network.userAgent=keep->network.userAgent;}
        return o;}
};
enum class TransportState { Empty, Opening, Playing, Paused, Ended, Stopping, Failed };
struct PlayerSnapshot {
    PresentationSettings presentation, presentationEffective;
    // XeSS/FSR owns Present; the UI greys the pacing controls when true.
    bool presentationProviderOwned=false;
    uint64_t presentationRevision=0;
    std::wstring presentationStatus;
    TransportState transport=TransportState::Empty;
    uint64_t sessionId=0,rejectedRevision=0;float volume=1;bool muted=false,audioAvailable=false;
    // Video memory that kept growing with no settings change (MiB above the session's low point),
    // 0 until the watchdog fires. Field logs 2026-10-01: +3 GB a minute in fullscreen only.
    uint64_t vramRunawayMiB=0;
    // Fullscreen-only sustained growth: the UI exits fullscreen and blocks it
    // for this source session. It does not change effects or driver settings.
    bool vramFullscreenUnsafe=false;
    // Known injected components loaded when the growth was seen (gfx::riskyInjections).
    std::wstring vramInjected;
    std::wstring status=L"请打开视频或图片";
    // Set only after the encoder has finished. The actual extension can differ
    // from the request (HDR screenshots are JXR); no UI-thread file polling.
    std::wstring savedFramePath;
    EnhancementSettings desired,applied;bool applying=false;
    bool nrActive=false,srActive=false,fgActive=false;
    bool nrAutoActive=false;
    uint32_t nrAutoPercent=0,nrAutoWidth=0,nrAutoHeight=0;
    std::wstring nrAutoStatus;
    // NR inference reuse: whether the preview graph runs it, whether Auto
    // chooses the level, the level (0 infers every frame, 1/2/3 reuse one
    // frame in every 4/3/2), frames reused so far, and why a requested mode is
    // not running (ASCII reason from EnhanceGraph::nrReuseIneligibility).
    bool nrReuseActive=false,nrReuseAutomatic=false;
    uint32_t nrReuseLevel=0;
    uint64_t nrReuseFrames=0;
    std::string nrReuseUnavailable;
    bool videoHdrActive=false; // TrueHDR actually running this graph (needs an HDR display path)
    std::wstring backendWarning;
    std::wstring sourceNotice;
    std::wstring colorStatus;
    // Custom (format label): the source's HDR format as one word ("Dolby Vision
    // P7", "HDR Vivid", "HDR10+", "HDR10", "HLG", "SDR"). The start-up badge and
    // the player bar both read it; empty for captures and images.
    std::wstring sourceHdrLabel;
    std::wstring videoHdrStatus;
    // Beauty nodes: whether the graph runs any, the faces the shared tracker saw a
    // few frames ago, and a problem that keeps them from finding faces.
    bool beautyActive=false;
    uint32_t beautyFaces=0;
    std::wstring beautyNotice;
    sink::CaptureAudioState captureAudio;
    unsigned audioInputChannels=0,audioOutputChannels=0;
    std::vector<media::AudioTrack> audioTracks;
    int selectedAudioTrack=-1;
    bool audioRebuffering=false;
    uint64_t audioVideoWaits=0;
    // Network sources and files on a server (#48): buffered seconds, rates, reconnects,
    // whether playback is paused for buffering and why. active=false for local files.
    media::NetworkStats network;
    bool audioEndpointRecovering=false;HRESULT audioEndpointError=S_OK;uint64_t audioEndpointRecoveries=0;
    uint64_t seekRequested=0,seekPresented=0;double seekTarget=0;
    double position=0,duration=0,fps=0,lateMs=0,lateP95Ms=0;
    double nominalSourceFps=0;
    // Source geometry for the UI: coded size and the container's display aspect
    // ratio (0 = use coded size). Known at open time, before the first frame.
    uint32_t sourceWidth=0,sourceHeight=0;
    bool sourceHdr=false;
    // The source's own HDR numbers, straight from the container. They survive the
    // hardware-decode path, where there is no CPU luma to measure a histogram from.
    unsigned sourceMasteringPeakNits=0;
    // Dolby Vision: stream-level profile/RPU flag plus the current frame's L1 peak.
    unsigned sourceDoviProfile=0;
    bool sourceDoviRpu=false;
    unsigned sourceDoviMaxNits=0;
    // True when the source declares a Dolby Vision enhancement layer, which is
    // what drives the "auto" composite switch in the UI.
    bool sourceDoviEnhancement=false;
    // True when the source carries HDR10+ (SMPTE ST 2094-40) scene metadata. The
    // follow switch serves this too, so the UI has to know the format to name it.
    bool sourceHdr10Plus=false;
    unsigned sourceMaxCllNits=0;
    unsigned sourceMaxFallNits=0;
    // Custom: the live coverage the presenter measured with its 64x36 probe (see
    // HdrOutputTuning.h). Percentages of the last probed frame; invalid until the
    // first probe completes, so the panel can say "not measured" instead of 0%.
    bool hdrCoverageValid=false;
    double hdrAboveCeilingPercent=0.0;
    double hdrInRollOffPercent=0.0;
    double hdrInLiftPercent=0.0;
    double sourceDisplayAspect=0.0;
    int sourceRotationDegrees=0;
    // Poster frame for the minimal-mode bar, as a small RGBA8 image. Empty when
    // unavailable; the UI must not fabricate one.
    std::vector<uint8_t> posterRgba;
    uint32_t posterWidth=0,posterHeight=0;
    diagnostics::FrameMetrics metrics;
    std::optional<double> submissionFps;
    std::optional<double> submissionLow1Fps; // 1% low of the presents, as an overlay reports it
    uint32_t flowPerf=0;int contentFps=0;
    // PushFPS Frame Pacing Repair counters (PushPacingRepair.h): kept frames seen, re-timed.
    uint64_t pacingFrames=0,pacingRepaired=0;
    int hudLockState=0;  // PushFPS Live HUD Lock: EnhanceGraph::hudLockState()
    int hudStillState=0; // PushFPS still rule: EnhanceGraph::hudStillState()
    int crosshairHoldState=0; // PushFPS Crosshair Hold: EnhanceGraph::crosshairHoldState()
    int crosshairHoldPixels=0; // pixels it held in the frame sampled last (0 = no crosshair found there)
    int subtitleHoldState=0;  // PushFPS Subtitle Hold: EnhanceGraph::subtitleHoldState()
    int subtitleHoldPixels=0; // pixels of the subtitle band it held in the frame sampled last
    uint64_t crosshairHoldFrames=0; // real frames the pass has seen since it was made
    // PushFlow while it runs: precision ("fp32"/"fp16"), full-resolution rebuild, inference p50/p95 (ms).
    std::string pushFlowPrecision; bool pushFlowRebuild=false; double pushFlowP50Ms=0, pushFlowP95Ms=0; uint32_t pushFlowMotionWidth=0, pushFlowMotionHeight=0;
    // PushFPS: PushFlow 2's encoder cache ("" not asked for, "on", or why it is not used) and what it did.
    std::string pushFlowEncoderCache; uint64_t pushFlowEncoderRuns=0, pushFlowFeaturesKept=0, pushFlowFeaturesMade=0; double pushFlowWholeMs=0, pushFlowSplitMs=0, pushFlowSplitCheckDb=0;
    // PushFPS: PushFlow 2's Guard ("" not asked for, "on", or why not) and the frames it judged, mended and held.
    std::string pushFlowGuard; uint64_t pushFlowGuardFrames=0, pushFlowGuardMended=0, pushFlowGuardHeld=0;
    // PushFPS: PushFlow 2's Predict ("" not asked for, "on", or why not): forecast frames made, the pixels
    // counted in them, how many of those the motion would move and how many of those were held, and the
    // forecasts not shown because the next real frame was already due.
    std::string pushFlowPredict; uint64_t pushFlowPredictFrames=0, pushFlowPredictCounted=0, pushFlowPredictMoving=0, pushFlowPredictHeld=0, pushFlowPredictSuperseded=0;
    // PushFPS: PushFlow 2's Dual ("" not asked for, "on", "needs-predict", "needs-2x") and the frames it has
    // handed to recordings and streams in place of the ones shown: real frames, true in-between frames.
    std::string pushFlowDual; uint64_t pushFlowDualReal=0, pushFlowDualBetween=0;
    // PushFPS: PushFlow 2's Memory, a measurement: pairs measured (with Predict), and how far apart two real
    // frames are where the motion joins them (0..255, averaged over pairs).
    uint64_t pushFlowMotionPairs=0; float pushFlowAlignedApart=0;
    uint64_t frames=0,generated=0;
    uint64_t captureReceived=0,captureDropped=0,nrEvaluated=0,nvofExecuted=0;
    // PushFPS: Present call to the vblank that showed the frame (milliseconds, -1 unknown), and how the
    // picture reaches the screen: 0 composed by the desktop, 1 an overlay plane, 2 directly, -1 unknown.
    double presentToScanoutMs=-1,presentToScanoutP95Ms=-1;int compositionMode=-1;
    uint64_t captureRateSkipped=0,processedCompleted=0;
    bool captureHalfRate=false;
    bool captureSampledBlack=false; // Sparse CPU-source samples, not no-signal detection.
    bool captureRecovering=false;unsigned captureReconnectAttempts=0;
    double captureFps=0,captureReadAgeMs=0,captureAgeMs=0,captureAgeP95Ms=0;
    double schedulingWaitP95Ms=0,processCpuP95Ms=0,presentCpuP95Ms=0;
    // Realtime file preview: dropped enhancement opportunities this metrics
    // window, and measured media-advance / wall-clock playback speed (1.0 =
    // normal speed). Zero previewSkipped means every decoded frame was enhanced.
    uint64_t previewSkipped=0;
    // Paused/still-image frames re-rendered because a live parameter changed
    // (colour is fused into the ingest dispatch, so a paused picture only
    // updates when the cached source frame is processed again). Test-visible
    // proof that "adjust while paused" works.
    uint64_t pausedFrameRefreshes=0;
    double playbackSpeed=0;
    double playbackRate=1; // requested file-preview transport rate; separate from measurement
    bool fgBudgetLimited=false,xessGenerationSuppressed=false;
    unsigned previewFgMultiplier=0;
    // Live FG output hint (DisplayFgBudget.h), never applied automatically:
    // 0 = the output fits the panel, 1 = suggest turning generation off,
    // otherwise the suggested lower multiplier. Refresh and content rate are
    // the inputs of that suggestion.
    unsigned displayFgSuggestion=0;
    double displayFgRefreshHz=0,displayFgContentFps=0;
    // Variable refresh of the presenting monitor (NVIDIA only, gfx::vrrLevel):
    // 0 unknown, 1 the monitor cannot do VRR, 2 it can but VRR is not possible
    // now, 3 possible, 4 active. presentationVrrSync: the 可变刷新 switch made
    // Automatic present like Vsync for it.
    unsigned displayVrr=0;
    bool displayVrrAmd=false; // the level above came from the AMD driver (FreeSync)
    bool presentationVrrSync=false;
    // Runtime-reported DLSSG MultiFrameCountMax: 1 = 2X only, 5 = 6X.
    // 0 = not queried yet (no session). The UI uses this to offer only the
    // multipliers the active GPU can actually honour.
    int fgMultiFrameMax=0;
    bool fgCapabilityKnown=false;
    // DLSS-G performance mode of the active graph (EnhanceGraph::dlssgLiteState):
    // 0 not requested, 1 active, 2 runtime missing, 3 GPU keeps the default runtime.
    uint32_t dlssgLiteState=0;
    // XeSS frame-generation ceiling (generated frames; 1 = stock 2X, 3 = 4X).
    // 0 = no XeSS session has reported yet; the settings UI then falls back to
    // the audited-provider probe.
    int xessMaxInterpolatedFrames=0;
    // AMD FSR frame-generation ceiling (generated frames per presented frame;
    // 1 = 2X). 0 = no FSR session has reported yet.
    int fsrMaxGeneratedFrames=0;
    std::string fsrProviderVersion;
    std::string fsrSrProviderVersion;   // PushFPS 4.2.1: the FSR upscaler's version, "" while FSR does not upscale
    bool running=false,failed=false,image=false,capture=false,remotePlay=false;
    // The session ended because the GPU stopped completing work (frame-ready timeout or a
    // removed device / driver reset), not because the source failed.
    bool gpuFailed=false;
    int remotePlayState=0; uint64_t remotePlaySkipped=0;
    bool remoteRecovering=false;unsigned remoteReconnectAttempts=0;std::wstring remoteRecoveryMessage;
    remoteplay::SessionInbox::Snapshot remoteStream;
    double remoteReceivedFps=0,remoteDecodedFps=0;bool remoteRatesReady=false;uint64_t remoteReceived=0,remoteDecoded=0,remoteIngressDropped=0;
    bool remoteMicrophoneLive=false;   // PushFPS voice chat: the console hears this PC's microphone now
#ifdef VEYRA_ENABLE_MOONLIGHT
    bool moonlightActive=false;source::MoonlightStats moonlight;
#endif
#ifdef VEYRA_ENABLE_XBOX
    bool xboxActive=false;source::XboxStats xbox;
#endif
};
class EngineController {
public:
    EngineController();
    ~EngineController();
    bool idle()const;
    bool prewarmEnhancement(EnhancementSettings,unsigned width=1920,unsigned height=1080);
    void setEnhancementPrewarmEnabled(bool);
    bool requestSettings(EnhancementSettings,std::optional<ChainRuntimeOrder> = std::nullopt,uint64_t* acceptedRevision=nullptr);
    // PushFPS: something outside the settings that shapes the graph has changed (PushFlow's motion size,
    // pushflow/PushFlowMotion.h). The current settings get a new revision, so the run loop describes the
    // graph again and rebuilds it if the description differs, exactly as after a settings change.
    void refreshGraph();
    void requestPresentation(PresentationSettings);
    void open(HWND video,const std::wstring& path,PlayerOptions options);
#ifdef VEYRA_ENABLE_REMOTEPLAY
    void openRemotePlay(HWND, source::RemotePlayConnectDesc, PlayerOptions);
    remoteplay::ControllerFeedback remotePlayFeedback();
    void remotePlayController(const remoteplay::ControllerState&);
    void remotePlayLoginPin(std::string);
    // PushFPS voice chat: mute or unmute this PC's microphone in a PS5 stream (not PS5 controller control).
    void remotePlayMicrophoneMuted(bool);
    // PS5 手柄控制 beside a capture card: a controller-only Remote Play session
    // (the console's keepalive video and audio are dropped on receipt). Start and
    // stop are queued to a dedicated thread, so callers never wait for the
    // console, and a newer request replaces a queued one. The session survives
    // capture re-opens; opening any other source, or a PS5 stream (the console has
    // one Remote Play slot), stops it. Pad, rumble and login PIN go through the
    // remotePlay* calls above when no PS5 stream runs.
    void startRemoteControl(source::RemotePlayConnectDesc);
    void stopRemoteControl();
    struct RemoteControlStatus{
        bool requested=false; // a start is queued or a session exists
        bool starting=false,alive=false,connected=false,pinRequired=false,recovering=false,failed=false;
        bool keepaliveReady=false;double keepaliveFps=0; // console video samples dropped per second
        std::wstring message;
    };
    RemoteControlStatus remoteControlStatus()const;
    bool remoteControlRequested()const; // cheap: a start is queued or a session exists
#endif
#ifdef VEYRA_ENABLE_MOONLIGHT
    void openMoonlight(HWND, source::MoonlightConnectDesc, PlayerOptions);
    // Input for the running Moonlight session; all are no-ops when none is streaming.
    void moonlightController(const remoteplay::ControllerState&);
    remoteplay::ControllerFeedback moonlightFeedback();
    void moonlightKey(uint32_t virtualKey, uint32_t scanCode, bool extended, bool down);
    void moonlightMouseMove(int dx, int dy);
    void moonlightMouseButton(int button, bool down);
    void moonlightScroll(int delta, bool horizontal);
    void moonlightReleaseInput();
#endif
#ifdef VEYRA_ENABLE_XBOX
    void openXbox(HWND, source::XboxConnectDesc, PlayerOptions);
    // Pad and rumble for the running Xbox session; no-ops when none is streaming.
    void xboxController(const remoteplay::ControllerState&);
    remoteplay::ControllerFeedback xboxFeedback();
#endif
    void stop();
    void comparison(int mode,bool base,float split=.5f){comparisonMode_=mode;comparisonBase_=base;comparisonSplit_=std::clamp(split,0.0f,1.0f);}
    void pause(bool p);
    bool setPlaybackRate(double rate);
    void setFullscreenMemoryProtection(bool enabled);
    void observeHdrCoverage(bool on){hdrCoverageObserved_=on;}
    void previewView(PreviewView view){if(!std::isfinite(view.zoom)||!std::isfinite(view.centerX)||!std::isfinite(view.centerY)||!std::isfinite(view.displayAspect)||view.displayAspect<0||view.mode<0||view.mode>6)return;std::lock_guard lock(mutex_);view.zoom=std::clamp(view.zoom,.05f,64.0f);view.cropLeft=PreviewView::cropSide(view.cropLeft);view.cropTop=PreviewView::cropSide(view.cropTop);view.cropRight=PreviewView::cropSide(view.cropRight);view.cropBottom=PreviewView::cropSide(view.cropBottom);if(view.displayAspect==0)view.displayAspect=previewView_.displayAspect;view.rotationDegrees=previewView_.rotationDegrees;view.userQuarterTurns=previewView_.userQuarterTurns;previewView_=view;}
    bool rotatePreviewClockwise(uint64_t session){std::lock_guard lock(mutex_);if(session!=snapshot_.sessionId||(!snapshot_.running&&!snapshot_.image))return false;previewView_.rotateClockwise();return true;}
    PreviewView previewView()const{std::lock_guard lock(mutex_);return previewView_;}
    void setVolume(float gain,bool mute);
    // Local capture monitoring only. Recording takes PCM before this gain.
    void setCaptureAudioMonitoring(bool enabled);
    bool selectAudioTrack(uint64_t sessionId,int streamIndex);
    void seek(double seconds){if(!std::isfinite(seconds)||seconds<0)return;std::lock_guard lock(mutex_);snapshot_.seekTarget=seconds;++snapshot_.seekRequested;seekSeconds_.store(seconds);}
    void saveFrame(const std::wstring& path);
    void startExport(const std::wstring& input,const std::wstring& output,PlayerOptions,bool hevc);
    PlayerSnapshot snapshot()const;
    // Live recording of the processed picture and the played sound
    // (LiveRecorder.h). Any thread; the file opens with the next presented
    // frame and is finished in the background.
    bool requestRecording(const RecordingSettings& options,std::wstring& error);
    void stopRecording();
    RecordingStatus recordingSnapshot()const;
    // PushFPS: the second recording (the vertical copy). It never changes the first one.
    bool requestSecondRecording(const RecordingSettings& options,std::wstring& error);
    void stopSecondRecording();
    RecordingStatus secondRecordingSnapshot()const;
    bool setVirtualCamera(bool enabled,uint32_t fps,std::wstring& error);
    bool uninstallVirtualCamera(std::wstring& error);
    bool installVirtualCamera(std::wstring& error);
    VirtualCameraStatus virtualCameraSnapshot()const;
    // Instant replay (#103): while enabled, the last options.replaySeconds of
    // whatever is presented stay in memory, across sessions; saveReplay()
    // writes them to a new file in the background. Any thread.
    bool setReplay(bool enabled,const RecordingSettings& options,std::wstring& error);
    bool saveReplay(const std::wstring& directory,std::wstring& error);
    RecordingStatus replaySnapshot()const;
    // PushFPS going live (LiveRecorder::Mode::Stream): the presented picture and sound to RTMP/RTMPS
    // servers, several at once from one encode. Stays on across source changes until stopped.
    bool requestStreaming(const RecordingSettings& options,std::wstring& error);
    void stopStreaming();
    RecordingStatus streamingSnapshot()const;
    // PushFPS automatic HUD detection (PushHudSampler.h): samples presented
    // real frames while it runs; hudDetection() polls it. Any thread.
    void startHudDetection(uint32_t samples,uint32_t intervalMs){hud_->start(samples,intervalMs);}
    void cancelHudDetection(){hud_->cancel();}
    PushHudSampler::Status hudDetection(){return hud_->status();}
    // PushFPS ground-truth quality test (PushQualityProbe.h): withholds every second frame from frame
    // generation for a few seconds and compares what it made with the originals.
    void startQualityTest(const PushQualityProbe::Options& o){quality_->start(o);}
    void cancelQualityTest(){quality_->cancel();}
    PushQualityProbe::Status qualityTest(){return quality_->status();}
    // PushFPS Remote Play latency test (PushInputLatencyProbe.h): presented-frame readback around a press.
    PushInputLatencyProbe& inputLatencyProbe(){return *inputLatency_;}
    // Refresh/VRR state of monitors, shared by every session's presenter and
    // the UI; queried in the background on events only. Any thread.
    DisplayStateService& displayStates(){return displayStates_;}
private:
    void run(HWND,std::wstring,PlayerOptions,std::shared_ptr<source::RemotePlayConnectDesc> remoteRequest={},std::shared_ptr<source::MoonlightConnectDesc> moonlightRequest={},std::shared_ptr<source::XboxConnectDesc> xboxRequest={});
    void runLargeImage(HWND,const sink::RgbaImage&,PlayerOptions,gfx::D3D12DeviceContext&,gfx::CommandSlotRing&);
    void post(std::function<void()>);
    void dispatch();
    void status(const std::wstring&,bool failed=false);
    void noteEngineHang(uint64_t stalledMs);
#ifdef VEYRA_ENABLE_REMOTEPLAY
    void controlLoop(std::stop_token);
    void waitRemoteControlIdle();
#endif
    mutable std::mutex mutex_;
    PlayerSnapshot snapshot_;
    PresentationSettings presentation_;
    uint64_t presentationRevision_=1;
    std::shared_ptr<FrameFlowWindow> activeFlow_;
    std::shared_ptr<source::RemotePlaySessionSource> activeRemote_;
#ifdef VEYRA_ENABLE_REMOTEPLAY
    std::shared_ptr<source::RemotePlaySessionSource> controlRemote_; // mutex_; PS5 手柄控制
    // controlMutex_ guards the queue below; the session thread never holds it
    // (nor mutex_) while it talks to the console.
    mutable std::mutex controlMutex_;
    std::condition_variable_any controlWake_;
    std::condition_variable controlIdle_;
    std::optional<std::shared_ptr<source::RemotePlayConnectDesc>> controlPending_; // empty pointer = stop
    bool controlBusy_=false,controlStarting_=false,controlExited_=false;
    std::jthread controlThread_; // started on first use; joined (bounded) in the destructor
#endif
    std::shared_ptr<source::MoonlightSessionSource> activeMoonlight_;
    std::shared_ptr<source::XboxSessionSource> activeXbox_;
    PreviewView previewView_;
    std::wstring savePath_;
    // Live recorder: every session's presenter records into it; it owns its
    // encoder and writer threads. Created before the engine thread starts.
    std::unique_ptr<LiveRecorder> recorder_=std::make_unique<LiveRecorder>();
    // PushFPS: a second recording beside the first, with its own size and shape (the vertical copy
    // for TikTok, Shorts and Reels: one session, two files). It takes the same presented frames,
    // has its own encoder, writer and file, and is idle unless a copy was asked for.
    std::unique_ptr<LiveRecorder> secondRecorder_=std::make_unique<LiveRecorder>();
    // The instant replay buffer, a second recorder; both take the presented
    // frame through captures_.
    std::unique_ptr<LiveRecorder> replay_=std::make_unique<LiveRecorder>(true);
    // PushFPS automatic HUD detection reads presented frames the same way.
    std::unique_ptr<PushHudSampler> hud_=std::make_unique<PushHudSampler>();
    // PushFPS going live: a third recorder in stream mode.
    std::unique_ptr<LiveRecorder> stream_=std::make_unique<LiveRecorder>(LiveRecorder::Mode::Stream);
    PresentCaptureFanout recorders_{*recorder_,*secondRecorder_};
    PresentCaptureFanout recordings_{recorders_,*replay_};
    PresentCaptureFanout liveOutputs_{recordings_,*stream_};
    std::unique_ptr<PushQualityProbe> quality_=std::make_unique<PushQualityProbe>();
    std::unique_ptr<PushInputLatencyProbe> inputLatency_=std::make_unique<PushInputLatencyProbe>();
    PresentCaptureFanout probes_{*quality_,*inputLatency_};
    PresentCaptureFanout hudAndQuality_{*hud_,probes_};
    // 2.2.0's virtual camera: one more optional consumer of presented frames.
    std::unique_ptr<VirtualCamera> virtualCamera_=std::make_unique<VirtualCamera>();
    PresentCaptureFanout outputsAndProbes_{liveOutputs_,hudAndQuality_};
    PresentCaptureFanout captures_{outputsAndProbes_,*virtualCamera_};
    // Declared before worker_: it outlives the engine thread's presenters.
    DisplayStateService displayStates_;
    std::thread worker_;
    std::unique_ptr<PreviewGpuSession> prewarmed_; // engine dispatcher only
    std::atomic<bool> prewarmEnabled_{false}; // off until the saved preference enables it (default off since 2026-10-05)
    std::condition_variable wake_;std::function<void()> pending_;bool shutdown_=false,busy_=false;
    // Owner-thread stall detection (field logs(15)); created before worker_ starts.
    std::unique_ptr<EngineWatchdog> watchdog_;
    std::condition_variable dispatchDone_;bool dispatchExited_=false; // mutex_
    std::wstring hangPreviousStatus_; // mutex_
    EnhancementSettings desired_;uint64_t nextRevision_=1;
    std::optional<ChainRuntimeOrder> desiredNodeOrder_; // protected by mutex_, paired with desired_
    std::atomic<bool> stop_{false},paused_{false},muted_{false};
    // PushFlow 2's Predict, engine thread only: the newest real frame's time and when it is due, so a
    // forecast made from the frame before it is never shown after it; and how many were dropped for that.
    int64_t predictNewestReal100ns_=0,predictNewestDue100ns_=0;uint64_t predictSuperseded_=0;
    // PushFlow 2's Dual, engine thread only: frames handed to recordings in place of the ones shown.
    uint64_t dualRealGiven_=0,dualBetweenGiven_=0;
    std::atomic<bool> captureAudioMonitoring_{true};
    std::atomic<float> volume_{1};uint64_t sessionId_=0;
    std::atomic<double> seekSeconds_{-1};
    std::atomic<double> requestedPlaybackRate_{1};
    std::atomic<bool> fullscreenMemoryProtection_{false};
    std::atomic<bool> hdrCoverageObserved_{false};
    int pendingAudioTrack_=-1;uint64_t pendingAudioSession_=0; // mutex_
    std::atomic<int> comparisonMode_{0};std::atomic<bool> comparisonBase_{false};std::atomic<float> comparisonSplit_{.5f};
    // Guarded by mutex_: DLSSG MultiFrameCountMax of the active session.
    int fgMultiFrameMaxCap_=0;
    // Guarded by mutex_: XeSS provider ceiling of the active session (0 = unknown).
    int xessMaxInterpolatedFramesCap_=0;
    // Guarded by mutex_: AMD FSR ceiling of the active session (0 = unknown).
    int fsrMaxGeneratedFramesCap_=0;
};
}
