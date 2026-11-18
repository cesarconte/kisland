/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QVariantMap>

class MediaEngine : public QObject
{
    Q_OBJECT

public:
    explicit MediaEngine(QObject *parent = nullptr);
    ~MediaEngine() override;

    bool mediaPlaying() const { return m_mediaPlaying; }
    QString trackTitle() const { return m_trackTitle; }
    QString trackArtist() const { return m_trackArtist; }
    QString trackAlbum() const { return m_trackAlbum; }
    QString trackArtUrl() const { return m_trackArtUrl; }
    bool isVideoMedia() const { return m_isVideoMedia; }
    QString playerName() const { return m_playerName; }
    qint64 trackPositionSecs() const { return m_trackPositionSecs; }
    qint64 trackDurationSecs() const { return m_trackDurationSecs; }
    QString trackElapsedText() const;
    QString trackRemainingText() const;
    double trackProgress() const;
    bool shuffleEnabled() const { return m_shuffleEnabled; }
    QString loopStatus() const { return m_loopStatus; }
    QVariantList spectrumBars() const { return m_spectrumBars; }

    void pollPlayers(bool songRecognizerListening = false);
    void playPauseMedia();
    void nextTrack();
    void previousTrack();
    void seekToRatio(double ratio);
    void skipBySeconds(int deltaSeconds);
    void toggleShuffle();
    void cycleLoopStatus();
    void raiseMediaPlayer();

    void startPipeWireSpectrumMonitor();
    void stopPipeWireSpectrumMonitor();

    static QVariantList computeSpectrumBarsFromPcm(const QByteArray &pcmChunk, int barCount = 6);
    static QVariantMap extractMprisMetadata(const QVariant &metadataVariant);
    static QVariantMap parsePlayerctlLine(const QString &line);
    static QString resolveCoverArtOrVideoThumbnail(const QString &artUrl, const QString &mediaUrl, bool *isVideoOut = nullptr);

Q_SIGNALS:
    void mediaStateChanged();
    void spectrumChanged();

private Q_SLOTS:
    void onPipeWireAudioChunk();

private:
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

    QVariantList m_spectrumBars = {0.15, 0.15, 0.15, 0.15, 0.15, 0.15};
    QProcess *m_pwMonitorProc = nullptr;
    QByteArray m_audioPcmBuffer;
};
