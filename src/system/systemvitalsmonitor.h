/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>

class SystemVitalsMonitor : public QObject
{
    Q_OBJECT

public:
    explicit SystemVitalsMonitor(QObject *parent = nullptr);
    ~SystemVitalsMonitor() override = default;

    int cpuUsagePercent() const { return m_cpuUsagePercent; }
    int ramUsagePercent() const { return m_ramUsagePercent; }

    void updateVitals();

Q_SIGNALS:
    void vitalsChanged();

private:
    int m_cpuUsagePercent = 0;
    int m_ramUsagePercent = 0;
    qint64 m_prevCpuTotal = 0;
    qint64 m_prevCpuIdle = 0;
};
