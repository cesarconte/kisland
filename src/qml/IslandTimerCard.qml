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

Rectangle {
    id: timerCardRoot

    Layout.fillWidth: true
    implicitHeight: timerCol.implicitHeight + CachyTheme.spacingMedium * 2
    Layout.preferredHeight: implicitHeight
    radius: CachyTheme.radiusSmall
    color: CachyPalette.cardBackground
    border.width: 1
    border.color: Plasmoid.timerActive ? CachyTheme.accentColor : CachyPalette.specularBorder

    ColumnLayout {
        id: timerCol
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: CachyTheme.spacingMedium
        spacing: CachyTheme.spacingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: CachyTheme.spacingSmall

            PlasmaComponents3.Label {
                Layout.fillWidth: true
                font.bold: true
                text: Plasmoid.timerActive
                    ? i18n("⏱  %1: %2", Plasmoid.timerLabel, Plasmoid.timerRemainingText)
                    : i18n("⏱  Temporizadores Rápidos (Live Activity)")
            }

            CachyButton {
                visible: Plasmoid.timerActive
                iconName: Plasmoid.timerPaused ? "media-playback-start-symbolic" : "media-playback-pause-symbolic"
                text: Plasmoid.timerPaused ? i18n("Reanudar") : i18n("Pausar")
                onClicked: Plasmoid.togglePauseTimer()
            }

            CachyButton {
                visible: Plasmoid.timerActive
                iconName: "process-stop-symbolic"
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
            spacing: CachyTheme.spacingSmall

            Repeater {
                model: [
                    { secs: 60, label: "1 min" },
                    { secs: 300, label: "5 min" },
                    { secs: 600, label: "10 min" },
                    { secs: 1500, label: "🍅 Pomodoro (25m)" }
                ]
                CachyButton {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.label
                    font: Kirigami.Theme.smallFont
                    onClicked: Plasmoid.startQuickTimer(modelData.secs, modelData.label)
                }
            }
        }

        // Custom time spinboxes (Hours, Minutes, Seconds)
        RowLayout {
            Layout.fillWidth: true
            visible: !Plasmoid.timerActive
            spacing: CachyTheme.spacingSmall

            PlasmaComponents3.SpinBox {
                id: customHoursSpin
                Layout.fillWidth: true
                from: 0
                to: 23
                value: 0
                editable: true
                textFromValue: function(value) { return value + " h"; }
                valueFromText: function(text) { return parseInt(text) || 0; }
            }

            PlasmaComponents3.SpinBox {
                id: customMinsSpin
                Layout.fillWidth: true
                from: 0
                to: 59
                value: 15
                editable: true
                textFromValue: function(value) { return value + " min"; }
                valueFromText: function(text) { return parseInt(text) || 0; }
            }

            PlasmaComponents3.SpinBox {
                id: customSecsSpin
                Layout.fillWidth: true
                from: 0
                to: 59
                value: 0
                editable: true
                textFromValue: function(value) { return value + " s"; }
                valueFromText: function(text) { return parseInt(text) || 0; }
            }
        }

        // Label row and Start button
        RowLayout {
            Layout.fillWidth: true
            visible: !Plasmoid.timerActive
            spacing: CachyTheme.spacingSmall

            PlasmaComponents3.TextField {
                id: customLabelField
                Layout.fillWidth: true
                placeholderText: i18n("Etiqueta (opcional)")
                clearButtonShown: true
                onAccepted: startCustomTimerBtn.clicked()
            }

            CachyButton {
                id: startCustomTimerBtn
                iconName: "media-playback-start-symbolic"
                text: i18n("Iniciar")
                primary: true
                enabled: (customHoursSpin.value * 3600 + customMinsSpin.value * 60 + customSecsSpin.value) > 0
                onClicked: {
                    const totalSecs = customHoursSpin.value * 3600 + customMinsSpin.value * 60 + customSecsSpin.value;
                    if (totalSecs > 0) {
                        let lbl = customLabelField.text.trim();
                        if (lbl.length === 0) {
                            let parts = [];
                            if (customHoursSpin.value > 0) parts.push(customHoursSpin.value + "h");
                            if (customMinsSpin.value > 0) parts.push(customMinsSpin.value + "m");
                            if (customSecsSpin.value > 0) parts.push(customSecsSpin.value + "s");
                            lbl = parts.join(" ");
                        }
                        Plasmoid.startQuickTimer(totalSecs, lbl);
                    }
                }
            }
        }
    }
}
