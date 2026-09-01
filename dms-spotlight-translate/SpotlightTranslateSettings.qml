import QtQuick
import qs.Common
import qs.Widgets
import qs.Modules.Plugins

PluginSettings {
    id: root
    pluginId: "spotlightTranslate"

    StyledText {
        width: parent.width
        text: I18n.tr("Spotlight Translate Settings")
        font.pixelSize: Theme.fontSizeLarge
        font.weight: Font.Bold
        color: Theme.surfaceText
    }

    StyledRect {
        width: parent.width
        height: settingsColumn.implicitHeight + Theme.spacingL * 2
        radius: Theme.cornerRadius
        color: Theme.surfaceContainerHigh

        Column {
            id: settingsColumn
            anchors.fill: parent
            anchors.margins: Theme.spacingL
            spacing: Theme.spacingM

            StringSetting {
                settingKey: "trigger"
                label: I18n.tr("Trigger Prefix")
                description: I18n.tr("Type this prefix to start translating")
                placeholder: ">"
                defaultValue: ">"
            }

            StyledText {
                width: parent.width
                text: I18n.tr("Chinese input is translated to English. Other input is translated to Simplified Chinese. Use 'en text', 'zh text', or 'ja: text' to override the target language.")
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.surfaceVariantText
                wrapMode: Text.WordWrap
            }
        }
    }
}
