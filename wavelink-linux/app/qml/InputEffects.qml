// ElgatoDeviceAudioEffects for the Wave:3 ("Hardware effects": Enhanced lowcut; "Software effects":
// Voice Focus and the channel's effects) or the plain effect list for any other channel. While the mic
// is in no mix (IsLegacyDeviceNotInMix), "Software effects" only asks to add it to one
// (QuickAddToMixBorder) and the effects and "Add effect" are hidden.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

ColumnLayout {
    id: fx
    property string inputId           // mixer channel, "" while the mic is not one
    property bool wave3: false
    property real sideMargin: Theme.contentPanelPadding
    spacing: 16

    readonly property var cfg: Engine.wave3 && Engine.wave3.config ? Engine.wave3.config : ({})
    readonly property var wave3Device: Engine.inputDevices.find(d => d.name.indexOf("Elgato_Wave_3") >= 0)
    readonly property bool inAnyMix: {
        Engine.cellRevision
        return inputId !== "" && Engine.mixes.some(m => Engine.cell(inputId, m.id).active === true)
    }

    ColumnLayout {  // Hardware effects
        visible: fx.wave3
        Layout.fillWidth: true
        Layout.leftMargin: fx.sideMargin
        Layout.rightMargin: fx.sideMargin
        spacing: 16
        SectionHeader { text: Assets.text("ElgatoDeviceProperties_Header_HardwareEffects") }
        ToggleProperty {  // in a PropertyListViewItem: 48 px row
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            title: Assets.text("ElgatoDevice_LowCutOnOff_Wave1_Wave3")
            tip: Assets.text("ElgatoDevice_LowCutOnOff_Description")
            checked: fx.cfg.lowcut_enable === true
            onToggled: (on) => Engine.setWave3("lowcut_enable", on)
        }
    }
    Separator { visible: fx.wave3 }

    ColumnLayout {  // Software effects
        Layout.fillWidth: true
        Layout.leftMargin: fx.sideMargin
        Layout.rightMargin: fx.sideMargin
        spacing: 16
        SectionHeader {
            visible: fx.wave3
            text: Assets.text("ElgatoDeviceProperties_Header_SoftwareEffects")
        }

        Rectangle {  // QuickAddToMixBorder (ContentBorder: 1 px, 4 px corners, 12 px padding)
            id: quickAdd
            visible: fx.wave3 && !fx.inAnyMix
            Layout.fillWidth: true
            implicitHeight: quickAddColumn.implicitHeight + 24
            radius: 4
            color: "transparent"
            border.width: 1
            border.color: Theme.cardStrokeColorDefaultBrush
            ColumnLayout {
                id: quickAddColumn
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                WText {
                    Layout.fillWidth: true
                    variant: "body"
                    wrapMode: Text.Wrap
                    text: Assets.text("ElgatoDevicePage_Effects_AddLegacyDeviceToMix_Header")
                }
                WText {
                    variant: "bodyStrong"
                    text: Assets.text("ElgatoDevicePage_Effects_AddLegacyDeviceToMix_AddToThis")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    WComboBox {
                        id: mixChoice
                        Layout.fillWidth: true
                        textRole: "name"
                        model: Engine.mixes
                    }
                    WButton {
                        text: Assets.text("ElgatoDevicePage_Effects_AddLegacyDeviceToMix_Go")
                        enabled: (fx.inputId !== "" || fx.wave3Device !== undefined) && mixChoice.currentIndex >= 0
                        onClicked: {  // AddDeviceToSelectedMix: add the channel if needed, activate that mix
                            let id = fx.inputId
                            if (id === "") {
                                id = "hw:" + fx.wave3Device.name
                                Engine.addHardwareInput(fx.wave3Device.name, "Elgato Wave:3")
                            }
                            Engine.setCellActive(id, Engine.mixes[mixChoice.currentIndex].id, true)
                        }
                    }
                }
            }
        }

        EffectsList {
            visible: !quickAdd.visible
            Layout.fillWidth: true
            inputId: fx.inputId
            showVoiceFocus: fx.wave3
        }
    }
}
