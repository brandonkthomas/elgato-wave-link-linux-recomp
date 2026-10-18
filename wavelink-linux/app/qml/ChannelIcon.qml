// InputChannelIcon (size S): 40 px tile, 8 px corners, 28 px glyph in MixingTableChannelMixIconForegroundColor
// on the icon's color; larger sizes scale corners and glyph alike. Defaults follow
// IconDefinitions.AppInputIcons / DeviceInputIcons.
import QtQuick

Rectangle {
    id: tile
    property string name
    property string kind: "software"
    property real size: Theme.mixingTableChannelIconSize

    readonly property var defs: ({
        "System":  { glyph: Icons.displayOutlinedM, color: "#34BEFE" },
        "Music":   { glyph: Icons.musicOutlinedM, color: "#FF60D0" },
        "Browser": { glyph: Icons.windowOutlinedM, color: "#B260FF" },
        "Game":    { glyph: Icons.gameOutlinedM, color: "#FF3C4E" },
        "SFX":     { glyph: Icons.soundEffectOutlinedM, color: "#FB7900" },
        "Voice":   { glyph: Icons.messageOutlinedM, color: "#FBDB00" },
        "Voice Chat": { glyph: Icons.messageOutlinedM, color: "#FBDB00" },
        "Aux 1":   { glyph: Icons.auxiliaryOutlinedM, color: "#47F3E8" },
        "Aux 2":   { glyph: Icons.auxiliaryOutlinedM, color: "#6FF787" }
    })
    readonly property var def: kind === "hardware" ? { glyph: Icons.waveMicOutlinedL, color: "#8CA3FE" }
                                                   : (defs[name] || { glyph: Icons.displayOutlinedM, color: "#34BEFE" })

    width: size
    height: width
    radius: size / 5
    color: def.color

    FontIcon {
        anchors.centerIn: parent
        glyph: tile.def.glyph
        font.pixelSize: tile.size * 0.7
        width: font.pixelSize
        height: font.pixelSize
        color: Theme.mixingTableChannelMixIconForegroundColor
    }
}
