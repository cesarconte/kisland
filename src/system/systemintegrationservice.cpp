/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "systemintegrationservice.h"

#include <KIO/CommandLauncherJob>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

SystemIntegrationService::SystemIntegrationService(QObject *parent)
    : QObject(parent)
{
}

void SystemIntegrationService::updateKityState()
{
    QDBusInterface kityIface(QStringLiteral("org.cesar.kity"),
                             QStringLiteral("/Kity"),
                             QStringLiteral("org.freedesktop.DBus.Properties"),
                             QDBusConnection::sessionBus());
    QString detectedKity = QStringLiteral("idle");
    if (kityIface.isValid()) {
        const QVariant stProp = kityIface.property("state");
        if (stProp.isValid() && !stProp.toString().isEmpty()) {
            detectedKity = stProp.toString();
        }
    }
    if (m_kityState != detectedKity) {
        m_kityState = detectedKity;
        Q_EMIT kityStateChanged();
        Q_EMIT kiriStateChanged();
    }
}

void SystemIntegrationService::invokeKity()
{
    const QString kityBin = QStandardPaths::findExecutable(QStringLiteral("kity"),
                                                           {QDir::homePath() + QStringLiteral("/.local/bin"),
                                                            QStringLiteral("/usr/local/bin"),
                                                            QStringLiteral("/usr/bin")});
    if (kityBin.isEmpty()) {
        return;
    }

    auto *job = new KIO::CommandLauncherJob(kityBin, {QStringLiteral("-t")});
    job->setDesktopName(QStringLiteral("org.cesar.kity"));
    job->start();
}

void SystemIntegrationService::invokeKiriVoice()
{
    invokeKity();
}

void SystemIntegrationService::launchDropover()
{
    const QString bin = QStandardPaths::findExecutable(QStringLiteral("kdropover"),
                                                       {QDir::homePath() + QStringLiteral("/.local/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/bin")});
    if (!bin.isEmpty()) {
        QProcess::startDetached(bin, {});
    }
}

void SystemIntegrationService::launchLiveCaptions()
{
    const QString bin = QStandardPaths::findExecutable(QStringLiteral("klivecaptions"),
                                                       {QDir::homePath() + QStringLiteral("/.local/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/bin")});
    if (!bin.isEmpty()) {
        QProcess::startDetached(bin, {});
    }
}

void SystemIntegrationService::toggleFocusDnd()
{
    QDBusInterface focusIface(QStringLiteral("org.cesar.kfocus"),
                              QStringLiteral("/KFocus"),
                              QStringLiteral("org.cesar.kfocus"),
                              QDBusConnection::sessionBus());
    if (focusIface.isValid()) {
        focusIface.asyncCall(QStringLiteral("toggleDnd"));
        QTimer::singleShot(200, this, &SystemIntegrationService::updateFocusMode);
        return;
    }

    QDBusInterface notifIface(QStringLiteral("org.freedesktop.Notifications"),
                              QStringLiteral("/org/freedesktop/Notifications"),
                              QStringLiteral("org.freedesktop.Notifications"),
                              QDBusConnection::sessionBus());
    if (notifIface.isValid()) {
        const bool isInhibited = notifIface.property("Inhibited").toBool();
        if (isInhibited) {
            notifIface.call(QStringLiteral("UnInhibit"), 1u);
        } else {
            notifIface.call(QStringLiteral("Inhibit"), QStringLiteral("org.cesar.kisland"), QStringLiteral("Modo no molestar"), QVariantMap());
        }
    }

    QTimer::singleShot(200, this, &SystemIntegrationService::updateFocusMode);
}

void SystemIntegrationService::updateBluetoothAndPeripherals()
{
    const QString btctl = QStandardPaths::findExecutable(QStringLiteral("bluetoothctl"));
    if (btctl.isEmpty()) return;

    QProcess proc;
    proc.start(btctl, {QStringLiteral("devices"), QStringLiteral("Connected")});
    if (!proc.waitForFinished(800) || proc.exitCode() != 0) {
        return;
    }

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QSet<QString> currentDevices;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QStringLiteral("Device "))) continue;
        const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 3) continue;

        const QString mac = parts.at(1);
        const QString name = parts.mid(2).join(QLatin1Char(' '));
        currentDevices.insert(mac);

        if (m_btInitialized && !m_knownBtDevices.contains(mac)) {
            int batteryPercent = -1;
            QString iconName = QStringLiteral("audio-headphones");
            const QString lowerName = name.toLower();
            if (lowerName.contains(QStringLiteral("speaker")) || lowerName.contains(QStringLiteral("xb13")) || lowerName.contains(QStringLiteral("altavoz"))) {
                iconName = QStringLiteral("audio-speakers");
            } else if (lowerName.contains(QStringLiteral("mouse")) || lowerName.contains(QStringLiteral("ratón"))) {
                iconName = QStringLiteral("input-mouse");
            } else if (lowerName.contains(QStringLiteral("keyboard")) || lowerName.contains(QStringLiteral("teclado"))) {
                iconName = QStringLiteral("input-keyboard");
            }

            QProcess infoProc;
            infoProc.start(btctl, {QStringLiteral("info"), mac});
            if (infoProc.waitForFinished(600) && infoProc.exitCode() == 0) {
                const QString infoOut = QString::fromUtf8(infoProc.readAllStandardOutput());
                static const QRegularExpression rxBattery(QStringLiteral(R"(Battery Percentage:\s*(?:0x[0-9a-fA-F]+\s*)?\(?(\d+)\)?)"));
                const auto match = rxBattery.match(infoOut);
                if (match.hasMatch()) {
                    batteryPercent = match.captured(1).toInt();
                }
            }

            QString subtitle = QStringLiteral("Conectado");
            if (batteryPercent >= 0) {
                subtitle = QStringLiteral("Conectado • %1%").arg(batteryPercent);
            }
            Q_EMIT peripheralAlert(name, subtitle, iconName);
        }
    }

    m_knownBtDevices = currentDevices;
    m_btInitialized = true;
}

void SystemIntegrationService::updateFocusMode()
{
    bool newActive = false;
    bool newDnd = false;
    QString newModeName;
    QString newModeIcon;
    QString newModeId;

    const QString configPath = QDir::homePath() + QStringLiteral("/.config/kfocus/active_mode.json");
    QFile f(configPath);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            newActive = obj.value(QStringLiteral("active")).toBool();
            newDnd = obj.value(QStringLiteral("dndActive")).toBool();
            newModeName = obj.value(QStringLiteral("modeName")).toString();
            newModeIcon = obj.value(QStringLiteral("modeIcon")).toString();
            newModeId = obj.value(QStringLiteral("modeId")).toString();
        }
    } else {
        QDBusInterface notifIface(QStringLiteral("org.freedesktop.Notifications"),
                                  QStringLiteral("/org/freedesktop/Notifications"),
                                  QStringLiteral("org.freedesktop.Notifications"),
                                  QDBusConnection::sessionBus());
        if (notifIface.isValid()) {
            newDnd = notifIface.property("Inhibited").toBool();
            newActive = newDnd;
            if (newDnd) {
                newModeName = QStringLiteral("No molestar");
                newModeIcon = QStringLiteral("notifications-disabled");
            }
        }
    }

    if (m_focusActive != newActive || m_focusDndActive != newDnd || m_focusModeName != newModeName || m_focusModeIcon != newModeIcon) {
        const bool stateChanged = (m_focusActive != newActive || m_focusDndActive != newDnd);
        m_focusActive = newActive;
        m_focusDndActive = newDnd;
        m_focusModeName = newModeName;
        m_focusModeIcon = newModeIcon;
        m_focusModeId = newModeId;
        Q_EMIT focusStateChanged();

        if (m_focusInitialized && stateChanged) {
            QString alertTitle;
            QString alertIcon = m_focusModeIcon.isEmpty() ? QStringLiteral("notifications-disabled") : m_focusModeIcon;
            if (m_focusActive) {
                alertTitle = m_focusModeName.isEmpty() ? QStringLiteral("Concentración activada") : QStringLiteral("Modo %1").arg(m_focusModeName);
            } else if (m_focusDndActive) {
                alertTitle = QStringLiteral("No molestar: Activado");
            } else {
                alertTitle = QStringLiteral("No molestar: Desactivado");
                alertIcon = QStringLiteral("preferences-desktop-notification-bell");
            }
            Q_EMIT focusAlert(alertTitle, alertIcon);
        }
    }
    m_focusInitialized = true;
}

QVariantList SystemIntegrationService::getKdeConnectDevices() const
{
    QVariantList list;

    QDBusInterface iface(QStringLiteral("org.kde.kdeconnect"),
                         QStringLiteral("/modules/kdeconnect"),
                         QStringLiteral("org.kde.kdeconnect.daemon"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        const QDBusReply<QMap<QString, QString>> reply = iface.call(QStringLiteral("deviceNames"), true, true);
        if (reply.isValid()) {
            const auto map = reply.value();
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                QVariantMap d;
                d[QStringLiteral("id")] = it.key();
                d[QStringLiteral("name")] = it.value();
                list.append(d);
            }
            if (!list.isEmpty()) {
                return list;
            }
        }
    }

    const QString kdecli = QStandardPaths::findExecutable(QStringLiteral("kdeconnect-cli"));
    if (!kdecli.isEmpty()) {
        QProcess proc;
        proc.start(kdecli, {QStringLiteral("-a"), QStringLiteral("--id-name-only")});
        if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
            const QString out = QString::fromUtf8(proc.readAllStandardOutput());
            const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QString trimmed = line.trimmed();
                const int spaceIdx = trimmed.indexOf(QLatin1Char(' '));
                if (spaceIdx > 0) {
                    QVariantMap d;
                    d[QStringLiteral("id")] = trimmed.left(spaceIdx);
                    d[QStringLiteral("name")] = trimmed.mid(spaceIdx + 1).trimmed();
                    list.append(d);
                }
            }
        }
    }

    return list;
}

bool SystemIntegrationService::shareToKdeConnect(const QString &filePath, const QString &deviceId)
{
    QString cleanPath = filePath;
    if (cleanPath.startsWith(QStringLiteral("file://"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    if (cleanPath.isEmpty() || !QFileInfo::exists(cleanPath)) {
        return false;
    }

    const QString kdecli = QStandardPaths::findExecutable(QStringLiteral("kdeconnect-cli"));
    if (kdecli.isEmpty()) {
        return false;
    }

    QStringList args;
    args << QStringLiteral("--share") << cleanPath;
    if (!deviceId.isEmpty()) {
        args << QStringLiteral("-d") << deviceId;
    }

    return QProcess::startDetached(kdecli, args);
}

void SystemIntegrationService::openKdeConnectSettings()
{
    const QString app = QStandardPaths::findExecutable(QStringLiteral("kdeconnect-app"));
    if (!app.isEmpty()) {
        QProcess::startDetached(app, {});
    }
}

void SystemIntegrationService::openFileLocation(const QString &filePath)
{
    QString cleanPath = filePath;
    if (cleanPath.startsWith(QStringLiteral("file://"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    if (cleanPath.isEmpty()) {
        return;
    }

    const QFileInfo fi(cleanPath);
    if (!fi.exists()) {
        return;
    }

    const QString dolphin = QStandardPaths::findExecutable(QStringLiteral("dolphin"));
    if (!dolphin.isEmpty()) {
        QProcess::startDetached(dolphin, {QStringLiteral("--select"), fi.absoluteFilePath()});
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
    }
}
