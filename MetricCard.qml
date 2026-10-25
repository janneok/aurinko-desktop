import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    required property string heading
    required property string value
    required property string glyph
    property string detail: ""
    property color glyphColor: "#ffcb53"

    implicitHeight: 116
    radius: 12
    color: "#131e2d"
    border.color: "#34445b"
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Text {
                text: root.glyph
                color: root.glyphColor
                font.pixelSize: 20
                Layout.preferredWidth: 22
            }
            Text {
                text: root.heading
                color: "#becada"
                font.pixelSize: 12
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
        Text {
            text: root.value
            color: "#f4f8ff"
            font.weight: Font.DemiBold
            font.pixelSize: root.value.length > 11 ? 19 : 27
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
        Text {
            text: root.detail
            visible: root.detail !== ""
            color: "#96a9c1"
            font.pixelSize: 11
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
}
