/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "kislandapplet.h"

#include <cmath>

#include <KPluginFactory>

#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

K_PLUGIN_CLASS_WITH_JSON(KIslandApplet, "metadata.json")

KIslandApplet::KIslandApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_pollTimer(new QTimer(this))
    , m_countdownTimer(new QTimer(this))
{
}

KIslandApplet::~KIslandApplet()
{
    stopPipeWireSpectrumMonitor();
}

void KIslandApplet::init()
{
    Plasma::Applet::init();

    connect(m_pollTimer, &QTimer::timeout, this, &KIslandApplet::pollMprisAndActivities);
    m_pollTimer->start(1800);

    connect(m_countdownTimer, &QTimer::timeout, this, &KIslandApplet::onTimerTick);

    pollMprisAndActivities();
}

bool KIslandApplet::mediaPlaying() const { return m_mediaPlaying; }
QString KIslandApplet::trackTitle() const { return m_trackTitle; }
QString KIslandApplet::trackArtist() const { return m_trackArtist; }
QString KIslandApplet::trackAlbum() const { return m_trackAlbum; }
QString KIslandApplet::trackArtUrl() const { return m_trackArtUrl; }
bool KIslandApplet::isVideoMedia() const { return m_isVideoMedia; }
QString KIslandApplet::playerName() const { return m_playerName; }
QVariantList KIslandApplet::spectrumBars() const { return m_spectrumBars; }
bool KIslandApplet::timerActive() const { return m_timerActive; }
QString KIslandApplet::timerRemainingText() const { return m_timerRemainingText; }
QString KIslandApplet::timerLabel() const { return m_timerLabel; }
QString KIslandApplet::kiriState() const { return m_kiriState; }
int KIslandApplet::cpuUsagePercent() const { return m_cpuUsagePercent; }
int KIslandApplet::ramUsagePercent() const { return m_ramUsagePercent; }

QVariantList KIslandApplet::computeSpectrumBarsFromPcm(const QByteArray &pcmChunk, int barCount)
{
    QVariantList result;
    if (barCount <= 0) return result;

    const int totalSamples = pcmChunk.size() / static_cast<int>(sizeof(qint16));
    if (totalSamples < barCount) {
        for (int i = 0; i < barCount; ++i) result.append(0.15);
        return result;
    }

    const qint16 *samples = reinterpret_cast<const qint16 *>(pcmChunk.constData());
    const int samplesPerBar = totalSamples / barCount;

    for (int b = 0; b < barCount; ++b) {
        double sumSq = 0.0;
        const int offset = b * samplesPerBar;
        for (int i = 0; i < samplesPerBar; ++i) {
            const double norm = samples[offset + i] / 32768.0;
            sumSq += norm * norm;
        }
        const double rms = std::sqrt(sumSq / samplesPerBar);
        const double barHeight = qBound(0.15, rms * 5.2, 1.0);
        result.append(barHeight);
    }
    return result;
}

QString KIslandApplet::formatCountdown(qint64 remainingSeconds)
{
    const qint64 clamped = qMax<qint64>(0, remainingSeconds);
    const qint64 mins = clamped / 60;
    const qint64 secs = clamped % 60;
    return QStringLiteral("%1:%2")
        .arg(mins, 2, 10, QLatin1Char('0'))
        .arg(secs, 2, 10, QLatin1Char('0'));
}

QVariantMap KIslandApplet::extractMprisMetadata(const QVariant &metadataVariant)
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

QVariantMap KIslandApplet::parsePlayerctlLine(const QString &line)
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

QString KIslandApplet::resolveCoverArtOrVideoThumbnail(const QString &artUrl, const QString &mediaUrl, bool *isVideoOut)
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

    // 1. Direct mpris:artUrl if present and valid
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

    // 2. Check standard folder cover / poster images next to the audio or video file
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

    // 3. If it is a video file (like .mkv / .mp4 in mpv), extract a preview frame via ffmpeg (cached in /tmp)
    if (isVid && !ffmpeg.isEmpty()) {
        const QString thumbPath = QDir::tempPath() + QStringLiteral("/kisland_vidthumb_%1.jpg").arg(QString::fromLatin1(pathHash));
        if (!QFileInfo::exists(thumbPath)) {
            QProcess proc;
            // Fast input seek (-ss 15) before -i takes <80ms on local NVMe/HDD
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

    // 4. If it is an audio file with embedded ID3/FLAC cover art, extract via ffmpeg (cached in /tmp)
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

void KIslandApplet::pollMprisAndActivities()
{
    updateSystemVitals();

    // 1. Check Kiri Assistant state via D-Bus
    QDBusInterface kiriIface(QStringLiteral("org.cesar.kiri"),
                             QStringLiteral("/Kiri"),
                             QStringLiteral("org.freedesktop.DBus.Properties"),
                             QDBusConnection::sessionBus());
    QString detectedKiri = QStringLiteral("idle");
    if (kiriIface.isValid()) {
        const QVariant stProp = kiriIface.property("state");
        if (stProp.isValid() && !stProp.toString().isEmpty()) {
            detectedKiri = stProp.toString();
        }
    }
    if (m_kiriState != detectedKiri) {
        m_kiriState = detectedKiri;
        Q_EMIT kiriStateChanged();
    }

    // 2. Discover MPRIS2 Media Players (D-Bus Properties.Get + playerctl fallback)
    bool foundPlaying = false;
    QString newTitle;
    QString newArtist;
    QString newAlbum;
    QString newRawArtUrl;
    QString newMediaUrl;
    QString newPlayer;
    QString chosenService;

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
                if (foundPlaying) {
                    break;
                }
            }
        }
    }

    // Fallback to playerctl if D-Bus property map returned empty title
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
                          || m_trackArtUrl != resolvedArt || m_isVideoMedia != detectedIsVideo);
    m_mediaPlaying = foundPlaying;
    m_trackTitle = newTitle;
    m_trackArtist = newArtist;
    m_trackAlbum = newAlbum;
    m_trackArtUrl = resolvedArt;
    m_isVideoMedia = detectedIsVideo;
    m_playerName = newPlayer;
    m_activeMprisService = chosenService;

    if (m_mediaPlaying) {
        startPipeWireSpectrumMonitor();
    } else {
        stopPipeWireSpectrumMonitor();
    }

    if (changed) {
        Q_EMIT mediaStateChanged();
    }
}

void KIslandApplet::startPipeWireSpectrumMonitor()
{
    if (m_pwMonitorProc && m_pwMonitorProc->state() != QProcess::NotRunning) {
        return;
    }

    const QString pwRecord = QStandardPaths::findExecutable(QStringLiteral("pw-record"));
    if (pwRecord.isEmpty()) return;

    m_pwMonitorProc = new QProcess(this);
    connect(m_pwMonitorProc, &QProcess::readyReadStandardOutput, this, &KIslandApplet::onPipeWireAudioChunk);

    // Capture PipeWire default output sink monitor (respecting EasyEffects DSP chain)
    QStringList args;
    args << QStringLiteral("--raw")
         << QStringLiteral("--format=s16")
         << QStringLiteral("--rate=8000")
         << QStringLiteral("--channels=1")
         << QStringLiteral("-P") << QStringLiteral("stream.capture.sink=true")
         << QStringLiteral("-");

    m_pwMonitorProc->start(pwRecord, args);
}

void KIslandApplet::stopPipeWireSpectrumMonitor()
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
    m_spectrumBars = {0.15, 0.25, 0.20, 0.30, 0.18, 0.15};
    Q_EMIT spectrumChanged();
}

void KIslandApplet::onPipeWireAudioChunk()
{
    if (!m_pwMonitorProc) return;
    const QByteArray chunk = m_pwMonitorProc->readAllStandardOutput();
    if (chunk.size() >= 256) {
        m_spectrumBars = computeSpectrumBarsFromPcm(chunk.right(512), 6);
        Q_EMIT spectrumChanged();
    }
}

void KIslandApplet::playPauseMedia()
{
    if (m_activeMprisService.isEmpty()) return;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("PlayPause"));
    QTimer::singleShot(250, this, &KIslandApplet::pollMprisAndActivities);
}

void KIslandApplet::nextTrack()
{
    if (m_activeMprisService.isEmpty()) return;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Next"));
    QTimer::singleShot(350, this, &KIslandApplet::pollMprisAndActivities);
}

void KIslandApplet::previousTrack()
{
    if (m_activeMprisService.isEmpty()) return;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Previous"));
    QTimer::singleShot(350, this, &KIslandApplet::pollMprisAndActivities);
}

void KIslandApplet::startQuickTimer(int durationSeconds, const QString &label)
{
    if (durationSeconds <= 0) return;
    m_timerTarget = QDateTime::currentDateTime().addSecs(durationSeconds);
    m_timerLabel = label;
    m_timerActive = true;
    m_timerRemainingText = formatCountdown(durationSeconds);
    m_countdownTimer->start(1000);
    Q_EMIT timerChanged();
}

void KIslandApplet::cancelQuickTimer()
{
    m_countdownTimer->stop();
    m_timerActive = false;
    m_timerRemainingText.clear();
    Q_EMIT timerChanged();
}

void KIslandApplet::onTimerTick()
{
    if (!m_timerActive) return;
    const qint64 rem = QDateTime::currentDateTime().secsTo(m_timerTarget);
    if (rem <= 0) {
        cancelQuickTimer();
        return;
    }
    m_timerRemainingText = formatCountdown(rem);
    Q_EMIT timerChanged();
}

void KIslandApplet::invokeKiriVoice()
{
    QDBusInterface iface(QStringLiteral("org.cesar.kiri"),
                         QStringLiteral("/Kiri"),
                         QString(),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("toggleRecording"));
}

void KIslandApplet::updateSystemVitals()
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

#include "kislandapplet.moc"
