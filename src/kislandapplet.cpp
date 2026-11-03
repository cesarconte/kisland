/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "kislandapplet.h"

#include "notificationdigest.h"
#include "songrecognizer.h"

#include <cmath>

#include <KPluginFactory>

#include <QCryptographicHash>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

K_PLUGIN_CLASS_WITH_JSON(KIslandApplet, "metadata.json")

KIslandApplet::KIslandApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_pollTimer(new QTimer(this))
    , m_countdownTimer(new QTimer(this))
{
    m_digest = new NotificationDigest(this);
    m_songRecognizer = new SongRecognizer(this);
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

    m_digest->start();

    pollMprisAndActivities();
}

bool KIslandApplet::mediaPlaying() const { return m_mediaPlaying; }
QString KIslandApplet::trackTitle() const { return m_trackTitle; }
QString KIslandApplet::trackArtist() const { return m_trackArtist; }
QString KIslandApplet::trackAlbum() const { return m_trackAlbum; }
QString KIslandApplet::trackArtUrl() const { return m_trackArtUrl; }
bool KIslandApplet::isVideoMedia() const { return m_isVideoMedia; }
QString KIslandApplet::playerName() const { return m_playerName; }
qint64 KIslandApplet::trackPositionSecs() const { return m_trackPositionSecs; }
qint64 KIslandApplet::trackDurationSecs() const { return m_trackDurationSecs; }
QString KIslandApplet::trackElapsedText() const { return formatCountdown(m_trackPositionSecs); }
QString KIslandApplet::trackRemainingText() const
{
    if (m_trackDurationSecs <= 0) {
        return QStringLiteral("--:--");
    }
    const qint64 rem = qMax<qint64>(0, m_trackDurationSecs - m_trackPositionSecs);
    return QStringLiteral("-") + formatCountdown(rem);
}
double KIslandApplet::trackProgress() const
{
    if (m_trackDurationSecs <= 0) {
        return 0.0;
    }
    return qBound(0.0, static_cast<double>(m_trackPositionSecs) / static_cast<double>(m_trackDurationSecs), 1.0);
}
bool KIslandApplet::shuffleEnabled() const { return m_shuffleEnabled; }
QString KIslandApplet::loopStatus() const { return m_loopStatus; }
QVariantList KIslandApplet::spectrumBars() const { return m_spectrumBars; }
bool KIslandApplet::timerActive() const { return m_timerActive; }
bool KIslandApplet::timerPaused() const { return m_timerPaused; }
double KIslandApplet::timerProgress() const
{
    if (!m_timerActive || m_timerTotalSecs <= 0) {
        return 0.0;
    }
    const qint64 rem = m_timerPaused
        ? m_timerPausedRemainingSecs
        : qMax<qint64>(0, QDateTime::currentDateTime().secsTo(m_timerTarget));
    return qBound(0.0, static_cast<double>(rem) / static_cast<double>(m_timerTotalSecs), 1.0);
}
QString KIslandApplet::timerRemainingText() const { return m_timerRemainingText; }
QString KIslandApplet::timerLabel() const { return m_timerLabel; }
QString KIslandApplet::kiriState() const { return m_kiriState; }
int KIslandApplet::cpuUsagePercent() const { return m_cpuUsagePercent; }
int KIslandApplet::ramUsagePercent() const { return m_ramUsagePercent; }
int KIslandApplet::outputVolumePercent() const { return m_outputVolumePercent; }
bool KIslandApplet::outputMuted() const { return m_outputMuted; }
QString KIslandApplet::outputDeviceName() const { return m_outputDeviceName; }
QObject *KIslandApplet::notificationDigest() const { return m_digest; }
QObject *KIslandApplet::songRecognizer() const { return m_songRecognizer; }
bool KIslandApplet::micActive() const { return m_micActive; }
QString KIslandApplet::micApp() const { return m_micApp; }
bool KIslandApplet::cameraOrScreenActive() const { return m_cameraOrScreenActive; }
QString KIslandApplet::cameraOrScreenApp() const { return m_cameraOrScreenApp; }
bool KIslandApplet::focusActive() const { return m_focusActive; }
QString KIslandApplet::focusModeName() const { return m_focusModeName; }
QString KIslandApplet::focusModeIcon() const { return m_focusModeIcon; }
bool KIslandApplet::focusDndActive() const { return m_focusDndActive; }

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

QVariantMap KIslandApplet::parseWpctlStatus(const QString &wpctlOutput, const QString &kiriState)
{
    QVariantMap res;
    int volumePercent = 40;
    bool muted = false;
    QString deviceName = QStringLiteral("PipeWire");
    bool micInUse = (kiriState == QStringLiteral("listening"));
    QString micAppName = micInUse ? QStringLiteral("Kiri") : QString();
    bool camInUse = false;
    QString camAppName;

    const QStringList lines = wpctlOutput.split(QLatin1Char('\n'));
    QString section;
    QString subSection;
    QString currentStreamClient;

    static const QRegularExpression sinkLineRe(QStringLiteral(R"(^\s*│?\s*\*\s*\d+\.\s+(.+?)\s+\[vol:\s*([0-9.]+)(\s+MUTED)?\])"));
    // Stream client headers have <= 9 leading spaces (port lines like "163. monitor_FL" have 12+ spaces)
    static const QRegularExpression streamHeaderRe(QStringLiteral(R"(^\s{1,9}\d+\.\s+([^<>]+?)\s*$)"));

    auto isPortName = [](const QString &name) {
        return name.startsWith(QStringLiteral("monitor_"))
            || name.startsWith(QStringLiteral("input_"))
            || name.startsWith(QStringLiteral("output_"))
            || name.startsWith(QStringLiteral("capture_"))
            || name.startsWith(QStringLiteral("playback_"));
    };

    for (const QString &rawLine : lines) {
        const QString trimmed = rawLine.trimmed();
        if (trimmed == QStringLiteral("Audio")) {
            section = QStringLiteral("Audio");
            subSection.clear();
            continue;
        } else if (trimmed == QStringLiteral("Video")) {
            section = QStringLiteral("Video");
            subSection.clear();
            continue;
        } else if (trimmed == QStringLiteral("Settings")) {
            section = QStringLiteral("Settings");
            subSection.clear();
            continue;
        }

        if (trimmed.contains(QStringLiteral("├─ Sinks:")) || trimmed.contains(QStringLiteral("└─ Sinks:"))) {
            subSection = QStringLiteral("Sinks");
            continue;
        } else if (trimmed.contains(QStringLiteral("├─ Sources:")) || trimmed.contains(QStringLiteral("└─ Sources:"))) {
            subSection = QStringLiteral("Sources");
            continue;
        } else if (trimmed.contains(QStringLiteral("├─ Filters:")) || trimmed.contains(QStringLiteral("└─ Filters:"))) {
            subSection = QStringLiteral("Filters");
            continue;
        } else if (trimmed.contains(QStringLiteral("├─ Streams:")) || trimmed.contains(QStringLiteral("└─ Streams:"))) {
            subSection = QStringLiteral("Streams");
            currentStreamClient.clear();
            continue;
        }

        if (section == QStringLiteral("Audio") && subSection == QStringLiteral("Sinks")) {
            const QRegularExpressionMatch m = sinkLineRe.match(rawLine);
            if (m.hasMatch()) {
                deviceName = m.captured(1).trimmed();
                const double vol = m.captured(2).toDouble();
                volumePercent = qBound(0, qRound(vol * 100.0), 150);
                muted = !m.captured(3).isEmpty();
            }
        } else if (section == QStringLiteral("Audio") && subSection == QStringLiteral("Streams")) {
            const QRegularExpressionMatch hm = streamHeaderRe.match(rawLine);
            if (hm.hasMatch() && !rawLine.contains(QLatin1Char('<')) && !rawLine.contains(QLatin1Char('>'))) {
                const QString candidate = hm.captured(1).trimmed();
                if (!isPortName(candidate)) {
                    currentStreamClient = candidate;
                }
            } else if (rawLine.contains(QLatin1Char('<')) && rawLine.contains(QStringLiteral("[active]"))
                       && rawLine.contains(QStringLiteral(":capture_"))) {
                if (!currentStreamClient.isEmpty()
                    && !isPortName(currentStreamClient)
                    && !(currentStreamClient == QStringLiteral("pw-record") && kiriState != QStringLiteral("listening"))) {
                    micInUse = true;
                    micAppName = (currentStreamClient == QStringLiteral("pw-record")) ? QStringLiteral("Kiri") : currentStreamClient;
                }
            }
        } else if (section == QStringLiteral("Video") && subSection == QStringLiteral("Streams")) {
            const QRegularExpressionMatch hm = streamHeaderRe.match(rawLine);
            if (hm.hasMatch() && !rawLine.contains(QLatin1Char('<')) && !rawLine.contains(QLatin1Char('>'))) {
                const QString candidate = hm.captured(1).trimmed();
                if (!isPortName(candidate)) {
                    currentStreamClient = candidate;
                }
            } else if (rawLine.contains(QStringLiteral("[active]"))) {
                camInUse = true;
                camAppName = currentStreamClient.isEmpty() ? QStringLiteral("Vídeo / Pantalla") : currentStreamClient;
            }
        }
    }

    res[QStringLiteral("volumePercent")] = volumePercent;
    res[QStringLiteral("muted")] = muted;
    res[QStringLiteral("deviceName")] = deviceName;
    res[QStringLiteral("micActive")] = micInUse;
    res[QStringLiteral("micApp")] = micAppName;
    res[QStringLiteral("cameraOrScreenActive")] = camInUse;
    res[QStringLiteral("cameraOrScreenApp")] = camAppName;
    return res;
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

    updatePipeWireAudioAndPrivacy();
    updateBluetoothAndPeripherals();
    updateFocusMode();

    // 2. Discover MPRIS2 Media Players (D-Bus Properties.Get + playerctl fallback)
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

void KIslandApplet::seekToRatio(double ratio)
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
    QTimer::singleShot(250, this, &KIslandApplet::pollMprisAndActivities);
}

void KIslandApplet::skipBySeconds(int deltaSeconds)
{
    if (m_activeMprisService.isEmpty()) return;
    const qint64 offsetMicros = static_cast<qint64>(deltaSeconds) * 1000000LL;
    QDBusInterface iface(m_activeMprisService,
                         QStringLiteral("/org/mpris/MediaPlayer2"),
                         QStringLiteral("org.mpris.MediaPlayer2.Player"),
                         QDBusConnection::sessionBus());
    iface.asyncCall(QStringLiteral("Seek"), offsetMicros);
    QTimer::singleShot(250, this, &KIslandApplet::pollMprisAndActivities);
}

void KIslandApplet::toggleShuffle()
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

void KIslandApplet::cycleLoopStatus()
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

void KIslandApplet::raiseMediaPlayer()
{
    if (!m_activeMprisService.isEmpty()) {
        QDBusInterface iface(m_activeMprisService,
                             QStringLiteral("/org/mpris/MediaPlayer2"),
                             QStringLiteral("org.mpris.MediaPlayer2"),
                             QDBusConnection::sessionBus());
        iface.asyncCall(QStringLiteral("Raise"));
    }

    // Many players (including mpv-mpris on Wayland) report CanRaise=false.
    // Use KWin Wayland's native Scripting D-Bus API to unminimize & activate the player window.
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

void KIslandApplet::startQuickTimer(int durationSeconds, const QString &label)
{
    if (durationSeconds <= 0) return;
    m_timerTotalSecs = durationSeconds;
    m_timerPausedRemainingSecs = durationSeconds;
    m_timerPaused = false;
    m_timerTarget = QDateTime::currentDateTime().addSecs(durationSeconds);
    m_timerLabel = label;
    m_timerActive = true;
    m_timerRemainingText = formatCountdown(durationSeconds);
    m_countdownTimer->start(1000);
    Q_EMIT timerChanged();
}

void KIslandApplet::togglePauseTimer()
{
    if (!m_timerActive) return;
    if (!m_timerPaused) {
        m_timerPausedRemainingSecs = qMax<qint64>(1, QDateTime::currentDateTime().secsTo(m_timerTarget));
        m_timerPaused = true;
        m_countdownTimer->stop();
    } else {
        m_timerTarget = QDateTime::currentDateTime().addSecs(m_timerPausedRemainingSecs);
        m_timerPaused = false;
        m_countdownTimer->start(1000);
    }
    Q_EMIT timerChanged();
}

void KIslandApplet::cancelQuickTimer()
{
    m_countdownTimer->stop();
    m_timerActive = false;
    m_timerPaused = false;
    m_timerTotalSecs = 0;
    m_timerPausedRemainingSecs = 0;
    m_timerRemainingText.clear();
    Q_EMIT timerChanged();
}

void KIslandApplet::onTimerTick()
{
    if (!m_timerActive || m_timerPaused) return;
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

void KIslandApplet::setOutputVolume(int percent)
{
    const int clamped = qBound(0, percent, 150);
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (!wpctl.isEmpty()) {
        const QString ratioStr = QString::number(clamped / 100.0, 'f', 2);
        QProcess::startDetached(wpctl, {QStringLiteral("set-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), ratioStr});
    }
    if (m_outputVolumePercent != clamped) {
        m_outputVolumePercent = clamped;
        Q_EMIT audioChanged();
        Q_EMIT volumeAlert(m_outputVolumePercent, m_outputMuted);
    }
}

void KIslandApplet::toggleOutputMute()
{
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (!wpctl.isEmpty()) {
        QProcess::startDetached(wpctl, {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("toggle")});
    }
    m_outputMuted = !m_outputMuted;
    Q_EMIT audioChanged();
    Q_EMIT volumeAlert(m_outputVolumePercent, m_outputMuted);
}

void KIslandApplet::launchDropover()
{
    const QString bin = QStandardPaths::findExecutable(QStringLiteral("kdropover"),
                                                       {QDir::homePath() + QStringLiteral("/.local/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/bin")});
    if (!bin.isEmpty()) {
        QProcess::startDetached(bin, {});
    }
}

void KIslandApplet::launchLiveCaptions()
{
    const QString bin = QStandardPaths::findExecutable(QStringLiteral("klivecaptions"),
                                                       {QDir::homePath() + QStringLiteral("/.local/bin"), QStringLiteral("/usr/local/bin"), QStringLiteral("/usr/bin")});
    if (!bin.isEmpty()) {
        QProcess::startDetached(bin, {});
    }
}

void KIslandApplet::updatePipeWireAudioAndPrivacy()
{
    const QString wpctl = QStandardPaths::findExecutable(QStringLiteral("wpctl"));
    if (wpctl.isEmpty()) return;

    QProcess proc;
    proc.start(wpctl, {QStringLiteral("status")});
    if (!proc.waitForFinished(450) || proc.exitCode() != 0) {
        return;
    }

    const QVariantMap parsed = parseWpctlStatus(QString::fromUtf8(proc.readAllStandardOutput()), m_kiriState);
    const int newVol = parsed.value(QStringLiteral("volumePercent"), m_outputVolumePercent).toInt();
    const bool newMuted = parsed.value(QStringLiteral("muted"), m_outputMuted).toBool();
    const QString newDev = parsed.value(QStringLiteral("deviceName"), m_outputDeviceName).toString();

    if (m_outputVolumePercent != newVol || m_outputMuted != newMuted || m_outputDeviceName != newDev) {
        const bool volChanged = (m_outputVolumePercent != newVol || m_outputMuted != newMuted);
        m_outputVolumePercent = newVol;
        m_outputMuted = newMuted;
        m_outputDeviceName = newDev;
        Q_EMIT audioChanged();
        if (m_volumeInitialized && volChanged) {
            Q_EMIT volumeAlert(m_outputVolumePercent, m_outputMuted);
        }
    }
    m_volumeInitialized = true;

    const bool newMic = parsed.value(QStringLiteral("micActive")).toBool();
    const QString newMicApp = parsed.value(QStringLiteral("micApp")).toString();
    bool newCam = parsed.value(QStringLiteral("cameraOrScreenActive")).toBool();
    QString newCamApp = parsed.value(QStringLiteral("cameraOrScreenApp")).toString();

    // Also detect direct V4L2 webcam usage (/dev/video*) in /proc/<pid>/fd for apps not routing video via PipeWire
    if (!newCam) {
        const QDir procDir(QStringLiteral("/proc"));
        const QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &pidStr : pids) {
            bool ok = false;
            pidStr.toInt(&ok);
            if (!ok) continue;

            const QDir fdDir(QStringLiteral("/proc/%1/fd").arg(pidStr));
            const QFileInfoList fds = fdDir.entryInfoList(QDir::System | QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo &fdInfo : fds) {
                const QString target = fdInfo.symLinkTarget();
                if (target.startsWith(QStringLiteral("/dev/video"))) {
                    QFile commFile(QStringLiteral("/proc/%1/comm").arg(pidStr));
                    QString comm;
                    if (commFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        comm = QString::fromUtf8(commFile.readLine()).trimmed();
                    }
                    if (!comm.isEmpty()
                        && comm != QStringLiteral("pipewire")
                        && comm != QStringLiteral("wireplumber")
                        && comm != QStringLiteral("xdg-desktop-por")) {
                        newCam = true;
                        newCamApp = comm;
                        break;
                    }
                }
            }
            if (newCam) break;
        }
    }

    if (m_micActive != newMic || m_micApp != newMicApp
        || m_cameraOrScreenActive != newCam || m_cameraOrScreenApp != newCamApp) {
        m_micActive = newMic;
        m_micApp = newMicApp;
        m_cameraOrScreenActive = newCam;
        m_cameraOrScreenApp = newCamApp;
        Q_EMIT privacyChanged();
    }
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

QVariantList KIslandApplet::getKdeConnectDevices() const
{
    QVariantList list;

    // First try D-Bus interface of KDE Connect daemon
    QDBusInterface iface(QStringLiteral("org.kde.kdeconnect"),
                         QStringLiteral("/modules/kdeconnect"),
                         QStringLiteral("org.kde.kdeconnect.daemon"),
                         QDBusConnection::sessionBus());
    if (iface.isValid()) {
        const QDBusReply<QMap<QString, QString>> reply = iface.call(QStringLiteral("deviceNames"), true, true);
        if (reply.isValid()) {
            const auto map = reply.value();
            for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
                QVariantMap d;
                d[QStringLiteral("id")] = it.key();
                d[QStringLiteral("name")] = it.value();
                list.append(d);
            }
            if (!list.isEmpty()) {
                return list;
            }
        }
    }

    // Fallback: query kdeconnect-cli -a --id-name-only
    const QString kdecli = QStandardPaths::findExecutable(QStringLiteral("kdeconnect-cli"));
    if (!kdecli.isEmpty()) {
        QProcess proc;
        proc.start(kdecli, {QStringLiteral("-a"), QStringLiteral("--id-name-only")});
        if (proc.waitForFinished(1000) && proc.exitCode() == 0) {
            const QString out = QString::fromUtf8(proc.readAllStandardOutput());
            const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QString trimmed = line.trimmed();
                const int spaceIdx = trimmed.indexOf(QLatin1Char(' '));
                if (spaceIdx > 0) {
                    QVariantMap d;
                    d[QStringLiteral("id")] = trimmed.left(spaceIdx);
                    d[QStringLiteral("name")] = trimmed.mid(spaceIdx + 1).trimmed();
                    list.append(d);
                }
            }
        }
    }

    return list;
}

bool KIslandApplet::shareToKdeConnect(const QString &filePath, const QString &deviceId)
{
    QString cleanPath = filePath;
    if (cleanPath.startsWith(QStringLiteral("file://"))) {
        cleanPath = QUrl(cleanPath).toLocalFile();
    }
    if (cleanPath.isEmpty() || !QFileInfo::exists(cleanPath)) {
        return false;
    }

    const QString kdecli = QStandardPaths::findExecutable(QStringLiteral("kdeconnect-cli"));
    if (kdecli.isEmpty()) {
        return false;
    }

    QStringList args;
    args << QStringLiteral("--share") << cleanPath;
    if (!deviceId.isEmpty()) {
        args << QStringLiteral("-d") << deviceId;
    }

    return QProcess::startDetached(kdecli, args);
}

void KIslandApplet::toggleFocusDnd()
{
    QDBusInterface focusIface(QStringLiteral("org.cesar.kfocus"),
                              QStringLiteral("/KFocus"),
                              QStringLiteral("org.cesar.kfocus"),
                              QDBusConnection::sessionBus());
    if (focusIface.isValid()) {
        focusIface.asyncCall(QStringLiteral("toggleDnd"));
        QTimer::singleShot(200, this, &KIslandApplet::updateFocusMode);
        return;
    }

    QDBusInterface notifIface(QStringLiteral("org.freedesktop.Notifications"),
                              QStringLiteral("/org/freedesktop/Notifications"),
                              QStringLiteral("org.freedesktop.Notifications"),
                              QDBusConnection::sessionBus());
    if (notifIface.isValid()) {
        const bool isInhibited = notifIface.property("Inhibited").toBool();
        if (isInhibited) {
            notifIface.call(QStringLiteral("UnInhibit"), 1u);
        } else {
            notifIface.call(QStringLiteral("Inhibit"), QStringLiteral("org.cesar.kisland"), QStringLiteral("Modo no molestar"), QVariantMap());
        }
    }

    QTimer::singleShot(200, this, &KIslandApplet::updateFocusMode);
}

void KIslandApplet::updateBluetoothAndPeripherals()
{
    const QString btctl = QStandardPaths::findExecutable(QStringLiteral("bluetoothctl"));
    if (btctl.isEmpty()) return;

    QProcess proc;
    proc.start(btctl, {QStringLiteral("devices"), QStringLiteral("Connected")});
    if (!proc.waitForFinished(800) || proc.exitCode() != 0) {
        return;
    }

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QSet<QString> currentDevices;

    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.startsWith(QStringLiteral("Device "))) continue;
        const QStringList parts = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 3) continue;

        const QString mac = parts.at(1);
        const QString name = parts.mid(2).join(QLatin1Char(' '));
        currentDevices.insert(mac);

        if (m_btInitialized && !m_knownBtDevices.contains(mac)) {
            int batteryPercent = -1;
            QString iconName = QStringLiteral("audio-headphones");
            const QString lowerName = name.toLower();
            if (lowerName.contains(QStringLiteral("speaker")) || lowerName.contains(QStringLiteral("xb13")) || lowerName.contains(QStringLiteral("altavoz"))) {
                iconName = QStringLiteral("audio-speakers");
            } else if (lowerName.contains(QStringLiteral("mouse")) || lowerName.contains(QStringLiteral("ratón"))) {
                iconName = QStringLiteral("input-mouse");
            } else if (lowerName.contains(QStringLiteral("keyboard")) || lowerName.contains(QStringLiteral("teclado"))) {
                iconName = QStringLiteral("input-keyboard");
            }

            QProcess infoProc;
            infoProc.start(btctl, {QStringLiteral("info"), mac});
            if (infoProc.waitForFinished(600) && infoProc.exitCode() == 0) {
                const QString infoOut = QString::fromUtf8(infoProc.readAllStandardOutput());
                static const QRegularExpression rxBattery(QStringLiteral(R"(Battery Percentage:\s*(?:0x[0-9a-fA-F]+\s*)?\(?(\d+)\)?)"));
                const auto match = rxBattery.match(infoOut);
                if (match.hasMatch()) {
                    batteryPercent = match.captured(1).toInt();
                }
            }

            QString subtitle = QStringLiteral("Conectado");
            if (batteryPercent >= 0) {
                subtitle = QStringLiteral("Conectado • %1%").arg(batteryPercent);
            }
            Q_EMIT peripheralAlert(name, subtitle, iconName);
        }
    }

    m_knownBtDevices = currentDevices;
    m_btInitialized = true;
}

void KIslandApplet::updateFocusMode()
{
    bool newActive = false;
    bool newDnd = false;
    QString newModeName;
    QString newModeIcon;
    QString newModeId;

    const QString configPath = QDir::homePath() + QStringLiteral("/.config/kfocus/active_mode.json");
    QFile f(configPath);
    if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            newActive = obj.value(QStringLiteral("active")).toBool();
            newDnd = obj.value(QStringLiteral("dndActive")).toBool();
            newModeName = obj.value(QStringLiteral("modeName")).toString();
            newModeIcon = obj.value(QStringLiteral("modeIcon")).toString();
            newModeId = obj.value(QStringLiteral("modeId")).toString();
        }
    } else {
        QDBusInterface notifIface(QStringLiteral("org.freedesktop.Notifications"),
                                  QStringLiteral("/org/freedesktop/Notifications"),
                                  QStringLiteral("org.freedesktop.Notifications"),
                                  QDBusConnection::sessionBus());
        if (notifIface.isValid()) {
            newDnd = notifIface.property("Inhibited").toBool();
            newActive = newDnd;
            if (newDnd) {
                newModeName = QStringLiteral("No molestar");
                newModeIcon = QStringLiteral("notifications-disabled");
            }
        }
    }

    if (m_focusActive != newActive || m_focusDndActive != newDnd || m_focusModeName != newModeName || m_focusModeIcon != newModeIcon) {
        const bool stateChanged = (m_focusActive != newActive || m_focusDndActive != newDnd);
        m_focusActive = newActive;
        m_focusDndActive = newDnd;
        m_focusModeName = newModeName;
        m_focusModeIcon = newModeIcon;
        m_focusModeId = newModeId;
        Q_EMIT focusStateChanged();

        if (m_focusInitialized && stateChanged) {
            QString alertTitle;
            QString alertIcon = m_focusModeIcon.isEmpty() ? QStringLiteral("notifications-disabled") : m_focusModeIcon;
            if (m_focusActive) {
                alertTitle = m_focusModeName.isEmpty() ? QStringLiteral("Concentración activada") : QStringLiteral("Modo %1").arg(m_focusModeName);
            } else if (m_focusDndActive) {
                alertTitle = QStringLiteral("No molestar: Activado");
            } else {
                alertTitle = QStringLiteral("No molestar: Desactivado");
                alertIcon = QStringLiteral("preferences-desktop-notification-bell");
            }
            Q_EMIT focusAlert(alertTitle, alertIcon);
        }
    }
    m_focusInitialized = true;
}

#include "kislandapplet.moc"
