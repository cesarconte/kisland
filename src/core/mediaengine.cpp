/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "mediaengine.h"
#include "timermanager.h"

#include <cmath>

#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

MediaEngine::MediaEngine(QObject *parent)
    : QObject(parent)
{
}

MediaEngine::~MediaEngine()
{
    stopPipeWireSpectrumMonitor();
}

QString MediaEngine::trackElapsedText() const
{
    return TimerManager::formatCountdown(m_trackPositionSecs);
}

QString MediaEngine::trackRemainingText() const
{
    if (m_trackDurationSecs <= 0) {
        return QStringLiteral("--:--");
    }
    const qint64 rem = qMax<qint64>(0, m_trackDurationSecs - m_trackPositionSecs);
    return QStringLiteral("-%1").arg(TimerManager::formatCountdown(rem));
}

double MediaEngine::trackProgress() const
{
    if (m_trackDurationSecs <= 0) {
        return 0.0;
    }
    return qBound(0.0, static_cast<double>(m_trackPositionSecs) / static_cast<double>(m_trackDurationSecs), 1.0);
}

QVariantList MediaEngine::computeSpectrumBarsFromPcm(const QByteArray &pcmChunk, int barCount)
{
    QVariantList result;
    if (barCount <= 0) return result;

    constexpr int totalSamples = 256;
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

    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < totalSamples - 1; ++i) {
        if (i < j) {
            std::swap(re[i], re[j]);
        }
        int k = totalSamples >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    // Cooley-Tukey Radix-2 FFT
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

    // 6 log-distributed musical frequency bands spanning 60 Hz to 4000 Hz
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
}

QVariantMap MediaEngine::extractMprisMetadata(const QVariant &metadataVariant)
{
    QVariantMap map;
    QVariant unpacked = metadataVariant;
    if (unpacked.userType() == qMetaTypeId<QDBusVariant>()) {
        unpacked = unpacked.value<QDBusVariant>().variant();
    }
    if (unpacked.userType() == qMetaTypeId<QDBusArgument>()) {
        const QDBusArgument arg = unpacked.value<QDBusArgument>();
        arg >> map;
    } else if (unpacked.canConvert<QVariantMap>()) {
        map = unpacked.toMap();
    }
    return map;
}

QVariantMap MediaEngine::parsePlayerctlLine(const QString &line)
{
    QVariantMap res;
    const QStringList parts = line.trimmed().split(QLatin1Char('\t'));
    if (parts.size() < 6) {
        return res;
    }
    const QString status = parts.at(0).trimmed();
    const QString player = parts.at(1).trimmed();
    QString title = parts.at(2).trimmed();
    const QString artist = parts.at(3).trimmed();
    const QString album = parts.at(4).trimmed();
    const QString urlStr = parts.at(5).trimmed();
    const QString artUrlStr = parts.size() >= 7 ? parts.at(6).trimmed() : QString();

    if (title.isEmpty() && !urlStr.isEmpty()) {
        const QUrl u(urlStr);
        title = u.isLocalFile() ? QFileInfo(u.toLocalFile()).completeBaseName() : u.fileName();
    }

    res[QStringLiteral("playing")] = (status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0);
    res[QStringLiteral("status")] = status;
    res[QStringLiteral("player")] = player;
    res[QStringLiteral("title")] = title;
    res[QStringLiteral("artist")] = artist;
    res[QStringLiteral("album")] = album;
    res[QStringLiteral("url")] = urlStr;
    res[QStringLiteral("artUrl")] = artUrlStr;
    return res;
}

QString MediaEngine::resolveCoverArtOrVideoThumbnail(const QString &artUrl, const QString &mediaUrl, bool *isVideoOut)
{
    bool isVid = false;
    QString localMediaPath;
    if (!mediaUrl.isEmpty()) {
        const QUrl u(mediaUrl);
        localMediaPath = u.isLocalFile() ? u.toLocalFile() : mediaUrl;
        const QString ext = QFileInfo(localMediaPath).suffix().toLower();
        static const QStringList videoExts = {
            QStringLiteral("mkv"), QStringLiteral("mp4"), QStringLiteral("webm"),
            QStringLiteral("avi"), QStringLiteral("mov"), QStringLiteral("m4v"), QStringLiteral("ts")
        };
        if (videoExts.contains(ext)) {
            isVid = true;
        }
    }
    if (isVideoOut) {
        *isVideoOut = isVid;
    }

    if (!artUrl.trimmed().isEmpty()) {
        const QUrl au(artUrl.trimmed());
        if (au.isLocalFile()) {
            if (QFileInfo::exists(au.toLocalFile())) {
                return au.toString();
            }
        } else if (au.scheme() == QStringLiteral("http") || au.scheme() == QStringLiteral("https") || au.scheme() == QStringLiteral("data")) {
            return au.toString();
        }
    }

    if (localMediaPath.isEmpty() || !QFileInfo::exists(localMediaPath)) {
        return QString();
    }

    const QFileInfo mediaFi(localMediaPath);
    const QDir folder = mediaFi.absoluteDir();

    static const QStringList coverCandidates = {
        QStringLiteral("cover.jpg"), QStringLiteral("cover.png"),
        QStringLiteral("folder.jpg"), QStringLiteral("folder.png"),
        QStringLiteral("poster.jpg"), QStringLiteral("poster.png"),
        QStringLiteral("front.jpg"), QStringLiteral("album.jpg")
    };
    for (const QString &cand : coverCandidates) {
        const QString candPath = folder.filePath(cand);
        if (QFileInfo::exists(candPath)) {
            return QUrl::fromLocalFile(candPath).toString();
        }
    }

    const QByteArray pathHash = QCryptographicHash::hash(localMediaPath.toUtf8(), QCryptographicHash::Sha1).toHex();
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));

    if (isVid && !ffmpeg.isEmpty()) {
        const QString thumbPath = QDir::tempPath() + QStringLiteral("/kisland_vidthumb_%1.jpg").arg(QString::fromLatin1(pathHash));
        if (!QFileInfo::exists(thumbPath)) {
            QProcess proc;
            proc.start(ffmpeg, {QStringLiteral("-y"), QStringLiteral("-ss"), QStringLiteral("00:00:15"),
                                QStringLiteral("-i"), localMediaPath,
                                QStringLiteral("-frames:v"), QStringLiteral("1"),
                                QStringLiteral("-vf"), QStringLiteral("scale=240:-1"),
                                QStringLiteral("-q:v"), QStringLiteral("4"),
                                thumbPath});
            proc.waitForFinished(1200);
        }
        if (QFileInfo::exists(thumbPath) && QFileInfo(thumbPath).size() > 256) {
            return QUrl::fromLocalFile(thumbPath).toString();
        }
    }

    if (!isVid && !ffmpeg.isEmpty()) {
        const QString artCachePath = QDir::tempPath() + QStringLiteral("/kisland_audiocover_%1.jpg").arg(QString::fromLatin1(pathHash));
        if (!QFileInfo::exists(artCachePath)) {
            QProcess proc;
            proc.start(ffmpeg, {QStringLiteral("-y"), QStringLiteral("-i"), localMediaPath,
                                QStringLiteral("-an"), QStringLiteral("-vcodec"), QStringLiteral("mjpeg"),
                                QStringLiteral("-frames:v"), QStringLiteral("1"),
                                artCachePath});
            proc.waitForFinished(900);
        }
        if (QFileInfo::exists(artCachePath) && QFileInfo(artCachePath).size() > 256) {
            return QUrl::fromLocalFile(artCachePath).toString();
        }
    }

    return QString();
}

void MediaEngine::pollPlayers(bool songRecognizerListening)
{
    bool foundPlaying = false;
    QString newTitle;
    QString newArtist;
    QString newAlbum;
    QString newRawArtUrl;
    QString newMediaUrl;
    QString newPlayer;
    QString newObjectId;
    QString chosenService;
    qint64 newPosSecs = 0;
    qint64 newDurSecs = 0;
    bool newShuffle = false;
    QString newLoop = QStringLiteral("None");

    if (auto *busIface = QDBusConnection::sessionBus().interface()) {
        const QStringList services = busIface->registeredServiceNames().value();
        for (const QString &srv : services) {
            if (!srv.startsWith(QStringLiteral("org.mpris.MediaPlayer2."))) {
                continue;
            }

            QDBusMessage getAllMsg = QDBusMessage::createMethodCall(
                srv,
                QStringLiteral("/org/mpris/MediaPlayer2"),
                QStringLiteral("org.freedesktop.DBus.Properties"),
                QStringLiteral("GetAll"));
            getAllMsg << QStringLiteral("org.mpris.MediaPlayer2.Player");

            const QDBusMessage reply = QDBusConnection::sessionBus().call(getAllMsg, QDBus::Block, 400);
            if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) {
                continue;
            }

            const QVariantMap props = extractMprisMetadata(reply.arguments().first());
            const QString status = props.value(QStringLiteral("PlaybackStatus")).toString().trimmed();
            const QVariantMap meta = extractMprisMetadata(props.value(QStringLiteral("Metadata")));

            const QString urlStr = meta.value(QStringLiteral("xesam:url")).toString().trimmed();
            const QString artUrlStr = meta.value(QStringLiteral("mpris:artUrl")).toString().trimmed();
            QString title = meta.value(QStringLiteral("xesam:title")).toString().trimmed();
            if (title.isEmpty() && !urlStr.isEmpty()) {
                const QUrl u(urlStr);
                title = u.isLocalFile() ? QFileInfo(u.toLocalFile()).completeBaseName() : u.fileName();
            }

            QStringList artists = meta.value(QStringLiteral("xesam:artist")).toStringList();
            if (artists.isEmpty() && !meta.value(QStringLiteral("xesam:artist")).toString().isEmpty()) {
                artists << meta.value(QStringLiteral("xesam:artist")).toString();
            }
            const QString album = meta.value(QStringLiteral("xesam:album")).toString().trimmed();
            const QString cleanPlayerName = srv.mid(23).section(QLatin1Char('.'), 0, 0);

            const qint64 posMicros = props.value(QStringLiteral("Position")).toLongLong();
            const qint64 lenMicros = meta.value(QStringLiteral("mpris:length")).toLongLong();
            QString trackObjPath;
            const QVariant trackIdVar = meta.value(QStringLiteral("mpris:trackid"));
            if (trackIdVar.userType() == qMetaTypeId<QDBusObjectPath>()) {
                trackObjPath = trackIdVar.value<QDBusObjectPath>().path();
            } else {
                trackObjPath = trackIdVar.toString();
            }

            const bool isPlayingNow = (status.compare(QStringLiteral("Playing"), Qt::CaseInsensitive) == 0);
            if (isPlayingNow || (chosenService.isEmpty() && !title.isEmpty())) {
                chosenService = srv;
                foundPlaying = isPlayingNow;
                newTitle = title;
                newArtist = artists.join(QStringLiteral(", "));
                newAlbum = album;
                newRawArtUrl = artUrlStr;
                newMediaUrl = urlStr;
                newPlayer = cleanPlayerName;
                newObjectId = trackObjPath;
                newPosSecs = qMax<qint64>(0, posMicros / 1000000LL);
                newDurSecs = qMax<qint64>(0, lenMicros / 1000000LL);
                newShuffle = props.value(QStringLiteral("Shuffle")).toBool();
                const QString lp = props.value(QStringLiteral("LoopStatus")).toString().trimmed();
                newLoop = lp.isEmpty() ? QStringLiteral("None") : lp;
                if (foundPlaying) {
                    break;
                }
            }
        }
    }

    if (newTitle.isEmpty()) {
        const QString playerctl = QStandardPaths::findExecutable(QStringLiteral("playerctl"));
        if (!playerctl.isEmpty()) {
            QProcess proc;
            proc.start(playerctl, {QStringLiteral("-a"), QStringLiteral("metadata"), QStringLiteral("--format"),
                                   QStringLiteral("{{status}}\t{{playerName}}\t{{title}}\t{{artist}}\t{{album}}\t{{xesam:url}}\t{{mpris:artUrl}}")});
            if (proc.waitForFinished(500) && proc.exitCode() == 0) {
                const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                for (const QString &l : lines) {
                    const QVariantMap parsed = parsePlayerctlLine(l);
                    if (parsed.isEmpty()) continue;
                    const bool pl = parsed.value(QStringLiteral("playing")).toBool();
                    const QString t = parsed.value(QStringLiteral("title")).toString();
                    if (pl || (newTitle.isEmpty() && !t.isEmpty())) {
                        foundPlaying = pl;
                        newTitle = t;
                        newArtist = parsed.value(QStringLiteral("artist")).toString();
                        newAlbum = parsed.value(QStringLiteral("album")).toString();
                        newMediaUrl = parsed.value(QStringLiteral("url")).toString();
                        newRawArtUrl = parsed.value(QStringLiteral("artUrl")).toString();
                        newPlayer = parsed.value(QStringLiteral("player")).toString();
                        if (foundPlaying) break;
                    }
                }
            }
        }
    }

    bool detectedIsVideo = false;
    const QString resolvedArt = newTitle.isEmpty()
        ? QString()
        : resolveCoverArtOrVideoThumbnail(newRawArtUrl, newMediaUrl, &detectedIsVideo);

    const bool changed = (m_mediaPlaying != foundPlaying || m_trackTitle != newTitle
                          || m_trackArtist != newArtist || m_playerName != newPlayer
                          || m_trackArtUrl != resolvedArt || m_isVideoMedia != detectedIsVideo
                          || m_trackPositionSecs != newPosSecs || m_trackDurationSecs != newDurSecs
                          || m_shuffleEnabled != newShuffle || m_loopStatus != newLoop);
    m_mediaPlaying = foundPlaying;
    m_trackTitle = newTitle;
    m_trackArtist = newArtist;
    m_trackAlbum = newAlbum;
    m_trackArtUrl = resolvedArt;
    m_trackObjectId = newObjectId;
    m_isVideoMedia = detectedIsVideo;
    m_playerName = newPlayer;
    m_activeMprisService = chosenService;
    m_trackPositionSecs = newPosSecs;
    m_trackDurationSecs = newDurSecs;
    m_shuffleEnabled = newShuffle;
    m_loopStatus = newLoop;

    const bool shouldMonitorAudio = m_mediaPlaying || songRecognizerListening;
    if (shouldMonitorAudio) {
        startPipeWireSpectrumMonitor();
    } else {
        stopPipeWireSpectrumMonitor();
    }

    if (changed) {
        Q_EMIT mediaStateChanged();
    }
}

void MediaEngine::startPipeWireSpectrumMonitor()
{
    if (m_pwMonitorProc) {
        if (m_pwMonitorProc->state() != QProcess::NotRunning) {
            return;
        }
        m_pwMonitorProc->deleteLater();
        m_pwMonitorProc = nullptr;
    }

    const QString pwRecord = QStandardPaths::findExecutable(QStringLiteral("pw-record"));
    const QString parec = QStandardPaths::findExecutable(QStringLiteral("parec"));
    if (pwRecord.isEmpty() && parec.isEmpty()) return;

    m_pwMonitorProc = new QProcess(this);
    connect(m_pwMonitorProc, &QProcess::readyReadStandardOutput, this, &MediaEngine::onPipeWireAudioChunk);

    if (!pwRecord.isEmpty()) {
        QStringList args;
        args << QStringLiteral("--raw")
             << QStringLiteral("--format=s16")
             << QStringLiteral("--rate=8000")
             << QStringLiteral("--channels=1")
             << QStringLiteral("-P") << QStringLiteral("{\"target.object\":\"@DEFAULT_SINK@\",\"stream.capture.sink\":true}")
             << QStringLiteral("-");
        m_pwMonitorProc->start(pwRecord, args);
    } else {
        QStringList args;
        args << QStringLiteral("-d") << QStringLiteral("@DEFAULT_MONITOR@")
             << QStringLiteral("--rate=8000")
             << QStringLiteral("--channels=1")
             << QStringLiteral("--format=s16le");
        m_pwMonitorProc->start(parec, args);
    }
}

void MediaEngine::stopPipeWireSpectrumMonitor()
{
    if (m_pwMonitorProc) {
        disconnect(m_pwMonitorProc, nullptr, this, nullptr);
        if (m_pwMonitorProc->state() != QProcess::NotRunning) {
            m_pwMonitorProc->terminate();
            if (!m_pwMonitorProc->waitForFinished(400)) {
                m_pwMonitorProc->kill();
            }
        }
        m_pwMonitorProc->deleteLater();
        m_pwMonitorProc = nullptr;
    }
    m_audioPcmBuffer.clear();
    m_spectrumBars = {0.15, 0.15, 0.15, 0.15, 0.15, 0.15};
    Q_EMIT spectrumChanged();
}

void MediaEngine::onPipeWireAudioChunk()
{
    if (!m_pwMonitorProc) return;
    const QByteArray chunk = m_pwMonitorProc->readAllStandardOutput();
    if (chunk.isEmpty()) return;

    m_audioPcmBuffer.append(chunk);

    constexpr int CHUNK_BYTES = 512;
    if (m_audioPcmBuffer.size() >= CHUNK_BYTES) {
        const QByteArray latestChunk = m_audioPcmBuffer.right(CHUNK_BYTES);
        if (m_audioPcmBuffer.size() > 1024) {
            m_audioPcmBuffer = m_audioPcmBuffer.right(CHUNK_BYTES);
        }
        m_spectrumBars = computeSpectrumBarsFromPcm(latestChunk, 6);
        Q_EMIT spectrumChanged();
    }
}

void MediaEngine::playPauseMedia()
{
    if (m_activeMprisService.isEmpty()) return;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("PlayPause"));
    QTimer::singleShot(250, this, [this]() { pollPlayers(); });
}

void MediaEngine::nextTrack()
{
    if (m_activeMprisService.isEmpty()) return;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Next"));
    QTimer::singleShot(350, this, [this]() { pollPlayers(); });
}

void MediaEngine::previousTrack()
{
    if (m_activeMprisService.isEmpty()) return;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Previous"));
    QTimer::singleShot(350, this, [this]() { pollPlayers(); });
}

void MediaEngine::seekToRatio(double ratio)
{
    if (m_activeMprisService.isEmpty() || m_trackDurationSecs <= 0) return;
    const double clamped = qBound(0.0, ratio, 1.0);
    const qint64 targetSecs = qRound64(clamped * static_cast<double>(m_trackDurationSecs));
    const qint64 targetMicros = targetSecs * 1000000LL;

    if (!m_trackObjectId.isEmpty()) {
        QDBusMessage msg = QDBusMessage::createMethodCall(
            m_activeMprisService,
            QStringLiteral("/org/mpris/MediaPlayer2"),
            QStringLiteral("org.mpris.MediaPlayer2.Player"),
            QStringLiteral("SetPosition"));
        msg << QVariant::fromValue(QDBusObjectPath(m_trackObjectId)) << targetMicros;
        QDBusConnection::sessionBus().asyncCall(msg);
    } else {
        const qint64 offsetMicros = (targetSecs - m_trackPositionSecs) * 1000000LL;
        QDBusInterface iface(m_activeMprisService,
                             QStringLiteral("/org/mpris/MediaPlayer2"),
                             QStringLiteral("org.mpris.MediaPlayer2.Player"),
                             QDBusConnection::sessionBus());
        iface.asyncCall(QStringLiteral("Seek"), offsetMicros);
    }

    m_trackPositionSecs = targetSecs;
    Q_EMIT mediaStateChanged();
    QTimer::singleShot(250, this, [this]() { pollPlayers(); });
}

void MediaEngine::skipBySeconds(int deltaSeconds)
{
    if (m_activeMprisService.isEmpty()) return;
    const qint64 offsetMicros = static_cast<qint64>(deltaSeconds) * 1000000LL;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Seek"), offsetMicros);
    QTimer::singleShot(250, this, [this]() { pollPlayers(); });
}

void MediaEngine::toggleShuffle()
{
    if (m_activeMprisService.isEmpty()) return;
    const bool nextState = !m_shuffleEnabled;
    QDBusMessage msg = QDBusMessage::createMethodCall(
        m_activeMprisService,
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Set"));
    msg << QStringLiteral("org.mpris.MediaPlayer2.Player")
        << QStringLiteral("Shuffle")
        << QVariant::fromValue(QDBusVariant(nextState));
    QDBusConnection::sessionBus().asyncCall(msg);
    m_shuffleEnabled = nextState;
    Q_EMIT mediaStateChanged();
}

void MediaEngine::cycleLoopStatus()
{
    if (m_activeMprisService.isEmpty()) return;
    QString nextLoop = QStringLiteral("None");
    if (m_loopStatus == QStringLiteral("None")) {
        nextLoop = QStringLiteral("Playlist");
    } else if (m_loopStatus == QStringLiteral("Playlist")) {
        nextLoop = QStringLiteral("Track");
    }
    QDBusMessage msg = QDBusMessage::createMethodCall(
        m_activeMprisService,
        QStringLiteral("/org/mpris/MediaPlayer2"),
        QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("Set"));
    msg << QStringLiteral("org.mpris.MediaPlayer2.Player")
        << QStringLiteral("LoopStatus")
        << QVariant::fromValue(QDBusVariant(nextLoop));
    QDBusConnection::sessionBus().asyncCall(msg);
    m_loopStatus = nextLoop;
    Q_EMIT mediaStateChanged();
}

void MediaEngine::raiseMediaPlayer()
{
    if (!m_activeMprisService.isEmpty()) {
        QDBusInterface iface(m_activeMprisService,
                             QStringLiteral("/org/mpris/MediaPlayer2"),
                             QStringLiteral("org.mpris.MediaPlayer2"),
                             QDBusConnection::sessionBus());
        iface.asyncCall(QStringLiteral("Raise"));
    }

    const QString target = m_playerName.trimmed().toLower();
    if (target.isEmpty()) return;

    const QString scriptPath = QDir::tempPath() + QStringLiteral("/kisland_kwin_raise.js");
    QFile f(scriptPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        const QString js = QStringLiteral(
            "const target = \"%1\";\n"
            "for (const w of workspace.windowList()) {\n"
            "    const rc = (w.resourceClass || \"\").toString().toLowerCase();\n"
            "    const rn = (w.resourceName || \"\").toString().toLowerCase();\n"
            "    const df = (w.desktopFileName || \"\").toString().toLowerCase();\n"
            "    const cap = (w.caption || \"\").toString().toLowerCase();\n"
            "    if (rc.includes(target) || rn.includes(target) || df.includes(target) || cap.includes(target)) {\n"
            "        if (w.minimized) w.minimized = false;\n"
            "        workspace.activeWindow = w;\n"
            "        break;\n"
            "    }\n"
            "}\n").arg(target);
        f.write(js.toUtf8());
        f.close();

        QDBusInterface kwinScripting(QStringLiteral("org.kde.KWin"),
                                     QStringLiteral("/Scripting"),
                                     QStringLiteral("org.kde.kwin.Scripting"),
                                     QDBusConnection::sessionBus());
        if (kwinScripting.isValid()) {
            const QString pluginName = QStringLiteral("kisland_raise_helper");
            kwinScripting.call(QStringLiteral("unloadScript"), pluginName);
            const QDBusReply<int> loadReply = kwinScripting.call(QStringLiteral("loadScript"), scriptPath, pluginName);
            if (loadReply.isValid() && loadReply.value() >= 0) {
                const QString scriptObjPath = QStringLiteral("/Scripting/Script%1").arg(loadReply.value());
                QDBusInterface scriptIface(QStringLiteral("org.kde.KWin"),
                                           scriptObjPath,
                                           QStringLiteral("org.kde.kwin.Script"),
                                           QDBusConnection::sessionBus());
                scriptIface.call(QStringLiteral("run"));
                kwinScripting.call(QStringLiteral("unloadScript"), pluginName);
            }
        }
    }
}
