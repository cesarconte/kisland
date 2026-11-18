/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantList>

class SystemIntegrationService : public QObject
{
    Q_OBJECT

public:
    explicit SystemIntegrationService(QObject *parent = nullptr);
    ~SystemIntegrationService() override = default;

    QString kityState() const { return m_kityState; }
    bool focusActive() const { return m_focusActive; }
    QString focusModeName() const { return m_focusModeName; }
    QString focusModeIcon() const { return m_focusModeIcon; }
    bool focusDndActive() const { return m_focusDndActive; }

    void updateKityState();
    void updateFocusMode();
    void updateBluetoothAndPeripherals();

    void invokeKity();
    void invokeKiriVoice();
    void launchDropover();
    void launchLiveCaptions();
    void toggleFocusDnd();

    QVariantList getKdeConnectDevices() const;
    bool shareToKdeConnect(const QString &filePath, const QString &deviceId);
    void openKdeConnectSettings();
    void openFileLocation(const QString &filePath);

Q_SIGNALS:
    void kityStateChanged();
    void kiriStateChanged();
    void focusStateChanged();
    void peripheralAlert(const QString &title, const QString &subtitle, const QString &icon);
    void focusAlert(const QString &title, const QString &icon);

private:
    QString m_kityState = QStringLiteral("idle");
    bool m_focusActive = false;
    bool m_focusDndActive = false;
    QString m_focusModeName;
    QString m_focusModeIcon;
    QString m_focusModeId;
    QSet<QString> m_knownBtDevices;
    bool m_btInitialized = false;
    bool m_focusInitialized = false;
};
