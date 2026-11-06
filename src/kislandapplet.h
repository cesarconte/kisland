/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <Plasma/Applet>
#include <QDateTime>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

class SongRecognizer;

class KIslandApplet : public Plasma::Applet
{
    Q_OBJECT

    Q_PROPERTY(bool mediaPlaying READ mediaPlaying NOTIFY mediaStateChanged)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY mediaStateChanged)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY mediaStateChanged)
    Q_PROPERTY(QString trackAlbum READ trackAlbum NOTIFY mediaStateChanged)
    Q_PROPERTY(QString trackArtUrl READ trackArtUrl NOTIFY mediaStateChanged)
    Q_PROPERTY(bool isVideoMedia READ isVideoMedia NOTIFY mediaStateChanged)
    Q_PROPERTY(QString playerName READ playerName NOTIFY mediaStateChanged)
    Q_PROPERTY(qint64 trackPositionSecs READ trackPositionSecs NOTIFY mediaStateChanged)
    Q_PROPERTY(qint64 trackDurationSecs READ trackDurationSecs NOTIFY mediaStateChanged)
    Q_PROPERTY(QString trackElapsedText READ trackElapsedText NOTIFY mediaStateChanged)
    Q_PROPERTY(QString trackRemainingText READ trackRemainingText NOTIFY mediaStateChanged)
    Q_PROPERTY(double trackProgress READ trackProgress NOTIFY mediaStateChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled NOTIFY mediaStateChanged)
    Q_PROPERTY(QString loopStatus READ loopStatus NOTIFY mediaStateChanged)
    Q_PROPERTY(QVariantList spectrumBars READ spectrumBars NOTIFY spectrumChanged)
    Q_PROPERTY(bool timerActive READ timerActive NOTIFY timerChanged)
    Q_PROPERTY(bool timerPaused READ timerPaused NOTIFY timerChanged)
    Q_PROPERTY(double timerProgress READ timerProgress NOTIFY timerChanged)
    Q_PROPERTY(QString timerRemainingText READ timerRemainingText NOTIFY timerChanged)
    Q_PROPERTY(QString timerLabel READ timerLabel NOTIFY timerChanged)
    Q_PROPERTY(QString kiriState READ kiriState NOTIFY kiriStateChanged)
    Q_PROPERTY(QString kityState READ kityState NOTIFY kityStateChanged)
    Q_PROPERTY(int cpuUsagePercent READ cpuUsagePercent NOTIFY vitalsChanged)
    Q_PROPERTY(int ramUsagePercent READ ramUsagePercent NOTIFY vitalsChanged)
    Q_PROPERTY(int outputVolumePercent READ outputVolumePercent NOTIFY audioChanged)
    Q_PROPERTY(bool outputMuted READ outputMuted NOTIFY audioChanged)
    Q_PROPERTY(QString outputDeviceName READ outputDeviceName NOTIFY audioChanged)
    Q_PROPERTY(QObject *songRecognizer READ songRecognizer CONSTANT)
    Q_PROPERTY(bool micActive READ micActive NOTIFY privacyChanged)
    Q_PROPERTY(QString micApp READ micApp NOTIFY privacyChanged)
    Q_PROPERTY(bool cameraOrScreenActive READ cameraOrScreenActive NOTIFY privacyChanged)
    Q_PROPERTY(QString cameraOrScreenApp READ cameraOrScreenApp NOTIFY privacyChanged)
    Q_PROPERTY(bool focusActive READ focusActive NOTIFY focusStateChanged)
    Q_PROPERTY(QString focusModeName READ focusModeName NOTIFY focusStateChanged)
    Q_PROPERTY(QString focusModeIcon READ focusModeIcon NOTIFY focusStateChanged)
    Q_PROPERTY(bool focusDndActive READ focusDndActive NOTIFY focusStateChanged)

public:
    explicit KIslandApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args);
    ~KIslandApplet() override;

    void init() override;

    bool mediaPlaying() const;
    QString trackTitle() const;
    QString trackArtist() const;
    QString trackAlbum() const;
    QString trackArtUrl() const;
    bool isVideoMedia() const;
    QString playerName() const;
    qint64 trackPositionSecs() const;
    qint64 trackDurationSecs() const;
    QString trackElapsedText() const;
    QString trackRemainingText() const;
    double trackProgress() const;
    bool shuffleEnabled() const;
    QString loopStatus() const;
    QVariantList spectrumBars() const;
    bool timerActive() const;
    bool timerPaused() const;
    double timerProgress() const;
    QString timerRemainingText() const;
    QString timerLabel() const;
    QString kiriState() const;
    QString kityState() const;
    int cpuUsagePercent() const;
    int ramUsagePercent() const;
    int outputVolumePercent() const;
    bool outputMuted() const;
    QString outputDeviceName() const;
    QObject *songRecognizer() const;
    bool micActive() const;
    QString micApp() const;
    bool cameraOrScreenActive() const;
    QString cameraOrScreenApp() const;
    bool focusActive() const;
    QString focusModeName() const;
    QString focusModeIcon() const;
    bool focusDndActive() const;

    // Q_INVOKABLE Actions
    Q_INVOKABLE void playPauseMedia();
    Q_INVOKABLE void nextTrack();
    Q_INVOKABLE void previousTrack();
    Q_INVOKABLE void seekToRatio(double ratio);
    Q_INVOKABLE void skipBySeconds(int deltaSeconds);
    Q_INVOKABLE void toggleShuffle();
    Q_INVOKABLE void cycleLoopStatus();
    Q_INVOKABLE void raiseMediaPlayer();
    Q_INVOKABLE void startQuickTimer(int durationSeconds, const QString &label = QStringLiteral("Temporizador"));
    Q_INVOKABLE void togglePauseTimer();
    Q_INVOKABLE void cancelQuickTimer();
    Q_INVOKABLE void invokeKity();
    Q_INVOKABLE void invokeKiriVoice();
    Q_INVOKABLE void setOutputVolume(int percent);
    Q_INVOKABLE void toggleOutputMute();
    Q_INVOKABLE void launchDropover();
    Q_INVOKABLE void launchLiveCaptions();
    Q_INVOKABLE QVariantList getKdeConnectDevices() const;
    Q_INVOKABLE bool shareToKdeConnect(const QString &filePath, const QString &deviceId);
    Q_INVOKABLE void openAirDropSettings();
    Q_INVOKABLE void openFileLocation(const QString &filePath);
    Q_INVOKABLE void toggleFocusDnd();

    // Static helpers for unit testing
    static QVariantList computeSpectrumBarsFromPcm(const QByteArray &pcmChunk, int barCount = 6);
    static QString formatCountdown(qint64 remainingSeconds);
    static QVariantMap parsePlayerctlLine(const QString &line);
    static QVariantMap extractMprisMetadata(const QVariant &metadataVariant);
    static QString resolveCoverArtOrVideoThumbnail(const QString &artUrl, const QString &mediaUrl, bool *isVideoOut = nullptr);
    static QVariantMap parseWpctlStatus(const QString &wpctlOutput, const QString &kiriState = QStringLiteral("idle"));

Q_SIGNALS:
    void mediaStateChanged();
    void spectrumChanged();
    void timerChanged();
    void timerFinished(const QString &label, qint64 totalSecs);
    void kiriStateChanged();
    void kityStateChanged();
    void vitalsChanged();
    void audioChanged();
    void privacyChanged();
    void focusStateChanged();
    void volumeAlert(int percent, bool muted);
    void peripheralAlert(const QString &title, const QString &subtitle, const QString &icon);
    void focusAlert(const QString &title, const QString &icon);

private Q_SLOTS:
    void pollMprisAndActivities();
    void onTimerTick();
    void onPipeWireAudioChunk();

private:
    void startPipeWireSpectrumMonitor();
    void stopPipeWireSpectrumMonitor();
    void updateSystemVitals();
    void updatePipeWireAudioAndPrivacy();
    void updateBluetoothAndPeripherals();
    void updateFocusMode();

    bool m_mediaPlaying = false;
    QString m_trackTitle;
    QString m_trackArtist;
    QString m_trackAlbum;
    QString m_trackArtUrl;
    QString m_trackObjectId;
    bool m_isVideoMedia = false;
    QString m_playerName;
    QString m_activeMprisService;
    qint64 m_trackPositionSecs = 0;
    qint64 m_trackDurationSecs = 0;
    bool m_shuffleEnabled = false;
    QString m_loopStatus = QStringLiteral("None");

    QVariantList m_spectrumBars = {0.15, 0.25, 0.20, 0.30, 0.18, 0.15};
    QProcess *m_pwMonitorProc = nullptr;

    bool m_timerActive = false;
    bool m_timerPaused = false;
    qint64 m_timerTotalSecs = 0;
    qint64 m_timerPausedRemainingSecs = 0;
    QDateTime m_timerTarget;
    QString m_timerLabel;
    QString m_timerRemainingText;

    QString m_kityState = QStringLiteral("idle");
    int m_cpuUsagePercent = 0;
    int m_ramUsagePercent = 0;
    qint64 m_prevCpuTotal = 0;
    qint64 m_prevCpuIdle = 0;

    int m_outputVolumePercent = 40;
    bool m_outputMuted = false;
    QString m_outputDeviceName = QStringLiteral("PipeWire");
    bool m_micActive = false;
    QString m_micApp;
    bool m_cameraOrScreenActive = false;
    QString m_cameraOrScreenApp;

    SongRecognizer *m_songRecognizer = nullptr;

    bool m_focusActive = false;
    bool m_focusDndActive = false;
    QString m_focusModeName;
    QString m_focusModeIcon;
    QString m_focusModeId;
    QSet<QString> m_knownBtDevices;
    bool m_btInitialized = false;
    bool m_volumeInitialized = false;
    bool m_focusInitialized = false;

    QTimer *m_pollTimer = nullptr;
    QTimer *m_countdownTimer = nullptr;
};
