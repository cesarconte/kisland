/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

/**
 * macOS "Music Recognition / Shazam" for Plasma: listens to whatever is playing on the
 * default PipeWire sink (post-EasyEffects, no hw:* pinning) for a few seconds and
 * identifies it with songrec or the bundled shazamio helper.
 *
 * Implements macOS Dynamic Island & System behavior:
 * - Emits native KDE Plasma Notification (revisitable in Notification Center)
 * - Temporary active result in Dynamic Island (auto-collapses after 8 seconds)
 * - Persistent Shazam History (~/.local/share/kisland/shazam_history.json)
 */
class SongRecognizer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool listening READ listening NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title NOTIFY resultChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY resultChanged)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY resultChanged)
    Q_PROPERTY(QString shazamUrl READ shazamUrl NOTIFY resultChanged)
    Q_PROPERTY(bool recognizedActive READ recognizedActive NOTIFY recognizedActiveChanged)
    Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)

public:
    explicit SongRecognizer(QObject *parent = nullptr);
    ~SongRecognizer() override;

    bool listening() const;
    QString status() const;
    QString title() const;
    QString artist() const;
    QString coverUrl() const;
    QString shazamUrl() const;
    bool recognizedActive() const;
    QVariantList history() const;

    Q_INVOKABLE void recognize(int seconds = 9);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void openInBrowser() const;
    Q_INVOKABLE void openUrl(const QString &url) const;
    Q_INVOKABLE void copyTrackInfo(const QString &text) const;
    Q_INVOKABLE void dismissCurrentResult();
    Q_INVOKABLE void clearHistory();

    // Pure helpers (unit-testable)
    static QVariantMap parseShazamJson(const QByteArray &json);
    static QString resolveBackend(QStringList *argsOut = nullptr);

Q_SIGNALS:
    void stateChanged();
    void resultChanged();
    void recognizedActiveChanged();
    void historyChanged();

private Q_SLOTS:
    void onNotificationActionInvoked(uint id, const QString &actionKey);

private:
    void setStatus(const QString &s, bool listening);
    void setRecognizedActive(bool active);
    void finishRecording();
    void runBackend();
    void loadHistory();
    void saveHistory();
    void sendPlasmaNotification(const QString &songTitle, const QString &songArtist, const QString &url);

    QProcess *m_rec = nullptr;
    QProcess *m_backend = nullptr;
    QTimer m_stopTimer;
    QTimer m_autoDismissTimer;
    QString m_wavPath;
    bool m_listening = false;
    bool m_recognizedActive = false;
    QString m_status;
    QVariantMap m_result;
    QVariantList m_history;
    QString m_historyFilePath;
    QHash<uint, QString> m_notificationUrls;
};
