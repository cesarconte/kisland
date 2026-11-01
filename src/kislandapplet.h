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
    Q_PROPERTY(QVariantList spectrumBars READ spectrumBars NOTIFY spectrumChanged)
    Q_PROPERTY(bool timerActive READ timerActive NOTIFY timerChanged)
    Q_PROPERTY(QString timerRemainingText READ timerRemainingText NOTIFY timerChanged)
    Q_PROPERTY(QString timerLabel READ timerLabel NOTIFY timerChanged)
    Q_PROPERTY(QString kiriState READ kiriState NOTIFY kiriStateChanged)
    Q_PROPERTY(int cpuUsagePercent READ cpuUsagePercent NOTIFY vitalsChanged)
    Q_PROPERTY(int ramUsagePercent READ ramUsagePercent NOTIFY vitalsChanged)

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
    QVariantList spectrumBars() const;
    bool timerActive() const;
    QString timerRemainingText() const;
    QString timerLabel() const;
    QString kiriState() const;
    int cpuUsagePercent() const;
    int ramUsagePercent() const;

    // Q_INVOKABLE Actions
    Q_INVOKABLE void playPauseMedia();
    Q_INVOKABLE void nextTrack();
    Q_INVOKABLE void previousTrack();
    Q_INVOKABLE void startQuickTimer(int durationSeconds, const QString &label = QStringLiteral("Temporizador"));
    Q_INVOKABLE void cancelQuickTimer();
    Q_INVOKABLE void invokeKiriVoice();

    // Static helpers for unit testing
    static QVariantList computeSpectrumBarsFromPcm(const QByteArray &pcmChunk, int barCount = 6);
    static QString formatCountdown(qint64 remainingSeconds);
    static QVariantMap parsePlayerctlLine(const QString &line);
    static QVariantMap extractMprisMetadata(const QVariant &metadataVariant);
    static QString resolveCoverArtOrVideoThumbnail(const QString &artUrl, const QString &mediaUrl, bool *isVideoOut = nullptr);

Q_SIGNALS:
    void mediaStateChanged();
    void spectrumChanged();
    void timerChanged();
    void kiriStateChanged();
    void vitalsChanged();

private Q_SLOTS:
    void pollMprisAndActivities();
    void onTimerTick();
    void onPipeWireAudioChunk();

private:
    void startPipeWireSpectrumMonitor();
    void stopPipeWireSpectrumMonitor();
    void updateSystemVitals();

    bool m_mediaPlaying = false;
    QString m_trackTitle;
    QString m_trackArtist;
    QString m_trackAlbum;
    QString m_trackArtUrl;
    bool m_isVideoMedia = false;
    QString m_playerName;
    QString m_activeMprisService;

    QVariantList m_spectrumBars = {0.15, 0.25, 0.20, 0.30, 0.18, 0.15};
    QProcess *m_pwMonitorProc = nullptr;

    bool m_timerActive = false;
    QDateTime m_timerTarget;
    QString m_timerLabel;
    QString m_timerRemainingText;

    QString m_kiriState = QStringLiteral("idle");
    int m_cpuUsagePercent = 0;
    int m_ramUsagePercent = 0;
    qint64 m_prevCpuTotal = 0;
    qint64 m_prevCpuIdle = 0;

    QTimer *m_pollTimer = nullptr;
    QTimer *m_countdownTimer = nullptr;
};
