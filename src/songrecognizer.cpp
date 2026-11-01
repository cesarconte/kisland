/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "songrecognizer.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUrl>

SongRecognizer::SongRecognizer(QObject *parent)
    : QObject(parent)
{
    m_stopTimer.setSingleShot(true);
    connect(&m_stopTimer, &QTimer::timeout, this, &SongRecognizer::finishRecording);
}

SongRecognizer::~SongRecognizer()
{
    for (QProcess *p : {m_rec, m_backend}) {
        if (p) {
            disconnect(p, nullptr, this, nullptr);
            p->kill();
        }
    }
}

bool SongRecognizer::listening() const { return m_listening; }
QString SongRecognizer::status() const { return m_status; }
QString SongRecognizer::title() const { return m_result.value(QStringLiteral("title")).toString(); }
QString SongRecognizer::artist() const { return m_result.value(QStringLiteral("artist")).toString(); }
QString SongRecognizer::coverUrl() const { return m_result.value(QStringLiteral("cover")).toString(); }
QString SongRecognizer::shazamUrl() const { return m_result.value(QStringLiteral("url")).toString(); }

void SongRecognizer::setStatus(const QString &s, bool listening)
{
    m_status = s;
    m_listening = listening;
    Q_EMIT stateChanged();
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
        setStatus(QStringLiteral("Instala songrec (sudo pacman -S songrec) o shazamio para reconocer canciones"), false);
        return;
    }
    const QString pwRecord = QStandardPaths::findExecutable(QStringLiteral("pw-record"));
    if (pwRecord.isEmpty()) {
        setStatus(QStringLiteral("pw-record no disponible (PipeWire)"), false);
        return;
    }

    m_result.clear();
    Q_EMIT resultChanged();
    m_wavPath = QDir::tempPath() + QStringLiteral("/kisland_shazam_%1.wav").arg(QCoreApplication::applicationPid());
    QFile::remove(m_wavPath);

    m_rec = new QProcess(this);
    // Capture the default sink monitor via PipeWire (respects EasyEffects chain; no hw:* devices).
    m_rec->start(pwRecord, {QStringLiteral("--rate=44100"), QStringLiteral("--channels=1"), QStringLiteral("--format=s16"),
                            QStringLiteral("-P"), QStringLiteral("stream.capture.sink=true"), m_wavPath});
    setStatus(QStringLiteral("Escuchando…"), true);
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
        setStatus(QStringLiteral("No se capturó audio (¿hay algo sonando?)"), false);
        return;
    }
    setStatus(QStringLiteral("Identificando…"), true);
    runBackend();
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
            setStatus(QStringLiteral("No se reconoció la canción"), false);
        } else {
            setStatus(QStringLiteral("%1 — %2").arg(title(), artist()), false);
        }
    });
    m_backend->start(bin, args);
}

void SongRecognizer::openInBrowser() const
{
    if (!shazamUrl().isEmpty()) {
        QDesktopServices::openUrl(QUrl(shazamUrl()));
    }
}
