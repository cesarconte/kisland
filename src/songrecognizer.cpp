/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "songrecognizer.h"

#include <KLocalizedString>
#include <KNotification>

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QStandardPaths>
#include <QUrl>

SongRecognizer::SongRecognizer(QObject *parent)
    : QObject(parent)
{
    m_stopTimer.setSingleShot(true);
    connect(&m_stopTimer, &QTimer::timeout, this, &SongRecognizer::finishRecording);

    m_autoDismissTimer.setSingleShot(true);
    connect(&m_autoDismissTimer, &QTimer::timeout, this, [this]() {
        setRecognizedActive(false);
    });

    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/kisland");
    QDir().mkpath(dataDir);
    m_historyFilePath = dataDir + QStringLiteral("/shazam_history.json");
    loadHistory();
}

SongRecognizer::~SongRecognizer()
{
    cancel();
}

bool SongRecognizer::listening() const { return m_listening; }
QString SongRecognizer::status() const { return m_status; }
QString SongRecognizer::title() const { return m_result.value(QStringLiteral("title")).toString(); }
QString SongRecognizer::artist() const { return m_result.value(QStringLiteral("artist")).toString(); }
QString SongRecognizer::coverUrl() const { return m_result.value(QStringLiteral("cover")).toString(); }
QString SongRecognizer::shazamUrl() const { return m_result.value(QStringLiteral("url")).toString(); }
bool SongRecognizer::recognizedActive() const { return m_recognizedActive; }
QVariantList SongRecognizer::history() const { return m_history; }

void SongRecognizer::setStatus(const QString &s, bool listening)
{
    m_status = s;
    m_listening = listening;
    Q_EMIT stateChanged();
}

void SongRecognizer::setRecognizedActive(bool active)
{
    if (m_recognizedActive != active) {
        m_recognizedActive = active;
        Q_EMIT recognizedActiveChanged();
    }
}

void SongRecognizer::loadHistory()
{
    m_history.clear();
    QFile f(m_historyFilePath);
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isArray()) {
            m_history = doc.array().toVariantList();
        }
    }
    Q_EMIT historyChanged();
}

void SongRecognizer::saveHistory()
{
    QFile f(m_historyFilePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QJsonDocument doc(QJsonArray::fromVariantList(m_history));
        f.write(doc.toJson(QJsonDocument::Indented));
    }
}

void SongRecognizer::clearHistory()
{
    m_history.clear();
    QFile::remove(m_historyFilePath);
    Q_EMIT historyChanged();
}

void SongRecognizer::dismissCurrentResult()
{
    m_autoDismissTimer.stop();
    setRecognizedActive(false);
}

void SongRecognizer::cancel()
{
    m_stopTimer.stop();
    m_autoDismissTimer.stop();
    for (QProcess *p : {m_rec, m_backend}) {
        if (p) {
            disconnect(p, nullptr, this, nullptr);
            p->kill();
            p->deleteLater();
        }
    }
    m_rec = nullptr;
    m_backend = nullptr;
    if (!m_wavPath.isEmpty()) {
        QFile::remove(m_wavPath);
    }
    setStatus(QString(), false);
    setRecognizedActive(false);
}

void SongRecognizer::copyTrackInfo(const QString &text) const
{
    if (auto *cb = QGuiApplication::clipboard()) {
        cb->setText(text, QClipboard::Clipboard);
    }
}

QVariantMap SongRecognizer::parseShazamJson(const QByteArray &json)
{
    QVariantMap out;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return out;
    }
    const QJsonObject track = doc.object().value(QStringLiteral("track")).toObject();
    if (track.isEmpty()) {
        return out;
    }
    out[QStringLiteral("title")] = track.value(QStringLiteral("title")).toString();
    out[QStringLiteral("artist")] = track.value(QStringLiteral("subtitle")).toString();
    out[QStringLiteral("cover")] = track.value(QStringLiteral("images")).toObject().value(QStringLiteral("coverart")).toString();
    out[QStringLiteral("url")] = track.value(QStringLiteral("url")).toString();
    return out;
}

QString SongRecognizer::resolveBackend(QStringList *argsOut)
{
    const QString songrec = QStandardPaths::findExecutable(QStringLiteral("songrec"));
    if (!songrec.isEmpty()) {
        if (argsOut) {
            *argsOut = {QStringLiteral("audio-file-to-recognized-song")};
        }
        return songrec;
    }
    const QString py = QDir::homePath() + QStringLiteral("/.local/share/kisland/venv/bin/python");
    QString script = QDir::homePath() + QStringLiteral("/.local/share/kisland/shazam_recognize.py");
    if (!QFileInfo::exists(script)) {
        script = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("kisland/shazam_recognize.py"));
    }
    if (QFileInfo::exists(py) && QFileInfo::exists(script)) {
        if (argsOut) {
            *argsOut = {script};
        }
        return py;
    }
    return QString();
}

void SongRecognizer::recognize(int seconds)
{
    if (m_listening) {
        return;
    }
    if (resolveBackend().isEmpty()) {
        setStatus(QStringLiteral("Instala songrec o shazamio para reconocer canciones"), false);
        return;
    }
    const QString pwRecord = QStandardPaths::findExecutable(QStringLiteral("pw-record"));
    if (pwRecord.isEmpty()) {
        setStatus(i18n("Grabador de audio no disponible"), false);
        return;
    }

    m_result.clear();
    setRecognizedActive(false);
    Q_EMIT resultChanged();

    m_wavPath = QDir::tempPath() + QStringLiteral("/kisland_shazam_%1.wav").arg(QCoreApplication::applicationPid());
    QFile::remove(m_wavPath);

    m_rec = new QProcess(this);
    // Capture default sink monitor via PipeWire (respects EasyEffects chain; no hw:* devices)
    m_rec->start(pwRecord, {QStringLiteral("--rate=44100"), QStringLiteral("--channels=1"), QStringLiteral("--format=s16"),
                            QStringLiteral("-P"), QStringLiteral("stream.capture.sink=true"), m_wavPath});
    setStatus(i18n("Escuchando música…"), true);
    m_stopTimer.start(qBound(4, seconds, 20) * 1000);
}

void SongRecognizer::finishRecording()
{
    if (m_rec) {
        QProcess *rec = m_rec;
        m_rec = nullptr;
        rec->terminate();
        if (!rec->waitForFinished(1500)) {
            rec->kill();
            rec->waitForFinished(500);
        }
        rec->deleteLater();
    }
    if (!QFileInfo::exists(m_wavPath) || QFileInfo(m_wavPath).size() < 20000) {
        setStatus(i18n("No se detectó audio"), false);
        return;
    }
    setStatus(i18n("Buscando coincidencias…"), true);
    runBackend();
}

void SongRecognizer::sendPlasmaNotification(const QString &songTitle, const QString &songArtist, const QString &url)
{
    auto *notif = new KNotification(QStringLiteral("notification"), KNotification::CloseOnTimeout, this);
    notif->setComponentName(QStringLiteral("plasma_applet_org.cesar.kisland"));
    notif->setTitle(i18n("Shazam — Canción identificada"));
    notif->setText(QStringLiteral("%1\n%2").arg(songTitle, songArtist));
    notif->setIconName(QStringLiteral("emblem-music-symbolic"));
    if (!url.isEmpty()) {
        auto *action = notif->addAction(i18n("Abrir en Shazam"));
        connect(action, &KNotificationAction::activated, this, [this, url]() {
            openUrl(url);
        });
    }
    notif->sendEvent();
}

void SongRecognizer::runBackend()
{
    QStringList args;
    const QString bin = resolveBackend(&args);
    args << m_wavPath;

    m_backend = new QProcess(this);
    connect(m_backend, &QProcess::finished, this, [this]() {
        QProcess *p = m_backend;
        m_backend = nullptr;
        const QByteArray out = p->readAllStandardOutput();
        p->deleteLater();
        QFile::remove(m_wavPath);

        m_result = parseShazamJson(out);
        Q_EMIT resultChanged();

        if (m_result.isEmpty()) {
            setStatus(i18n("No se han encontrado resultados"), false);
            QTimer::singleShot(4000, this, [this]() {
                if (!m_listening && !m_recognizedActive) {
                    setStatus(QString(), false);
                }
            });
        } else {
            const QString t = title();
            const QString a = artist();
            const QString u = shazamUrl();
            const QString c = coverUrl();

            setStatus(QStringLiteral("%1 — %2").arg(t, a), false);

            // Add to persistent Shazam History
            QVariantMap historyEntry;
            historyEntry[QStringLiteral("id")] = QString::number(QDateTime::currentMSecsSinceEpoch());
            historyEntry[QStringLiteral("title")] = t;
            historyEntry[QStringLiteral("artist")] = a;
            historyEntry[QStringLiteral("cover")] = c;
            historyEntry[QStringLiteral("url")] = u;
            historyEntry[QStringLiteral("timestamp")] = QLocale::system().toString(QDateTime::currentDateTime(), QStringLiteral("d MMM, hh:mm"));

            m_history.prepend(historyEntry);
            if (m_history.size() > 50) {
                m_history.removeLast();
            }
            saveHistory();
            Q_EMIT historyChanged();

            // Set temporary active result in Dynamic Island (8 seconds)
            setRecognizedActive(true);
            m_autoDismissTimer.start(8000);

            // Deliver native Plasma notification into Plasma notification system
            sendPlasmaNotification(t, a, u);
        }
    });
    m_backend->start(bin, args);
}

void SongRecognizer::openInBrowser() const
{
    openUrl(shazamUrl());
}

void SongRecognizer::openUrl(const QString &url) const
{
    if (!url.isEmpty()) {
        QDesktopServices::openUrl(QUrl(url));
    }
}
