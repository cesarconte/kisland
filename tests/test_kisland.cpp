/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include <cmath>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
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

    void testMprisAndPlayerctlParsing()
    {
        // Simulate mpv MPRIS line with title and URL but no artist or album (like an MKV video in mpv)
        const QString line = QStringLiteral("Paused\tmpv\tEvil - S02E10 - O Is for Ovaphobia\t\t\tfile:///media/Series/Evil.S02E10.mkv");
        const QStringList parts = line.split(QLatin1Char('\t'));
        QCOMPARE(parts.size(), 6);
        QCOMPARE(parts.at(0), QStringLiteral("Paused"));
        QCOMPARE(parts.at(1), QStringLiteral("mpv"));
        QCOMPARE(parts.at(2), QStringLiteral("Evil - S02E10 - O Is for Ovaphobia"));

        // Live D-Bus test against active MPRIS2 session services (e.g. mpv)
        if (auto *busIface = QDBusConnection::sessionBus().interface()) {
            const QStringList services = busIface->registeredServiceNames().value();
            for (const QString &srv : services) {
                if (!srv.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) continue;

                QDBusMessage getAllMsg = QDBusMessage::createMethodCall(
                    srv,
                    QStringLiteral("/org/mpris/MediaPlayer2"),
                    QStringLiteral("org.freedesktop.DBus.Properties"),
                    QStringLiteral("GetAll"));
                getAllMsg << QStringLiteral("org.mpris.MediaPlayer2.Player");

                const QDBusMessage reply = QDBusConnection::sessionBus().call(getAllMsg, QDBus::Block, 500);
                QCOMPARE(reply.type(), QDBusMessage::ReplyMessage);
                QVERIFY(!reply.arguments().isEmpty());

                QVariantMap props;
                const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
                arg >> props;
                QVERIFY(props.contains(QStringLiteral("PlaybackStatus")));
                QVERIFY(props.contains(QStringLiteral("Metadata")));

                QVariant metaVar = props.value(QStringLiteral("Metadata"));
                if (metaVar.userType() == qMetaTypeId<QDBusVariant>()) {
                    metaVar = metaVar.value<QDBusVariant>().variant();
                }
                QVariantMap meta;
                if (metaVar.userType() == qMetaTypeId<QDBusArgument>()) {
                    metaVar.value<QDBusArgument>() >> meta;
                }
                const QString title = meta.value(QStringLiteral("xesam:title")).toString().trimmed();
                qDebug() << "Detected live MPRIS2 player:" << srv << "status:" << props.value(QStringLiteral("PlaybackStatus")).toString() << "title:" << title;
                QVERIFY(!title.isEmpty());
            }
        }
    }
};

QTEST_MAIN(TestKIsland)
#include "test_kisland.moc"
