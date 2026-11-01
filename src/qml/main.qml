/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    preferredRepresentation: compactRepresentation

    compactRepresentation: Item {
        id: compactPillRoot
        Layout.minimumWidth: pillRow.implicitWidth + Kirigami.Units.largeSpacing * 3.0
        Layout.preferredWidth: Layout.minimumWidth
        Layout.fillHeight: true

        Behavior on Layout.minimumWidth {
            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
        }

        // Apple Dynamic Island Capsule (Liquid Glass Dark Pill)
        Rectangle {
            id: pillCapsule
            anchors.centerIn: parent
            width: parent.width - 8
            height: Math.min(parent.height - 4, Kirigami.Units.gridUnit * 1.65)
            radius: height / 2
            color: Qt.rgba(0.06, 0.07, 0.10, 0.94)
            border.width: 1
            border.color: Plasmoid.mediaPlaying
                ? Qt.rgba(0.0, 0.90, 0.46, 0.48)
                : (Plasmoid.timerActive ? Qt.rgba(1.0, 0.70, 0.0, 0.60) : Qt.rgba(1.0, 1.0, 1.0, 0.22))

            // Specular top sheen
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Math.max(0, pillCapsule.radius - 1)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.14) }
                    GradientStop { position: 0.5; color: Qt.rgba(1.0, 1.0, 1.0, 0.0) }
                }
            }

            RowLayout {
                id: pillRow
                anchors.centerIn: parent
                spacing: Kirigami.Units.mediumSpacing * 1.15

                // Left side: Album Art / Video Thumbnail (if available) or Status Dot
                Rectangle {
                    visible: Plasmoid.trackArtUrl.length > 0 && !Plasmoid.timerActive
                    implicitWidth: 20
                    implicitHeight: 20
                    radius: 5
                    color: "#263238"
                    border.width: 1
                    border.color: Qt.rgba(1.0, 1.0, 1.0, 0.30)
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: Plasmoid.trackArtUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                    }
                }

                // Status / Kiri orb dot (when no cover art is shown)
                Rectangle {
                    visible: Plasmoid.trackArtUrl.length === 0 || Plasmoid.timerActive
                    implicitWidth: 8
                    implicitHeight: 8
                    radius: 4
                    color: {
                        if (Plasmoid.kiriState === "listening") return "#00e5ff";
                        if (Plasmoid.mediaPlaying) return "#00e676";
                        if (Plasmoid.trackTitle.length > 0) return "#ffb300";
                        if (Plasmoid.timerActive) return "#ffb300";
                        return Kirigami.Theme.highlightColor;
                    }
                }

                // Compact label: Clean Media title (without crowded ▶ symbol when cover art is shown), Countdown, or System Vitals
                PlasmaComponents3.Label {
                    color: "#ffffff"
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    font.weight: Font.Medium
                    text: {
                        if (Plasmoid.timerActive) {
                            return "⏱  " + Plasmoid.timerRemainingText;
                        }
                        if (Plasmoid.trackTitle.length > 0) {
                            const prefix = (Plasmoid.trackArtUrl.length === 0)
                                ? (Plasmoid.mediaPlaying ? "▶  " : "⏸  ")
                                : (!Plasmoid.mediaPlaying ? "⏸  " : "");
                            const t = Plasmoid.trackTitle;
                            return prefix + (t.length > 26 ? t.substring(0, 26) + "…" : t);
                        }
                        return "CPU " + Plasmoid.cpuUsagePercent + "%  ·  RAM " + Plasmoid.ramUsagePercent + "%";
                    }
                }

                // Live PipeWire Audio Spectrum Bars when Media Track is Present
                Row {
                    visible: Plasmoid.trackTitle.length > 0
                    spacing: 3
                    Layout.alignment: Qt.AlignVCenter

                    Repeater {
                        model: Plasmoid.spectrumBars

                        Rectangle {
                            required property var modelData
                            width: 3
                            height: Plasmoid.mediaPlaying ? Math.max(4, Number(modelData) * 14) : 4
                            radius: 1.5
                            color: Plasmoid.mediaPlaying ? "#00e676" : "#90a4ae"
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on height { NumberAnimation { duration: 80 } }
                        }
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.expanded = !root.expanded
            }
        }
    }

    fullRepresentation: Item {
        id: expandedIsland
        implicitWidth: Kirigami.Units.gridUnit * 24
        implicitHeight: Kirigami.Units.gridUnit * 17.5

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            // Header
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.mediumSpacing
                Kirigami.Heading {
                    level: 3
                    text: i18n("🏝️ KIsland · Live Activities")
                    Layout.fillWidth: true
                }
                PlasmaComponents3.Button {
                    icon.name: "audio-input-microphone-symbolic"
                    text: i18n("Kiri Voz")
                    onClicked: Plasmoid.invokeKiriVoice()
                }
            }

            // 1. Media Player Card (MPRIS2 + PipeWire Live Waveform)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: mediaCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: Kirigami.Units.largeSpacing
                color: Qt.rgba(Kirigami.Theme.alternateBackgroundColor.r,
                               Kirigami.Theme.alternateBackgroundColor.g,
                               Kirigami.Theme.alternateBackgroundColor.b, 0.80)
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.18)

                ColumnLayout {
                    id: mediaCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.mediumSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.largeSpacing

                        // Album Cover Art / Video Preview Frame (Apple Now Playing & Dynamic Island style)
                        Rectangle {
                            implicitWidth: Plasmoid.isVideoMedia ? 88 : 64
                            implicitHeight: 64
                            radius: 10
                            color: Qt.rgba(0.0, 0.0, 0.0, 0.35)
                            border.width: 1
                            border.color: Qt.rgba(1.0, 1.0, 1.0, 0.25)
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
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
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
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Repeater {
                                model: Plasmoid.spectrumBars
                                Rectangle {
                                    required property var modelData
                                    width: 4
                                    height: Plasmoid.mediaPlaying ? Math.max(6, Number(modelData) * 26) : 6
                                    radius: 2
                                    color: Plasmoid.mediaPlaying ? "#00e676" : Kirigami.Theme.disabledTextColor
                                    anchors.verticalCenter: parent.verticalCenter
                                    Behavior on height { NumberAnimation { duration: 80 } }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Kirigami.Units.largeSpacing * 1.4

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-skip-backward-symbolic"
                            implicitWidth: Kirigami.Units.gridUnit * 2.2
                            implicitHeight: Kirigami.Units.gridUnit * 2.2
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.previousTrack()
                        }
                        PlasmaComponents3.ToolButton {
                            icon.name: Plasmoid.mediaPlaying ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
                            implicitWidth: Kirigami.Units.gridUnit * 2.4
                            implicitHeight: Kirigami.Units.gridUnit * 2.4
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.playPauseMedia()
                        }
                        PlasmaComponents3.ToolButton {
                            icon.name: "media-skip-forward-symbolic"
                            implicitWidth: Kirigami.Units.gridUnit * 2.2
                            implicitHeight: Kirigami.Units.gridUnit * 2.2
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.nextTrack()
                        }
                    }
                }
            }

            // 2. Live Activity Timer Card
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: timerCol.implicitHeight + Kirigami.Units.largeSpacing * 2
                radius: Kirigami.Units.largeSpacing
                color: Qt.rgba(Kirigami.Theme.alternateBackgroundColor.r,
                               Kirigami.Theme.alternateBackgroundColor.g,
                               Kirigami.Theme.alternateBackgroundColor.b, 0.80)
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.18)

                ColumnLayout {
                    id: timerCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.mediumSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.mediumSpacing
                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            font.bold: true
                            text: Plasmoid.timerActive
                                ? i18n("⏱  %1: %2", Plasmoid.timerLabel, Plasmoid.timerRemainingText)
                                : i18n("⏱  Temporizadores Rápidos (Live Activity)")
                        }
                        PlasmaComponents3.Button {
                            visible: Plasmoid.timerActive
                            icon.name: "process-stop-symbolic"
                            text: i18n("Cancelar")
                            onClicked: Plasmoid.cancelQuickTimer()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !Plasmoid.timerActive
                        spacing: Kirigami.Units.mediumSpacing

                        Repeater {
                            model: [
                                { secs: 60, label: "1 min" },
                                { secs: 300, label: "5 min" },
                                { secs: 600, label: "10 min" },
                                { secs: 1500, label: "🍅 Pomodoro (25m)" }
                            ]
                            PlasmaComponents3.Button {
                                required property var modelData
                                Layout.fillWidth: true
                                text: modelData.label
                                font: Kirigami.Theme.smallFont
                                onClicked: Plasmoid.startQuickTimer(modelData.secs, modelData.label)
                            }
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
