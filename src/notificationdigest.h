/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QVariantList>

/**
 * Passively monitors org.freedesktop.Notifications.Notify on the session bus
 * (via `busctl --user monitor`, no interception, no privileges) and builds an
 * Apple-Intelligence-style digest of the stacked notifications using local Ollama.
 */
class NotificationDigest : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList recent READ recent NOTIFY recentChanged)
    Q_PROPERTY(int count READ count NOTIFY recentChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(bool summarizing READ summarizing NOTIFY summaryChanged)

public:
    explicit NotificationDigest(QObject *parent = nullptr);
    ~NotificationDigest() override;

    QVariantList recent() const;
    int count() const;
    QString summary() const;
    bool summarizing() const;

    void start();
    void stop();

    Q_INVOKABLE void clear();
    Q_INVOKABLE void summarize();

    // Pure helpers (unit-testable)
    static QVariantMap parseBusctlNotifyLine(const QByteArray &jsonLine);
    static QString buildDigestPrompt(const QVariantList &items);
    static bool isIgnoredApp(const QString &appName);

Q_SIGNALS:
    void recentChanged();
    void summaryChanged();

private Q_SLOTS:
    void onReadyRead();

private:
    void append(const QVariantMap &item);

    QProcess *m_proc = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QByteArray m_buffer;
    QVariantList m_recent;
    QString m_summary;
    bool m_summarizing = false;
    static constexpr int MaxItems = 30;
};
