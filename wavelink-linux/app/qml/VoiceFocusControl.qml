// ElgatoDeviceAudioEffectSliderControl for Voice Focus: a ToggleSwitchPropertyControl (switch, title,
// credit, info) and, while on, an expander row "Weak [slider] Strong" (typography-body-s labels,
// NormalizedSlider from SliderMinimumValue 0.7). Wave Link credits ai-coustics here; this port runs
// DeepFilterNet, so it credits that instead.
import QtQuick
import QtQuick.Layouts
import WaveLink.Backend

ColumnLayout {
    id: control
    property var effect                  // the input's VoiceFocus effect, if one was ever enabled
    property bool available: true        // DeepFilterNet plugin present
    readonly property bool isOn: effect !== undefined && effect !== null && !effect.bypass
    readonly property real enhancement: effect && effect.params && effect.params.Enhancement !== undefined
                                        ? effect.params.Enhancement : 0.9
    signal switched(bool value)
    signal enhancementMoved(real value)
    spacing: 0

    ToggleProperty {
        Layout.fillWidth: true
        Layout.preferredHeight: 48
        enabled: control.available
        title: Assets.text("ElgatoDevice_VoiceFocus_Title")
        tip: control.available ? Assets.text("ElgatoDevice_VoiceFocus_TeachingTip_Subtitle")
                               : "Install the DeepFilterNet LADSPA plugin to use Voice Focus."
        checked: control.isOn
        onToggled: (on) => control.switched(on)
        WText {
            variant: "caption"
            text: "DeepFilterNet"
            color: Theme.textFillColorSecondaryBrush
        }
    }
    RowLayout {
        visible: control.isOn
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        Layout.topMargin: -2
        Layout.bottomMargin: 8
        spacing: Theme.headerControlSpacing
        WText {
            variant: "caption"
            text: Assets.text("ElgatoDevice_AudioEffectValue_Weak")
        }
        WSlider {
            Layout.fillWidth: true
            from: 0.7
            to: 1.0
            value: control.enhancement
            onMoved: control.enhancementMoved(value)
        }
        WText {
            variant: "caption"
            text: Assets.text("ElgatoDevice_AudioEffectValue_Strong")
        }
    }
}
