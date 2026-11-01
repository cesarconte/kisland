/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "notificationdigest.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

NotificationDigest::NotificationDigest(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

NotificationDigest::~NotificationDigest()
{
    stop();
}

QVariantList NotificationDigest::recent() const { return m_recent; }
int NotificationDigest::count() const { return m_recent.size(); }
QString NotificationDigest::summary() const { return m_summary; }
bool NotificationDigest::summarizing() const { return m_summarizing; }

bool NotificationDigest::isIgnoredApp(const QString &appName)
{
    const QString a = appName.trimmed().toLower();
    return a.isEmpty() || a == QStringLiteral("kisland") || a == QStringLiteral("kiri")
        || a == QStringLiteral("kdictation") || a == QStringLiteral("notify-send-test");
}

QVariantMap NotificationDigest::parseBusctlNotifyLine(const QByteArray &jsonLine)
{
    QVariantMap out;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonLine, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return out;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("type")).toString() != QStringLiteral("method_call")) {
        return out;
    }
    if (root.value(QStringLiteral("member")).toString() != QStringLiteral("Notify")) {
        return out;
    }
    const QJsonObject payload = root.value(QStringLiteral("payload")).toObject();
    const QJsonArray data = payload.value(QStringLiteral("data")).toArray();
    if (data.size() < 5) {
        return out;
    }
    const QString app = data.at(0).toString();
    const QString summary = data.at(3).toString().trimmed();
    const QString body = data.at(4).toString().trimmed();
    if (isIgnoredApp(app) || (summary.isEmpty() && body.isEmpty())) {
        return out;
    }
    out[QStringLiteral("app")] = app;
    out[QStringLiteral("summary")] = summary;
    out[QStringLiteral("body")] = body;
    out[QStringLiteral("time")] = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm"));
    return out;
}

QString NotificationDigest::buildDigestPrompt(const QVariantList &items)
{
    QStringList lines;
    for (const QVariant &v : items) {
        const QVariantMap m = v.toMap();
        lines << QStringLiteral("[%1] %2 — %3: %4")
                     .arg(m.value(QStringLiteral("time")).toString(),
                          m.value(QStringLiteral("app")).toString(),
                          m.value(QStringLiteral("summary")).toString(),
                          m.value(QStringLiteral("body")).toString().left(220));
    }
    return QStringLiteral("Notificaciones recibidas:\n") + lines.join(QLatin1Char('\n'));
}

void NotificationDigest::start()
{
    if (m_proc) {
        return;
    }
    const QString busctl = QStandardPaths::findExecutable(QStringLiteral("busctl"));
    if (busctl.isEmpty()) {
        return;
    }
    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &NotificationDigest::onReadyRead);
    connect(m_proc, &QProcess::finished, this, [this]() {
        if (m_proc) {
            m_proc->deleteLater();
            m_proc = nullptr;
        }
    });
    m_proc->start(busctl, {QStringLiteral("--user"), QStringLiteral("monitor"), QStringLiteral("--json=short"),
                           QStringLiteral("--match"),
                           QStringLiteral("type='method_call',interface='org.freedesktop.Notifications',member='Notify'")});
}

void NotificationDigest::stop()
{
    if (m_proc) {
        disconnect(m_proc, nullptr, this, nullptr);
        m_proc->terminate();
        if (!m_proc->waitForFinished(300)) {
            m_proc->kill();
        }
        m_proc->deleteLater();
        m_proc = nullptr;
    }
}

void NotificationDigest::onReadyRead()
{
    if (!m_proc) {
        return;
    }
    m_buffer += m_proc->readAllStandardOutput();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        const QVariantMap item = parseBusctlNotifyLine(line);
        if (!item.isEmpty()) {
            append(item);
        }
    }
    if (m_buffer.size() > 1 << 20) {
        m_buffer.clear();
    }
}

void NotificationDigest::append(const QVariantMap &item)
{
    m_recent.prepend(item);
    while (m_recent.size() > MaxItems) {
        m_recent.removeLast();
    }
    Q_EMIT recentChanged();
}

void NotificationDigest::clear()
{
    m_recent.clear();
    m_summary.clear();
    Q_EMIT recentChanged();
    Q_EMIT summaryChanged();
}

void NotificationDigest::summarize()
{
    if (m_recent.isEmpty() || m_summarizing) {
        return;
    }
    m_summarizing = true;
    m_summary = QStringLiteral("Resumiendo notificaciones…");
    Q_EMIT summaryChanged();

    QJsonObject req;
    req[QStringLiteral("model")] = QStringLiteral("qwen2.5:7b");
    req[QStringLiteral("stream")] = false;
    QJsonArray msgs;
    msgs.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("system")},
                            {QStringLiteral("content"),
                             QStringLiteral("Eres el resumidor de notificaciones de KIsland. Agrupa por aplicación y resume en español "
                                            "de España, con máximo 4 viñetas muy breves, priorizando lo urgente.")}});
    msgs.append(QJsonObject{{QStringLiteral("role"), QStringLiteral("user")},
                            {QStringLiteral("content"), buildDigestPrompt(m_recent)}});
    req[QStringLiteral("messages")] = msgs;

    QNetworkRequest nr(QUrl(QStringLiteral("http://127.0.0.1:11434/api/chat")));
    nr.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QNetworkReply *reply = m_nam->post(nr, QJsonDocument(req).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_summarizing = false;
        if (reply->error() != QNetworkReply::NoError) {
            m_summary = QStringLiteral("No se pudo conectar con Ollama local.");
        } else {
            m_summary = QJsonDocument::fromJson(reply->readAll())
                            .object()
                            .value(QStringLiteral("message"))
                            .toObject()
                            .value(QStringLiteral("content"))
                            .toString()
                            .trimmed();
            if (m_summary.isEmpty()) {
                m_summary = QStringLiteral("Sin resumen disponible.");
            }
        }
        Q_EMIT summaryChanged();
    });
}
