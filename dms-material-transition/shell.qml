//@ pragma Env QT_WAYLAND_DISABLE_WINDOWDECORATION=1
//@ pragma AppId com.danklinux.dms-material-transition

import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland

ShellRoot {
    id: root

    property color backgroundColor: "#141218"
    property color primaryColor: "#d0bcff"
    property color secondaryColor: "#ccc2dc"
    property color accentContainerColor: "#4f378b"
    property real coverOpacity: 1
    property real bloomOpacity: 1
    property real bloomScale: 1
    property bool minimumElapsed: false
    property bool shellReady: false
    property bool dismissing: false

    function selectPalette(data) {
        const light = data?.colors?.light;
        const dark = data?.colors?.dark;
        if (!light || !dark)
            return;

        let useLight = false;
        try {
            const sessionText = sessionFile.text();
            useLight = JSON.parse(sessionText).isLightMode === true;
        } catch (error) {
            useLight = false;
        }

        const palette = useLight ? light : dark;
        backgroundColor = palette.background || backgroundColor;
        primaryColor = palette.primary || primaryColor;
        secondaryColor = palette.secondary || secondaryColor;
        accentContainerColor = palette.primary_container || accentContainerColor;
    }

    function tryDismiss() {
        if (dismissing || !minimumElapsed || !shellReady)
            return;
        dismissing = true;
        revealAnimation.start();
    }

    FileView {
        id: sessionFile
        path: Quickshell.env("HOME") + "/.local/state/DankMaterialShell/session.json"
        printErrors: false
    }

    FileView {
        path: Quickshell.env("HOME") + "/.cache/DankMaterialShell/dms-colors.json"
        printErrors: false
        onLoaded: {
            try {
                root.selectPalette(JSON.parse(text()));
            } catch (error) {
                console.warn("dms-material-transition: invalid DMS color cache", error);
            }
        }
    }

    Variants {
        model: Quickshell.screens

        PanelWindow {
            id: transitionWindow

            required property var modelData
            screen: modelData
            color: root.backgroundColor

            WlrLayershell.layer: WlrLayer.Overlay
            WlrLayershell.exclusionMode: ExclusionMode.Ignore
            WlrLayershell.namespace: "dms:material-startup"

            anchors.top: true
            anchors.bottom: true
            anchors.left: true
            anchors.right: true

            mask: Region {
                item: Item {}
            }

            Item {
                width: transitionWindow.width
                height: transitionWindow.height
                opacity: root.coverOpacity

                Item {
                    id: bloom

                    anchors.centerIn: parent
                    width: 180
                    height: 180
                    opacity: root.bloomOpacity
                    scale: root.bloomScale

                    Rectangle {
                        id: leftPetal
                        width: 62
                        height: 94
                        radius: 31
                        color: root.secondaryColor
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: -36
                        anchors.verticalCenterOffset: -12
                        rotation: -38
                        transformOrigin: Item.BottomRight

                        SequentialAnimation on scale {
                            loops: Animation.Infinite
                            NumberAnimation { to: 1.08; duration: 520; easing.type: Easing.OutCubic }
                            NumberAnimation { to: 0.94; duration: 680; easing.type: Easing.InOutCubic }
                            NumberAnimation { to: 1.0; duration: 420; easing.type: Easing.OutCubic }
                        }
                    }

                    Rectangle {
                        id: rightPetal
                        width: 62
                        height: 94
                        radius: 31
                        color: root.primaryColor
                        anchors.centerIn: parent
                        anchors.horizontalCenterOffset: 36
                        anchors.verticalCenterOffset: -12
                        rotation: 38
                        transformOrigin: Item.BottomLeft

                        SequentialAnimation on scale {
                            loops: Animation.Infinite
                            PauseAnimation { duration: 120 }
                            NumberAnimation { to: 0.94; duration: 540; easing.type: Easing.InOutCubic }
                            NumberAnimation { to: 1.09; duration: 640; easing.type: Easing.OutCubic }
                            NumberAnimation { to: 1.0; duration: 320; easing.type: Easing.OutCubic }
                        }
                    }

                    Rectangle {
                        id: seed
                        width: 76
                        height: 76
                        radius: 27
                        color: root.accentContainerColor
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: 29
                        rotation: 45

                        SequentialAnimation on rotation {
                            loops: Animation.Infinite
                            NumberAnimation { to: 51; duration: 700; easing.type: Easing.InOutCubic }
                            NumberAnimation { to: 39; duration: 700; easing.type: Easing.InOutCubic }
                            NumberAnimation { to: 45; duration: 360; easing.type: Easing.OutCubic }
                        }

                        Rectangle {
                            width: 22
                            height: 22
                            radius: 11
                            color: root.primaryColor
                            anchors.centerIn: parent
                        }
                    }

                    Rectangle {
                        id: orbitDot
                        width: 14
                        height: 14
                        radius: 7
                        color: root.primaryColor
                        x: 83
                        y: 5

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.25; duration: 520; easing.type: Easing.InOutCubic }
                            NumberAnimation { to: 1.0; duration: 520; easing.type: Easing.InOutCubic }
                        }

                        SequentialAnimation on y {
                            loops: Animation.Infinite
                            NumberAnimation { to: 0; duration: 520; easing.type: Easing.InOutCubic }
                            NumberAnimation { to: 9; duration: 520; easing.type: Easing.InOutCubic }
                        }
                    }
                }
            }
        }
    }

    Process {
        id: readinessProbe
        command: ["dms", "ipc", "call", "lock", "status"]
        onExited: function (exitCode, exitStatus) {
            if (exitCode === 0) {
                root.shellReady = true;
                probeTimer.stop();
                root.tryDismiss();
            }
        }
    }

    Timer {
        id: probeTimer
        interval: 120
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: {
            if (!readinessProbe.running)
                readinessProbe.running = true;
        }
    }

    Timer {
        interval: 1100
        running: true
        onTriggered: {
            root.minimumElapsed = true;
            root.tryDismiss();
        }
    }

    Timer {
        // The greeter no longer supplies a second exit animation.  Do not
        // leave the login handoff covered while optional DMS modules finish
        // initializing; the compositor is already ready at this point.
        interval: 900
        running: true
        onTriggered: {
            if (root.dismissing)
                return;
            root.shellReady = true;
            root.minimumElapsed = true;
            root.tryDismiss();
        }
    }

    ParallelAnimation {
        id: revealAnimation

        NumberAnimation {
            target: root
            property: "coverOpacity"
            to: 0
            duration: 560
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: root
            property: "bloomOpacity"
            to: 0
            duration: 360
            easing.type: Easing.OutCubic
        }

        NumberAnimation {
            target: root
            property: "bloomScale"
            to: 1.65
            duration: 560
            easing.type: Easing.OutCubic
        }

        onFinished: Qt.quit()
    }
}
