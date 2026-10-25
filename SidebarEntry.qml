import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    required property string itemText
    required property string glyph
    property bool active: false
    signal clicked()

    height: 48
    radius: 10
    color: active ? "#254775" : mouse.containsMouse ? "#20344d" : "transparent"
    border.width: active ? 1 : 0
    border.color: "#446ca0"

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 16
        spacing: 14

        Text {
            text: root.glyph
            color: root.active ? "#ffca5c" : "#aebdd0"
            font.pixelSize: 22
            width: 28
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            text: root.itemText
            color: "#edf5ff"
            font.pixelSize: 15
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
