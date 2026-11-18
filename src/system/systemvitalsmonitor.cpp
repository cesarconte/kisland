/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "systemvitalsmonitor.h"

#include <QFile>
#include <QStringList>

SystemVitalsMonitor::SystemVitalsMonitor(QObject *parent)
    : QObject(parent)
{
}

void SystemVitalsMonitor::updateVitals()
{
    // CPU utilization delta from /proc/stat
    QFile statFile(QStringLiteral("/proc/stat"));
    if (statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString line = QString::fromUtf8(statFile.readLine()).simplified();
        const QStringList parts = line.split(QLatin1Char(' '));
        if (parts.size() >= 5) {
            qint64 user = parts.at(1).toLongLong();
            qint64 nice = parts.at(2).toLongLong();
            qint64 sys = parts.at(3).toLongLong();
            qint64 idle = parts.at(4).toLongLong();
            qint64 iowait = parts.value(5).toLongLong();
            const qint64 total = user + nice + sys + idle + iowait;
            const qint64 totalDelta = total - m_prevCpuTotal;
            const qint64 idleDelta = idle - m_prevCpuIdle;
            if (m_prevCpuTotal > 0 && totalDelta > 0) {
                m_cpuUsagePercent = qBound(0, static_cast<int>(100 * (totalDelta - idleDelta) / totalDelta), 100);
            }
            m_prevCpuTotal = total;
            m_prevCpuIdle = idle;
        }
    }

    // RAM utilization from /proc/meminfo
    QFile memFile(QStringLiteral("/proc/meminfo"));
    if (memFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qint64 totalKb = 0;
        qint64 availKb = 0;
        const QStringList lines = QString::fromUtf8(memFile.readAll()).split(QLatin1Char('\n'));
        for (const QString &l : lines) {
            if (l.startsWith(QStringLiteral("MemTotal:"))) {
                totalKb = l.simplified().section(QLatin1Char(' '), 1, 1).toLongLong();
            } else if (l.startsWith(QStringLiteral("MemAvailable:"))) {
                availKb = l.simplified().section(QLatin1Char(' '), 1, 1).toLongLong();
            }
        }
        if (totalKb > 0) {
            m_ramUsagePercent = qBound(0, static_cast<int>(100 * (totalKb - availKb) / totalKb), 100);
        }
    }
    Q_EMIT vitalsChanged();
}
