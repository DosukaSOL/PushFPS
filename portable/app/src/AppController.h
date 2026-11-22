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
    Q_PROPERTY(bool removeDuplicates READ removeDuplicates WRITE setRemoveDuplicates NOTIFY settingsChanged)
    Q_PROPERTY(int quality READ quality WRITE setQuality NOTIFY settingsChanged)
    Q_PROPERTY(bool audioPassthrough READ audioPassthrough WRITE setAudioPassthrough NOTIFY settingsChanged)
    Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY settingsChanged)
    Q_PROPERTY(qreal brightness READ brightness WRITE setBrightness NOTIFY settingsChanged)
    Q_PROPERTY(qreal contrast READ contrast WRITE setContrast NOTIFY settingsChanged)
    Q_PROPERTY(qreal saturation READ saturation WRITE setSaturation NOTIFY settingsChanged)
    Q_PROPERTY(qreal gamma READ gamma WRITE setGamma NOTIFY settingsChanged)
    Q_PROPERTY(bool showStats READ showStats WRITE setShowStats NOTIFY settingsChanged)
    // 4.1: one tap for the shortest delay, the sound moved by hand, the capture card connected when the
    // app opens, and how a picture of another shape than the window is shown.
    Q_PROPERTY(bool lowLatency READ lowLatency WRITE setLowLatency NOTIFY settingsChanged)
    Q_PROPERTY(int audioTrimMs READ audioTrimMs WRITE setAudioTrimMs NOTIFY settingsChanged)
    Q_PROPERTY(bool autoConnect READ autoConnect WRITE setAutoConnect NOTIFY settingsChanged)
    Q_PROPERTY(int fillMode READ fillMode WRITE setFillMode NOTIFY settingsChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY settingsChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY settingsChanged)
    Q_PROPERTY(QStringList languageCodes READ languageCodes CONSTANT)
    Q_PROPERTY(QStringList languageNames READ languageNames CONSTANT)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    Q_PROPERTY(bool portable READ portable CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString platformName READ platformName CONSTANT)
    Q_PROPERTY(QString startPage READ startPage CONSTANT)
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
    bool showStats() const { return showStats_; }
    void setShowStats(bool v);
    bool lowLatency() const { return lowLatency_; }
    void setLowLatency(bool v);
    int audioTrimMs() const { return audioTrim_; }
    void setAudioTrimMs(int ms);
    bool autoConnect() const { return autoConnect_; }
    void setAutoConnect(bool v);
    int fillMode() const { return fillMode_; }
    void setFillMode(int m);
    bool alwaysOnTop() const { return onTop_; }
    void setAlwaysOnTop(bool v);
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

    Q_INVOKABLE void refreshDevices();
    Q_INVOKABLE void startCamera();
    Q_INVOKABLE void startFile(const QUrl& url);
    Q_INVOKABLE void startTestPattern(bool uneven = false);
    Q_INVOKABLE void stop();
    Q_INVOKABLE QString newScreenshotPath() const;
    Q_INVOKABLE void openScreenshotFolder() const;
    Q_INVOKABLE void resetPicture();
    Q_INVOKABLE void clearError();

    // Self-test (command line) access.
    CaptureSource* source() const { return source_; }

signals:
    void devicesChanged();
    void runningChanged();
    void settingsChanged();
    void statsChanged();
    void errorTextChanged();

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
    int targetMode_ = 3, quality_ = 1;
    bool pacing_ = false, hud_ = false, dedup_ = true, audioOn_ = true, showStats_ = false, onTop_ = false;
    bool lowLatency_ = false, autoConnect_ = false, applyingLowLatency_ = false, savedCameraFound_ = false;
    bool autoConnectPending_ = false;
    QByteArray audioInId_, audioOutId_;
    QString audioInName_, audioOutName_;
    int audioTrim_ = 0, fillMode_ = 0;
    qreal volume_ = 1.0, brightness_ = 0, contrast_ = 1, saturation_ = 1, gamma_ = 1;
    QString language_ = QStringLiteral("auto");
    QVariantMap stats_;
    QString error_;
    QTimer statsTimer_;
    qint64 startedAt_ = 0;
};

}  // namespace pushfps
