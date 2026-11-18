/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "timermanager.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

TimerManager::TimerManager(QObject *parent)
    : QObject(parent)
    , m_countdownTimer(new QTimer(this))
{
    connect(m_countdownTimer, &QTimer::timeout, this, &TimerManager::onTimerTick);
}

double TimerManager::progress() const
{
    if (!m_timerActive || m_timerTotalSecs <= 0) {
        return 0.0;
    }
    const qint64 rem = m_timerPaused
        ? m_timerPausedRemainingSecs
        : qMax<qint64>(0, QDateTime::currentDateTime().secsTo(m_timerTarget));
    const double elapsed = static_cast<double>(m_timerTotalSecs - rem);
    return qBound(0.0, elapsed / static_cast<double>(m_timerTotalSecs), 1.0);
}

QString TimerManager::formatCountdown(qint64 remainingSeconds)
{
    const qint64 clamped = qMax<qint64>(0, remainingSeconds);
    const qint64 hours = clamped / 3600;
    const qint64 mins = (clamped % 3600) / 60;
    const qint64 secs = clamped % 60;
    if (hours > 0) {
        return QStringLiteral("%1:%2:%3")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(mins, 2, 10, QLatin1Char('0'))
            .arg(secs, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2")
        .arg(mins, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

void TimerManager::startQuickTimer(int durationSeconds, const QString &label)
{
    if (durationSeconds <= 0) return;
    m_timerTotalSecs = durationSeconds;
    m_timerPausedRemainingSecs = durationSeconds;
    m_timerPaused = false;
    m_timerTarget = QDateTime::currentDateTime().addSecs(durationSeconds);
    m_timerLabel = label;
    m_timerActive = true;
    m_timerRemainingText = formatCountdown(durationSeconds);
    m_countdownTimer->start(1000);
    Q_EMIT timerChanged();
}

void TimerManager::togglePauseTimer()
{
    if (!m_timerActive) return;
    if (!m_timerPaused) {
        m_timerPausedRemainingSecs = qMax<qint64>(1, QDateTime::currentDateTime().secsTo(m_timerTarget));
        m_timerPaused = true;
        m_countdownTimer->stop();
    } else {
        m_timerTarget = QDateTime::currentDateTime().addSecs(m_timerPausedRemainingSecs);
        m_timerPaused = false;
        m_countdownTimer->start(1000);
    }
    Q_EMIT timerChanged();
}

void TimerManager::cancelQuickTimer()
{
    m_countdownTimer->stop();
    m_timerActive = false;
    m_timerPaused = false;
    m_timerTotalSecs = 0;
    m_timerPausedRemainingSecs = 0;
    m_timerRemainingText.clear();
    Q_EMIT timerChanged();
}

void TimerManager::onTimerTick()
{
    if (!m_timerActive || m_timerPaused) return;
    const qint64 rem = QDateTime::currentDateTime().secsTo(m_timerTarget);
    if (rem <= 0) {
        const QString finishedLabel = m_timerLabel.isEmpty() ? i18n("Temporizador") : m_timerLabel;
        const qint64 totalSecs = m_timerTotalSecs;
        cancelQuickTimer();

        QString alarmSound = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("sounds/macOS-GoldenGate/stereo/alarm-clock-elapsed.oga"));
        if (alarmSound.isEmpty()) {
            alarmSound = QStringLiteral("/usr/share/sounds/macOS-GoldenGate/stereo/alarm-clock-elapsed.oga");
        }
        if (QFile::exists(alarmSound)) {
            QProcess::startDetached(QStringLiteral("pw-play"), {alarmSound});
        }

        QDBusMessage msg = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("/org/freedesktop/Notifications"),
            QStringLiteral("org.freedesktop.Notifications"),
            QStringLiteral("Notify")
        );
        QVariantMap hints;
        hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("org.kde.plasmashell"));
        hints.insert(QStringLiteral("category"), QStringLiteral("timer"));
        hints.insert(QStringLiteral("urgency"), uchar(2)); // High urgency

        msg << QStringLiteral("KIsland")
            << 0u
            << QStringLiteral("chronometer")
            << i18n("¡Se acabó el tiempo!")
            << finishedLabel
            << QStringList()
            << hints
            << -1;

        QDBusConnection::sessionBus().asyncCall(msg);

        Q_EMIT timerFinished(finishedLabel, totalSecs);
        return;
    }
    m_timerRemainingText = formatCountdown(rem);
    Q_EMIT timerChanged();
}
