// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 DosukaSOL and the PushFPS contributors. Part of PushFPS, a modified version of Veyra.
#pragma once
// Everything the interface talks to: sources, devices, frame generation settings, picture, sound,
// statistics, language. Settings are saved as they change.
#include "AudioPassthrough.h"
#include "CaptureSource.h"
#include "Translator.h"

#include "veyra/portable/Engine.h"

#include <QCameraDevice>
#include <QMediaDevices>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <memory>

class QQmlEngine;
class QSettings;

namespace pushfps {

class AppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList videoDevices READ videoDevices NOTIFY devicesChanged)
    Q_PROPERTY(int videoDeviceIndex READ videoDeviceIndex WRITE setVideoDeviceIndex NOTIFY devicesChanged)
    Q_PROPERTY(QStringList formats READ formats NOTIFY devicesChanged)
    Q_PROPERTY(int formatIndex READ formatIndex WRITE setFormatIndex NOTIFY devicesChanged)
    Q_PROPERTY(QStringList audioInputs READ audioInputs NOTIFY devicesChanged)
    Q_PROPERTY(int audioInputIndex READ audioInputIndex WRITE setAudioInputIndex NOTIFY devicesChanged)
    Q_PROPERTY(QStringList audioOutputs READ audioOutputs NOTIFY devicesChanged)
    Q_PROPERTY(int audioOutputIndex READ audioOutputIndex WRITE setAudioOutputIndex NOTIFY devicesChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString sourceKind READ sourceKind NOTIFY runningChanged)
    Q_PROPERTY(int targetMode READ targetMode WRITE setTargetMode NOTIFY settingsChanged)
    Q_PROPERTY(bool pacingRepair READ pacingRepair WRITE setPacingRepair NOTIFY settingsChanged)
    Q_PROPERTY(bool hudLock READ hudLock WRITE setHudLock NOTIFY settingsChanged)
    // Crosshair Hold (portable/core CrosshairHold.h): off unless switched on.
    Q_PROPERTY(bool crosshairHold READ crosshairHold WRITE setCrosshairHold NOTIFY settingsChanged)
    // HUD protection boxes (the Windows build's "Exclude all effects" regions, its pushSetExclusionRegions):
    // at most four {left, top, right, bottom, ellipse} as shares of the picture, kept as they were cleaned;
    // FrameView shows the older real frame inside them in every generated frame. Set by setHudRegions.
    Q_PROPERTY(QVariantList hudRegions READ hudRegions NOTIFY settingsChanged)
    Q_PROPERTY(double hudFeather READ hudFeather NOTIFY settingsChanged)
    Q_PROPERTY(bool subtitleHold READ subtitleHold WRITE setSubtitleHold NOTIFY settingsChanged)
    Q_PROPERTY(bool removeDuplicates READ removeDuplicates WRITE setRemoveDuplicates NOTIFY settingsChanged)
    Q_PROPERTY(int quality READ quality WRITE setQuality NOTIFY settingsChanged)
    Q_PROPERTY(bool audioPassthrough READ audioPassthrough WRITE setAudioPassthrough NOTIFY settingsChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY settingsChanged)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY settingsChanged)
    Q_PROPERTY(qreal contrast READ contrast WRITE setContrast NOTIFY settingsChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY settingsChanged)
    Q_PROPERTY(qreal gamma READ gamma WRITE setGamma NOTIFY settingsChanged)
    // 4.1: one tap for the shortest delay, the sound moved by hand, the capture card connected when the
    // app opens, and how a picture of another shape than the window is shown.
    Q_PROPERTY(bool lowLatency READ lowLatency WRITE setLowLatency NOTIFY settingsChanged)
    Q_PROPERTY(int audioTrimMs READ audioTrimMs WRITE setAudioTrimMs NOTIFY settingsChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY settingsChanged)
    Q_PROPERTY(int fillMode READ fillMode WRITE setFillMode NOTIFY settingsChanged)
    // The live comparison with the picture before PushFPS's processing, as the Windows build's
    // compareMode: 0 off, 2 split (the line at compareSplit, a share of the picture's width), 1 the
    // original alone. Frame generation pauses while it is on, as on Windows. Not kept across starts;
    // which side is which is preference pushCompareSwap.
    Q_PROPERTY(int compareMode READ compareMode WRITE setCompareMode NOTIFY compareChanged)
    Q_PROPERTY(double compareSplit READ compareSplit WRITE setCompareSplit NOTIFY compareChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY settingsChanged)
    // Upscaling with NVIDIA Image Scaling, under the Windows build's names: on or off, and the size it
    // upscales towards (1 = 2K, 2 = 4K, 4 = 5K, 5 = 6K: FrameView::upscaleTargetSize). Its sharpening is the
    // preference nisSharpness.
    Q_PROPERTY(bool srEnabled READ srEnabled WRITE setSrEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int srTargetIndex READ srTargetIndex WRITE setSrTargetIndex NOTIFY settingsChanged)
    // The upscaling algorithm, by the Windows build's numbers (engine::kVideoSrNis = 6). The preview has
    // NVIDIA Image Scaling only; another value is never kept.
    Q_PROPERTY(int videoSrQuality READ videoSrQuality WRITE setVideoSrQuality NOTIFY settingsChanged)
    // What the picture's upscaling does now (FrameView::UpscaleState) and its sizes; Main.qml hands them
    // over from the picture, which is drawn on another thread. Not saved.
    Q_PROPERTY(int upscaleState READ upscaleState WRITE setUpscaleState NOTIFY upscaleChanged)
    Q_PROPERTY(QString upscaleSizes READ upscaleSizes WRITE setUpscaleSizes NOTIFY upscaleChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY settingsChanged)
    Q_PROPERTY(QStringList languageCodes READ languageCodes CONSTANT)
    Q_PROPERTY(QStringList languageNames READ languageNames CONSTANT)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool portable READ portable CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString platformName READ platformName CONSTANT)
    Q_PROPERTY(QString startPage READ startPage CONSTANT)
    // What is playing, by name (the capture card, the file, the test picture); empty with nothing playing.
    Q_PROPERTY(QString sourceTitle READ sourceTitle NOTIFY runningChanged)
    // The interface's own choices, under the Windows build's preference names so the QML the two share
    // reads them the same way (theme, accent, folded sides and sections). Checked in setPreference.
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY preferencesChanged)
public:
    AppController(QQmlEngine* qml, CatalogTranslator* translator, QString startPage, QObject* parent = nullptr);
    ~AppController() override;
    std::shared_ptr<veyra::portable::Engine> engine() const { return engine_; }

    QStringList videoDevices() const;
    int videoDeviceIndex() const { return videoIndex_; }
    void setVideoDeviceIndex(int i);
    QStringList formats() const;
    int formatIndex() const { return formatIndex_; }
    void setFormatIndex(int i);
    QStringList audioInputs() const;
    int audioInputIndex() const { return audioInIndex_; }
    void setAudioInputIndex(int i);
    QStringList audioOutputs() const;
    int audioOutputIndex() const { return audioOutIndex_; }
    void setAudioOutputIndex(int i);
    bool running() const;
    QString sourceKind() const;
    int targetMode() const { return targetMode_; }
    void setTargetMode(int m);
    bool pacingRepair() const { return pacing_; }
    void setPacingRepair(bool v);
    bool hudLock() const { return hud_; }
    void setHudLock(bool v);
    bool crosshairHold() const { return crosshairHold_; }
    void setCrosshairHold(bool v);
    QVariantList hudRegions() const { return hudRegions_; }
    double hudFeather() const { return hudFeather_; }
    bool subtitleHold() const { return subtitleHold_; }
    void setSubtitleHold(bool v);
    bool removeDuplicates() const { return dedup_; }
    void setRemoveDuplicates(bool v);
    int quality() const { return quality_; }
    void setQuality(int q);
    bool audioPassthrough() const { return audioOn_; }
    void setAudioPassthrough(bool v);
    qreal volume() const { return volume_; }
    void setVolume(qreal v);
    qreal brightness() const { return brightness_; }
    void setBrightness(qreal v);
    qreal contrast() const { return contrast_; }
    void setContrast(qreal v);
    qreal saturation() const { return saturation_; }
    void setSaturation(qreal v);
    qreal gamma() const { return gamma_; }
    void setGamma(qreal v);
    bool lowLatency() const { return lowLatency_; }
    void setLowLatency(bool v);
    int audioTrimMs() const { return audioTrim_; }
    void setAudioTrimMs(int ms);
    bool autoConnect() const { return autoConnect_; }
    void setAutoConnect(bool v);
    int fillMode() const { return fillMode_; }
    void setFillMode(int m);
    int compareMode() const { return compareMode_; }
    void setCompareMode(int m);
    double compareSplit() const { return compareSplit_; }
    void setCompareSplit(double v);
    bool alwaysOnTop() const { return onTop_; }
    void setAlwaysOnTop(bool v);
    bool srEnabled() const { return srEnabled_; }
    void setSrEnabled(bool v);
    int srTargetIndex() const { return srTarget_; }
    void setSrTargetIndex(int v);
    int videoSrQuality() const { return srAlgorithm_; }
    void setVideoSrQuality(int v);
    int upscaleState() const { return upscaleState_; }
    void setUpscaleState(int v) { if (v != upscaleState_) { upscaleState_ = v; emit upscaleChanged(); } }
    QString upscaleSizes() const { return upscaleSizes_; }
    void setUpscaleSizes(const QString& v) { if (v != upscaleSizes_) { upscaleSizes_ = v; emit upscaleChanged(); } }
    QString language() const { return language_; }
    void setLanguage(const QString& code);
    QStringList languageCodes() const;
    QStringList languageNames() const;
    QVariantMap stats() const { return stats_; }
    QString errorText() const { return error_; }
    bool portable() const;
    QString version() const;
    QString platformName() const;
    QString startPage() const { return startPage_; }
    QString sourceTitle() const { return running() ? sourceTitle_ : QString(); }
    QVariantMap preferences() const { return prefs_; }
    Q_INVOKABLE void setPreference(const QString& key, const QVariant& value);

    Q_INVOKABLE void refreshDevices();
    // Check my setup (CheckSetupDialog.qml, rules in the Windows build's PushDiagnose.js):
    // a sharper capture format for the card playing now, as the Windows build's PushCaptureSharper.h
    // decides it: { index, fps, pixel } (index in `formats`), or an empty map;
    Q_INVOKABLE QVariantMap sharperFormat() const;
    // and whether the playing card's own sound input was found (what "Automatic" passes through).
    Q_INVOKABLE bool captureSoundFound() const;
    Q_INVOKABLE void startCamera();
    Q_INVOKABLE void startFile(const QUrl& url);
    Q_INVOKABLE void startTestPattern(bool uneven = false);
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString newScreenshotPath() const;
    Q_INVOKABLE void openScreenshotFolder() const;
    Q_INVOKABLE void resetPicture();
    // A dialog of the main window asked for by a part that cannot reach it ("crop"): Main.qml opens it.
    Q_INVOKABLE void openDialog(const QString& key) { emit dialogRequested(key); }
    // The boxes and their feather (0..64 picture pixels); an empty list turns HUD protection off. Always accepted.
    Q_INVOKABLE bool setHudRegions(const QVariantList& regions, double feather);
    Q_INVOKABLE void clearError();

    // Self-test (command line) access.
    CaptureSource* source() const { return source_; }

signals:
    void devicesChanged();
    void runningChanged();
    void settingsChanged();
    void compareChanged();
    void statsChanged();
    void errorTextChanged();
    void preferencesChanged();
    void upscaleChanged();
    void dialogRequested(const QString& key);

private:
    void applyEngineSettings();
    void save(const QString& key, const QVariant& value);
    void setError(const QString& text);
    void updateStats();
    void startAudioIfWanted();
    void startCameraNow();
    QString formatKey() const;
    void restoreFormat();
    int savedCameraIndex() const;
    void rememberCamera();

    QQmlEngine* qml_;
    CatalogTranslator* translator_;
    QString startPage_;
    std::unique_ptr<QSettings> settings_;
    std::shared_ptr<veyra::portable::Engine> engine_;
    CaptureSource* source_ = nullptr;
    AudioPassthrough* audio_ = nullptr;
    QMediaDevices* devices_ = nullptr;
    QList<QCameraDevice> cameras_;
    QList<QAudioDevice> inputs_, outputs_;
    int videoIndex_ = 0, formatIndex_ = -1, audioInIndex_ = 0, audioOutIndex_ = 0;
    int playingVideo_ = -1, playingFormat_ = -1;  // what the running capture was opened with
    int targetMode_ = 3, quality_ = 1;
    bool pacing_ = false, hud_ = false, dedup_ = true, audioOn_ = true, onTop_ = false;
    bool srEnabled_ = false;
    int srTarget_ = 2, srAlgorithm_ = 6, upscaleState_ = 0;
    QString upscaleSizes_;
    bool lowLatency_ = false, autoConnect_ = false, applyingLowLatency_ = false, savedCameraFound_ = false;
    bool crosshairHold_ = false;
    QVariantList hudRegions_;
    double hudFeather_ = 8.0;
    bool subtitleHold_ = false;   // Subtitle Hold: off unless switched on (not everyone uses subtitles)
    bool autoConnectPending_ = false;
    QByteArray audioInId_, audioOutId_;
    QString audioInName_, audioOutName_;
    int audioTrim_ = 0, fillMode_ = 0;
    int compareMode_ = 0;
    double compareSplit_ = 0.5;
    qreal volume_ = 1.0, brightness_ = 0, contrast_ = 1, saturation_ = 1, gamma_ = 1;
    QString language_ = QStringLiteral("auto");
    QString sourceTitle_;
    QVariantMap prefs_;
    void loadPreferences();
    QVariantMap stats_;
    QString error_;
    QTimer statsTimer_;
    qint64 startedAt_ = 0;
};

}  // namespace pushfps
