// NavigationPageElgatoDevice for the Wave:3 (Grid padding 16, column spacing 16): the device panel
// (image in a 5* row, name, Mute, stereo meter) and the 390 px properties panel with the "Input &
// monitoring" / "Effects" / "Settings" tabs above Sound Check. Property sections are 16 px from the
// panel edges and 16 px apart (ElgatoDevicePropertiesVerticalSpacing).
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

Item {
    id: page
    property int initialTab: 0
    readonly property var dev: Engine.wave3
    readonly property var cfg: dev && dev.config ? dev.config : ({})
    readonly property bool connected: dev && dev.connected === true

    // The Wave:3 as a mixer channel, if it is one: level meter, effects, Sound Check and its name.
    readonly property var micInput: Engine.inputs.find(i => i.kind === "hardware" && (i.device || "").indexOf("Elgato_Wave_3") >= 0)
    readonly property string deviceName: micInput ? micInput.name : "Elgato Wave:3"

    function setValue(key, value) { Engine.setWave3(key, value) }
    function text(key) { return Assets.text(key) }

    component Section: ColumnLayout {  // PropertiesStackPanelLrMargin
        Layout.fillWidth: true
        Layout.leftMargin: Theme.contentPanelPadding
        Layout.rightMargin: Theme.contentPanelPadding
        spacing: 16
    }
    component ValueRow: RowLayout {  // label left, value right
        property string label
        property string value
        Layout.fillWidth: true
        WText { variant: "body"; text: parent.label; Layout.fillWidth: true }
        WText { variant: "body"; text: parent.value }
    }

    WText {
        anchors.centerIn: parent
        visible: !page.connected
        variant: "subtitle"
        color: Theme.textFillColorSecondaryBrush
        text: "No Elgato Wave:3 connected"
    }

    RowLayout {
        visible: page.connected
        anchors.fill: parent
        anchors.margins: 16
        spacing: 16

        Card {  // ContentPanel (16 px padding), Grid with RowSpacing 16
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                id: deviceColumn
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16
                // WinUI star rows (*, 5*, *, *) split what the Auto rows and the six gaps leave; Qt's
                // stretch factors do not divide proportionally, so the share is computed here.
                readonly property real star: Math.max(0, (height - 6 * spacing - deviceName.implicitHeight - 16
                                                          - muteButton.implicitHeight - meter.implicitHeight) / 8)
                Item { Layout.preferredHeight: deviceColumn.star }
                Image {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 5 * deviceColumn.star
                    source: Assets.image("ElgatoWave3.png")
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                }
                Item { Layout.preferredHeight: deviceColumn.star }
                WText {  // typography-heading-m
                    id: deviceName
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: 16
                    variant: "subtitle"
                    text: page.deviceName
                }
                WButton {  // MuteButtonIconAndTextStyle
                    id: muteButton
                    Layout.fillWidth: true
                    Layout.maximumWidth: Theme.muteButtonMaxWidth
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    danger: page.cfg.input_mute === true
                    glyph: page.cfg.input_mute ? Icons.microphoneOffOutlinedM : Icons.microphoneOutlinedM
                    text: page.text(page.cfg.input_mute ? "ElgatoDevice_Unmute" : "ElgatoDevice_Mute")
                    onClicked: page.setValue("input_mute", !page.cfg.input_mute)
                }
                Column {  // LevelmeterStereoHorizontal, spacing 2
                    id: meter
                    Layout.fillWidth: true
                    Layout.maximumWidth: Theme.muteButtonMaxWidth
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    spacing: 2
                    Repeater {
                        model: 2
                        Rectangle {
                            required property int index
                            width: parent.width
                            height: 4
                            radius: 2
                            color: Theme.levelmeterBackgroundBrush
                            LevelMeter {
                                anchors.fill: parent
                                peak: {
                                    Engine.meterRevision
                                    if (!page.micInput || page.cfg.input_mute) return 0
                                    const p = Engine.inputPeak(page.micInput.id)
                                    return parent.index === 0 ? p.x : p.y
                                }
                            }
                        }
                    }
                }
                Item { Layout.preferredHeight: deviceColumn.star }
            }
        }

        Card {  // ContentPanelBase, bottom padding 16
            Layout.preferredWidth: 390
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                anchors.fill: parent
                anchors.bottomMargin: Theme.contentPanelPadding
                spacing: 0
                TabStrip {
                    id: tabs
                    Layout.fillWidth: true
                    current: page.initialTab
                    tabs: [page.text("ElgatoDevicePage_TabViewHeader_InputMonitoring"),
                           page.text("ElgatoDevicePage_TabViewHeader_Effects"),
                           page.text("ElgatoDevicePage_TabViewHeader_Settings")]
                }

                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: Theme.contentPanelPadding + 4  // matches where Wave Link's first header sits
                    contentHeight: content.implicitHeight + Theme.contentPanelPadding
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}

                    ColumnLayout {
                        id: content
                        width: parent.width
                        spacing: 16

                        // ---- Input & monitoring ----
                        Section {
                            visible: tabs.current === 0
                            SectionHeader { text: page.text("ElgatoDeviceProperties_Header_Input") }
                            PropertySlider {
                                Layout.fillWidth: true
                                glyph: Icons.microphoneOutlinedM
                                title: page.text("ElgatoDeviceProperties_InputGain")
                                from: 0
                                to: 40
                                stepSize: 0.5
                                value: page.cfg.input_gain || 0
                                valueText: (page.cfg.input_gain || 0).toFixed(page.cfg.input_gain % 1 ? 1 : 0) + " dB"
                                onMoved: (v) => page.setValue("input_gain", v)
                            }
                            ToggleProperty {
                                Layout.fillWidth: true
                                title: page.text("ElgatoDevice_GainLock_Title")
                                tip: page.text("ElgatoDevice_GainLock_Description")
                                checked: page.cfg.gain_lock === true
                                onToggled: (on) => page.setValue("gain_lock", on)
                            }
                            ToggleProperty {
                                Layout.fillWidth: true
                                title: page.text("ElgatoDevice_Clipguard_Title")
                                tip: page.text("ElgatoDevice_Clipguard_Description")
                                checked: page.cfg.clipguard_enable === true
                                onToggled: (on) => page.setValue("clipguard_enable", on)
                            }
                        }
                        Separator {
                            visible: tabs.current === 0
                            Layout.topMargin: -4  // the Clipguard expander's -24 margin nets out 4 px tighter
                        }
                        Section {
                            visible: tabs.current === 0
                            SectionHeader { text: page.text("ElgatoDeviceProperties_Header_Monitoring") }
                            PropertySlider {  // ElgatoDevicePropertyVolumeControl
                                id: headphones
                                Layout.fillWidth: true
                                glyph: Icons.headphonesOutlinedM
                                title: page.text("ElgatoDeviceProperties_OutputVolume")
                                from: 0
                                to: 100
                                stepSize: 1
                                // Linear in dB over the device's -60..0 dB range.
                                value: ((page.cfg.headphone_volume ?? -60) + 60) / 60 * 100
                                valueText: Math.round(headphones.value) + "%"
                                onMoved: (v) => page.setValue("headphone_volume", Math.round((v / 100 * 60 - 60) * 2) / 2)
                                MuteToggle {
                                    variant: "redBorder"
                                    volume: headphones.value / 100
                                    checked: page.cfg.headphone_mute === true
                                    onToggled: page.setValue("headphone_mute", checked)
                                }
                            }
                            ColumnLayout {  // SelfMonitoringControl: Mic/PC mix with mic and PC icons below
                                Layout.fillWidth: true
                                spacing: Theme.headerControlSpacing
                                PropertySlider {
                                    id: mix
                                    Layout.fillWidth: true
                                    title: page.text("ElgatoDevice_MicPcMix")
                                    from: 0
                                    to: 100
                                    stepSize: 5
                                    // Legacy devices show the mix inverted (ConvertDirectMonitorVolume).
                                    value: 100 - (page.cfg.direct_monitor ?? 0)
                                    valueText: Math.round(mix.value) + "%"
                                    onMoved: (v) => page.setValue("direct_monitor", 100 - v)
                                }
                                Item {
                                    Layout.fillWidth: true
                                    Layout.rightMargin: 44 + Theme.horizontalTextItemSpacing
                                    implicitHeight: 20
                                    FontIcon { anchors.left: parent.left; glyph: Icons.microphoneOutlinedM }
                                    FontIcon { anchors.right: parent.right; glyph: Icons.displayOutlinedM }
                                }
                            }
                        }

                        // ---- Effects ----
                        InputEffects {
                            visible: tabs.current === 1
                            Layout.fillWidth: true
                            wave3: true
                            inputId: page.micInput ? page.micInput.id : ""
                        }

                        // ---- Settings ----
                        Section {
                            visible: tabs.current === 2
                            SectionHeader { text: page.text("ElgatoDeviceProperties_Header_General") }
                            RowLayout {  // ElgatoDeviceGeneralSectionControl: two equal columns
                                Layout.fillWidth: true
                                spacing: 0
                                WText {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    variant: "body"
                                    text: page.text("ElgatoDevice_Name")
                                }
                                WTextField {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    enabled: page.micInput !== undefined
                                    text: page.deviceName
                                    onEditingFinished: {
                                        const name = text.trim()
                                        if (page.micInput && name !== "" && name !== page.micInput.name)
                                            Engine.renameInput(page.micInput.id, name)
                                    }
                                }
                            }
                            ValueRow { label: page.text("Elgato.BaseClasses.WinUI/Resources/Settings_Device_SerialNumber"); value: page.dev.serial || "" }
                            ValueRow { label: page.text("ElgatoDevice_FirmwareVersion"); value: page.dev.firmware || "" }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                WText {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    variant: "body"
                                    text: page.text("ElgatoDevice_LEDDirection")
                                }
                                WComboBox {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    model: [page.text("LedDirection_LeftToRight"), page.text("LedDirection_RightToLeft")]
                                    currentIndex: page.cfg.leds_flip ? 1 : 0
                                    onActivated: (i) => page.setValue("leds_flip", i === 1)
                                }
                            }
                        }
                    }
                }

                Separator {}
                SoundCheck {
                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.contentPanelPadding
                    Layout.rightMargin: Theme.contentPanelPadding
                    Layout.topMargin: 16
                    inputId: page.micInput ? page.micInput.id : ""
                }
            }
        }
    }
}
