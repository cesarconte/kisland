/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import CachyOS.UI
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

Rectangle {
    id: mediaCardRoot

    required property var rootPlasmoid

    Layout.fillWidth: true
    implicitHeight: mediaCol.implicitHeight + CachyTheme.spacingMedium * 2
    Layout.preferredHeight: implicitHeight
    radius: CachyTheme.radiusSmall
    color: CachyPalette.cardBackground
    border.width: 1
    border.color: CachyPalette.specularBorder

    ColumnLayout {
        id: mediaCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: CachyTheme.spacingMedium
        spacing: CachyTheme.spacingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingMedium

            // Clickable Album Cover Art / Video Preview Frame
            Rectangle {
                readonly property int frameW: Plasmoid.isVideoMedia ? 88 : 64
                implicitWidth: frameW
                implicitHeight: 64
                Layout.preferredWidth: frameW
                Layout.preferredHeight: 64
                radius: CachyTheme.radiusSmall
                color: CachyPalette.windowBackground
                border.width: 1
                border.color: coverMouse.containsMouse ? CachyTheme.accentColor : CachyPalette.specularBorder
                clip: true

                Image {
                    id: coverArtImg
                    anchors.fill: parent
                    anchors.margins: 1
                    visible: Plasmoid.trackArtUrl.length > 0 && status === Image.Ready
                    source: Plasmoid.trackArtUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                }

                Kirigami.Icon {
                    anchors.centerIn: parent
                    visible: !coverArtImg.visible
                    width: 32
                    height: 32
                    source: Plasmoid.isVideoMedia ? "video-x-generic" : "media-optical-audio"
                }

                MouseArea {
                    id: coverMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: Plasmoid.trackTitle.length > 0
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    onClicked: {
                        mediaCardRoot.rootPlasmoid.expanded = false;
                        Plasmoid.raiseMediaPlayer();
                    }
                }

                QQC2.ToolTip.visible: coverMouse.containsMouse && Plasmoid.trackTitle.length > 0
                QQC2.ToolTip.text: i18n("Hacer clic para mostrar %1", Plasmoid.playerName)
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    font.bold: true
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize + 1
                    elide: Text.ElideRight
                    text: Plasmoid.trackTitle.length > 0
                        ? Plasmoid.trackTitle
                        : i18n("Sin reproducción multimedia activa")
                }

                PlasmaComponents3.Label {
                    Layout.fillWidth: true
                    opacity: 0.82
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                    text: {
                        if (Plasmoid.trackArtist.length > 0 && Plasmoid.trackAlbum.length > 0) {
                            return Plasmoid.trackArtist + "  —  " + Plasmoid.trackAlbum;
                        }
                        if (Plasmoid.trackArtist.length > 0 && Plasmoid.playerName.length > 0) {
                            return Plasmoid.trackArtist + "  ·  " + Plasmoid.playerName;
                        }
                        if (Plasmoid.trackArtist.length > 0) {
                            return Plasmoid.trackArtist;
                        }
                        if (Plasmoid.playerName.length > 0) {
                            return (Plasmoid.isVideoMedia ? "🎬 Vídeo en " : "🎵 Audio en ") + Plasmoid.playerName + (Plasmoid.mediaPlaying ? "  ·  Reproduciendo" : "  ·  En pausa");
                        }
                        return i18n("Inicia música o vídeo en cualquier aplicación (MPRIS2)");
                    }
                }
            }

            // Live Equalizer Bars
            Row {
                spacing: 4
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: CachyTheme.spacingSmall
                Layout.rightMargin: CachyTheme.spacingSmall

                Repeater {
                    model: Plasmoid.spectrumBars
                    Rectangle {
                        required property var modelData
                        width: 4
                        height: Plasmoid.mediaPlaying ? Math.max(6, Number(modelData) * 26) : 6
                        radius: 2
                        color: Plasmoid.mediaPlaying ? CachyTheme.successColor : CachyPalette.textMuted
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on height { NumberAnimation { duration: 80 } }
                    }
                }
            }
        }

        // Interactive Media Scrubber (Elapsed Time — Slider — Remaining Time)
        RowLayout {
            Layout.fillWidth: true
            visible: Plasmoid.trackTitle.length > 0
            spacing: CachyTheme.spacingSmall

            PlasmaComponents3.Label {
                font: Kirigami.Theme.smallFont
                color: CachyPalette.text
                opacity: 0.75
                Layout.minimumWidth: 42
                text: Plasmoid.trackElapsedText
            }

            CachySlider {
                id: trackScrubber
                Layout.fillWidth: true
                from: 0.0
                to: 1.0
                enabled: Plasmoid.trackDurationSecs > 0
                value: pressed ? value : Plasmoid.trackProgress
                onMoved: Plasmoid.seekToRatio(value)
            }

            PlasmaComponents3.Label {
                font: Kirigami.Theme.smallFont
                color: CachyPalette.text
                opacity: 0.75
                Layout.minimumWidth: 48
                horizontalAlignment: Text.AlignRight
                text: Plasmoid.trackRemainingText
            }
        }

        // Full Transport Controls
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: CachyTheme.spacingMedium

            CachyToolbarButton {
                iconName: "media-playlist-shuffle-symbolic"
                checkable: true
                checked: Plasmoid.shuffleEnabled
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.toggleShuffle()
                tooltipText: i18n("Aleatorio")
            }

            CachyToolbarButton {
                iconName: "media-seek-backward-symbolic"
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.skipBySeconds(-10)
                tooltipText: i18n("Retroceder 10 s")
            }

            CachyToolbarButton {
                iconName: "media-skip-backward-symbolic"
                implicitWidth: Kirigami.Units.gridUnit * 2.0
                implicitHeight: Kirigami.Units.gridUnit * 2.0
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.previousTrack()
            }

            CachyToolbarButton {
                iconName: Plasmoid.mediaPlaying ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
                implicitWidth: Kirigami.Units.gridUnit * 2.3
                implicitHeight: Kirigami.Units.gridUnit * 2.3
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.playPauseMedia()
            }

            CachyToolbarButton {
                iconName: "media-skip-forward-symbolic"
                implicitWidth: Kirigami.Units.gridUnit * 2.0
                implicitHeight: Kirigami.Units.gridUnit * 2.0
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.nextTrack()
            }

            CachyToolbarButton {
                iconName: "media-seek-forward-symbolic"
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.skipBySeconds(10)
                tooltipText: i18n("Avanzar 10 s")
            }

            CachyToolbarButton {
                iconName: "media-playlist-repeat-symbolic"
                checkable: true
                checked: Plasmoid.loopStatus !== "None"
                enabled: Plasmoid.trackTitle.length > 0
                onClicked: Plasmoid.cycleLoopStatus()
                tooltipText: i18n("Bucle: %1", Plasmoid.loopStatus)
            }
        }
    }
}
