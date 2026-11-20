/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import CachyOS.UI
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

Rectangle {
    id: shazamCardRoot

    Layout.fillWidth: true
    implicitHeight: shazamCol.implicitHeight + CachyTheme.spacingMedium * 2
    Layout.preferredHeight: implicitHeight
    radius: CachyTheme.radiusSmall
    color: CachyPalette.cardBackground
    border.width: 1
    border.color: (Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening) ? CachyTheme.accentColor : CachyPalette.specularBorder

    ColumnLayout {
        id: shazamCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: CachyTheme.spacingMedium
        spacing: CachyTheme.spacingSmall

        // Header row: Title + Actions
        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            CachyIcon {
                size: 20
                name: "emblem-music-symbolic"
                color: CachyPalette.accentText
            }

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                font.bold: true
                text: i18n("Reconocimiento de música (Shazam)")
            }

            CachyToolbarButton {
                visible: Boolean(Plasmoid.songRecognizer && Plasmoid.songRecognizer.history.length > 0)
                iconName: "edit-clear-history"
                onClicked: {
                    if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.clearHistory()
                }
                tooltipText: i18n("Limpiar historial de Shazam")
            }

            CachyButton {
                visible: Boolean(Plasmoid.songRecognizer && !Plasmoid.songRecognizer.listening)
                iconName: "audio-headphones-symbolic"
                text: i18n("Reconocer canción")
                onClicked: {
                    if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.recognize(9)
                }
            }

            CachyButton {
                visible: Boolean(Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening)
                iconName: "process-stop-symbolic"
                text: i18n("Cancelar")
                onClicked: {
                    if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.cancel()
                }
            }
        }

        // Listening State Card
        Rectangle {
            visible: Boolean(Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening)
            Layout.fillWidth: true
            implicitHeight: 44
            radius: CachyTheme.radiusSmall
            color: Qt.rgba(0.0, 0.53, 1.0, 0.12)
            border.width: 1
            border.color: CachyTheme.accentColor

            RowLayout {
                anchors.fill: parent
                anchors.margins: CachyTheme.spacingSmall
                spacing: CachyTheme.spacingMedium

                CachyBusyIndicator {
                    implicitWidth: 20
                    implicitHeight: 20
                    running: Boolean(Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening)
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    text: (Plasmoid.songRecognizer && Plasmoid.songRecognizer.status.length > 0)
                        ? Plasmoid.songRecognizer.status
                        : i18n("Escuchando música…")
                    color: CachyPalette.text
                    font.weight: Font.Medium
                }
            }
        }

        // Current Recognized Track Card (Active Result)
        Rectangle {
            visible: Boolean(Plasmoid.songRecognizer && !Plasmoid.songRecognizer.listening && Plasmoid.songRecognizer.title.length > 0 && Plasmoid.songRecognizer.recognizedActive)
            Layout.fillWidth: true
            implicitHeight: activeResultRow.implicitHeight + CachyTheme.spacingSmall * 2
            radius: CachyTheme.radiusSmall
            color: CachyPalette.windowBackground
            border.width: 1
            border.color: CachyTheme.accentColor

            RowLayout {
                id: activeResultRow
                anchors.fill: parent
                anchors.margins: CachyTheme.spacingSmall
                spacing: CachyTheme.spacingMedium

                Rectangle {
                    implicitWidth: 44
                    implicitHeight: 44
                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    radius: CachyTheme.radiusSmall
                    color: CachyPalette.cardBackground
                    clip: true

                    Image {
                        anchors.fill: parent
                        visible: Boolean(Plasmoid.songRecognizer && Plasmoid.songRecognizer.coverUrl.length > 0 && status === Image.Ready)
                        source: (Plasmoid.songRecognizer ? Plasmoid.songRecognizer.coverUrl : "")
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                    CachyIcon {
                        anchors.centerIn: parent
                        size: 24
                        visible: !Plasmoid.songRecognizer || Plasmoid.songRecognizer.coverUrl.length === 0
                        name: "emblem-music-symbolic"
                        color: CachyPalette.textSecondary
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        font.bold: true
                        elide: Text.ElideRight
                        text: Plasmoid.songRecognizer ? Plasmoid.songRecognizer.title : ""
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        opacity: 0.82
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        text: Plasmoid.songRecognizer ? Plasmoid.songRecognizer.artist : ""
                    }
                }

                CachyToolbarButton {
                    iconName: "edit-copy"
                    onClicked: {
                        if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.copyTrackInfo(Plasmoid.songRecognizer.title + " — " + Plasmoid.songRecognizer.artist)
                    }
                    tooltipText: i18n("Copiar título y artista")
                }

                CachyToolbarButton {
                    visible: Boolean(Plasmoid.songRecognizer && Plasmoid.songRecognizer.shazamUrl.length > 0)
                    iconName: "emblem-music-symbolic"
                    onClicked: {
                        if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.openInBrowser()
                    }
                    tooltipText: i18n("Abrir en Shazam")
                }

                CachyToolbarButton {
                    iconName: "window-close"
                    onClicked: {
                        if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.dismissCurrentResult()
                    }
                    tooltipText: i18n("Cerrar")
                }
            }
        }

        // Persistent Shazam History List
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            visible: Boolean(Plasmoid.songRecognizer && !Plasmoid.songRecognizer.listening && Plasmoid.songRecognizer.history.length > 0)

            PlasmaComponents3.Label {
                text: i18n("Historial de canciones reconocidas:")
                font.pixelSize: 11
                color: CachyPalette.textMuted
                font.bold: true
            }

            Repeater {
                model: Plasmoid.songRecognizer ? Plasmoid.songRecognizer.history.slice(0, 5) : []

                delegate: Rectangle {
                    id: historyItemDelegate
                    required property var modelData
                    required property int index

                    Layout.fillWidth: true
                    implicitHeight: 38
                    radius: 4
                    color: CachyPalette.windowBackground
                    border.width: 1
                    border.color: CachyPalette.specularBorder

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: CachyTheme.spacingSmall
                        anchors.rightMargin: CachyTheme.spacingSmall
                        spacing: CachyTheme.spacingSmall

                        Rectangle {
                            implicitWidth: 26
                            implicitHeight: 26
                            radius: 4
                            color: CachyPalette.cardBackground
                            clip: true

                            Image {
                                anchors.fill: parent
                                visible: Boolean(historyItemDelegate.modelData.cover && historyItemDelegate.modelData.cover.length > 0)
                                source: historyItemDelegate.modelData.cover || ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                            }

                            CachyIcon {
                                anchors.centerIn: parent
                                size: 16
                                visible: !historyItemDelegate.modelData.cover || historyItemDelegate.modelData.cover.length === 0
                                name: "emblem-music-symbolic"
                                color: CachyPalette.textSecondary
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                text: historyItemDelegate.modelData.title || ""
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                text: (historyItemDelegate.modelData.artist || "") + (historyItemDelegate.modelData.timestamp ? " · " + historyItemDelegate.modelData.timestamp : "")
                                font.pixelSize: 10
                                color: CachyPalette.textMuted
                                elide: Text.ElideRight
                            }
                        }

                        CachyToolbarButton {
                            iconName: "edit-copy"
                            implicitWidth: 26
                            implicitHeight: 26
                            onClicked: {
                                if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.copyTrackInfo((historyItemDelegate.modelData.title || "") + " — " + (historyItemDelegate.modelData.artist || ""))
                            }
                            tooltipText: i18n("Copiar")
                        }

                        CachyToolbarButton {
                            visible: Boolean(historyItemDelegate.modelData.url && historyItemDelegate.modelData.url.length > 0)
                            iconName: "emblem-music-symbolic"
                            implicitWidth: 26
                            implicitHeight: 26
                            onClicked: {
                                if (Plasmoid.songRecognizer) Plasmoid.songRecognizer.openUrl(historyItemDelegate.modelData.url)
                            }
                            tooltipText: i18n("Abrir en Shazam")
                        }
                    }
                }
            }
        }

        // Empty State Message
        PlasmaComponents3.Label {
            visible: Boolean(Plasmoid.songRecognizer && !Plasmoid.songRecognizer.listening && Plasmoid.songRecognizer.history.length === 0 && !Plasmoid.songRecognizer.recognizedActive)
            Layout.fillWidth: true
            text: (Plasmoid.songRecognizer && Plasmoid.songRecognizer.status.length > 0)
                ? Plasmoid.songRecognizer.status
                : i18n("Pulsa «Reconocer canción» para identificar cualquier música que suene en tu equipo.")
            color: CachyPalette.textMuted
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }
}
