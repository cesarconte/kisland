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
    Plasmoid.backgroundHints: PlasmaCore.Types.DefaultBackground

    compactRepresentation: Item {
        id: compactPillRoot
        Layout.minimumWidth: pillRow.implicitWidth + Kirigami.Units.largeSpacing * 3.0
        Layout.preferredWidth: Layout.minimumWidth
        Layout.fillHeight: true

        Behavior on Layout.minimumWidth {
            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
        }

        // Apple Dynamic Island Capsule (Native Breeze Pill)
        Rectangle {
            id: pillCapsule
            anchors.centerIn: parent
            width: parent.width - 6
            height: Math.max(18, Math.min(parent.height - 2, Kirigami.Units.gridUnit * 1.75))
            radius: height / 2
            color: Kirigami.Theme.backgroundColor
            border.width: 1
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.18)
            clip: true

            RowLayout {
                id: pillRow
                anchors.centerIn: parent
                spacing: Kirigami.Units.mediumSpacing * 1.15

                // Left side: Album Art / Video Thumbnail or Shazam Icon or Fallback Music Icon in rounded box
                Rectangle {
                    id: compactArtFrame
                    visible: (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive || Plasmoid.trackTitle.length > 0) && !Plasmoid.timerActive
                    readonly property int boxSize: Math.max(12, pillCapsule.height - 6)
                    implicitWidth: boxSize
                    implicitHeight: boxSize
                    Layout.preferredWidth: boxSize
                    Layout.preferredHeight: boxSize
                    Layout.alignment: Qt.AlignVCenter
                    radius: Math.max(3, Math.round(boxSize * 0.24))
                    color: Kirigami.Theme.alternateBackgroundColor
                    border.width: 1
                    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.18)
                    clip: true

                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        source: Plasmoid.songRecognizer.recognizedActive ? Plasmoid.songRecognizer.coverUrl : Plasmoid.trackArtUrl
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: source.toString().length > 0
                    }

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: Math.max(10, parent.width - 4)
                        height: Math.max(10, parent.height - 4)
                        source: (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive) ? "emblem-music-symbolic" : "audio-x-generic"
                        color: (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive) ? "#0088ff" : Kirigami.Theme.highlightColor
                        visible: !Plasmoid.songRecognizer.recognizedActive ? (Plasmoid.trackArtUrl.length === 0 || Plasmoid.songRecognizer.listening) : (Plasmoid.songRecognizer.coverUrl.length === 0)
                    }
                }

                // Status / Kiri orb dot (when no track or shazam is active)
                Rectangle {
                    visible: (!Plasmoid.songRecognizer.listening && !Plasmoid.songRecognizer.recognizedActive && Plasmoid.trackTitle.length === 0) || Plasmoid.timerActive
                    implicitWidth: 8
                    implicitHeight: 8
                    Layout.alignment: Qt.AlignVCenter
                    radius: 4
                    color: {
                        if (Plasmoid.kiriState === "listening") return "#00e5ff";
                        if (Plasmoid.timerActive) return Kirigami.Theme.neutralTextColor;
                        return Kirigami.Theme.highlightColor;
                    }
                }

                // Compact label: Clean Media title (no play icons, matching macOS Dynamic Island), Shazam, Countdown, or System Vitals
                PlasmaComponents3.Label {
                    Layout.alignment: Qt.AlignVCenter
                    color: Kirigami.Theme.textColor
                    font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
                    font.weight: Font.Medium
                    text: {
                        if (Plasmoid.songRecognizer.listening) {
                            return i18n("Escuchando…");
                        }
                        if (Plasmoid.songRecognizer.recognizedActive && Plasmoid.songRecognizer.title.length > 0) {
                            const song = Plasmoid.songRecognizer.title + (Plasmoid.songRecognizer.artist.length > 0 ? " — " + Plasmoid.songRecognizer.artist : "");
                            return song.length > 28 ? song.substring(0, 28) + "…" : song;
                        }
                        if (Plasmoid.timerActive) {
                            return "⏱  " + Plasmoid.timerRemainingText;
                        }
                        if (Plasmoid.trackTitle.length > 0) {
                            const t = Plasmoid.trackTitle;
                            return t.length > 28 ? t.substring(0, 28) + "…" : t;
                        }
                        return "CPU " + Plasmoid.cpuUsagePercent + "%  ·  RAM " + Plasmoid.ramUsagePercent + "%";
                    }
                }

                // Live PipeWire Audio Spectrum Bars when Media Track or Shazam is Present
                Row {
                    visible: Plasmoid.trackTitle.length > 0 || Plasmoid.songRecognizer.listening
                    spacing: 3
                    Layout.alignment: Qt.AlignVCenter

                    Repeater {
                        model: Plasmoid.spectrumBars

                        Rectangle {
                            required property var modelData
                            width: 3
                            readonly property real maxBarH: Math.max(8, pillCapsule.height - 8)
                            height: (Plasmoid.mediaPlaying || Plasmoid.songRecognizer.listening) ? Math.max(4, Number(modelData) * maxBarH) : 4
                            radius: 1.5
                            color: Plasmoid.songRecognizer.listening ? "#0088ff" : (Plasmoid.mediaPlaying ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor)
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

        readonly property int targetWidth: Math.max(Kirigami.Units.gridUnit * 29, mainColumn.implicitWidth + Kirigami.Units.largeSpacing * 2)
        readonly property int targetHeight: mainColumn.implicitHeight + Kirigami.Units.largeSpacing * 2

        implicitWidth: targetWidth
        implicitHeight: targetHeight
        Layout.minimumWidth: targetWidth
        Layout.minimumHeight: targetHeight
        Layout.preferredWidth: targetWidth
        Layout.preferredHeight: targetHeight

        ColumnLayout {
            id: mainColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
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
                    radius: 6
                    color: Kirigami.Theme.alternateBackgroundColor
                    border.width: 1
                    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)
                    implicitWidth: vitalsLbl.implicitWidth + 12
                    implicitHeight: vitalsLbl.implicitHeight + 6
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight

                    PlasmaComponents3.Label {
                        id: vitalsLbl
                        anchors.centerIn: parent
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.textColor
                        opacity: 0.85
                        text: "CPU " + Plasmoid.cpuUsagePercent + "% · RAM " + Plasmoid.ramUsagePercent + "%"
                    }
                }

                // Active Privacy Pill inside Expanded Island
                Rectangle {
                    visible: Plasmoid.micActive || Plasmoid.cameraOrScreenActive
                    radius: 6
                    color: Qt.rgba(Kirigami.Theme.neutralTextColor.r, Kirigami.Theme.neutralTextColor.g, Kirigami.Theme.neutralTextColor.b, 0.15)
                    border.width: 1
                    border.color: Kirigami.Theme.neutralTextColor
                    implicitWidth: privLbl.implicitWidth + 12
                    implicitHeight: privLbl.implicitHeight + 6
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight

                    PlasmaComponents3.Label {
                        id: privLbl
                        anchors.centerIn: parent
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.neutralTextColor
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
                implicitHeight: mediaCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                Layout.preferredHeight: implicitHeight
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                ColumnLayout {
                    id: mediaCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.mediumSpacing

                        // Clickable Album Cover Art / Video Preview Frame (Raises player window on click)
                        Rectangle {
                            readonly property int frameW: Plasmoid.isVideoMedia ? 88 : 64
                            implicitWidth: frameW
                            implicitHeight: 64
                            Layout.preferredWidth: frameW
                            Layout.preferredHeight: 64
                            radius: 8
                            color: Kirigami.Theme.backgroundColor
                            border.width: 1
                            border.color: coverMouse.containsMouse ? Kirigami.Theme.highlightColor : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.20)
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
                                    root.expanded = false;
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
                            Layout.leftMargin: Kirigami.Units.smallSpacing
                            Layout.rightMargin: Kirigami.Units.smallSpacing
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
                            color: Kirigami.Theme.textColor
                            opacity: 0.75
                            Layout.minimumWidth: 42
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
                            color: Kirigami.Theme.textColor
                            opacity: 0.75
                            Layout.minimumWidth: 48
                            horizontalAlignment: Text.AlignRight
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
                implicitHeight: volRow.implicitHeight + Kirigami.Units.mediumSpacing * 1.8
                Layout.preferredHeight: implicitHeight
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                RowLayout {
                    id: volRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Kirigami.Units.mediumSpacing
                    anchors.rightMargin: Kirigami.Units.mediumSpacing
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
                        color: Kirigami.Theme.textColor
                        opacity: 0.85
                        Layout.minimumWidth: 42
                        horizontalAlignment: Text.AlignRight
                        text: Plasmoid.outputMuted ? i18n("Mudo") : (Plasmoid.outputVolumePercent + "%")
                    }
                }
            }

            // 3. Music Recognition (macOS Shazam integration) — listens to the PipeWire default sink
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: shazamCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                Layout.preferredHeight: implicitHeight
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Plasmoid.songRecognizer.listening ? "#0088ff" : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                ColumnLayout {
                    id: shazamCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    // Header row: Title + Actions
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            source: "emblem-music-symbolic"
                            color: Plasmoid.songRecognizer.listening ? "#0088ff" : Kirigami.Theme.highlightColor
                        }

                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            font.bold: true
                            text: i18n("Reconocimiento de música (Shazam)")
                        }

                        PlasmaComponents3.ToolButton {
                            visible: Plasmoid.songRecognizer.history.length > 0
                            icon.name: "edit-clear-history"
                            onClicked: Plasmoid.songRecognizer.clearHistory()
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Limpiar historial de Shazam")
                        }

                        PlasmaComponents3.Button {
                            visible: !Plasmoid.songRecognizer.listening
                            icon.name: "audio-headphones-symbolic"
                            text: i18n("Reconocer canción")
                            onClicked: Plasmoid.songRecognizer.recognize(9)
                        }

                        PlasmaComponents3.Button {
                            visible: Plasmoid.songRecognizer.listening
                            icon.name: "process-stop-symbolic"
                            text: i18n("Cancelar")
                            onClicked: Plasmoid.songRecognizer.cancel()
                        }
                    }

                    // Listening State Card
                    Rectangle {
                        visible: Plasmoid.songRecognizer.listening
                        Layout.fillWidth: true
                        implicitHeight: 44
                        radius: Kirigami.Units.smallSpacing
                        color: Qt.rgba(0.0, 0.53, 1.0, 0.12)
                        border.width: 1
                        border.color: "#0088ff"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.smallSpacing
                            spacing: Kirigami.Units.mediumSpacing

                            PlasmaComponents3.BusyIndicator {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                running: Plasmoid.songRecognizer.listening
                            }

                            PlasmaComponents3.Label {
                                Layout.fillWidth: true
                                text: Plasmoid.songRecognizer.status.length > 0
                                    ? Plasmoid.songRecognizer.status
                                    : i18n("Escuchando música…")
                                color: Kirigami.Theme.textColor
                                font.weight: Font.Medium
                            }
                        }
                    }

                    // Current Recognized Track Card (Active Result)
                    Rectangle {
                        visible: !Plasmoid.songRecognizer.listening && Plasmoid.songRecognizer.title.length > 0 && Plasmoid.songRecognizer.recognizedActive
                        Layout.fillWidth: true
                        implicitHeight: activeResultRow.implicitHeight + Kirigami.Units.smallSpacing * 2
                        radius: Kirigami.Units.smallSpacing
                        color: Kirigami.Theme.backgroundColor
                        border.width: 1
                        border.color: Kirigami.Theme.highlightColor

                        RowLayout {
                            id: activeResultRow
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.smallSpacing
                            spacing: Kirigami.Units.mediumSpacing

                            Rectangle {
                                implicitWidth: 44
                                implicitHeight: 44
                                Layout.preferredWidth: 44
                                Layout.preferredHeight: 44
                                radius: 6
                                color: Kirigami.Theme.alternateBackgroundColor
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    visible: Plasmoid.songRecognizer.coverUrl.length > 0 && status === Image.Ready
                                    source: Plasmoid.songRecognizer.coverUrl
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                }
                                Kirigami.Icon {
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 24
                                    visible: Plasmoid.songRecognizer.coverUrl.length === 0
                                    source: "emblem-music-symbolic"
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                PlasmaComponents3.Label {
                                    Layout.fillWidth: true
                                    font.bold: true
                                    elide: Text.ElideRight
                                    text: Plasmoid.songRecognizer.title
                                }

                                PlasmaComponents3.Label {
                                    Layout.fillWidth: true
                                    opacity: 0.82
                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                    elide: Text.ElideRight
                                    text: Plasmoid.songRecognizer.artist
                                }
                            }

                            PlasmaComponents3.ToolButton {
                                icon.name: "edit-copy"
                                onClicked: Plasmoid.songRecognizer.copyTrackInfo(Plasmoid.songRecognizer.title + " — " + Plasmoid.songRecognizer.artist)
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: i18n("Copiar título y artista")
                            }

                            PlasmaComponents3.ToolButton {
                                visible: Plasmoid.songRecognizer.shazamUrl.length > 0
                                icon.name: "emblem-music-symbolic"
                                onClicked: Plasmoid.songRecognizer.openInBrowser()
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: i18n("Abrir en Shazam")
                            }

                            PlasmaComponents3.ToolButton {
                                icon.name: "window-close"
                                onClicked: Plasmoid.songRecognizer.dismissCurrentResult()
                                QQC2.ToolTip.visible: hovered
                                QQC2.ToolTip.text: i18n("Cerrar")
                            }
                        }
                    }

                    // Persistent Shazam History List
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: !Plasmoid.songRecognizer.listening && Plasmoid.songRecognizer.history.length > 0

                        PlasmaComponents3.Label {
                            text: i18n("Historial de canciones reconocidas:")
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            font.bold: true
                        }

                        Repeater {
                            model: Plasmoid.songRecognizer.history.slice(0, 5)

                            delegate: Rectangle {
                                id: historyItemDelegate
                                required property var modelData
                                required property int index

                                Layout.fillWidth: true
                                implicitHeight: 38
                                radius: 4
                                color: Kirigami.Theme.backgroundColor
                                border.width: 1
                                border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.10)

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: Kirigami.Units.smallSpacing
                                    anchors.rightMargin: Kirigami.Units.smallSpacing
                                    spacing: Kirigami.Units.smallSpacing

                                    Rectangle {
                                        implicitWidth: 26
                                        implicitHeight: 26
                                        radius: 4
                                        color: Kirigami.Theme.alternateBackgroundColor
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            visible: Boolean(historyItemDelegate.modelData.cover && historyItemDelegate.modelData.cover.length > 0)
                                            source: historyItemDelegate.modelData.cover || ""
                                            fillMode: Image.PreserveAspectCrop
                                            asynchronous: true
                                        }

                                        Kirigami.Icon {
                                            anchors.centerIn: parent
                                            width: 16
                                            height: 16
                                            visible: !historyItemDelegate.modelData.cover || historyItemDelegate.modelData.cover.length === 0
                                            source: "emblem-music-symbolic"
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        PlasmaComponents3.Label {
                                            Layout.fillWidth: true
                                            text: historyItemDelegate.modelData.title || ""
                                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }

                                        PlasmaComponents3.Label {
                                            Layout.fillWidth: true
                                            text: (historyItemDelegate.modelData.artist || "") + (historyItemDelegate.modelData.timestamp ? " · " + historyItemDelegate.modelData.timestamp : "")
                                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize - 1
                                            color: Kirigami.Theme.disabledTextColor
                                            elide: Text.ElideRight
                                        }
                                    }

                                    PlasmaComponents3.ToolButton {
                                        icon.name: "edit-copy"
                                        implicitWidth: 26
                                        implicitHeight: 26
                                        onClicked: Plasmoid.songRecognizer.copyTrackInfo((historyItemDelegate.modelData.title || "") + " — " + (historyItemDelegate.modelData.artist || ""))
                                        QQC2.ToolTip.visible: hovered
                                        QQC2.ToolTip.text: i18n("Copiar")
                                    }

                                    PlasmaComponents3.ToolButton {
                                        visible: Boolean(historyItemDelegate.modelData.url && historyItemDelegate.modelData.url.length > 0)
                                        icon.name: "emblem-music-symbolic"
                                        implicitWidth: 26
                                        implicitHeight: 26
                                        onClicked: Plasmoid.songRecognizer.openUrl(historyItemDelegate.modelData.url)
                                        QQC2.ToolTip.visible: hovered
                                        QQC2.ToolTip.text: i18n("Abrir en Shazam")
                                    }
                                }
                            }
                        }
                    }

                    // Empty State Message (when no history and not listening)
                    PlasmaComponents3.Label {
                        visible: !Plasmoid.songRecognizer.listening && Plasmoid.songRecognizer.history.length === 0 && !Plasmoid.songRecognizer.recognizedActive
                        Layout.fillWidth: true
                        text: Plasmoid.songRecognizer.status.length > 0
                            ? Plasmoid.songRecognizer.status
                            : i18n("Pulsa «Reconocer canción» para identificar cualquier música que suene en tu equipo.")
                        color: Kirigami.Theme.disabledTextColor
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // 4. Notification Digest (Apple Intelligence notification summaries via local Ollama)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: digestCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                Layout.preferredHeight: implicitHeight
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                ColumnLayout {
                    id: digestCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Kirigami.Units.mediumSpacing
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: "notifications-symbolic"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small
                        }
                        PlasmaComponents3.Label {
                            Layout.fillWidth: true
                            font.bold: true
                            text: Plasmoid.notificationDigest.count > 0
                                ? i18n("Notificaciones recientes (%1)", Plasmoid.notificationDigest.count)
                                : i18n("Sin notificaciones recientes")
                        }
                        PlasmaComponents3.Button {
                            visible: Plasmoid.notificationDigest.count > 0
                            icon.name: "text-speak-symbolic"
                            text: Plasmoid.notificationDigest.summarizing ? i18n("Resumiendo…") : i18n("Resumir")
                            enabled: !Plasmoid.notificationDigest.summarizing
                            onClicked: Plasmoid.notificationDigest.summarize()
                        }
                        PlasmaComponents3.ToolButton {
                            visible: Plasmoid.notificationDigest.count > 0
                            icon.name: "edit-clear-all-symbolic"
                            onClicked: Plasmoid.notificationDigest.clear()
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: i18n("Vaciar historial")
                        }
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        visible: Plasmoid.notificationDigest.summary.length > 0
                        wrapMode: Text.WordWrap
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        color: Kirigami.Theme.textColor
                        text: Plasmoid.notificationDigest.summary
                    }

                    PlasmaComponents3.Label {
                        Layout.fillWidth: true
                        visible: Plasmoid.notificationDigest.summary.length === 0 && Plasmoid.notificationDigest.count > 0
                        elide: Text.ElideRight
                        opacity: 0.8
                        font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                        text: {
                            const first = Plasmoid.notificationDigest.recent[0];
                            return first ? ("[" + first.time + "] " + first.app + ": " + first.summary) : "";
                        }
                    }
                }
            }

            // 5. Live Activity Timer Card with Progress Bar & Pause/Resume
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: timerCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                Layout.preferredHeight: implicitHeight
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                border.width: 1
                border.color: Plasmoid.timerActive ? Kirigami.Theme.highlightColor : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                ColumnLayout {
                    id: timerCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
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
        }
    }
}
