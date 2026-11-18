/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>

class TimerManager : public QObject
{
    Q_OBJECT

public:
    explicit TimerManager(QObject *parent = nullptr);
    ~TimerManager() override = default;

    bool isActive() const { return m_timerActive; }
    bool isPaused() const { return m_timerPaused; }
    double progress() const;
    QString remainingText() const { return m_timerRemainingText; }
    QString label() const { return m_timerLabel; }

    void startQuickTimer(int durationSeconds, const QString &label);
    void togglePauseTimer();
    void cancelQuickTimer();

    static QString formatCountdown(qint64 remainingSeconds);

Q_SIGNALS:
    void timerChanged();
    void timerFinished(const QString &label, qint64 totalSecs);

private Q_SLOTS:
    void onTimerTick();

private:
    bool m_timerActive = false;
    bool m_timerPaused = false;
    qint64 m_timerTotalSecs = 0;
    qint64 m_timerPausedRemainingSecs = 0;
    QDateTime m_timerTarget;
    QString m_timerLabel;
    QString m_timerRemainingText;
    QTimer *m_countdownTimer = nullptr;
};
