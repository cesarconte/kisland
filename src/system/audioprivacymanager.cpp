/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "audioprivacymanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

AudioPrivacyManager::AudioPrivacyManager(QObject *parent)
    : QObject(parent)
{
}

QVariantMap AudioPrivacyManager::parseWpctlStatus(const QString &wpctlOutput, const QString &kiriState)
{
    QVariantMap res;
    int volumePercent = 40;
    bool muted = false;
    QString deviceName = QStringLiteral("PipeWire");
    bool micInUse = (kiriState == QStringLiteral("listening"));
    QString micAppName = micInUse ? QStringLiteral("Kity") : QString();
    bool camInUse = false;
    QString camAppName;

    const QStringList lines = wpctlOutput.split(QLatin1Char('\n'));
    QString section;
    QString subSection;
    QString currentStreamClient;

    static const QRegularExpression sinkLineRe(QStringLiteral(R"(^\s*│?\s*\*\s*\d+\.\s+(.+?)\s+\[vol:\s*([0-9.]+)(\s+MUTED)?\])"));
    static const QRegularExpression streamHeaderRe(QStringLiteral(R"(^\s{1,9}\d+\.\s+([^<>]+?)\s*$)"));

    auto isPortName = [](const QString &name) {
        return name.startsWith(QStringLiteral("monitor_"))
            || name.startsWith(QStringLiteral("input_"))
            || name.startsWith(QStringLiteral("output_"))
            || name.startsWith(QStringLiteral("capture_"))
            || name.startsWith(QStringLiteral("playback_"));
    };

    for (const QString &rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed == QStringLiteral("Audio")) {
            section = QStringLiteral("Audio");
            subSection.clear();
            continue;
        } else if (trimmed == QStringLiteral("Video")) {
            section = QStringLiteral("Video");
            subSection.clear();
            continue;
        } else if (trimmed == QStringLiteral("Settings")) {
            section = QStringLiteral("Settings");
            subSection.clear();
            continue;
        }

        if (trimmed.contains(QStringLiteral("├─ Sinks:")) || trimmed.contains(QStringLiteral("└─ Sinks:"))) {
            subSection = QStringLiteral("Sinks");
            continue;
        } else if (trimmed.contains(QStringLiteral("├─ Sources:")) || trimmed.contains(QStringLiteral("└─ Sources:"))) {
            subSection = QStringLiteral("Sources");
            continue;
        } else if (trimmed.contains(QStringLiteral("├─ Filters:")) || trimmed.contains(QStringLiteral("└─ Filters:"))) {
            subSection = QStringLiteral("Filters");
            continue;
        } else if (trimmed.contains(QStringLiteral("├─ Streams:")) || trimmed.contains(QStringLiteral("└─ Streams:"))) {
            subSection = QStringLiteral("Streams");
            currentStreamClient.clear();
            continue;
        }

        if (section == QStringLiteral("Audio") && subSection == QStringLiteral("Sinks")) {
            const QRegularExpressionMatch m = sinkLineRe.match(rawLine);
            if (m.hasMatch()) {
                deviceName = m.captured(1).trimmed();
                const double vol = m.captured(2).toDouble();
                volumePercent = qBound(0, qRound(vol * 100.0), 150);
                muted = !m.captured(3).isEmpty();
            }
        } else if (section == QStringLiteral("Audio") && subSection == QStringLiteral("Streams")) {
            const QRegularExpressionMatch hm = streamHeaderRe.match(rawLine);
            if (hm.hasMatch() && !rawLine.contains(QLatin1Char('<')) && !rawLine.contains(QLatin1Char('>'))) {
                const QString candidate = hm.captured(1).trimmed();
                if (!isPortName(candidate)) {
                    currentStreamClient = candidate;
                }
            } else if (rawLine.contains(QLatin1Char('<')) && rawLine.contains(QStringLiteral("[active]"))
                       && rawLine.contains(QStringLiteral(":capture_"))) {
                if (!currentStreamClient.isEmpty()
                    && !isPortName(currentStreamClient)
                    && !(currentStreamClient == QStringLiteral("pw-record") && kiriState != QStringLiteral("listening"))) {
                    micInUse = true;
                    micAppName = (currentStreamClient == QStringLiteral("pw-record")) ? QStringLiteral("Kity") : currentStreamClient;
                }
            }
        } else if (section == QStringLiteral("Video") && subSection == QStringLiteral("Streams")) {
            const QRegularExpressionMatch hm = streamHeaderRe.match(rawLine);
            if (hm.hasMatch() && !rawLine.contains(QLatin1Char('<')) && !rawLine.contains(QLatin1Char('>'))) {
                const QString candidate = hm.captured(1).trimmed();
                if (!isPortName(candidate)) {
                    currentStreamClient = candidate;
                }
            } else if (rawLine.contains(QStringLiteral("[active]"))) {
                camInUse = true;
                camAppName = currentStreamClient.isEmpty() ? QStringLiteral("Vídeo / Pantalla") : currentStreamClient;
            }
        }
    }

    res[QStringLiteral("volumePercent")] = volumePercent;
    res[QStringLiteral("muted")] = muted;
    res[QStringLiteral("deviceName")] = deviceName;
    res[QStringLiteral("micActive")] = micInUse;
    res[QStringLiteral("micApp")] = micAppName;
    res[QStringLiteral("cameraOrScreenActive")] = camInUse;
    res[QStringLiteral("cameraOrScreenApp")] = camAppName;
    return res;
}

void AudioPrivacyManager::updatePipeWireAudioAndPrivacy(const QString &kiriState)
{
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (wpctl.isEmpty()) return;

    QProcess proc;
    proc.start(wpctl, {QStringLiteral("status")});
    if (!proc.waitForFinished(450) || proc.exitCode() != 0) {
        return;
    }

    const QVariantMap parsed = parseWpctlStatus(QString::fromUtf8(proc.readAllStandardOutput()), kiriState);
    const int newVol = parsed.value(QStringLiteral("volumePercent"), m_outputVolumePercent).toInt();
    const bool newMuted = parsed.value(QStringLiteral("muted"), m_outputMuted).toBool();
    const QString newDev = parsed.value(QStringLiteral("deviceName"), m_outputDeviceName).toString();

    if (m_outputVolumePercent != newVol || m_outputMuted != newMuted || m_outputDeviceName != newDev) {
        const bool volChanged = (m_outputVolumePercent != newVol || m_outputMuted != newMuted);
        m_outputVolumePercent = newVol;
        m_outputMuted = newMuted;
        m_outputDeviceName = newDev;
        Q_EMIT audioChanged();
        if (m_volumeInitialized && volChanged) {
            Q_EMIT volumeAlert(m_outputVolumePercent, m_outputMuted);
        }
    }
    m_volumeInitialized = true;

    const bool newMic = parsed.value(QStringLiteral("micActive")).toBool();
    const QString newMicApp = parsed.value(QStringLiteral("micApp")).toString();
    bool newCam = parsed.value(QStringLiteral("cameraOrScreenActive")).toBool();
    QString newCamApp = parsed.value(QStringLiteral("cameraOrScreenApp")).toString();

    if (!newCam) {
        const QDir procDir(QStringLiteral("/proc"));
        const QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &pidStr : pids) {
            bool ok = false;
            pidStr.toInt(&ok);
            if (!ok) continue;

            const QDir fdDir(QStringLiteral("/proc/%1/fd").arg(pidStr));
            const QFileInfoList fds = fdDir.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo &fdInfo : fds) {
                const QString target = fdInfo.symLinkTarget();
                if (target.startsWith(QStringLiteral("/dev/video"))) {
                    QFile commFile(QStringLiteral("/proc/%1/comm").arg(pidStr));
                    QString comm;
                    if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        comm = QString::fromUtf8(commFile.readLine()).trimmed();
                    }
                    if (!comm.isEmpty()
                        && comm != QStringLiteral("pipewire")
                        && comm != QStringLiteral("wireplumber")
                        && comm != QStringLiteral("xdg-desktop-por")) {
                        newCam = true;
                        newCamApp = comm;
                        break;
                    }
                }
            }
            if (newCam) break;
        }
    }

    if (m_micActive != newMic || m_micApp != newMicApp
        || m_cameraOrScreenActive != newCam || m_cameraOrScreenApp != newCamApp) {
        m_micActive = newMic;
        m_micApp = newMicApp;
        m_cameraOrScreenActive = newCam;
        m_cameraOrScreenApp = newCamApp;
        Q_EMIT privacyChanged();
    }
}

void AudioPrivacyManager::setOutputVolume(int percent)
{
    const int clamped = qBound(0, percent, 150);
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (!wpctl.isEmpty()) {
        const QString ratioStr = QString::number(clamped / 100.0, 'f', 2);
        QProcess::startDetached(wpctl, {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), ratioStr});
    }
    if (m_outputVolumePercent != clamped) {
        m_outputVolumePercent = clamped;
        Q_EMIT audioChanged();
        Q_EMIT volumeAlert(m_outputVolumePercent, m_outputMuted);
    }
}

void AudioPrivacyManager::toggleOutputMute()
{
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (!wpctl.isEmpty()) {
        QProcess::startDetached(wpctl, {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("toggle")});
    }
    m_outputMuted = !m_outputMuted;
    Q_EMIT audioChanged();
    Q_EMIT volumeAlert(m_outputVolumePercent, m_outputMuted);
}
