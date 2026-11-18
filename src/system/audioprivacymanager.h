/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>
#include <QVariantMap>

class AudioPrivacyManager : public QObject
{
    Q_OBJECT

public:
    explicit AudioPrivacyManager(QObject *parent = nullptr);
    ~AudioPrivacyManager() override = default;

    int outputVolumePercent() const { return m_outputVolumePercent; }
    bool outputMuted() const { return m_outputMuted; }
    QString outputDeviceName() const { return m_outputDeviceName; }
    bool micActive() const { return m_micActive; }
    QString micApp() const { return m_micApp; }
    bool cameraOrScreenActive() const { return m_cameraOrScreenActive; }
    QString cameraOrScreenApp() const { return m_cameraOrScreenApp; }

    void updatePipeWireAudioAndPrivacy(const QString &kiriState = QStringLiteral("idle"));
    void setOutputVolume(int percent);
    void toggleOutputMute();

    static QVariantMap parseWpctlStatus(const QString &wpctlOutput, const QString &kiriState = QStringLiteral("idle"));

Q_SIGNALS:
    void audioChanged();
    void privacyChanged();
    void volumeAlert(int percent, bool muted);

private:
    int m_outputVolumePercent = 40;
    bool m_outputMuted = false;
    QString m_outputDeviceName = QStringLiteral("PipeWire");
    bool m_micActive = false;
    QString m_micApp;
    bool m_cameraOrScreenActive = false;
    QString m_cameraOrScreenApp;
    bool m_volumeInitialized = false;
};
