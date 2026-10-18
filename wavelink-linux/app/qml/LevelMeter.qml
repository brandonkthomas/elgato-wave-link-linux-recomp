// Elgato Levelmeter (BaseClasses Levelmeter.cs + DefaultLevelmeterHorizontalStyle): a gradient
// green -> yellow (from 85 %) -> red (from 92.5 %) revealed up to the level, a 2 px peak-hold line
// held for 1 s then falling 2 units/s, and 150 ms level animation. Values are normalized over
// -60..0 dBFS (AudioMixUtility.MapLinearToNormalized(peak, -60)).
import QtQuick

Item {
    id: meter
    property real peak: 0           // linear sample peak since the previous frame
    property bool vertical: false
    property bool peakVisible: true

    readonly property real level: peak <= 1e-6 ? 0 : Math.max(0, Math.min(1, (20 * Math.log10(peak) + 60) / 60))
    property real shown: 0
    property real held: 0
    property double heldAt: 0

    onLevelChanged: {
        shown = level
        const now = Date.now()
        if (level >= held) {
            held = level
            heldAt = now
        }
    }
    Behavior on shown { NumberAnimation { duration: 150 } }

    Timer {  // PeakHoldCalculator: 1 s hold, then 2.0 per second decay
        interval: 16
        running: meter.peakVisible && meter.held > 0
        repeat: true
        onTriggered: {
            const elapsed = (Date.now() - meter.heldAt) / 1000
            if (elapsed > 1) meter.held = Math.max(meter.level, meter.held - 2 * interval / 1000)
        }
    }

    Rectangle {  // gradient spans the whole meter; the clip reveals it up to the level
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: parent.width * meter.shown
        clip: true
        color: "transparent"
        Rectangle {
            width: meter.width
            height: meter.height
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: Theme.colorGreen60 }
                GradientStop { position: 0.85; color: Theme.colorGreen60 }
                GradientStop { position: 0.8501; color: Theme.colorYellow30 }
                GradientStop { position: 0.925; color: Theme.colorYellow30 }
                GradientStop { position: 0.9251; color: Theme.colorRed70 }
                GradientStop { position: 1.0; color: Theme.colorRed70 }
            }
        }
    }
    Rectangle {
        visible: meter.peakVisible && meter.held > 0.001
        x: Math.max(0, Math.min(meter.width - width, meter.width * meter.held - width / 2))
        width: 2
        height: parent.height
        color: Theme.controlStrongStrokeColorDefault
    }
}
