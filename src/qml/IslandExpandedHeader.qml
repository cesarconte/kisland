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
import org.kde.plasma.plasmoid

ColumnLayout {
    id: headerRoot

    required property var rootPlasmoid
    required property var shelfModel

    Layout.fillWidth: true
    spacing: CachyTheme.spacingMedium

    // Header Row: Vitals + Focus chip + Privacy Pill + Quick Launchers
    RowLayout {
        Layout.fillWidth: true
        spacing: CachyTheme.spacingSmall

        CachyHeading {
            level: 4
            text: i18n("KIsland")
        }

        Rectangle {
            radius: CachyTheme.radiusSmall
            color: CachyPalette.cardBackground
            border.width: 1
            border.color: CachyPalette.specularBorder
            implicitWidth: vitalsLbl.implicitWidth + 12
            implicitHeight: vitalsLbl.implicitHeight + 6
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight

            PlasmaComponents3.Label {
                id: vitalsLbl
                anchors.centerIn: parent
                font.pixelSize: 11
                color: CachyPalette.text
                opacity: 0.85
                text: "CPU " + Plasmoid.cpuUsagePercent + "% · RAM " + Plasmoid.ramUsagePercent + "%"
            }
        }

        // Focus Mode / No Molestar Chip
        Rectangle {
            id: focusChipRect
            radius: CachyTheme.radiusSmall
            color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? Qt.rgba(1, 0.6, 0, 0.18) : CachyPalette.cardBackground
            border.width: 1
            border.color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? CachyTheme.warningColor : CachyPalette.specularBorder
            implicitWidth: focusChipRow.implicitWidth + 14
            implicitHeight: vitalsLbl.implicitHeight + 6
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight

            RowLayout {
                id: focusChipRow
                anchors.centerIn: parent
                spacing: 4

                CachyIcon {
                    size: 13
                    name: Plasmoid.focusModeIcon.length > 0 ? Plasmoid.focusModeIcon : (Plasmoid.focusDndActive ? "notifications-disabled" : "preferences-desktop-notification-bell")
                    color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? CachyTheme.warningColor : CachyPalette.text
                }

                PlasmaComponents3.Label {
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    color: (Plasmoid.focusDndActive || Plasmoid.focusActive) ? CachyTheme.warningColor : CachyPalette.text
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
            radius: CachyTheme.radiusSmall
            color: Qt.rgba(CachyPalette.textSecondary.r, CachyPalette.textSecondary.g, CachyPalette.textSecondary.b, 0.15)
            border.width: 1
            border.color: CachyPalette.textSecondary
            implicitWidth: privLbl.implicitWidth + 12
            implicitHeight: privLbl.implicitHeight + 6
            Layout.preferredWidth: implicitWidth
            Layout.preferredHeight: implicitHeight

            PlasmaComponents3.Label {
                id: privLbl
                anchors.centerIn: parent
                font.pixelSize: 11
                color: CachyPalette.textSecondary
                text: {
                    const parts = [];
                    if (Plasmoid.micActive) parts.push("🎙️ " + Plasmoid.micApp);
                    if (Plasmoid.cameraOrScreenActive) parts.push("📹 " + Plasmoid.cameraOrScreenApp);
                    return parts.join(" · ");
                }
            }
        }

        Item { Layout.fillWidth: true }

        CachyToolbarButton {
            iconName: rootPlasmoid.pinned ? "window-pin" : "window-unpin"
            checkable: true
            checked: rootPlasmoid.pinned
            onToggled: (next) => { rootPlasmoid.pinned = next }
            tooltipText: rootPlasmoid.pinned ? i18n("Desfijar") : i18n("Fijar")
        }

        CachyToolbarButton {
            visible: Plasmoid.configuration.showQuickLaunchers !== false
            iconName: "folder-stash-symbolic"
            onClicked: Plasmoid.launchDropover()
            tooltipText: i18n("Dropover")
        }

        CachyToolbarButton {
            visible: Plasmoid.configuration.showQuickLaunchers !== false
            iconName: "view-list-text-symbolic"
            onClicked: Plasmoid.launchLiveCaptions()
            tooltipText: i18n("Subtítulos en vivo")
        }

        CachyToolbarButton {
            visible: Plasmoid.configuration.showQuickLaunchers !== false
            iconName: "kity-symbolic"
            onClicked: Plasmoid.invokeKity()
            tooltipText: i18n("Kity")
        }
    }

    // Tabs Switcher
    CachySegmentedControl {
        id: tabSegment
        Layout.fillWidth: true
        model: [
            i18n("Multimedia"),
            headerRoot.shelfModel.count > 0 ? i18n("Bandeja (%1)", headerRoot.shelfModel.count) : i18n("Bandeja")
        ]
        currentIndex: rootPlasmoid.currentTab
        onCurrentIndexChanged: {
            if (rootPlasmoid.currentTab !== currentIndex) {
                rootPlasmoid.currentTab = currentIndex;
            }
        }
        Connections {
            target: rootPlasmoid
            function onCurrentTabChanged() {
                tabSegment.currentIndex = rootPlasmoid.currentTab;
            }
        }
    }
}
