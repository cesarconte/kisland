/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "songrecognizer.h"

#include <cmath>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>
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
                const QString artUrl = meta.value(QStringLiteral("mpris:artUrl")).toString().trimmed();
                qDebug() << "Detected live MPRIS2 player:" << srv << "status:" << props.value(QStringLiteral("PlaybackStatus")).toString() << "title:" << title << "artUrl:" << artUrl;
                QVERIFY(!title.isEmpty());
            }
        }
    }

    void testWpctlStatusAndPrivacyParsing()
    {
        const QString sampleWpctl = QStringLiteral(
            "Audio\n"
            " ├─ Sinks:\n"
            " │      52. Audio Interno Estéreo digital (IEC958) [vol: 0.40]\n"
            " │  *  211. SRS-XB13                            [vol: 0.65]\n"
            " └─ Streams:\n"
            "       160. cava\n"
            "            116. input_FL        < SRS-XB13:monitor_FL\t[active]\n"
            "       213. pw-record\n"
            "            163. monitor_FL\n"
            "            188. input_FL        < Easy Effects Source:capture_FL\t[active]\n"
            "       280. Firefox\n"
            "            119. input_FL        < Easy Effects Source:capture_FL\t[active]\n"
            "Video\n"
            " └─ Streams:\n"
            "       301. obs\n"
            "            302. input_1         < kwin_wayland:output_1\t[active]\n");

        static const QRegularExpression sinkLineRe(QStringLiteral(R"(^\s*│?\s*\*\s*\d+\.\s+(.+?)\s+\[vol:\s*([0-9.]+)(\s+MUTED)?\])"));
        static const QRegularExpression streamHeaderRe(QStringLiteral(R"(^\s{1,9}\d+\.\s+([A-Za-z0-9_. -]+?)\s*$)"));

        QString deviceName;
        int volPercent = 0;
        bool micActive = false;
        QString micApp;
        bool camActive = false;
        QString camApp;

        QString section;
        QString subSection;
        QString currentClient;
        for (const QString &rawLine : sampleWpctl.split(QLatin1Char('\n'))) {
            const QString trimmed = rawLine.trimmed();
            if (trimmed == QStringLiteral("Audio") || trimmed == QStringLiteral("Video")) {
                section = trimmed;
                subSection.clear();
                continue;
            }
            if (trimmed.contains(QStringLiteral("Sinks:"))) {
                subSection = QStringLiteral("Sinks");
                continue;
            }
            if (trimmed.contains(QStringLiteral("Streams:"))) {
                subSection = QStringLiteral("Streams");
                currentClient.clear();
                continue;
            }
            if (section == QStringLiteral("Audio") && subSection == QStringLiteral("Sinks")) {
                const auto m = sinkLineRe.match(rawLine);
                if (m.hasMatch()) {
                    deviceName = m.captured(1).trimmed();
                    volPercent = qRound(m.captured(2).toDouble() * 100.0);
                }
            } else if (section == QStringLiteral("Audio") && subSection == QStringLiteral("Streams")) {
                const auto hm = streamHeaderRe.match(rawLine);
                if (hm.hasMatch() && !rawLine.contains(QLatin1Char('<'))) {
                    currentClient = hm.captured(1).trimmed();
                } else if (rawLine.contains(QLatin1Char('<')) && rawLine.contains(QStringLiteral("[active]"))
                           && rawLine.contains(QStringLiteral(":capture_"))) {
                    micActive = true;
                    micApp = currentClient;
                }
            } else if (section == QStringLiteral("Video") && subSection == QStringLiteral("Streams")) {
                const auto hm = streamHeaderRe.match(rawLine);
                if (hm.hasMatch() && !rawLine.contains(QLatin1Char('<'))) {
                    currentClient = hm.captured(1).trimmed();
                } else if (rawLine.contains(QStringLiteral("[active]"))) {
                    camActive = true;
                    camApp = currentClient;
                }
            }
        }

        QCOMPARE(deviceName, QStringLiteral("SRS-XB13"));
        QCOMPARE(volPercent, 65);
        QVERIFY(micActive);
        QCOMPARE(micApp, QStringLiteral("Firefox"));
        QVERIFY(camActive);
        QCOMPARE(camApp, QStringLiteral("obs"));
    }

    void testShazamJsonParsing()
    {
        const QByteArray json = R"({"track":{"title":"Bohemian Rhapsody","subtitle":"Queen","url":"https://www.shazam.com/track/1","images":{"coverart":"https://img/cover.jpg"}}})";
        const QVariantMap r = SongRecognizer::parseShazamJson(json);
        QCOMPARE(r.value(QStringLiteral("title")).toString(), QStringLiteral("Bohemian Rhapsody"));
        QCOMPARE(r.value(QStringLiteral("artist")).toString(), QStringLiteral("Queen"));
        QCOMPARE(r.value(QStringLiteral("cover")).toString(), QStringLiteral("https://img/cover.jpg"));
        QVERIFY(SongRecognizer::parseShazamJson(R"({"matches":[]})").isEmpty());
        QVERIFY(SongRecognizer::parseShazamJson("garbage").isEmpty());
    }

    void testMainQmlCompilesWithoutErrors()
    {
        QQmlEngine engine;
        const QString qmlPath = QStringLiteral(KISLAND_SOURCE_DIR) + QStringLiteral("/src/qml/main.qml");
        QVERIFY(QFile::exists(qmlPath));
        QQmlComponent comp(&engine, QUrl::fromLocalFile(qmlPath));
        QVERIFY2(!comp.isError(), qPrintable(comp.errorString()));
    }
};

QTEST_MAIN(TestKIsland)
#include "test_kisland.moc"
