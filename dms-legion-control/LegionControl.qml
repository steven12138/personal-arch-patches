import QtQuick
import Quickshell
import Quickshell.Io
import qs.Common
import qs.Services
import qs.Widgets
import qs.Modules.Plugins

PluginComponent {
    id: root
    property var state: ({ available: false })
    property string errorText: ""

    function refresh() {
        if (!statusProcess.running)
            statusProcess.running = true;
    }
    function control(key, value) {
        Quickshell.execDetached(["pkexec", "legion-dmsctl", "set", key, String(value)]);
        refreshTimer.restart();
    }
    function textOrDash(value, suffix) { return value === null || value === undefined ? "—" : value + suffix; }

    Timer { id: refreshTimer; interval: 2000; running: true; repeat: true; onTriggered: root.refresh() }
    Process {
        id: statusProcess
        command: ["legion-dmsctl", "status"]
        stdout: StdioCollector {
            onStreamFinished: {
                try { root.state = JSON.parse(text); root.errorText = ""; }
                catch (error) { root.errorText = "Unable to parse Legion status"; }
            }
        }
        stderr: StdioCollector { onStreamFinished: { if (text.trim()) root.errorText = text.trim(); } }
    }
    Component.onCompleted: refresh()

    horizontalBarPill: Component {
        Row {
            spacing: Theme.spacingXS
            DankIcon { name: "sports_esports"; color: Theme.primary; anchors.verticalCenter: parent.verticalCenter }
            StyledText { text: textOrDash(root.state.temps?.cpuC, "°"); color: Theme.surfaceText; anchors.verticalCenter: parent.verticalCenter }
        }
    }
    verticalBarPill: Component {
        Column {
            spacing: Theme.spacingXS
            DankIcon { name: "sports_esports"; color: Theme.primary; anchors.horizontalCenter: parent.horizontalCenter }
            StyledText { text: textOrDash(root.state.temps?.cpuC, "°"); color: Theme.surfaceText; anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
    popoutWidth: 400
    popoutHeight: 600
    popoutContent: Component {
        PopoutComponent {
            id: popout
            headerText: "Legion Control"
            detailsText: root.state.available ? "Live sensor data · updates every 2 s" : "Legion kernel interface unavailable"
            showCloseButton: true
            Column {
                width: parent.width
                id: content
                spacing: Theme.spacingS
                    StyledText { visible: root.errorText.length > 0; text: root.errorText; color: Theme.error; wrapMode: Text.WordWrap; width: parent.width }
                    Grid {
                        columns: 3; spacing: Theme.spacingS
                        Repeater {
                            model: [
                                { label: "CPU", value: root.textOrDash(root.state.temps?.cpuC, " °C") },
                                { label: "GPU", value: root.textOrDash(root.state.temps?.gpuC, " °C") },
                                { label: "IC", value: root.textOrDash(root.state.temps?.icC, " °C") },
                                { label: "Fan 1", value: root.textOrDash(root.state.fans?.fan1Rpm, " RPM") },
                                { label: "Fan 2", value: root.textOrDash(root.state.fans?.fan2Rpm, " RPM") },
                                { label: "Battery", value: root.textOrDash(root.state.battery?.powerW, " W") }
                            ]
                            delegate: StyledRect {
                                width: (content.width - 2 * Theme.spacingS) / 3; height: 54; radius: Theme.cornerRadius; color: Theme.surfaceContainerHigh
                                Column {
                                    anchors.centerIn: parent
                                    StyledText {
                                        text: modelData.label
                                        color: Theme.surfaceVariantText
                                        font.pixelSize: Theme.fontSizeSmall - 1
                                    }
                                    StyledText {
                                        text: modelData.value
                                        color: Theme.surfaceText
                                        font.weight: Font.Medium
                                    }
                                }
                            }
                        }
                    }
                    StyledText { text: "Targets " + root.textOrDash(root.state.fans?.fan1TargetRpm, "") + " / " + root.textOrDash(root.state.fans?.fan2TargetRpm, " RPM"); color: Theme.surfaceVariantText; font.pixelSize: Theme.fontSizeSmall }
                    StyledText { text: "Performance"; font.weight: Font.Bold; color: Theme.surfaceText }
                    DankButtonGroup {
                        width: parent.width
                        maximumWidth: width
                        model: ["low-power", "balanced", "performance", "custom"]
                        currentIndex: model.indexOf(root.state.profile)
                        size: "small"
                        onSelectionChanged: (index, selected) => { if (selected) root.control("profile", model[index]); }
                    }
                    StyledText { visible: root.state.controls?.conservation !== null || root.state.controls?.rapidCharge !== null; text: "Charging"; font.weight: Font.Bold; color: Theme.surfaceText }
                    Row {
                        visible: root.state.controls?.conservation !== null || root.state.controls?.rapidCharge !== null
                        spacing: Theme.spacingS
                        DankButton { visible: root.state.controls?.conservation !== null; text: root.state.controls?.conservation ? "✓ Battery protection" : "Battery protection"; backgroundColor: root.state.controls?.conservation ? Theme.buttonBg : Theme.surfaceVariant; onClicked: root.control("conservation", root.state.controls?.conservation ? 0 : 1) }
                        DankButton { visible: root.state.controls?.rapidCharge !== null; text: root.state.controls?.rapidCharge ? "✓ Rapid charge" : "Rapid charge"; backgroundColor: root.state.controls?.rapidCharge ? Theme.buttonBg : Theme.surfaceVariant; onClicked: root.control("rapid-charge", root.state.controls?.rapidCharge ? 0 : 1) }
                    }
                    StyledText { text: "Lighting"; font.weight: Font.Bold; color: Theme.surfaceText }
                    Grid {
                        columns: 3
                        spacing: Theme.spacingS
                        DankButton { width: (content.width - 2 * Theme.spacingS) / 3; text: root.state.controls?.yLogo ? "✓ Y logo" : "Y logo"; backgroundColor: root.state.controls?.yLogo ? Theme.buttonBg : Theme.surfaceVariant; onClicked: root.control("y-logo", root.state.controls?.yLogo ? 0 : 1) }
                        DankButton { width: (content.width - 2 * Theme.spacingS) / 3; text: root.state.controls?.ioPort ? "✓ I/O" : "I/O"; backgroundColor: root.state.controls?.ioPort ? Theme.buttonBg : Theme.surfaceVariant; onClicked: root.control("io-port", root.state.controls?.ioPort ? 0 : 1) }
                        DankButton { width: (content.width - 2 * Theme.spacingS) / 3; text: root.state.controls?.keyboard ? "✓ Keyboard" : "Keyboard"; backgroundColor: root.state.controls?.keyboard ? Theme.buttonBg : Theme.surfaceVariant; onClicked: root.control("keyboard", ((root.state.controls?.keyboard || 0) + 1) % 3) }
                    }
                    StyledText { text: "GPU mode · reboot may be required"; font.weight: Font.Bold; color: Theme.surfaceText }
                    DankButtonGroup {
                        width: parent.width
                        maximumWidth: width
                        model: ["0", "1", "2", "3"]
                        currentIndex: root.state.controls?.igpuMode ?? -1
                        size: "small"
                        onSelectionChanged: (index, selected) => { if (selected) root.control("igpu-mode", index); }
                    }
            }
        }
    }
}
