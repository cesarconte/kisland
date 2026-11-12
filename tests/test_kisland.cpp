/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "songrecognizer.h"

#include <algorithm>
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
        };

        QCOMPARE(fmt(0), QStringLiteral("00:00"));
        QCOMPARE(fmt(65), QStringLiteral("01:05"));
        QCOMPARE(fmt(1500), QStringLiteral("25:00"));
        QCOMPARE(fmt(3665), QStringLiteral("01:01:05"));
    }

    void testSpectrumBarComputation()
    {
        const int barCount = 6;
        constexpr int totalSamples = 256;

        auto computeBars = [=](const QByteArray &pcmChunk) {
            QVariantList result;
            if (pcmChunk.size() < totalSamples * static_cast<int>(sizeof(qint16))) {
                for (int i = 0; i < barCount; ++i) result.append(0.15);
                return result;
            }
            const qint16 *samples = reinterpret_cast<const qint16 *>(pcmChunk.constData());
            std::array<double, totalSamples> re;
            std::array<double, totalSamples> im;
            im.fill(0.0);
            for (int i = 0; i < totalSamples; ++i) {
                re[i] = samples[i] / 32768.0;
            }

            int j = 0;
            for (int i = 0; i < totalSamples - 1; ++i) {
                if (i < j) std::swap(re[i], re[j]);
                int k = totalSamples >> 1;
                while (k <= j) { j -= k; k >>= 1; }
                j += k;
            }

            int step = 2;
            while (step <= totalSamples) {
                const int half = step >> 1;
                const double angle = -2.0 * M_PI / step;
                const double w_re = std::cos(angle);
                const double w_im = std::sin(angle);
                for (int i = 0; i < totalSamples; i += step) {
                    double u_re = 1.0;
                    double u_im = 0.0;
                    for (int m = 0; m < half; ++m) {
                        const int pos = i + m;
                        const double t_re = u_re * re[pos + half] - u_im * im[pos + half];
                        const double t_im = u_re * im[pos + half] + u_im * re[pos + half];
                        re[pos + half] = re[pos] - t_re;
                        im[pos + half] = im[pos] - t_im;
                        re[pos] += t_re;
                        im[pos] += t_im;
                        const double next_u_re = u_re * w_re - u_im * w_im;
                        u_im = u_re * w_im + u_im * w_re;
                        u_re = next_u_re;
                    }
                }
                step <<= 1;
            }

            static const std::array<std::pair<int, int>, 6> bands = {{
                {2, 6}, {7, 15}, {16, 31}, {32, 63}, {64, 95}, {96, 127}
            }};

            for (int b = 0; b < barCount; ++b) {
                int k_start = 2;
                int k_end = 127;
                if (b < static_cast<int>(bands.size())) {
                    k_start = bands[b].first;
                    k_end = bands[b].second;
                }
                double sum_mag = 0.0;
                for (int k = k_start; k <= k_end; ++k) {
                    sum_mag += std::sqrt(re[k] * re[k] + im[k] * im[k]);
                }
                const double avg_mag = sum_mag / ((k_end - k_start + 1) * (totalSamples / 2.0));
                const double weight = 1.0 + 1.5 * (static_cast<double>(b) / std::max(1, barCount - 1));
                const double energy = avg_mag * weight;
                const double scaled = 0.15 + 0.85 * std::min(1.0, std::sqrt(energy) * 3.2);
                result.append(scaled);
            }
            return result;
        };

        // 1. Silent PCM
        QByteArray silentPcm(totalSamples * sizeof(qint16), 0);
        const QVariantList silentBars = computeBars(silentPcm);
        QCOMPARE(silentBars.size(), barCount);
        for (const QVariant &b : silentBars) {
            QCOMPARE(b.toDouble(), 0.15);
        }

        // 2. Tone PCM (1000 Hz)
        QByteArray tonePcm(totalSamples * sizeof(qint16), 0);
        qint16 *samples = reinterpret_cast<qint16 *>(tonePcm.data());
        for (int i = 0; i < totalSamples; ++i) {
            samples[i] = static_cast<qint16>(16000.0 * std::sin(2.0 * M_PI * 1000.0 * i / 8000.0));
        }
        const QVariantList toneBars = computeBars(tonePcm);
        QCOMPARE(toneBars.size(), barCount);
        for (const QVariant &b : toneBars) {
            QVERIFY(b.toDouble() >= 0.15 && b.toDouble() <= 1.0);
        }
        // Band 3 (1000-2000Hz, bin 32) should be stimulated significantly
        QVERIFY(toneBars.at(3).toDouble() > 0.4);

        // 3. Small chunk fallback
        QByteArray smallPcm(10, 0);
        const QVariantList smallBars = computeBars(smallPcm);
        QCOMPARE(smallBars.size(), barCount);
        for (const QVariant &b : smallBars) {
            QCOMPARE(b.toDouble(), 0.15);
        }
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
                const QString status = props.value(QStringLiteral("PlaybackStatus")).toString();
                qDebug() << "Detected live MPRIS2 player:" << srv << "status:" << status << "title:" << title << "artUrl:" << artUrl;
                if (status == QStringLiteral("Playing")) {
                    QVERIFY(!title.isEmpty());
                } else {
                    QVERIFY(!status.isEmpty());
                }
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

    void testMainQmlLayoutConstraintsAndKdeConnectNaming()
    {
        const QString qmlPath = QStringLiteral(KISLAND_SOURCE_DIR) + QStringLiteral("/src/qml/main.qml");
        QFile f(qmlPath);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString qml = QString::fromUtf8(f.readAll());

        // Enforce tight Layout bounds to avoid wasted empty space
        QVERIFY(qml.contains(QStringLiteral("Layout.maximumHeight: targetHeight")));
        QVERIFY(qml.contains(QStringLiteral("Layout.maximumWidth: targetWidth")));

        // Enforce native KDE Connect naming without hybrid parentheses
        QVERIFY(!qml.contains(QStringLiteral("AirDrop (KDE Connect)")));
        QVERIFY(!qml.contains(QStringLiteral("AirDrop / KDE Connect")));
        QVERIFY(qml.contains(QStringLiteral("text: i18n(\"KDE Connect\")")));
    }
};

QTEST_MAIN(TestKIsland)
#include "test_kisland.moc"
