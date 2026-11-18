/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#include "kislandapplet.h"

#include "audioprivacymanager.h"
#include "mediaengine.h"
#include "songrecognizer.h"
#include "systemintegrationservice.h"
#include "systemvitalsmonitor.h"
#include "timermanager.h"

#include <KPluginFactory>
#include <QTimer>

KIslandApplet::KIslandApplet(QObject *parent, const KPluginMetaData &data, const QVariantList &args)
    : Plasma::Applet(parent, data, args)
    , m_mediaEngine(new MediaEngine(this))
    , m_timerManager(new TimerManager(this))
    , m_audioPrivacyManager(new AudioPrivacyManager(this))
    , m_vitalsMonitor(new SystemVitalsMonitor(this))
    , m_systemService(new SystemIntegrationService(this))
    , m_songRecognizer(new SongRecognizer(this))
    , m_pollTimer(new QTimer(this))
{
    // Wire MediaEngine signals
    connect(m_mediaEngine, &MediaEngine::mediaStateChanged, this, &KIslandApplet::mediaStateChanged);
    connect(m_mediaEngine, &MediaEngine::spectrumChanged, this, &KIslandApplet::spectrumChanged);

    // Wire TimerManager signals
    connect(m_timerManager, &TimerManager::timerChanged, this, &KIslandApplet::timerChanged);
    connect(m_timerManager, &TimerManager::timerFinished, this, &KIslandApplet::timerFinished);

    // Wire AudioPrivacyManager signals
    connect(m_audioPrivacyManager, &AudioPrivacyManager::audioChanged, this, &KIslandApplet::audioChanged);
    connect(m_audioPrivacyManager, &AudioPrivacyManager::privacyChanged, this, &KIslandApplet::privacyChanged);
    connect(m_audioPrivacyManager, &AudioPrivacyManager::volumeAlert, this, &KIslandApplet::volumeAlert);

    // Wire SystemVitalsMonitor signals
    connect(m_vitalsMonitor, &SystemVitalsMonitor::vitalsChanged, this, &KIslandApplet::vitalsChanged);

    // Wire SystemIntegrationService signals
    connect(m_systemService, &SystemIntegrationService::kityStateChanged, this, &KIslandApplet::kityStateChanged);
    connect(m_systemService, &SystemIntegrationService::kiriStateChanged, this, &KIslandApplet::kiriStateChanged);
    connect(m_systemService, &SystemIntegrationService::focusStateChanged, this, &KIslandApplet::focusStateChanged);
    connect(m_systemService, &SystemIntegrationService::peripheralAlert, this, &KIslandApplet::peripheralAlert);
    connect(m_systemService, &SystemIntegrationService::focusAlert, this, &KIslandApplet::focusAlert);
}

KIslandApplet::~KIslandApplet() = default;

void KIslandApplet::init()
{
    Plasma::Applet::init();

    connect(m_pollTimer, &QTimer::timeout, this, &KIslandApplet::pollActivities);
    m_pollTimer->start(1800);

    if (m_songRecognizer) {
        connect(m_songRecognizer, &SongRecognizer::stateChanged, this, [this]() {
            const bool shouldMonitor = m_mediaEngine->mediaPlaying() || (m_songRecognizer && m_songRecognizer->listening());
            if (shouldMonitor) {
                m_mediaEngine->startPipeWireSpectrumMonitor();
            } else {
                m_mediaEngine->stopPipeWireSpectrumMonitor();
            }
        });
    }

    pollActivities();
}

void KIslandApplet::pollActivities()
{
    m_vitalsMonitor->updateVitals();
    m_systemService->updateKityState();
    m_audioPrivacyManager->updatePipeWireAudioAndPrivacy(m_systemService->kityState());
    m_systemService->updateBluetoothAndPeripherals();
    m_systemService->updateFocusMode();

    const bool recognizerListening = m_songRecognizer && m_songRecognizer->listening();
    m_mediaEngine->pollPlayers(recognizerListening);
}

// Media Engine property getters
bool KIslandApplet::mediaPlaying() const { return m_mediaEngine->mediaPlaying(); }
QString KIslandApplet::trackTitle() const { return m_mediaEngine->trackTitle(); }
QString KIslandApplet::trackArtist() const { return m_mediaEngine->trackArtist(); }
QString KIslandApplet::trackAlbum() const { return m_mediaEngine->trackAlbum(); }
QString KIslandApplet::trackArtUrl() const { return m_mediaEngine->trackArtUrl(); }
bool KIslandApplet::isVideoMedia() const { return m_mediaEngine->isVideoMedia(); }
QString KIslandApplet::playerName() const { return m_mediaEngine->playerName(); }
qint64 KIslandApplet::trackPositionSecs() const { return m_mediaEngine->trackPositionSecs(); }
qint64 KIslandApplet::trackDurationSecs() const { return m_mediaEngine->trackDurationSecs(); }
QString KIslandApplet::trackElapsedText() const { return m_mediaEngine->trackElapsedText(); }
QString KIslandApplet::trackRemainingText() const { return m_mediaEngine->trackRemainingText(); }
double KIslandApplet::trackProgress() const { return m_mediaEngine->trackProgress(); }
bool KIslandApplet::shuffleEnabled() const { return m_mediaEngine->shuffleEnabled(); }
QString KIslandApplet::loopStatus() const { return m_mediaEngine->loopStatus(); }
QVariantList KIslandApplet::spectrumBars() const { return m_mediaEngine->spectrumBars(); }

// Timer Manager property getters
bool KIslandApplet::timerActive() const { return m_timerManager->isActive(); }
bool KIslandApplet::timerPaused() const { return m_timerManager->isPaused(); }
double KIslandApplet::timerProgress() const { return m_timerManager->progress(); }
QString KIslandApplet::timerRemainingText() const { return m_timerManager->remainingText(); }
QString KIslandApplet::timerLabel() const { return m_timerManager->label(); }

// System & Vitals property getters
QString KIslandApplet::kiriState() const { return m_systemService->kityState(); }
QString KIslandApplet::kityState() const { return m_systemService->kityState(); }
int KIslandApplet::cpuUsagePercent() const { return m_vitalsMonitor->cpuUsagePercent(); }
int KIslandApplet::ramUsagePercent() const { return m_vitalsMonitor->ramUsagePercent(); }

// Audio & Privacy property getters
int KIslandApplet::outputVolumePercent() const { return m_audioPrivacyManager->outputVolumePercent(); }
bool KIslandApplet::outputMuted() const { return m_audioPrivacyManager->outputMuted(); }
QString KIslandApplet::outputDeviceName() const { return m_audioPrivacyManager->outputDeviceName(); }
QObject *KIslandApplet::songRecognizer() const { return m_songRecognizer; }
bool KIslandApplet::micActive() const { return m_audioPrivacyManager->micActive(); }
QString KIslandApplet::micApp() const { return m_audioPrivacyManager->micApp(); }
bool KIslandApplet::cameraOrScreenActive() const { return m_audioPrivacyManager->cameraOrScreenActive(); }
QString KIslandApplet::cameraOrScreenApp() const { return m_audioPrivacyManager->cameraOrScreenApp(); }

// Focus property getters
bool KIslandApplet::focusActive() const { return m_systemService->focusActive(); }
QString KIslandApplet::focusModeName() const { return m_systemService->focusModeName(); }
QString KIslandApplet::focusModeIcon() const { return m_systemService->focusModeIcon(); }
bool KIslandApplet::focusDndActive() const { return m_systemService->focusDndActive(); }

// Media actions
void KIslandApplet::playPauseMedia() { m_mediaEngine->playPauseMedia(); }
void KIslandApplet::nextTrack() { m_mediaEngine->nextTrack(); }
void KIslandApplet::previousTrack() { m_mediaEngine->previousTrack(); }
void KIslandApplet::seekToRatio(double ratio) { m_mediaEngine->seekToRatio(ratio); }
void KIslandApplet::skipBySeconds(int deltaSeconds) { m_mediaEngine->skipBySeconds(deltaSeconds); }
void KIslandApplet::toggleShuffle() { m_mediaEngine->toggleShuffle(); }
void KIslandApplet::cycleLoopStatus() { m_mediaEngine->cycleLoopStatus(); }
void KIslandApplet::raiseMediaPlayer() { m_mediaEngine->raiseMediaPlayer(); }

// Timer actions
void KIslandApplet::startQuickTimer(int durationSeconds, const QString &label)
{
    m_timerManager->startQuickTimer(durationSeconds, label);
}
void KIslandApplet::togglePauseTimer() { m_timerManager->togglePauseTimer(); }
void KIslandApplet::cancelQuickTimer() { m_timerManager->cancelQuickTimer(); }

// System & Integration actions
void KIslandApplet::invokeKity() { m_systemService->invokeKity(); }
void KIslandApplet::invokeKiriVoice() { m_systemService->invokeKiriVoice(); }
void KIslandApplet::launchDropover() { m_systemService->launchDropover(); }
void KIslandApplet::launchLiveCaptions() { m_systemService->launchLiveCaptions(); }
QVariantList KIslandApplet::getKdeConnectDevices() const { return m_systemService->getKdeConnectDevices(); }
bool KIslandApplet::shareToKdeConnect(const QString &filePath, const QString &deviceId)
{
    return m_systemService->shareToKdeConnect(filePath, deviceId);
}
void KIslandApplet::openKdeConnectSettings() { m_systemService->openKdeConnectSettings(); }
void KIslandApplet::openFileLocation(const QString &filePath) { m_systemService->openFileLocation(filePath); }
void KIslandApplet::toggleFocusDnd() { m_systemService->toggleFocusDnd(); }

// Audio actions
void KIslandApplet::setOutputVolume(int percent) { m_audioPrivacyManager->setOutputVolume(percent); }
void KIslandApplet::toggleOutputMute() { m_audioPrivacyManager->toggleOutputMute(); }

// Static test helpers
QVariantList KIslandApplet::computeSpectrumBarsFromPcm(const QByteArray &pcmChunk, int barCount)
{
    return MediaEngine::computeSpectrumBarsFromPcm(pcmChunk, barCount);
}

QString KIslandApplet::formatCountdown(qint64 remainingSeconds)
{
    return TimerManager::formatCountdown(remainingSeconds);
}

QVariantMap KIslandApplet::parsePlayerctlLine(const QString &line)
{
    return MediaEngine::parsePlayerctlLine(line);
}

QVariantMap KIslandApplet::extractMprisMetadata(const QVariant &metadataVariant)
{
    return MediaEngine::extractMprisMetadata(metadataVariant);
}

QString KIslandApplet::resolveCoverArtOrVideoThumbnail(const QString &artUrl, const QString &mediaUrl, bool *isVideoOut)
{
    return MediaEngine::resolveCoverArtOrVideoThumbnail(artUrl, mediaUrl, isVideoOut);
}

QVariantMap KIslandApplet::parseWpctlStatus(const QString &wpctlOutput, const QString &kiriState)
{
    return AudioPrivacyManager::parseWpctlStatus(wpctlOutput, kiriState);
}

K_PLUGIN_CLASS_WITH_JSON(KIslandApplet, "metadata.json")

#include "kislandapplet.moc"
