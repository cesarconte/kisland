/*
    SPDX-FileCopyrightText: 2026 Cesar
    SPDX-License-Identifier: GPL-3.0-or-later
*/

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts

import CachyOS.UI
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

    // Transient Dynamic Island HUD state
    property string pillHudType: "" // "volume", "peripheral", "focus", "airdrop", "timer_done"
    property string hudTitle: ""
    property string hudSubtitle: ""
    property string hudIcon: ""
    property int hudValue: 0

    Timer {
        id: pillHudTimer
        interval: root.pillHudType === "timer_done" ? 6500 : (root.pillHudType === "volume" ? 1800 : 2800)
        repeat: false
        onTriggered: {
            if (root.pillHudType === "timer_done") {
                Plasmoid.dismissTimer();
            }
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

    function stopAutoCollapse() {
        autoCollapseTimer.stop();
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
            if (!Plasmoid.timerRinging && root.pillHudType === "timer_done") {
                root.pillHudType = "";
            }
        }
        function onTimerFinished(label, totalSecs) {
            root.showHud("timer_done", i18n("¡Se acabó el tiempo!"), label, "chronometer", 100);
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

    // 1. Compact Representation (Dynamic Island Pill)
    compactRepresentation: IslandCompactPill {
        rootPlasmoid: root
    }

    // 2. Full Representation (Expanded Island Popup)
    fullRepresentation: Item {
        id: expandedIsland

        readonly property int targetWidth: Math.max(464, mainColumn.implicitWidth + CachyTheme.spacingLarge * 2)
        readonly property int targetHeight: mainColumn.implicitHeight + CachyTheme.spacingLarge * 2

        implicitWidth: targetWidth
        implicitHeight: targetHeight
        width: targetWidth
        height: targetHeight
        Layout.minimumWidth: targetWidth
        Layout.maximumWidth: targetWidth
        Layout.preferredWidth: targetWidth
        Layout.minimumHeight: targetHeight
        Layout.maximumHeight: targetHeight
        Layout.preferredHeight: targetHeight

        ColumnLayout {
            id: mainColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: CachyTheme.spacingLarge
            spacing: CachyTheme.spacingMedium

            // Modular Subcomponent 1: Header + Vitals + Focus + Quick Launchers + Tabs
            IslandExpandedHeader {
                rootPlasmoid: root
                shelfModel: shelfModel
            }

            // Tab 0: Multimedia Cards
            ColumnLayout {
                id: multimediaTabCol
                Layout.fillWidth: true
                visible: root.currentTab === 0
                spacing: CachyTheme.spacingMedium

                // Modular Subcomponent 2: Media Player Card
                IslandMediaCard {
                    rootPlasmoid: root
                }

                // Modular Subcomponent 3: Shazam Music Recognition Card
                IslandMusicRecognitionCard {
                }

                // Modular Subcomponent 4: Live Activity Timer Card
                IslandTimerCard {
                }
            }

            // Tab 1: Drop Shelf & KDE Connect Section
            // text: i18n("KDE Connect")
            IslandDropShelfCard {
                visible: root.currentTab === 1
                rootPlasmoid: root
                shelfModel: shelfModel
            }
        }
    }
}
