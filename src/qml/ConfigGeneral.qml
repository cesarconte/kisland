import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.SimpleKCM {
    id: root

    // Config aliases for Plasma automatic persistence
    property alias cfg_autoExpand: autoExpandCheck.checked
    property alias cfg_autoCollapseDuration: autoCollapseDurationSpin.value
    property alias cfg_hideWhenIdle: hideWhenIdleCheck.checked
    property alias cfg_showVitalsInCompact: showVitalsCheck.checked
    property alias cfg_showWaveform: showWaveformCheck.checked
    property alias cfg_showArtInCompact: showArtCheck.checked
    property alias cfg_showPrivacyDots: showPrivacyDotsCheck.checked
    property alias cfg_notifyOnRecognized: notifyOnRecognizedCheck.checked
    property alias cfg_showQuickLaunchers: showQuickLaunchersCheck.checked

    Kirigami.FormLayout {
        // --- SECCIÓN: COMPORTAMIENTO ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Comportamiento de la Dynamic Island")
        }

        QQC2.CheckBox {
            id: autoExpandCheck
            Kirigami.FormData.label: i18n("Live Activities:")
            text: i18n("Auto-expandir al cambiar de pista o iniciar temporizador")
        }

        QQC2.SpinBox {
            id: autoCollapseDurationSpin
            Kirigami.FormData.label: i18n("Tiempo visible:")
            from: 2
            to: 15
            stepSize: 1
            textFromValue: function(value) {
                return i18n("%1 segundos", value);
            }
            valueFromText: function(text) {
                return parseInt(text);
            }
            enabled: autoExpandCheck.checked
        }

        QQC2.CheckBox {
            id: hideWhenIdleCheck
            Kirigami.FormData.label: i18n("Visibilidad:")
            text: i18n("Ocultar la píldora en el panel cuando no haya actividad (estilo macOS)")
        }

        // --- SECCIÓN: ELEMENTOS DE LA PÍLDORA ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Elementos de la Píldora Compacta")
        }

        QQC2.CheckBox {
            id: showArtCheck
            Kirigami.FormData.label: i18n("Multimedia:")
            text: i18n("Mostrar carátula del álbum o miniatura de vídeo")
        }

        QQC2.CheckBox {
            id: showWaveformCheck
            text: i18n("Mostrar onda de audio dinámica (PipeWire)")
        }

        QQC2.CheckBox {
            id: showVitalsCheck
            Kirigami.FormData.label: i18n("Telemetría:")
            text: i18n("Mostrar uso de CPU y RAM cuando esté en reposo")
            enabled: !hideWhenIdleCheck.checked
        }

        QQC2.CheckBox {
            id: showPrivacyDotsCheck
            Kirigami.FormData.label: i18n("Privacidad:")
            text: i18n("Mostrar indicadores de micrófono (ámbar) y cámara/pantalla (verde)")
        }

        // --- SECCIÓN: RECONOCIMIENTO SHAZAM ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Reconocimiento de Música (Shazam)")
        }

        QQC2.CheckBox {
            id: notifyOnRecognizedCheck
            Kirigami.FormData.label: i18n("Notificaciones:")
            text: i18n("Enviar notificación al sistema al identificar una canción")
        }

        // --- SECCIÓN: ISLA EXPANDIDA ---
        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Isla Expandida")
        }

        QQC2.CheckBox {
            id: showQuickLaunchersCheck
            Kirigami.FormData.label: i18n("Accesos directos:")
            text: i18n("Mostrar botones rápidos de utilidades (Dropover, Subtítulos en vivo)")
        }
    }
}
