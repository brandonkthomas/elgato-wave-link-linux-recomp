// Elgato EQ editor, after the plugin's WaveDraggerTable: ±12 dB over 20 Hz–20 kHz on a log axis
// (x = (log10 f - log10 20) / 3 · width), the plugin's shaded regions (Rumble / Sub-bass … Air), the
// summed response and one handle per visible band. Drag a handle to set frequency and gain, scroll on
// it to change Q, double-click it to bypass it, double-click the graph to add a band, Delete removes
// the selected one. The controls below edit the selected band precisely. The curve uses the engine's
// filter formulas (JUCE's), so it shows what is heard.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

ColumnLayout {
    id: eq
    property string inputId
    property string effectId
    property var values: ({})  // the effect's params, id -> value
    property int selected: -1
    spacing: 12

    readonly property int bands: 8
    readonly property var typeNames: ["High-pass", "Low shelf", "Peak", "High shelf", "Low-pass"]
    readonly property var bandColors: ["#34BEFE", "#FF60D0", "#B260FF", "#FF3C4E", "#FB7900", "#E5C500", "#2FD3C8", "#4CC864"]
    readonly property var regions: [  // ElgatoEQBackground.svg
        { from: 20, to: 80, label: "Rumble / Sub-bass", shaded: true },
        { from: 80, to: 300, label: "Boom / Warmth", shaded: false },
        { from: 300, to: 1400, label: "Boxy", shaded: true },
        { from: 1400, to: 5000, label: "Nasal", shaded: false },
        { from: 5000, to: 12000, label: "Presence", shaded: true },
        { from: 12000, to: 20000, label: "Air", shaded: false }
    ]
    readonly property var freqLabels: [[20, "20 Hz"], [80, "80 Hz"], [300, "300 Hz"], [1400, "1.4 kHz"],
                                       [5000, "5 kHz"], [12000, "12 kHz"], [20000, "20 kHz"]]

    function key(band, field) { return "Band " + (band + 1) + " Filter " + field }
    function val(band, field) { const x = values[key(band, field)]; return x === undefined ? 0 : x }
    function set(band, field, value) { Engine.setEffectParam(inputId, effectId, key(band, field), value) }
    function hasGain(band) { const t = Math.round(val(band, "Type")); return t === 1 || t === 2 || t === 3 }
    function visibleBand(band) { return val(band, "Visible") >= 0.5 }
    function db(gain) { return 20 * Math.log10(Math.max(gain, 1e-6)) }

    // JUCE IIR::Coefficients factories, as in the engine (normalised by a0).
    function coefficients(type, fs, f, q, gain) {
        f = Math.min(Math.max(f, 2), fs * 0.499)
        q = Math.max(q, 0.01)
        let b0, b1, b2, a0, a1, a2
        if (type === 0 || type === 4) {
            const n = type === 0 ? Math.tan(Math.PI * f / fs) : 1 / Math.tan(Math.PI * f / fs)
            const n2 = n * n, invQ = 1 / q, c1 = 1 / (1 + invQ * n + n2)
            b0 = c1; b1 = type === 0 ? -2 * c1 : 2 * c1; b2 = c1
            a0 = 1; a1 = type === 0 ? c1 * 2 * (n2 - 1) : c1 * 2 * (1 - n2); a2 = c1 * (1 - invQ * n + n2)
        } else if (type === 2) {
            const A = Math.sqrt(Math.max(gain, 1e-15)), w = 2 * Math.PI * f / fs
            const alpha = Math.sin(w) / (2 * q), c2 = -2 * Math.cos(w)
            b0 = 1 + alpha * A; b1 = c2; b2 = 1 - alpha * A
            a0 = 1 + alpha / A; a1 = c2; a2 = 1 - alpha / A
        } else {
            const A = Math.sqrt(Math.max(gain, 1e-15)), am1 = A - 1, ap1 = A + 1
            const w = 2 * Math.PI * f / fs, cw = Math.cos(w), beta = Math.sin(w) * Math.sqrt(A) / q, amc = am1 * cw
            if (type === 1) {
                b0 = A * (ap1 - amc + beta); b1 = A * 2 * (am1 - ap1 * cw); b2 = A * (ap1 - amc - beta)
                a0 = ap1 + amc + beta; a1 = -2 * (am1 + ap1 * cw); a2 = ap1 + amc - beta
            } else {
                b0 = A * (ap1 + amc + beta); b1 = A * -2 * (am1 + ap1 * cw); b2 = A * (ap1 + amc - beta)
                a0 = ap1 - amc + beta; a1 = 2 * (am1 - ap1 * cw); a2 = ap1 - amc - beta
            }
        }
        return [b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0]
    }
    function magnitudeDb(c, f, fs) {
        const w = 2 * Math.PI * f / fs, cw = Math.cos(w), c2w = Math.cos(2 * w)
        const num = c[0] * c[0] + c[1] * c[1] + c[2] * c[2] + 2 * (c[0] * c[1] + c[1] * c[2]) * cw + 2 * c[0] * c[2] * c2w
        const den = 1 + c[3] * c[3] + c[4] * c[4] + 2 * (c[3] + c[3] * c[4]) * cw + 2 * c[4] * c2w
        return 10 * Math.log10(Math.max(num / den, 1e-12))
    }

    function addBand(freq, gainDb) {
        for (let b = 0; b < bands; b++) {
            if (visibleBand(b)) continue
            set(b, "Type", 2)
            set(b, "Frequency", freq)
            set(b, "Quality", 0.707)
            set(b, "Gain", Math.pow(10, gainDb / 20))
            set(b, "Bypass", 0)
            set(b, "Visible", 1)
            selected = b
            return
        }
    }
    function deleteBand(b) {  // hidden bands still run in the plugin: leave this one neutral
        set(b, "Visible", 0)
        set(b, "Bypass", 1)
        set(b, "Gain", 1)
        if (selected === b) selected = -1
    }

    Keys.onDeletePressed: if (selected >= 0) deleteBand(selected)
    focus: true

    Rectangle {  // the plugin's white graph panel
        id: panel
        Layout.fillWidth: true
        Layout.preferredHeight: 360
        radius: 8
        color: "#ffffff"
        border.width: 1
        border.color: Theme.cardStrokeColorDefaultBrush

        Item {
            id: graph
            x: 52
            y: 16
            width: panel.width - x - 24
            height: panel.height - y - 52

            function xOf(f) { return (Math.log10(f) - Math.log10(20)) / 3 * width }
            function fOf(x) { return 20 * Math.pow(10, 3 * Math.min(Math.max(x / width, 0), 1)) }
            function yOf(d) { return height / 2 - d / 12 * (height / 2) }
            function dOf(y) { return Math.min(Math.max((height / 2 - y) / (height / 2) * 12, -12), 12) }

            Repeater {  // shaded regions and their names
                model: eq.regions
                Item {
                    required property var modelData
                    x: graph.xOf(modelData.from)
                    width: graph.xOf(modelData.to) - x
                    height: graph.height
                    Rectangle {
                        anchors.fill: parent
                        visible: parent.modelData.shaded
                        color: "#f5f5f5"
                        radius: 4
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: graph.height + 8
                        text: parent.modelData.label
                        font.family: Style.textFont
                        font.pixelSize: 11
                        color: "#646464"
                    }
                }
            }
            Repeater {  // gain grid and labels: 12dB, 9 … -12
                model: 9
                Item {
                    required property int index
                    readonly property real gain: 12 - index * 3
                    y: graph.yOf(gain)
                    width: graph.width
                    Rectangle {
                        width: parent.width
                        height: 1
                        color: parent.gain === 0 ? "#d0d0d0" : "#ececec"
                    }
                    Text {
                        x: -width - 10
                        y: -height / 2
                        text: parent.gain === 12 ? "12dB" : String(parent.gain)
                        font.family: Style.textFont
                        font.pixelSize: 11
                        color: "#646464"
                    }
                }
            }
            Repeater {
                model: eq.freqLabels
                Text {
                    required property var modelData
                    x: Math.min(graph.xOf(modelData[0]) - width / 2, graph.width - width)
                    y: graph.height + 26
                    text: modelData[1]
                    font.family: Style.textFont
                    font.pixelSize: 11
                    color: "#646464"
                }
            }

            Canvas {  // summed response
                id: curve
                anchors.fill: parent
                property var values: eq.values
                onValuesChanged: requestPaint()
                onWidthChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const fs = 48000
                    const filters = []
                    for (let b = 0; b < eq.bands; b++) {
                        if (eq.val(b, "Bypass") >= 0.5) continue
                        filters.push(eq.coefficients(Math.round(eq.val(b, "Type")), fs, eq.val(b, "Frequency"),
                                                     eq.val(b, "Quality"), eq.val(b, "Gain")))
                    }
                    ctx.beginPath()
                    for (let x = 0; x <= width; x += 2) {
                        const f = graph.fOf(x)
                        let d = 0
                        for (const c of filters) d += eq.magnitudeDb(c, f, fs)
                        const y = Math.min(Math.max(graph.yOf(d), -4), height + 4)
                        if (x === 0) ctx.moveTo(x, y)
                        else ctx.lineTo(x, y)
                    }
                    ctx.lineWidth = 2.5
                    ctx.strokeStyle = "#1e1e1e"
                    ctx.stroke()
                }
            }

            MouseArea {  // double-click on the graph adds a band there
                anchors.fill: parent
                onDoubleClicked: (mouse) => eq.addBand(graph.fOf(mouse.x), graph.dOf(mouse.y))
                onClicked: eq.selected = -1
            }

            Repeater {  // band handles
                model: eq.bands
                Rectangle {
                    id: handle
                    required property int index
                    readonly property bool bypassed: eq.val(index, "Bypass") >= 0.5
                    visible: eq.visibleBand(index)
                    width: eq.selected === index ? 20 : 16
                    height: width
                    radius: width / 2
                    x: graph.xOf(eq.val(index, "Frequency")) - width / 2
                    y: graph.yOf(eq.hasGain(index) ? eq.db(eq.val(index, "Gain")) : 0) - height / 2
                    color: bypassed ? "#bdbdbd" : eq.bandColors[index]
                    border.width: eq.selected === index ? 3 : 2
                    border.color: "#ffffff"
                    Text {
                        anchors.centerIn: parent
                        text: handle.index + 1
                        font.family: Style.textFont
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        color: "#ffffff"
                    }
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        cursorShape: Qt.SizeAllCursor
                        onPressed: { eq.selected = handle.index; eq.forceActiveFocus() }
                        onPositionChanged: (mouse) => {
                            if (!pressed) return
                            const p = mapToItem(graph, mouse.x, mouse.y)
                            eq.set(handle.index, "Frequency", Math.round(graph.fOf(p.x) * 10) / 10)
                            if (eq.hasGain(handle.index))
                                eq.set(handle.index, "Gain", Math.pow(10, graph.dOf(p.y) / 20))
                        }
                        onDoubleClicked: eq.set(handle.index, "Bypass", handle.bypassed ? 0 : 1)
                        onWheel: (wheel) => {
                            const q = eq.val(handle.index, "Quality") * Math.pow(1.1, wheel.angleDelta.y / 120)
                            eq.set(handle.index, "Quality", Math.min(Math.max(q, 0.1), 10))
                        }
                    }
                }
            }
        }
    }

    RowLayout {  // selected band
        Layout.fillWidth: true
        spacing: 16
        visible: eq.selected >= 0
        WComboBox {
            Layout.preferredWidth: 140
            model: eq.typeNames
            currentIndex: eq.selected >= 0 ? Math.round(eq.val(eq.selected, "Type")) : 0
            onActivated: (i) => eq.set(eq.selected, "Type", i)
        }
        PropertySlider {
            Layout.fillWidth: true
            title: "Frequency"
            from: 0
            to: 1
            value: eq.selected >= 0 ? (Math.log10(eq.val(eq.selected, "Frequency")) - Math.log10(20)) / 3 : 0
            valueText: {
                const f = eq.selected >= 0 ? eq.val(eq.selected, "Frequency") : 0
                return f >= 1000 ? (f / 1000).toFixed(f >= 10000 ? 1 : 2) + " kHz" : Math.round(f) + " Hz"
            }
            onMoved: (v) => eq.set(eq.selected, "Frequency", Math.round(20 * Math.pow(10, 3 * v) * 10) / 10)
        }
        PropertySlider {
            Layout.fillWidth: true
            enabled: eq.selected >= 0 && eq.hasGain(eq.selected)
            title: "Gain"
            from: -12
            to: 12
            value: eq.selected >= 0 ? eq.db(eq.val(eq.selected, "Gain")) : 0
            valueText: (value > 0 ? "+" : "") + value.toFixed(1) + " dB"
            onMoved: (v) => eq.set(eq.selected, "Gain", Math.pow(10, v / 20))
        }
        PropertySlider {
            Layout.fillWidth: true
            title: "Q"
            from: Math.log10(0.1)
            to: 1
            value: eq.selected >= 0 ? Math.log10(eq.val(eq.selected, "Quality")) : 0
            valueText: Math.pow(10, value).toFixed(2)
            onMoved: (v) => eq.set(eq.selected, "Quality", Math.round(Math.pow(10, v) * 100) / 100)
        }
        WToggleSwitch {
            label: "On"
            checked: eq.selected >= 0 && eq.val(eq.selected, "Bypass") < 0.5
            onToggled: eq.set(eq.selected, "Bypass", checked ? 0 : 1)
        }
        QuietButton {
            glyph: Icons.trashOutlinedM
            tip: "Delete band"
            onClicked: eq.deleteBand(eq.selected)
        }
    }
    WText {
        visible: eq.selected < 0
        Layout.fillWidth: true
        variant: "caption"
        color: Theme.textFillColorSecondaryBrush
        wrapMode: Text.Wrap
        text: "Drag a point to change a band, scroll on it to change its width (Q), double-click it to turn it off. Double-click the graph to add a band; Delete removes the selected one."
    }
}
