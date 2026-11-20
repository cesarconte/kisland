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

ColumnLayout {
    id: dropShelfRoot

    required property var rootPlasmoid
    required property var shelfModel

    Layout.fillWidth: true
    spacing: CachyTheme.spacingMedium

    // Shelf Container with DropArea
    Rectangle {
        id: dropShelfBox
        Layout.fillWidth: true
        implicitHeight: Math.max(160, shelfListCol.implicitHeight + CachyTheme.spacingMedium * 2)
        Layout.preferredHeight: implicitHeight
        radius: CachyTheme.radiusSmall
        color: CachyPalette.cardBackground
        border.width: shelfDropArea.containsDrag ? 2 : 1
        border.color: shelfDropArea.containsDrag ? CachyTheme.accentColor : CachyPalette.specularBorder

        DropArea {
            id: shelfDropArea
            anchors.fill: parent
            onEntered: (drag) => {
                dropShelfBox.border.color = CachyTheme.accentColor;
                dropShelfBox.border.width = 2;
            }
            onExited: {
                dropShelfBox.border.color = CachyPalette.specularBorder;
                dropShelfBox.border.width = 1;
            }
            onDropped: (drop) => {
                dropShelfBox.border.color = CachyPalette.specularBorder;
                dropShelfBox.border.width = 1;
                const urls = dropShelfRoot.rootPlasmoid.extractUrlsFromDrop(drop);
                for (let i = 0; i < urls.length; ++i) {
                    dropShelfRoot.rootPlasmoid.addFileToShelf(urls[i]);
                }
                drop.acceptProposedAction();
            }
        }

        ColumnLayout {
            id: shelfListCol
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: CachyTheme.spacingMedium
            spacing: CachyTheme.spacingSmall

            RowLayout {
                Layout.fillWidth: true
                spacing: CachyTheme.spacingSmall

                CachyHeading {
                    level: 5
                    text: i18n("Bandeja de Archivos (Drop Shelf)")
                }

                Item { Layout.fillWidth: true }

                CachyButton {
                    visible: dropShelfRoot.shelfModel.count > 0
                    iconName: "edit-clear-all"
                    text: i18n("Vaciar")
                    flat: true
                    onClicked: dropShelfRoot.shelfModel.clear()
                }
            }

            // Empty state
            Rectangle {
                visible: dropShelfRoot.shelfModel.count === 0
                Layout.fillWidth: true
                implicitHeight: 110
                radius: CachyTheme.radiusSmall
                color: Qt.rgba(CachyPalette.text.r, CachyPalette.text.g, CachyPalette.text.b, 0.03)
                border.width: 1
                border.color: Qt.rgba(CachyPalette.text.r, CachyPalette.text.g, CachyPalette.text.b, 0.12)

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    CachyIcon {
                        Layout.alignment: Qt.AlignHCenter
                        size: 32
                        name: "folder-stash-symbolic"
                        color: CachyPalette.textSecondary
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
                        font.pixelSize: 11
                        opacity: 0.5
                        text: i18n("Retén archivos para moverlos entre aplicaciones o enviarlos con KDE Connect")
                    }
                }
            }

            // File Cards List
            Flow {
                id: shelfFlow
                Layout.fillWidth: true
                visible: dropShelfRoot.shelfModel.count > 0
                spacing: CachyTheme.spacingSmall

                Repeater {
                    model: dropShelfRoot.shelfModel

                    Item {
                        id: shelfItem
                        required property int index
                        required property string fileName
                        required property string fileUrl
                        required property string filePath
                        required property string fileIcon

                        width: Math.min(220, Math.floor((dropShelfBox.width - CachyTheme.spacingMedium * 2 - CachyTheme.spacingSmall) / 2))
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
                            radius: CachyTheme.radiusSmall
                            color: fileMouse.containsMouse ? CachyPalette.cardBackgroundHover : CachyPalette.windowBackground
                            border.width: 1
                            border.color: fileMouse.drag.active ? CachyTheme.accentColor : CachyPalette.specularBorder

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6

                                CachyIcon {
                                    size: 26
                                    name: fileIcon
                                    Layout.alignment: Qt.AlignVCenter
                                    colored: false
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 1

                                    PlasmaComponents3.Label {
                                        Layout.fillWidth: true
                                        text: fileName
                                        font.weight: Font.Medium
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }

                                    PlasmaComponents3.Label {
                                        text: i18n("Arrastra hacia afuera")
                                        font.pixelSize: 9
                                        opacity: 0.6
                                    }
                                }

                                CachyToolbarButton {
                                    iconName: "document-open-folder"
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    onClicked: Plasmoid.openFileLocation(shelfItem.filePath)
                                    tooltipText: i18n("Mostrar en gestor de archivos")
                                }

                                CachyToolbarButton {
                                    iconName: "edit-delete-remove"
                                    Layout.preferredWidth: 22
                                    Layout.preferredHeight: 22
                                    onClicked: dropShelfRoot.shelfModel.remove(index)
                                    tooltipText: i18n("Quitar de la bandeja")
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

    // KDE Connect Device Sharing Section (native KDE Plasma integration)
    Rectangle {
        id: airdropCard
        Layout.fillWidth: true
        implicitHeight: airdropCol.implicitHeight + CachyTheme.spacingMedium * 2
        Layout.preferredHeight: implicitHeight
        radius: CachyTheme.radiusSmall
        color: CachyPalette.cardBackground
        border.width: 1
        border.color: CachyPalette.specularBorder

        property var kdeDevices: []

        Component.onCompleted: {
            kdeDevices = Plasmoid.getKdeConnectDevices();
        }

        Timer {
            interval: 4000
            running: dropShelfRoot.rootPlasmoid.currentTab === 1
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
            anchors.margins: CachyTheme.spacingMedium
            spacing: CachyTheme.spacingSmall

            RowLayout {
                Layout.fillWidth: true
                spacing: CachyTheme.spacingSmall

                CachyIcon {
                    size: 16
                    name: "kdeconnect"
                    colored: false
                }

                CachyHeading {
                    level: 5
                    text: i18n("KDE Connect")
                }

                Item { Layout.fillWidth: true }

                PlasmaComponents3.Label {
                    font.pixelSize: 11
                    opacity: 0.6
                    text: airdropCard.kdeDevices.length > 0
                        ? (airdropCard.kdeDevices.length === 1
                            ? i18n("1 dispositivo disponible")
                            : i18n("%1 dispositivos disponibles", airdropCard.kdeDevices.length))
                        : i18n("Sin dispositivos disponibles")
                }

                CachyToolbarButton {
                    iconName: "settings-configure"
                    implicitWidth: 24
                    implicitHeight: 24
                    onClicked: Plasmoid.openKdeConnectSettings()
                    tooltipText: i18n("Configurar KDE Connect")
                }
            }

            // Device buttons
            RowLayout {
                Layout.fillWidth: true
                visible: airdropCard.kdeDevices.length > 0
                spacing: CachyTheme.spacingSmall

                Repeater {
                    model: airdropCard.kdeDevices

                    CachyButton {
                        required property var modelData
                        iconName: "smartphone"
                        text: i18n("Enviar a %1", modelData.name)
                        enabled: dropShelfRoot.shelfModel.count > 0
                        onClicked: {
                            for (let i = 0; i < dropShelfRoot.shelfModel.count; ++i) {
                                const item = dropShelfRoot.shelfModel.get(i);
                                Plasmoid.shareToKdeConnect(item.fileUrl, modelData.id);
                            }
                            dropShelfRoot.rootPlasmoid.showHud("airdrop", i18n("Enviando a %1", modelData.name), i18n("%1 archivo(s)", dropShelfRoot.shelfModel.count), "kdeconnect", 0);
                        }
                    }
                }
            }

            PlasmaComponents3.Label {
                visible: airdropCard.kdeDevices.length === 0
                Layout.fillWidth: true
                font.pixelSize: 11
                opacity: 0.7
                wrapMode: Text.WordWrap
                text: i18n("Vincula tu teléfono o tableta con KDE Connect para transferir archivos al instante con un solo clic.")
            }
        }
    }
}
