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
    Plasmoid.backgroundHints: PlasmaCore.Types.TranslucentBackground

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
            width: parent.width - 6
            height: Math.max(18, Math.min(parent.height - 2, Kirigami.Units.gridUnit * 1.75))
            radius: height / 2
            color: Qt.rgba(0.06, 0.07, 0.10, 0.94)
            border.width: 1
            border.color: Plasmoid.mediaPlaying
                ? Qt.rgba(0.0, 0.90, 0.46, 0.48)
                : (Plasmoid.timerActive ? Qt.rgba(1.0, 0.70, 0.0, 0.60) : Qt.rgba(1.0, 1.0, 1.0, 0.22))
            clip: true

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

                // Left side: Album Art / Video Thumbnail dynamically sized inside pill height
                Rectangle {
                    id: compactArtFrame
                    visible: Plasmoid.trackArtUrl.length > 0 && !Plasmoid.timerActive
                    readonly property int boxSize: Math.max(12, pillCapsule.height - 6)
                    implicitWidth: boxSize
                    implicitHeight: boxSize
                    Layout.preferredWidth: boxSize
                    Layout.preferredHeight: boxSize
                    Layout.alignment: Qt.AlignVCenter
                    radius: Math.max(3, Math.round(boxSize * 0.24))
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
                    Layout.alignment: Qt.AlignVCenter
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
                    Layout.alignment: Qt.AlignVCenter
                    color: "#ffffff"
                    font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
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
                            readonly property real maxBarH: Math.max(8, pillCapsule.height - 8)
                            height: Plasmoid.mediaPlaying ? Math.max(4, Number(modelData) * maxBarH) : 4
                            radius: 1.5
                            color: Plasmoid.mediaPlaying ? "#00e676" : "#90a4ae"
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on height { NumberAnimation { duration: 80 } }
                        }
                    }
                }

                // macOS Dynamic Island Privacy Indicator Dots (Amber = Microphone, Green = Camera/Screen)
                Row {
                    visible: Plasmoid.micActive || Plasmoid.cameraOrScreenActive
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        visible: Plasmoid.micActive
                        width: 7
                        height: 7
                        radius: 3.5
                        color: "#ff9100"
                    }

                    Rectangle {
                        visible: Plasmoid.cameraOrScreenActive
                        width: 7
                        height: 7
                        radius: 3.5
                        color: "#00e676"
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
        implicitWidth: Kirigami.Units.gridUnit * 26
        implicitHeight: Kirigami.Units.gridUnit * 24.5

        // Outer Liquid Glass Shell (matches Kiri & KDropover)
        Rectangle {
            id: islandGlassShell
            anchors.fill: parent
            anchors.margins: 2
            radius: Kirigami.Units.largeSpacing
            color: Qt.rgba(0.08, 0.09, 0.13, 0.88)
            border.width: 1
            border.color: Plasmoid.mediaPlaying
                ? Qt.rgba(0.0, 0.90, 0.46, 0.45)
                : Qt.rgba(1.0, 1.0, 1.0, 0.24)

            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: Math.max(0, islandGlassShell.radius - 1)
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.15) }
                    GradientStop { position: 0.35; color: Qt.rgba(1.0, 1.0, 1.0, 0.02) }
                    GradientStop { position: 1.0; color: Qt.rgba(0.0, 0.0, 0.0, 0.14) }
                }
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.10)
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.mediumSpacing

            // Header + Vitals + Quick Launchers (Alcove / NotchNook style)
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    level: 4
                    text: i18n("🏝️ KIsland")
                }

                Rectangle {
                    radius: 8
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.08)
                    border.width: 1
                    border.color: Qt.rgba(1.0, 1.0, 1.0, 0.14)
                    implicitWidth: vitalsLbl.implicitWidth + 12
                    implicitHeight: vitalsLbl.implicitHeight + 6

                    PlasmaComponents3.Label {
                        id: vitalsLbl
                        anchors.centerIn: parent
                        font: Kirigami.Theme.smallFont
                        color: "#b0bec5"
                        text: "CPU " + Plasmoid.cpuUsagePercent + "% · RAM " + Plasmoid.ramUsagePercent + "%"
                    }
                }

                // Active Privacy Pill inside Expanded Island
                Rectangle {
                    visible: Plasmoid.micActive || Plasmoid.cameraOrScreenActive
                    radius: 8
                    color: Qt.rgba(1.0, 0.57, 0.0, 0.18)
                    border.width: 1
                    border.color: "#ff9100"
                    implicitWidth: privLbl.implicitWidth + 12
                    implicitHeight: privLbl.implicitHeight + 6

                    PlasmaComponents3.Label {
                        id: privLbl
                        anchors.centerIn: parent
                        font: Kirigami.Theme.smallFont
                        color: "#ffd180"
                        text: {
                            const parts = [];
                            if (Plasmoid.micActive) parts.push("🎙️ " + Plasmoid.micApp);
                            if (Plasmoid.cameraOrScreenActive) parts.push("📹 " + Plasmoid.cameraOrScreenApp);
                            return parts.join(" · ");
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                PlasmaComponents3.ToolButton {
                    icon.name: "folder-stash-symbolic"
                    onClicked: Plasmoid.launchDropover()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("Abrir KDropover (Estantería de archivos)")
                }

                PlasmaComponents3.ToolButton {
                    icon.name: "view-list-text-symbolic"
                    onClicked: Plasmoid.launchLiveCaptions()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("Abrir KLiveCaptions (Subtítulos en vivo)")
                }

                PlasmaComponents3.Button {
                    icon.name: "audio-input-microphone-symbolic"
                    text: i18n("Kiri")
                    onClicked: Plasmoid.invokeKiriVoice()
                }
            }

            // 1. Media Player Card (MPRIS2 + Interactive Scrubber + Shuffle/Loop + PipeWire Waveform)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: mediaCol.implicitHeight + Kirigami.Units.largeSpacing * 1.6
                radius: Kirigami.Units.mediumSpacing
                color: Qt.rgba(0.02, 0.03, 0.05, 0.42)
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.15)

                ColumnLayout {
                    id: mediaCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.mediumSpacing

                        // Clickable Album Cover Art / Video Preview Frame (Raises player window on click)
                        Rectangle {
                            implicitWidth: Plasmoid.isVideoMedia ? 88 : 64
                            implicitHeight: 64
                            radius: 10
                            color: Qt.rgba(0.0, 0.0, 0.0, 0.35)
                            border.width: 1
                            border.color: coverMouse.containsMouse ? "#00e5ff" : Qt.rgba(1.0, 1.0, 1.0, 0.25)
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
                                onClicked: Plasmoid.raiseMediaPlayer()
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

                    // Interactive Media Scrubber (Elapsed Time — Slider — Remaining Time)
                    RowLayout {
                        Layout.fillWidth: true
                        visible: Plasmoid.trackTitle.length > 0
                        spacing: Kirigami.Units.smallSpacing

                        PlasmaComponents3.Label {
                            font: Kirigami.Theme.smallFont
                            color: "#b0bec5"
                            text: Plasmoid.trackElapsedText
                        }

                        PlasmaComponents3.Slider {
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
                            color: "#b0bec5"
                            text: Plasmoid.trackRemainingText
                        }
                    }

                    // Full Transport Controls (Shuffle, -10s, Prev, Play/Pause, Next, +10s, Loop)
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: Kirigami.Units.mediumSpacing

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-playlist-shuffle-symbolic"
                            checkable: true
                            checked: Plasmoid.shuffleEnabled
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.toggleShuffle()
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Aleatorio (Shuffle)")
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-seek-backward-symbolic"
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.skipBySeconds(-10)
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Retroceder 10 s")
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-skip-backward-symbolic"
                            implicitWidth: Kirigami.Units.gridUnit * 2.0
                            implicitHeight: Kirigami.Units.gridUnit * 2.0
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.previousTrack()
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: Plasmoid.mediaPlaying ? "media-playback-pause-symbolic" : "media-playback-start-symbolic"
                            implicitWidth: Kirigami.Units.gridUnit * 2.3
                            implicitHeight: Kirigami.Units.gridUnit * 2.3
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.playPauseMedia()
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-skip-forward-symbolic"
                            implicitWidth: Kirigami.Units.gridUnit * 2.0
                            implicitHeight: Kirigami.Units.gridUnit * 2.0
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.nextTrack()
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-seek-forward-symbolic"
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.skipBySeconds(10)
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Avanzar 10 s")
                        }

                        PlasmaComponents3.ToolButton {
                            icon.name: "media-playlist-repeat-symbolic"
                            checkable: true
                            checked: Plasmoid.loopStatus !== "None"
                            enabled: Plasmoid.trackTitle.length > 0
                            onClicked: Plasmoid.cycleLoopStatus()
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Bucle: %1", Plasmoid.loopStatus)
                        }
                    }
                }
            }

            // 2. PipeWire Output Volume & Active Sink Bar (Alcove / Control Center style)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: volRow.implicitHeight + Kirigami.Units.mediumSpacing * 1.6
                radius: Kirigami.Units.mediumSpacing
                color: Qt.rgba(0.02, 0.03, 0.05, 0.42)
                border.width: 1
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.15)

                RowLayout {
                    id: volRow
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing * 1.5
                    spacing: Kirigami.Units.smallSpacing

                    PlasmaComponents3.ToolButton {
                        icon.name: Plasmoid.outputMuted ? "audio-volume-muted-symbolic" : "audio-volume-high-symbolic"
                        onClicked: Plasmoid.toggleOutputMute()
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: Plasmoid.outputMuted ? i18n("Reactivar sonido") : i18n("Silenciar salida")
                    }

                    PlasmaComponents3.Label {
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.maximumWidth: Kirigami.Units.gridUnit * 6.5
                        text: Plasmoid.outputDeviceName
                    }

                    PlasmaComponents3.Slider {
                        id: volumeSlider
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        stepSize: 1
                        value: pressed ? value : Plasmoid.outputVolumePercent
                        onMoved: Plasmoid.setOutputVolume(Math.round(value))
                    }

                    PlasmaComponents3.Label {
                        font: Kirigami.Theme.smallFont
                        color: "#b0bec5"
                        Layout.minimumWidth: 36
                        horizontalAlignment: Text.AlignRight
                        text: Plasmoid.outputMuted ? i18n("Mudo") : (Plasmoid.outputVolumePercent + "%")
                    }
                }
            }

            // 3. Live Activity Timer Card with Progress Bar & Pause/Resume
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: timerCol.implicitHeight + Kirigami.Units.mediumSpacing * 1.8
                radius: Kirigami.Units.mediumSpacing
                color: Qt.rgba(0.02, 0.03, 0.05, 0.42)
                border.width: 1
                border.color: Plasmoid.timerActive ? Qt.rgba(1.0, 0.70, 0.0, 0.45) : Qt.rgba(1.0, 1.0, 1.0, 0.15)

                ColumnLayout {
                    id: timerCol
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing
                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            font.bold: true
                            text: Plasmoid.timerActive
                                ? i18n("⏱  %1: %2", Plasmoid.timerLabel, Plasmoid.timerRemainingText)
                                : i18n("⏱  Temporizadores Rápidos (Live Activity)")
                        }
                        PlasmaComponents3.Button {
                            visible: Plasmoid.timerActive
                            icon.name: Plasmoid.timerPaused ? "media-playback-start-symbolic" : "media-playback-pause-symbolic"
                            text: Plasmoid.timerPaused ? i18n("Reanudar") : i18n("Pausar")
                            onClicked: Plasmoid.togglePauseTimer()
                        }
                        PlasmaComponents3.Button {
                            visible: Plasmoid.timerActive
                            icon.name: "process-stop-symbolic"
                            text: i18n("Cancelar")
                            onClicked: Plasmoid.cancelQuickTimer()
                        }
                    }

                    // Visual countdown bar when timer is active
                    PlasmaComponents3.ProgressBar {
                        Layout.fillWidth: true
                        visible: Plasmoid.timerActive
                        from: 0.0
                        to: 1.0
                        value: Plasmoid.timerProgress
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: !Plasmoid.timerActive
                        spacing: Kirigami.Units.smallSpacing

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
