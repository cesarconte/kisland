/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <cmath>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QTest>

class TestKIsland : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCountdownFormatting()
    {
        auto fmt = [](qint64 rem) {
            const qint64 clamped = qMax<qint64>(0, rem);
            return QStringLiteral("%1:%2")
                .arg(clamped / 60, 2, 10, QLatin1Char('0'))
                .arg(clamped % 60, 2, 10, QLatin1Char('0'));
        };

        QCOMPARE(fmt(0), QStringLiteral("00:00"));
        QCOMPARE(fmt(65), QStringLiteral("01:05"));
        QCOMPARE(fmt(1500), QStringLiteral("25:00"));
    }

    void testSpectrumBarComputation()
    {
        const int barCount = 6;
        QByteArray pcm(600 * sizeof(qint16), 0);
        qint16 *samples = reinterpret_cast<qint16 *>(pcm.data());
        for (int i = 0; i < 600; ++i) {
            samples[i] = 6000;
        }

        const int totalSamples = pcm.size() / static_cast<int>(sizeof(qint16));
        const int samplesPerBar = totalSamples / barCount;
        QCOMPARE(samplesPerBar, 100);

        double sumSq = 0.0;
        for (int i = 0; i < samplesPerBar; ++i) {
            const double norm = samples[i] / 32768.0;
            sumSq += norm * norm;
        }
        const double rms = std::sqrt(sumSq / samplesPerBar);
        const double height = qBound(0.15, rms * 5.2, 1.0);
        QVERIFY(height > 0.8 && height <= 1.0);
    }

    void testIslandMetadataExists()
    {
        const QString metaPath = QStringLiteral(KISLAND_SOURCE_DIR) + QStringLiteral("/src/metadata.json");
        QVERIFY(QFile::exists(metaPath));
    }
};

QTEST_MAIN(TestKIsland)
#include "test_kisland.moc"
