// Effect settings. Wave Link opens the plugin's own editor window here: the Elgato EQ gets its graph
// editor, the other built-in effects have no GUI of their own and show their parameters as sliders.
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import WaveLink.Backend

Popup {
    id: editor
    property string inputId
    property string effectId

    readonly property var effect: { Engine.effectRevision; return effectId !== "" ? Engine.effects(inputId).find(e => e.id === effectId) : undefined }
    // A string, so value changes do not rebuild the parameter rows below.
    readonly property string effectType: effect ? effect.type : ""
    readonly property var typeInfo: Engine.effectTypes.find(t => t.type === effectType)
    readonly property var params: typeInfo ? typeInfo.params : []
    readonly property bool isEq: effectType === "ElgatoEQ"

    function format(p, v) {
        if (p.unit === "ms") return Math.round(v) + " ms"
        if (p.unit === "dB") return v.toFixed(1) + " dB"
        if (p.unit === "%") return Math.round(v) + "%"
        if (p.min >= 0 && p.max <= 1) return Math.round(v * 100) + "%"
        return v.toFixed(2) + (p.unit ? " " + p.unit : "")
    }

    onEffectChanged: if (visible && !effect) close()  // removed meanwhile

    parent: Overlay.overlay
    anchors.centerIn: parent
    modal: true
    focus: true
    width: isEq ? Math.min(920, parent.width - 80) : 430
    padding: 24
    Overlay.modal: Rectangle { color: Theme.contentDialogSmokeFill }
    background: Rectangle {
        radius: 8
        color: Theme.contentDialogBackground
        border.width: 1
        border.color: Theme.contentDialogBorderBrush
    }

    contentItem: ColumnLayout {
        spacing: 12
        WText {
            Layout.fillWidth: true
            Layout.bottomMargin: 8
            variant: "subtitle"
            text: editor.effect ? (editor.effect.name || (editor.typeInfo ? editor.typeInfo.name : editor.effectType)) : ""
        }
        EqEditor {
            visible: editor.isEq
            Layout.fillWidth: true
            inputId: editor.inputId
            effectId: editor.effectId
            values: editor.effect && editor.effect.params ? editor.effect.params : ({})
        }
        Repeater {
            model: editor.isEq ? [] : editor.params
            ColumnLayout {
                id: paramRow
                required property var modelData
                readonly property real current: {
                    const v = editor.effect && editor.effect.params ? editor.effect.params[modelData.id] : undefined
                    return v !== undefined ? v : modelData["default"]
                }
                Layout.fillWidth: true
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    WText {
                        Layout.fillWidth: true
                        variant: "body"
                        text: paramRow.modelData.name
                    }
                    WText {
                        variant: "body"
                        color: Theme.textFillColorSecondaryBrush
                        text: editor.format(paramRow.modelData, paramRow.current)
                    }
                }
                WSlider {
                    Layout.fillWidth: true
                    from: paramRow.modelData.min
                    to: paramRow.modelData.max
                    stepSize: paramRow.modelData.unit === "ms" ? 1 : 0
                    value: paramRow.current
                    onMoved: Engine.setEffectParam(editor.inputId, editor.effectId, paramRow.modelData.id, value)
                }
            }
        }
        WText {
            visible: !editor.isEq && editor.params.length === 0
            variant: "body"
            color: Theme.textFillColorSecondaryBrush
            text: "This effect has no settings."
        }
        WButton {
            Layout.alignment: Qt.AlignRight
            Layout.topMargin: 12
            accent: true
            text: Assets.text("MixingTable_CloseWindow_Button")
            onClicked: editor.close()
        }
    }
}
