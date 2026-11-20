/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

class QDBusServiceWatcher;

class TimerManager : public QObject
{
    Q_OBJECT

public:
    explicit TimerManager(QObject *parent = nullptr);
    ~TimerManager() override = default;

    bool isActive() const { return m_timerActive; }
    bool isPaused() const { return m_timerPaused; }
    bool isRinging() const { return m_timerRinging; }
    bool isKClockBacked() const { return m_isKClockBacked; }
    double progress() const;
    QString remainingText() const { return m_timerRemainingText; }
    QString label() const { return m_timerLabel; }

    void startQuickTimer(int durationSeconds, const QString &label);
    void togglePauseTimer();
    void cancelQuickTimer();
    void dismissTimer();

    static QString formatCountdown(qint64 remainingSeconds);

Q_SIGNALS:
    void timerChanged();
    void timerFinished(const QString &label, qint64 totalSecs);

private Q_SLOTS:
    void onTimerTick();
    void onKClockTimerAdded(const QString &timerId);
    void onKClockTimerRemoved(const QString &timerId);
    void onKClockTimerPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProps, const QStringList &invalidatedProps);
    void onKClockTimerSignalStateChanged();
    void onKClockServiceRegistered(const QString &serviceName);
    void onKClockServiceUnregistered(const QString &serviceName);

private:
    void initKClockConnection();
    void checkKClockTimers();
    void attachToKClockTimer(const QString &timerId);
    void detachKClockTimer();
    void updateFromKClockProperties(const QVariantMap &props);
    void playFallbackAlarmSound();
    void sendFallbackNotification(const QString &finishedLabel);
    void resetInternalState();

    bool m_timerActive = false;
    bool m_timerPaused = false;
    bool m_timerRinging = false;
    bool m_isKClockBacked = false;

    qint64 m_timerTotalSecs = 0;
    qint64 m_timerPausedRemainingSecs = 0;
    QDateTime m_timerTarget;
    QString m_timerLabel;
    QString m_timerRemainingText;

    QString m_currentTimerId;
    QString m_currentTimerPath;
    QString m_currentTimerUuid;

    QTimer *m_countdownTimer = nullptr;
    QTimer *m_autoDismissTimer = nullptr;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;
};
