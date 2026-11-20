/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "timermanager.h"

#include <KLocalizedString>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QUuid>

static const QString s_kclockService = QStringLiteral("org.kde.kclockd");
static const QString s_kclockTimersPath = QStringLiteral("/Timers");
static const QString s_kclockModelInterface = QStringLiteral("org.kde.kclock.TimerModel");
static const QString s_kclockTimerInterface = QStringLiteral("org.kde.kclock.Timer");
static const QString s_dbusPropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");

// Matching the alarm audio length of KClock (alarm-clock-elapsed.oga is ~6.15s)
static constexpr int s_kclockAlarmDurationMs = 6500;

TimerManager::TimerManager(QObject *parent)
    : QObject(parent)
    , m_countdownTimer(new QTimer(this))
    , m_autoDismissTimer(new QTimer(this))
    , m_serviceWatcher(new QDBusServiceWatcher(s_kclockService, QDBusConnection::sessionBus(),
                                              QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration, this))
{
    connect(m_countdownTimer, &QTimer::timeout, this, &TimerManager::onTimerTick);

    m_autoDismissTimer->setSingleShot(true);
    connect(m_autoDismissTimer, &QTimer::timeout, this, &TimerManager::dismissTimer);

    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered,
            this, &TimerManager::onKClockServiceRegistered);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, &TimerManager::onKClockServiceUnregistered);

    initKClockConnection();
}

void TimerManager::initKClockConnection()
{
    QDBusConnection::sessionBus().connect(
        s_kclockService,
        s_kclockTimersPath,
        s_kclockModelInterface,
        QStringLiteral("timerAdded"),
        this,
        SLOT(onKClockTimerAdded(QString))
    );

    QDBusConnection::sessionBus().connect(
        s_kclockService,
        s_kclockTimersPath,
        s_kclockModelInterface,
        QStringLiteral("timerRemoved"),
        this,
        SLOT(onKClockTimerRemoved(QString))
    );

    checkKClockTimers();
}

void TimerManager::onKClockServiceRegistered(const QString &serviceName)
{
    Q_UNUSED(serviceName)
    // Only check if we are not actively tracking a live timer
    if (!m_timerActive) {
        checkKClockTimers();
    }
}

void TimerManager::onKClockServiceUnregistered(const QString &serviceName)
{
    Q_UNUSED(serviceName)
    if (m_isKClockBacked) {
        detachKClockTimer();
        m_isKClockBacked = false;
        Q_EMIT timerChanged();
    }
}

void TimerManager::checkKClockTimers()
{
    if (m_timerActive && !m_currentTimerId.isEmpty()) return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        s_kclockService,
        s_kclockTimersPath,
        s_kclockModelInterface,
        QStringLiteral("timers")
    );

    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QStringList> reply = *w;
        if (reply.isValid()) {
            const QStringList timersList = reply.value();
            for (const QString &tid : timersList) {
                const QString path = s_kclockTimersPath + QLatin1Char('/') + tid;
                QDBusInterface iface(s_kclockService, path, s_kclockTimerInterface, QDBusConnection::sessionBus());
                if (iface.isValid()) {
                    const bool running = iface.property("running").toBool();
                    const bool ringing = iface.property("ringing").toBool();
                    // Attach strictly to a timer that is actually running or ringing
                    if (running || ringing) {
                        attachToKClockTimer(tid);
                        break;
                    }
                }
            }
        }
    });
}

void TimerManager::attachToKClockTimer(const QString &timerId)
{
    if (timerId.isEmpty()) return;
    if (m_currentTimerId == timerId && m_isKClockBacked) return;

    detachKClockTimer();

    m_currentTimerId = timerId;
    m_currentTimerPath = s_kclockTimersPath + QLatin1Char('/') + timerId;
    m_currentTimerUuid = QUuid::fromString(timerId).toString(QUuid::WithBraces);
    m_isKClockBacked = true;

    QDBusConnection::sessionBus().connect(
        s_kclockService,
        m_currentTimerPath,
        s_dbusPropertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onKClockTimerPropertiesChanged(QString,QVariantMap,QStringList))
    );

    QDBusConnection::sessionBus().connect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("runningChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
    QDBusConnection::sessionBus().connect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("timeCompletedChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
    QDBusConnection::sessionBus().connect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("ringingChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
    QDBusConnection::sessionBus().connect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("labelChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );

    // Fetch initial properties via GetAll
    QDBusMessage msg = QDBusMessage::createMethodCall(
        s_kclockService,
        m_currentTimerPath,
        s_dbusPropertiesInterface,
        QStringLiteral("GetAll")
    );
    msg << s_kclockTimerInterface;

    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isValid()) {
            updateFromKClockProperties(reply.value());
        }
    });
}

void TimerManager::detachKClockTimer()
{
    if (m_currentTimerPath.isEmpty()) return;

    QDBusConnection::sessionBus().disconnect(
        s_kclockService,
        m_currentTimerPath,
        s_dbusPropertiesInterface,
        QStringLiteral("PropertiesChanged"),
        this,
        SLOT(onKClockTimerPropertiesChanged(QString,QVariantMap,QStringList))
    );
    QDBusConnection::sessionBus().disconnect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("runningChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
    QDBusConnection::sessionBus().disconnect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("timeCompletedChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
    QDBusConnection::sessionBus().disconnect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("ringingChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
    QDBusConnection::sessionBus().disconnect(
        s_kclockService,
        m_currentTimerPath,
        s_kclockTimerInterface,
        QStringLiteral("labelChanged"),
        this,
        SLOT(onKClockTimerSignalStateChanged())
    );
}

void TimerManager::onKClockTimerAdded(const QString &timerId)
{
    if (!m_timerActive || m_currentTimerId.isEmpty()) {
        attachToKClockTimer(timerId);
    }
}

void TimerManager::onKClockTimerRemoved(const QString &timerId)
{
    if (m_currentTimerId == timerId) {
        detachKClockTimer();
        resetInternalState();
    }
}

void TimerManager::onKClockTimerPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps)
{
    Q_UNUSED(invalidatedProps)
    if (interfaceName == s_kclockTimerInterface) {
        updateFromKClockProperties(changedProps);
    }
}

void TimerManager::onKClockTimerSignalStateChanged()
{
    if (m_currentTimerPath.isEmpty()) return;

    QDBusMessage msg = QDBusMessage::createMethodCall(
        s_kclockService,
        m_currentTimerPath,
        s_dbusPropertiesInterface,
        QStringLiteral("GetAll")
    );
    msg << s_kclockTimerInterface;

    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<QVariantMap> reply = *w;
        if (reply.isValid()) {
            updateFromKClockProperties(reply.value());
        }
    });
}

void TimerManager::updateFromKClockProperties(const QVariantMap &props)
{
    const bool running = props.value(QStringLiteral("running")).toBool();
    const bool ringing = props.value(QStringLiteral("ringing")).toBool();
    const qint64 tc = props.value(QStringLiteral("timeCompleted")).toLongLong();
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // If the timer is neither running nor ringing, and completion time is in the past, it's finished/idle.
    if (!running && !ringing && (tc <= now)) {
        detachKClockTimer();
        resetInternalState();
        return;
    }

    if (props.contains(QStringLiteral("length"))) {
        m_timerTotalSecs = props.value(QStringLiteral("length")).toLongLong();
    }
    if (props.contains(QStringLiteral("label"))) {
        m_timerLabel = props.value(QStringLiteral("label")).toString();
    }
    m_timerTarget = QDateTime::fromSecsSinceEpoch(tc);

    if (ringing) {
        if (!m_timerRinging) {
            m_countdownTimer->stop();
            m_timerRinging = true;
            m_timerPaused = false;
            m_timerRemainingText = QStringLiteral("00:00");
            const QString finishedLabel = m_timerLabel.isEmpty() ? i18n("Temporizador") : m_timerLabel;
            Q_EMIT timerFinished(finishedLabel, m_timerTotalSecs);

            // Auto-dismiss automatically after alarm audio duration (matching KClock behavior)
            m_autoDismissTimer->start(s_kclockAlarmDurationMs);
        }
    } else {
        m_timerRinging = false;
        m_timerPaused = !running;
    }

    m_timerActive = true;
    m_isKClockBacked = true;

    if (running && !m_timerRinging) {
        if (!m_countdownTimer->isActive()) {
            m_countdownTimer->start(1000);
        }
    } else {
        m_countdownTimer->stop();
    }

    onTimerTick();
    Q_EMIT timerChanged();
}

double TimerManager::progress() const
{
    if (!m_timerActive || m_timerTotalSecs <= 0) {
        return 0.0;
    }
    if (m_timerRinging) {
        return 1.0;
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

    m_autoDismissTimer->stop();

    // Optimistic local update for instant UI responsiveness
    m_timerTotalSecs = durationSeconds;
    m_timerPausedRemainingSecs = durationSeconds;
    m_timerPaused = false;
    m_timerRinging = false;
    m_timerTarget = QDateTime::currentDateTime().addSecs(durationSeconds);
    m_timerLabel = label;
    m_timerActive = true;
    m_timerRemainingText = formatCountdown(durationSeconds);
    m_countdownTimer->start(1000);
    Q_EMIT timerChanged();

    // Delegate to KClock daemon
    QDBusMessage msg = QDBusMessage::createMethodCall(
        s_kclockService,
        s_kclockTimersPath,
        s_kclockModelInterface,
        QStringLiteral("addTimer")
    );
    msg << durationSeconds << label << false << QString() << true;

    QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(msg);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        QDBusPendingReply<> reply = *w;
        if (!reply.isValid()) {
            m_isKClockBacked = false;
        }
    });
}

void TimerManager::togglePauseTimer()
{
    if (!m_timerActive || m_timerRinging) return;

    if (m_isKClockBacked && !m_currentTimerPath.isEmpty()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            s_kclockService,
            m_currentTimerPath,
            s_kclockTimerInterface,
            QStringLiteral("toggleRunning")
        );
        QDBusConnection::sessionBus().asyncCall(msg);
    }

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
    m_autoDismissTimer->stop();

    if (m_isKClockBacked && !m_currentTimerUuid.isEmpty()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            s_kclockService,
            s_kclockTimersPath,
            s_kclockModelInterface,
            QStringLiteral("removeTimer")
        );
        msg << m_currentTimerUuid;
        QDBusConnection::sessionBus().asyncCall(msg);
    }

    detachKClockTimer();
    resetInternalState();
}

void TimerManager::dismissTimer()
{
    m_autoDismissTimer->stop();

    if (m_isKClockBacked && !m_currentTimerPath.isEmpty()) {
        QDBusMessage dismissMsg = QDBusMessage::createMethodCall(
            s_kclockService,
            m_currentTimerPath,
            s_kclockTimerInterface,
            QStringLiteral("dismiss")
        );
        QDBusConnection::sessionBus().asyncCall(dismissMsg);

        if (!m_currentTimerUuid.isEmpty()) {
            QDBusMessage removeMsg = QDBusMessage::createMethodCall(
                s_kclockService,
                s_kclockTimersPath,
                s_kclockModelInterface,
                QStringLiteral("removeTimer")
            );
            removeMsg << m_currentTimerUuid;
            QDBusConnection::sessionBus().asyncCall(removeMsg);
        }
    }

    detachKClockTimer();
    resetInternalState();
}

void TimerManager::resetInternalState()
{
    m_countdownTimer->stop();
    m_autoDismissTimer->stop();
    m_timerActive = false;
    m_timerPaused = false;
    m_timerRinging = false;
    m_isKClockBacked = false;
    m_timerTotalSecs = 0;
    m_timerPausedRemainingSecs = 0;
    m_timerRemainingText.clear();
    m_timerLabel.clear();
    m_currentTimerId.clear();
    m_currentTimerPath.clear();
    m_currentTimerUuid.clear();
    Q_EMIT timerChanged();
}

void TimerManager::playFallbackAlarmSound()
{
    QString alarmSound = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("sounds/macOS-GoldenGate/stereo/alarm-clock-elapsed.oga"));
    if (alarmSound.isEmpty()) {
        alarmSound = QStringLiteral("/usr/share/sounds/macOS-GoldenGate/stereo/alarm-clock-elapsed.oga");
    }
    if (QFile::exists(alarmSound)) {
        QProcess::startDetached(QStringLiteral("pw-play"), {alarmSound});
    }
}

void TimerManager::sendFallbackNotification(const QString &finishedLabel)
{
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
}

void TimerManager::onTimerTick()
{
    if (!m_timerActive) return;

    if (m_timerRinging) {
        m_timerRemainingText = QStringLiteral("00:00");
        return;
    }

    if (m_timerPaused) {
        m_timerRemainingText = formatCountdown(m_timerPausedRemainingSecs);
        return;
    }

    const qint64 rem = QDateTime::currentDateTime().secsTo(m_timerTarget);
    if (rem <= 0) {
        m_countdownTimer->stop();
        m_timerRinging = true;
        m_timerPaused = false;
        m_timerRemainingText = QStringLiteral("00:00");
        const QString finishedLabel = m_timerLabel.isEmpty() ? i18n("Temporizador") : m_timerLabel;
        const qint64 totalSecs = m_timerTotalSecs;

        if (!m_isKClockBacked) {
            playFallbackAlarmSound();
            sendFallbackNotification(finishedLabel);
        }

        Q_EMIT timerFinished(finishedLabel, totalSecs);
        Q_EMIT timerChanged();

        // Auto-dismiss automatically after 6.5s (sound duration in KClock)
        m_autoDismissTimer->start(s_kclockAlarmDurationMs);
        return;
    }

    m_timerRemainingText = formatCountdown(rem);
    Q_EMIT timerChanged();
}
