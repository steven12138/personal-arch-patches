import QtQuick
import qs.Common
import qs.Widgets
import qs.Modules.Plugins

PluginSettings {
    id: root
    pluginId: "legionControl"

    StyledRect {
        width: parent.width
        implicitHeight: prerequisites.implicitHeight + Theme.spacingL * 2
        radius: Theme.cornerRadius
        color: Theme.surfaceContainerHigh

        Column {
            id: prerequisites
            anchors.fill: parent
            anchors.margins: Theme.spacingL
            spacing: Theme.spacingS

            Row {
                spacing: Theme.spacingS
                DankIcon { name: "info"; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
                StyledText { text: "Prerequisites"; font.pixelSize: Theme.fontSizeLarge; font.weight: Font.Medium; color: Theme.surfaceText }
            }
            StyledText {
                width: parent.width
                wrapMode: Text.WordWrap
                color: Theme.surfaceVariantText
                text: "Requires the Lenovo Legion kernel module (legion_laptop) and legion-dmsctl. Add Legion Control to the DankBar to view live data and open controls. Firmware changes use Polkit authentication."
            }
        }
    }
}
