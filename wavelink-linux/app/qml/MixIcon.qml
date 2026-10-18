// OutputMixIcon: 32 px white tile, 1 px border, 4 px corners, glyph in the channel-icon foreground.
// Default icon per mix id follows MixingTableOutputMixViewModel (Personal: headphones, slot 2: stream,
// slot 3: group, slots 4/5: numbers).
import QtQuick

Rectangle {
    id: tile
    property int slot: 0

    readonly property var slotGlyphs: [Icons.headphonesOutlinedL, Icons.streamOutlinedM, "",
                                       Icons.number4OutlinedL, Icons.number5OutlinedL]

    width: Theme.mixingTableMixIconSize
    height: width
    radius: 4
    color: Theme.mixingTableMixIconBackgroundColor
    border.width: 1
    border.color: Theme.mixingTableElementBorderBrush

    FontIcon {
        anchors.centerIn: parent
        glyph: tile.slotGlyphs[Math.max(0, Math.min(4, tile.slot))]
        font.pixelSize: 24
        width: 24
        height: 24
        color: Theme.mixingTableChannelMixIconForegroundColor
    }
}
