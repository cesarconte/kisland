/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import CachyOS.UI
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.plasmoid

Item {
    id: compactPillRoot

    required property var rootPlasmoid

    readonly property bool cfgHideWhenIdle: Plasmoid.configuration.hideWhenIdle === true
    readonly property bool cfgShowArt: Plasmoid.configuration.showArtInCompact !== false
    readonly property bool cfgShowWaveform: Plasmoid.configuration.showWaveform !== false
    readonly property bool cfgShowVitals: Plasmoid.configuration.showVitalsInCompact !== false
    readonly property bool cfgShowPrivacy: Plasmoid.configuration.showPrivacyDots !== false

    readonly property bool hasActiveIslandActivity: (rootPlasmoid.pillHudType !== "")
        || Plasmoid.mediaPlaying
        || Plasmoid.trackTitle.length > 0
        || Plasmoid.timerActive
        || Plasmoid.micActive
        || Plasmoid.cameraOrScreenActive
        || (Plasmoid.songRecognizer && (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive))

    visible: !cfgHideWhenIdle || hasActiveIslandActivity
    opacity: visible ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 200 } }

    Layout.minimumWidth: visible ? ((rootPlasmoid.pillHudType !== "" ? hudRow.implicitWidth : pillRow.implicitWidth) + CachyTheme.spacingLarge * 3.0) : 0
    Layout.preferredWidth: Layout.minimumWidth
    Layout.fillHeight: true

    Behavior on Layout.minimumWidth {
        NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
    }

    DropArea {
        anchors.fill: parent
        onEntered: (drag) => {
            pillCapsule.border.color = CachyTheme.accentColor;
            pillCapsule.border.width = 2;
            rootPlasmoid.currentTab = 1;
            rootPlasmoid.manualExpanded = true;
            rootPlasmoid.expanded = true;
        }
        onExited: {
            pillCapsule.border.color = Qt.rgba(1, 1, 1, 0.15);
            pillCapsule.border.width = 1;
        }
        onDropped: (drop) => {
            pillCapsule.border.color = Qt.rgba(1, 1, 1, 0.15);
            pillCapsule.border.width = 1;
            const urls = rootPlasmoid.extractUrlsFromDrop(drop);
            for (let i = 0; i < urls.length; ++i) {
                rootPlasmoid.addFileToShelf(urls[i]);
            }
            drop.acceptProposedAction();
            rootPlasmoid.currentTab = 1;
            rootPlasmoid.manualExpanded = true;
            rootPlasmoid.expanded = true;
        }
    }

    Rectangle {
        id: pillCapsule
        anchors.centerIn: parent
        width: parent.width - 6
        height: Math.max(18, parent.height)
        radius: height / 2
        color: "#000000"
        border.width: rootPlasmoid.pillHudType === "timer_done" ? 2 : 1
        border.color: rootPlasmoid.pillHudType === "timer_done" ? CachyTheme.warningColor : Qt.rgba(1, 1, 1, 0.15)
        clip: true

        // Transient Dynamic HUD Row
        RowLayout {
            id: hudRow
            anchors.centerIn: parent
            visible: rootPlasmoid.pillHudType !== ""
            spacing: CachyTheme.spacingSmall

            Kirigami.Icon {
                implicitWidth: 14
                implicitHeight: 14
                Layout.preferredWidth: 14
                Layout.preferredHeight: 14
                Layout.alignment: Qt.AlignVCenter
                source: rootPlasmoid.hudIcon
                isMask: true
                color: (rootPlasmoid.pillHudType === "timer_done" || rootPlasmoid.pillHudType === "focus")
                    ? CachyTheme.warningColor
                    : (rootPlasmoid.pillHudType === "volume" ? "#FFFFFF" : CachyTheme.successColor)
            }

            RowLayout {
                visible: rootPlasmoid.pillHudType === "volume"
                spacing: 6
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    implicitWidth: 54
                    implicitHeight: 5
                    radius: 2.5
                    color: Qt.rgba(1, 1, 1, 0.25)
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(0, Math.min(parent.width, parent.width * (rootPlasmoid.hudValue / 100.0)))
                        radius: 2.5
                        color: "#FFFFFF"
                    }
                }

                PlasmaComponents3.Label {
                    color: "#FFFFFF"
                    font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
                    font.weight: Font.Medium
                    text: rootPlasmoid.hudTitle
                }
            }

            RowLayout {
                visible: rootPlasmoid.pillHudType !== "volume"
                spacing: 4
                Layout.alignment: Qt.AlignVCenter

                PlasmaComponents3.Label {
                    color: rootPlasmoid.pillHudType === "timer_done" ? CachyTheme.warningColor : "#FFFFFF"
                    font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
                    font.weight: Font.DemiBold
                    text: rootPlasmoid.hudTitle
                    elide: Text.ElideRight
                    Layout.maximumWidth: 180
                }

                PlasmaComponents3.Label {
                    visible: rootPlasmoid.hudSubtitle.length > 0
                    color: Qt.rgba(1, 1, 1, 0.7)
                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                    text: "· " + rootPlasmoid.hudSubtitle
                }
            }
        }

        RowLayout {
            id: pillRow
            anchors.centerIn: parent
            visible: rootPlasmoid.pillHudType === ""
            spacing: CachyTheme.spacingMedium * 1.15

            Rectangle {
                id: compactArtFrame
                visible: compactPillRoot.cfgShowArt && ((Plasmoid.songRecognizer && (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive) || Plasmoid.trackTitle.length > 0) && !Plasmoid.timerActive)
                readonly property int boxSize: Math.max(12, pillCapsule.height - 6)
                implicitWidth: boxSize
                implicitHeight: boxSize
                Layout.preferredWidth: boxSize
                Layout.preferredHeight: boxSize
                Layout.alignment: Qt.AlignVCenter
                radius: Math.max(3, Math.round(boxSize * 0.24))
                color: CachyPalette.windowBackground
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.12)
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: (Plasmoid.songRecognizer && Plasmoid.songRecognizer.recognizedActive) ? Plasmoid.songRecognizer.coverUrl : Plasmoid.trackArtUrl
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    visible: source.toString().length > 0
                }

                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: Math.max(10, parent.width - 4)
                    height: Math.max(10, parent.height - 4)
                    source: (Plasmoid.songRecognizer && (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive)) ? "emblem-music-symbolic" : "audio-x-generic"
                    isMask: true
                    color: (Plasmoid.songRecognizer && (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive)) ? CachyTheme.accentColor : "#FFFFFF"
                    visible: !(Plasmoid.songRecognizer && Plasmoid.songRecognizer.recognizedActive) ? (Plasmoid.trackArtUrl.length === 0 || (Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening)) : (Plasmoid.songRecognizer.coverUrl.length === 0)
                }
            }

            Rectangle {
                visible: (!(Plasmoid.songRecognizer && (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive)) && (!compactPillRoot.cfgShowArt || Plasmoid.trackTitle.length === 0)) || Plasmoid.timerActive
                implicitWidth: 8
                implicitHeight: 8
                Layout.alignment: Qt.AlignVCenter
                radius: 4
                color: {
                    if (Plasmoid.kityState === "listening" || Plasmoid.kiriState === "listening") return CachyTheme.accentColor;
                    if (Plasmoid.timerActive) return CachyTheme.warningColor;
                    return "#FFFFFF";
                }
            }

            PlasmaComponents3.Label {
                Layout.alignment: Qt.AlignVCenter
                color: "#FFFFFF"
                font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
                font.weight: Font.Medium
                text: {
                    if (Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening) {
                        return i18n("Escuchando…");
                    }
                    if (Plasmoid.songRecognizer && Plasmoid.songRecognizer.recognizedActive && Plasmoid.songRecognizer.title.length > 0) {
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
                    if (compactPillRoot.cfgShowVitals) {
                        return "CPU " + Plasmoid.cpuUsagePercent + "%  ·  RAM " + Plasmoid.ramUsagePercent + "%";
                    }
                    return "";
                }
            }

            Row {
                visible: compactPillRoot.cfgShowWaveform && (Plasmoid.mediaPlaying || Plasmoid.trackTitle.length > 0 || (Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening))
                spacing: 3
                Layout.alignment: Qt.AlignVCenter

                Repeater {
                    model: Plasmoid.spectrumBars

                    Rectangle {
                        required property var modelData
                        width: 3
                        readonly property real maxBarH: Math.max(8, pillCapsule.height - 8)
                        height: (Plasmoid.mediaPlaying || (Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening)) ? Math.max(4, Number(modelData) * maxBarH) : 4
                        radius: 1.5
                        color: (Plasmoid.songRecognizer && Plasmoid.songRecognizer.listening) ? CachyTheme.accentColor : (Plasmoid.mediaPlaying ? CachyTheme.successColor : Qt.rgba(1, 1, 1, 0.35))
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on height { NumberAnimation { duration: 80 } }
                    }
                }
            }

            Row {
                visible: compactPillRoot.cfgShowPrivacy && (Plasmoid.micActive || Plasmoid.cameraOrScreenActive)
                spacing: 4
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    visible: Plasmoid.micActive
                    width: 7
                    height: 7
                    radius: 3.5
                    color: CachyTheme.warningColor
                }

                Rectangle {
                    visible: Plasmoid.cameraOrScreenActive
                    width: 7
                    height: 7
                    radius: 3.5
                    color: CachyTheme.successColor
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (rootPlasmoid.expanded) {
                    rootPlasmoid.stopAutoCollapse();
                    rootPlasmoid.manualExpanded = false;
                    rootPlasmoid.expanded = false;
                } else {
                    rootPlasmoid.stopAutoCollapse();
                    rootPlasmoid.manualExpanded = true;
                    rootPlasmoid.expanded = true;
                }
            }
        }
    }
}
