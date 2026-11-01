/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>

/**
 * macOS "Music Recognition / Shazam" for Plasma: listens to whatever is playing on the
 * default PipeWire sink (post-EasyEffects, no hw:* pinning) for a few seconds and
 * identifies it with songrec (if installed) or the bundled shazamio helper.
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

public:
    explicit SongRecognizer(QObject *parent = nullptr);
    ~SongRecognizer() override;

    bool listening() const;
    QString status() const;
    QString title() const;
    QString artist() const;
    QString coverUrl() const;
    QString shazamUrl() const;

    Q_INVOKABLE void recognize(int seconds = 9);
    Q_INVOKABLE void openInBrowser() const;

    // Pure helpers (unit-testable)
    static QVariantMap parseShazamJson(const QByteArray &json);
    static QString resolveBackend(QStringList *argsOut = nullptr);

Q_SIGNALS:
    void stateChanged();
    void resultChanged();

private:
    void setStatus(const QString &s, bool listening);
    void finishRecording();
    void runBackend();

    QProcess *m_rec = nullptr;
    QProcess *m_backend = nullptr;
    QTimer m_stopTimer;
    QString m_wavPath;
    bool m_listening = false;
    QString m_status;
    QVariantMap m_result;
};
