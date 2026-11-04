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

    property int currentTab: 0 // 0 = Multimedia, 1 = Bandeja (Drop Shelf)
    property bool pinned: false
    hideOnWindowDeactivate: !pinned && (currentTab !== 1)

    property string lastSeenTrackKey: ""
    property bool lastSeenTimerActive: false
    property bool manualExpanded: false

    // Transient Apple Dynamic Island HUD state
    property string pillHudType: "" // "volume", "peripheral", "focus", "airdrop"
    property string hudTitle: ""
    property string hudSubtitle: ""
    property string hudIcon: ""
    property int hudValue: 0

    Timer {
        id: pillHudTimer
        interval: root.pillHudType === "volume" ? 1800 : 2800
        repeat: false
        onTriggered: {
            root.pillHudType = "";
        }
    }

    function showHud(type, title, subtitle, icon, val) {
        root.pillHudType = type;
        root.hudTitle = title;
        root.hudSubtitle = subtitle;
        root.hudIcon = icon;
        root.hudValue = (typeof val === "number") ? val : 0;
        pillHudTimer.restart();
    }

    // Drop Shelf Model & Helpers
    ListModel {
        id: shelfModel
    }

    function extractUrlsFromDrop(drop) {
        let list = [];
        if (drop && drop.hasUrls && drop.urls) {
            for (let i = 0; i < drop.urls.length; ++i) {
                list.push(drop.urls[i].toString());
            }
        } else if (drop && drop.hasText && drop.text) {
            const lines = drop.text.split("\n");
            for (let i = 0; i < lines.length; ++i) {
                const l = lines[i].trim();
                if (l.length > 0) {
                    list.push(l);
                }
            }
        }
        return list;
    }

    function addFileToShelf(urlStr) {
        const clean = urlStr.toString().trim();
        if (clean.length === 0) return;
        let decoded = clean;
        if (clean.startsWith("file://")) {
            decoded = decodeURIComponent(clean.substring(7));
        }
        let name = decoded.substring(decoded.lastIndexOf('/') + 1);
        if (name.length === 0) name = decoded;
        for (let i = 0; i < shelfModel.count; ++i) {
            if (shelfModel.get(i).fileUrl === clean) return;
        }
        let icon = "application-x-zerosize";
        const lower = name.toLowerCase();
        if (lower.match(/\.(png|jpg|jpeg|webp|gif|svg|bmp)$/)) icon = "image-x-generic";
        else if (lower.match(/\.(mp4|mkv|webm|avi|mov|m4v)$/)) icon = "video-x-generic";
        else if (lower.match(/\.(mp3|flac|wav|ogg|m4a|aac)$/)) icon = "audio-x-generic";
        else if (lower.match(/\.(pdf|doc|docx|odt|txt|md|rtf)$/)) icon = "x-office-document";
        else if (lower.match(/\.(zip|tar|gz|bz2|xz|7z|rar)$/)) icon = "archive-extract";
        else if (lower.match(/\.(iso|img)$/)) icon = "media-optical";

        shelfModel.append({
            fileName: name,
            fileUrl: clean,
            filePath: decoded,
            fileIcon: icon
        });
    }

    onExpandedChanged: {
        if (!expanded) {
            autoCollapseTimer.stop();
            root.manualExpanded = false;
        }
    }

    Timer {
        id: autoCollapseTimer
        interval: (Plasmoid.configuration.autoCollapseDuration || 4) * 1000
        repeat: false
        onTriggered: {
            if (!root.manualExpanded && !root.pinned && root.currentTab !== 1) {
                root.expanded = false;
            }
        }
    }

    function checkTrackChange() {
        if (Plasmoid.configuration.autoExpand === false) return;

        const title = Plasmoid.trackTitle ? Plasmoid.trackTitle.trim() : "";
        const artist = Plasmoid.trackArtist ? Plasmoid.trackArtist.trim() : "";
        const currentKey = title + "::" + artist;

        if (title.length === 0) {
            root.lastSeenTrackKey = "";
            return;
        }

        // Only trigger when the actual song title or artist changes
        if (currentKey !== root.lastSeenTrackKey) {
            root.lastSeenTrackKey = currentKey;
            if (Plasmoid.mediaPlaying) {
                root.manualExpanded = false;
                root.expanded = true;
                autoCollapseTimer.restart();
            }
        }
    }

    function checkTimerChange() {
        if (Plasmoid.configuration.autoExpand === false) return;

        if (Plasmoid.timerActive && !root.lastSeenTimerActive) {
            root.lastSeenTimerActive = true;
            root.manualExpanded = false;
            root.expanded = true;
            autoCollapseTimer.restart();
        } else if (!Plasmoid.timerActive) {
            root.lastSeenTimerActive = false;
        }
    }

    Connections {
        target: Plasmoid
        function onMediaStateChanged() {
            root.checkTrackChange();
        }
        function onTimerChanged() {
            root.checkTimerChange();
        }
        function onVolumeAlert(percent, muted) {
            const volIcon = muted ? "audio-volume-muted" : (percent > 66 ? "audio-volume-high" : (percent > 33 ? "audio-volume-medium" : "audio-volume-low"));
            root.showHud("volume", muted ? i18n("Silenciado") : (percent + "%"), "", volIcon, percent);
        }
        function onPeripheralAlert(title, subtitle, icon) {
            root.showHud("peripheral", title, subtitle, icon, 0);
        }
        function onFocusAlert(title, icon) {
            root.showHud("focus", title, "", icon, 0);
        }
    }

    compactRepresentation: Item {
        id: compactPillRoot

        readonly property bool cfgHideWhenIdle: Plasmoid.configuration.hideWhenIdle === true
        readonly property bool cfgShowArt: Plasmoid.configuration.showArtInCompact !== false
        readonly property bool cfgShowWaveform: Plasmoid.configuration.showWaveform !== false
        readonly property bool cfgShowVitals: Plasmoid.configuration.showVitalsInCompact !== false
        readonly property bool cfgShowPrivacy: Plasmoid.configuration.showPrivacyDots !== false

        readonly property bool hasActiveIslandActivity: (root.pillHudType !== "") || Plasmoid.mediaPlaying || Plasmoid.trackTitle.length > 0 || Plasmoid.timerActive || Plasmoid.micActive || Plasmoid.cameraOrScreenActive || Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive

        visible: !cfgHideWhenIdle || hasActiveIslandActivity
        opacity: visible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        Layout.minimumWidth: visible ? ((root.pillHudType !== "" ? hudRow.implicitWidth : pillRow.implicitWidth) + Kirigami.Units.largeSpacing * 3.0) : 0
        Layout.preferredWidth: Layout.minimumWidth
        Layout.fillHeight: true

        Behavior on Layout.minimumWidth {
            NumberAnimation { duration: 240; easing.type: Easing.OutCubic }
        }

        // NotchNook-style DropArea onto the compact pill
        DropArea {
            anchors.fill: parent
            onEntered: (drag) => {
                pillCapsule.border.color = "#0a84ff";
                pillCapsule.border.width = 2;
                // Auto-expand island directly into Drop Shelf on hover/drag
                root.currentTab = 1;
                root.manualExpanded = true;
                root.expanded = true;
            }
            onExited: {
                pillCapsule.border.color = Qt.rgba(1, 1, 1, 0.15);
                pillCapsule.border.width = 1;
            }
            onDropped: (drop) => {
                pillCapsule.border.color = Qt.rgba(1, 1, 1, 0.15);
                pillCapsule.border.width = 1;
                const urls = root.extractUrlsFromDrop(drop);
                for (let i = 0; i < urls.length; ++i) {
                    root.addFileToShelf(urls[i]);
                }
                drop.acceptProposedAction();
                root.currentTab = 1;
                root.manualExpanded = true;
                root.expanded = true;
            }
        }

        // Apple Dynamic Island Capsule (macOS / iOS Pitch Black Island)
        Rectangle {
            id: pillCapsule
            anchors.centerIn: parent
            width: parent.width - 6
            height: Math.max(18, Math.min(parent.height - 2, Kirigami.Units.gridUnit * 1.75))
            radius: height / 2
            color: "#000000"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.15)
            clip: true

            // Transient Dynamic HUD Row (Volume, Bluetooth Peripherals, Focus/DND, AirDrop)
            RowLayout {
                id: hudRow
                anchors.centerIn: parent
                visible: root.pillHudType !== ""
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Icon {
                    implicitWidth: 14
                    implicitHeight: 14
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    Layout.alignment: Qt.AlignVCenter
                    source: root.hudIcon
                    color: root.pillHudType === "focus" ? "#ff9f0a" : (root.pillHudType === "volume" ? "#ffffff" : "#30d158")
                }

                RowLayout {
                    visible: root.pillHudType === "volume"
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
                            width: Math.max(0, Math.min(parent.width, parent.width * (root.hudValue / 100.0)))
                            radius: 2.5
                            color: "#ffffff"
                        }
                    }

                    PlasmaComponents3.Label {
                        color: "#ffffff"
                        font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
                        font.weight: Font.Medium
                        text: root.hudTitle
                    }
                }

                RowLayout {
                    visible: root.pillHudType !== "volume"
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    PlasmaComponents3.Label {
                        color: "#ffffff"
                        font.pixelSize: Math.min(Kirigami.Theme.smallFont.pixelSize, Math.max(10, pillCapsule.height - 8))
                        font.weight: Font.Medium
                        text: root.hudTitle
                        elide: Text.ElideRight
                        Layout.maximumWidth: 180
                    }

                    PlasmaComponents3.Label {
                        visible: root.hudSubtitle.length > 0
                        color: Qt.rgba(1, 1, 1, 0.7)
                        font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                        text: "· " + root.hudSubtitle
                    }
                }
            }

            RowLayout {
                id: pillRow
                anchors.centerIn: parent
                visible: root.pillHudType === ""
                spacing: Kirigami.Units.mediumSpacing * 1.15

                // Left side: Album Art / Video Thumbnail or Shazam Icon or Fallback Music Icon in rounded box
                Rectangle {
                    id: compactArtFrame
                    visible: compactPillRoot.cfgShowArt && ((Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive || Plasmoid.trackTitle.length > 0) && !Plasmoid.timerActive)
                    readonly property int boxSize: Math.max(12, pillCapsule.height - 6)
                    implicitWidth: boxSize
                    implicitHeight: boxSize
                    Layout.preferredWidth: boxSize
                    Layout.preferredHeight: boxSize
                    Layout.alignment: Qt.AlignVCenter
                    radius: Math.max(3, Math.round(boxSize * 0.24))
                    color: "#1c1c1e"
                    border.width: 1
                    border.color: Qt.rgba(1, 1, 1, 0.12)
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
                        color: (Plasmoid.songRecognizer.listening || Plasmoid.songRecognizer.recognizedActive) ? "#0a84ff" : "#ffffff"
                        visible: !Plasmoid.songRecognizer.recognizedActive ? (Plasmoid.trackArtUrl.length === 0 || Plasmoid.songRecognizer.listening) : (Plasmoid.songRecognizer.coverUrl.length === 0)
                    }
                }

                // Status / Kity orb dot (when no track or shazam is active)
                Rectangle {
                    visible: (!Plasmoid.songRecognizer.listening && !Plasmoid.songRecognizer.recognizedActive && (!compactPillRoot.cfgShowArt || Plasmoid.trackTitle.length === 0)) || Plasmoid.timerActive
                    implicitWidth: 8
                    implicitHeight: 8
                    Layout.alignment: Qt.AlignVCenter
                    radius: 4
                    color: {
                        if (Plasmoid.kityState === "listening" || Plasmoid.kiriState === "listening") return "#00e5ff";
                        if (Plasmoid.timerActive) return "#ff9500";
                        return "#ffffff";
                    }
                }

                // Compact label: Clean Media title (no play icons, matching macOS Dynamic Island), Shazam, Countdown, or System Vitals
                PlasmaComponents3.Label {
                    Layout.alignment: Qt.AlignVCenter
                    color: "#ffffff"
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
                        if (compactPillRoot.cfgShowVitals) {
                            return "CPU " + Plasmoid.cpuUsagePercent + "%  ·  RAM " + Plasmoid.ramUsagePercent + "%";
                        }
                        return "";
                    }
                }

                // Live PipeWire Audio Spectrum Bars when Media Track or Shazam is Present
                Row {
                    visible: compactPillRoot.cfgShowWaveform && (Plasmoid.trackTitle.length > 0 || Plasmoid.songRecognizer.listening)
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
                            color: Plasmoid.songRecognizer.listening ? "#0a84ff" : (Plasmoid.mediaPlaying ? "#30d158" : Qt.rgba(1, 1, 1, 0.35))
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on height { NumberAnimation { duration: 80 } }
                        }
                    }
                }

                // macOS Dynamic Island Privacy Indicator Dots (Amber = Microphone, Green = Camera/Screen)
                Row {
                    visible: compactPillRoot.cfgShowPrivacy && (Plasmoid.micActive || Plasmoid.cameraOrScreenActive)
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        visible: Plasmoid.micActive
                        width: 7
                        height: 7
                        radius: 3.5
                        color: "#ff9500"
                    }

                    Rectangle {
                        visible: Plasmoid.cameraOrScreenActive
                        width: 7
                        height: 7
                        radius: 3.5
                        color: "#34c759"
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.expanded) {
                        autoCollapseTimer.stop();
                        root.manualExpanded = false;
                        root.expanded = false;
                    } else {
                        autoCollapseTimer.stop();
                        root.manualExpanded = true;
                        root.expanded = true;
                    }
                }
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

            // Header + Vitals + Focus chip + Quick Launchers (Alcove / NotchNook style)
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

                // Focus Mode / No Molestar Chip (Interactive toggle linked to KFocus)
                Rectangle {
                    id: focusChipRect
                    radius: 6
                    color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? Qt.rgba(1, 0.6, 0, 0.18) : Kirigami.Theme.alternateBackgroundColor
                    border.width: 1
                    border.color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? "#ff9f0a" : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)
                    implicitWidth: focusChipRow.implicitWidth + 14
                    implicitHeight: vitalsLbl.implicitHeight + 6
                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight

                    RowLayout {
                        id: focusChipRow
                        anchors.centerIn: parent
                        spacing: 4

                        Kirigami.Icon {
                            implicitWidth: 13
                            implicitHeight: 13
                            source: Plasmoid.focusModeIcon.length > 0 ? Plasmoid.focusModeIcon : (Plasmoid.focusDndActive ? "notifications-disabled" : "preferences-desktop-notification-bell")
                            color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? "#ff9f0a" : Kirigami.Theme.textColor
                        }

                        PlasmaComponents3.Label {
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            font.weight: Font.Medium
                            color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? "#ff9f0a" : Kirigami.Theme.textColor
                            text: {
                                if (Plasmoid.focusActive && Plasmoid.focusModeName.length > 0) {
                                    return Plasmoid.focusModeName;
                                }
                                return Plasmoid.focusDndActive ? i18n("No molestar") : i18n("Normal");
                            }
                        }
                    }

                    MouseArea {
                        id: focusChipMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: Plasmoid.toggleFocusDnd()
                    }

                    QQC2.ToolTip.visible: focusChipMouse.containsMouse
                    QQC2.ToolTip.text: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? i18n("Desactivar No molestar / Concentración") : i18n("Activar No molestar")
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
                    icon.name: root.pinned ? "window-pin" : "window-unpin"
                    checkable: true
                    checked: root.pinned
                    onToggled: root.pinned = checked
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: root.pinned ? i18n("Desfijar (cerrar automáticamente al perder el foco)") : i18n("Fijar abierta (mantener abierta incluso al cambiar de ventana)")
                }

                PlasmaComponents3.ToolButton {
                    visible: Plasmoid.configuration.showQuickLaunchers !== false
                    icon.name: "folder-stash-symbolic"
                    onClicked: Plasmoid.launchDropover()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("Abrir KDropover (Estantería externa)")
                }

                PlasmaComponents3.ToolButton {
                    visible: Plasmoid.configuration.showQuickLaunchers !== false
                    icon.name: "view-list-text-symbolic"
                    onClicked: Plasmoid.launchLiveCaptions()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("Abrir KLiveCaptions (Subtítulos en vivo)")
                }

                PlasmaComponents3.ToolButton {
                    visible: Plasmoid.configuration.showQuickLaunchers !== false
                    icon.name: "kity-symbolic"
                    onClicked: Plasmoid.invokeKity()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("Abrir Kity (Asistente de IA y voz)")
                }
            }

            // Tabs Switcher
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                PlasmaComponents3.Button {
                    text: i18n("🎵 Multimedia")
                    flat: root.currentTab !== 0
                    highlighted: root.currentTab === 0
                    onClicked: root.currentTab = 0
                }

                PlasmaComponents3.Button {
                    text: shelfModel.count > 0 ? i18n("📁 Bandeja (%1)", shelfModel.count) : i18n("📁 Bandeja")
                    flat: root.currentTab !== 1
                    highlighted: root.currentTab === 1
                    onClicked: root.currentTab = 1
                }

                Item { Layout.fillWidth: true }
            }

            ColumnLayout {
                id: multimediaTabCol
                Layout.fillWidth: true
                visible: root.currentTab === 0
                spacing: Kirigami.Units.mediumSpacing

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

            // 4. Live Activity Timer Card with Progress Bar & Pause/Resume
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
            } // Close multimediaTabCol

            // 6. Drop Shelf (Bandeja de Archivos integrada NotchNook style)
            ColumnLayout {
                id: dropShelfTabCol
                Layout.fillWidth: true
                visible: root.currentTab === 1
                spacing: Kirigami.Units.mediumSpacing

                // Shelf Container with DropArea
                Rectangle {
                    id: dropShelfBox
                    Layout.fillWidth: true
                    implicitHeight: Math.max(160, shelfListCol.implicitHeight + Kirigami.Units.mediumSpacing * 2)
                    Layout.preferredHeight: implicitHeight
                    radius: Kirigami.Units.smallSpacing
                    color: Kirigami.Theme.alternateBackgroundColor
                    border.width: shelfDropArea.containsDrag ? 2 : 1
                    border.color: shelfDropArea.containsDrag ? "#0a84ff" : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                    DropArea {
                        id: shelfDropArea
                        anchors.fill: parent
                        onEntered: (drag) => {
                            dropShelfBox.border.color = "#0a84ff";
                            dropShelfBox.border.width = 2;
                        }
                        onExited: {
                            dropShelfBox.border.color = Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15);
                            dropShelfBox.border.width = 1;
                        }
                        onDropped: (drop) => {
                            dropShelfBox.border.color = Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15);
                            dropShelfBox.border.width = 1;
                            const urls = root.extractUrlsFromDrop(drop);
                            for (let i = 0; i < urls.length; ++i) {
                                root.addFileToShelf(urls[i]);
                            }
                            drop.acceptProposedAction();
                        }
                    }

                    ColumnLayout {
                        id: shelfListCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Kirigami.Units.mediumSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Heading {
                                level: 5
                                text: i18n("Bandeja de Archivos (Drop Shelf)")
                            }

                            Item { Layout.fillWidth: true }

                            PlasmaComponents3.Button {
                                visible: shelfModel.count > 0
                                icon.name: "edit-clear-all"
                                text: i18n("Vaciar")
                                flat: true
                                onClicked: shelfModel.clear()
                            }
                        }

                        // Empty state
                        Rectangle {
                            visible: shelfModel.count === 0
                            Layout.fillWidth: true
                            implicitHeight: 110
                            radius: 8
                            color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.03)
                            border.width: 1
                            border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.12)

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                Kirigami.Icon {
                                    Layout.alignment: Qt.AlignHCenter
                                    implicitWidth: 32
                                    implicitHeight: 32
                                    source: "folder-stash-symbolic"
                                    opacity: 0.6
                                }

                                PlasmaComponents3.Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    font.weight: Font.Medium
                                    opacity: 0.8
                                    text: i18n("Arrastra y suelta archivos aquí")
                                }

                                PlasmaComponents3.Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    font: Kirigami.Theme.smallFont
                                    opacity: 0.5
                                    text: i18n("Retén archivos para moverlos entre aplicaciones o enviarlos por AirDrop / KDE Connect")
                                }
                            }
                        }

                        // File Cards List
                        Flow {
                            id: shelfFlow
                            Layout.fillWidth: true
                            visible: shelfModel.count > 0
                            spacing: Kirigami.Units.smallSpacing

                            Repeater {
                                model: shelfModel

                                Item {
                                    id: shelfItem
                                    required property int index
                                    required property string fileName
                                    required property string fileUrl
                                    required property string filePath
                                    required property string fileIcon

                                    width: Math.min(220, Math.floor((dropShelfBox.width - Kirigami.Units.mediumSpacing * 2 - Kirigami.Units.smallSpacing) / 2))
                                    height: 54

                                    Drag.active: fileMouse.drag.active
                                    Drag.source: shelfItem
                                    Drag.hotSpot.x: width / 2
                                    Drag.hotSpot.y: height / 2
                                    Drag.mimeData: {
                                        "text/uri-list": fileUrl
                                    }
                                    Drag.dragType: Drag.Automatic

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: 6
                                        color: fileMouse.containsMouse ? Kirigami.Theme.hoverColor : Kirigami.Theme.backgroundColor
                                        border.width: 1
                                        border.color: fileMouse.drag.active ? Kirigami.Theme.highlightColor : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.18)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 6

                                            Kirigami.Icon {
                                                implicitWidth: 26
                                                implicitHeight: 26
                                                source: fileIcon
                                                Layout.alignment: Qt.AlignVCenter
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.alignment: Qt.AlignVCenter
                                                spacing: 1

                                                PlasmaComponents3.Label {
                                                    Layout.fillWidth: true
                                                    text: fileName
                                                    font.weight: Font.Medium
                                                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                                                    elide: Text.ElideMiddle
                                                }

                                                PlasmaComponents3.Label {
                                                    text: i18n("Arrastra hacia afuera")
                                                    font.pixelSize: Math.max(9, Kirigami.Theme.smallFont.pixelSize - 2)
                                                    opacity: 0.6
                                                }
                                            }

                                            PlasmaComponents3.ToolButton {
                                                icon.name: "edit-delete-remove"
                                                Layout.preferredWidth: 22
                                                Layout.preferredHeight: 22
                                                onClicked: shelfModel.remove(index)
                                            }
                                        }

                                        MouseArea {
                                            id: fileMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                            drag.target: shelfItem
                                            drag.axis: Drag.XAndYAxis
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // AirDrop / KDE Connect Device Sharing Section
                Rectangle {
                    id: airdropCard
                    Layout.fillWidth: true
                    implicitHeight: airdropCol.implicitHeight + Kirigami.Units.mediumSpacing * 2
                    Layout.preferredHeight: implicitHeight
                    radius: Kirigami.Units.smallSpacing
                    color: Kirigami.Theme.alternateBackgroundColor
                    border.width: 1
                    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.15)

                    property var kdeDevices: []

                    Component.onCompleted: {
                        kdeDevices = Plasmoid.getKdeConnectDevices();
                    }

                    Timer {
                        interval: 4000
                        running: expandedIsland.selectedTab === 1
                        repeat: true
                        onTriggered: {
                            airdropCard.kdeDevices = Plasmoid.getKdeConnectDevices();
                        }
                    }

                    ColumnLayout {
                        id: airdropCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: Kirigami.Units.mediumSpacing
                        spacing: Kirigami.Units.smallSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                implicitWidth: 16
                                implicitHeight: 16
                                source: "kdeconnect"
                            }

                            Kirigami.Heading {
                                level: 5
                                text: i18n("AirDrop (KDE Connect)")
                            }

                            Item { Layout.fillWidth: true }

                            PlasmaComponents3.Label {
                                font: Kirigami.Theme.smallFont
                                opacity: 0.6
                                text: airdropCard.kdeDevices.length > 0 ? i18n("%1 dispositivo(s) accesible(s)", airdropCard.kdeDevices.length) : i18n("Sin dispositivos vinculados activos")
                            }
                        }

                        // Device buttons
                        RowLayout {
                            Layout.fillWidth: true
                            visible: airdropCard.kdeDevices.length > 0
                            spacing: Kirigami.Units.smallSpacing

                            Repeater {
                                model: airdropCard.kdeDevices

                                PlasmaComponents3.Button {
                                    required property var modelData
                                    icon.name: "smartphone"
                                    text: i18n("Enviar a %1", modelData.name)
                                    enabled: shelfModel.count > 0
                                    onClicked: {
                                        for (let i = 0; i < shelfModel.count; ++i) {
                                            const item = shelfModel.get(i);
                                            Plasmoid.shareToKdeConnect(item.fileUrl, modelData.id);
                                        }
                                        root.showHud("airdrop", i18n("Enviando a %1", modelData.name), i18n("%1 archivo(s)", shelfModel.count), "kdeconnect", 0);
                                    }
                                }
                            }
                        }

                        PlasmaComponents3.Label {
                            visible: airdropCard.kdeDevices.length === 0
                            Layout.fillWidth: true
                            font: Kirigami.Theme.smallFont
                            opacity: 0.7
                            wrapMode: Text.WordWrap
                            text: i18n("Vincula tu teléfono o tablet con KDE Connect para enviar archivos con un clic estilo AirDrop.")
                        }
                    }
                }
            }
        }
    }
}
