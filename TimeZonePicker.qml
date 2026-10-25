import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Hakukentällinen, täysin offline-toimiva aikavyöhykevalitsin.
// timeZones tulee C++:n QTimeZone::availableTimeZoneIds()-rajapinnasta.
Item {
    id: picker
    implicitWidth: 330
    implicitHeight: 42

    property var timeZones: []
    property string selectedZone: "Europe/Helsinki"
    signal zoneSelected(string id)

    readonly property var filteredZones: {
        const query = zoneSearch.text.trim().toLowerCase()
        const values = []
        for (let i = 0; i < picker.timeZones.length; ++i) {
            const name = String(picker.timeZones[i])
            if (!query || name.toLowerCase().includes(query))
                values.push(name)
        }
        return values
    }

    function choose(zone) {
        picker.selectedZone = zone
        dropdown.close()
        picker.zoneSelected(zone)
    }

    function showPicker() {
        zoneSearch.text = ""
        zoneList.currentIndex = 0
        // Ylälaidassa sijaitsevan ScrollView'n leikkaus ei leikkaa popupia.
        const overlay = Overlay.overlay
        dropdown.width = Math.min(Math.max(picker.width, 380), overlay.width - 24)
        dropdown.height = Math.min(355, overlay.height - 24)
        const where = picker.mapToItem(overlay, 0, 0)
        dropdown.x = Math.max(12, Math.min(where.x, overlay.width - dropdown.width - 12))
        const roomBelow = overlay.height - where.y - picker.height
        const roomAbove = where.y
        if (roomBelow < dropdown.height + 8 && roomAbove >= dropdown.height + 8)
            dropdown.y = where.y - dropdown.height - 4
        else
            dropdown.y = Math.max(12, Math.min(where.y + picker.height + 4,
                                                overlay.height - dropdown.height - 12))
        dropdown.open()
    }

    Rectangle {
        anchors.fill: parent
        radius: 7
        color: "#182b43"
        border.color: dropdown.visible ? "#ffcc66" : "#465a74"
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: picker.selectedZone || "Valitse aikavyöhyke"
                color: "#f0f6ff"
                elide: Text.ElideRight
                font.pixelSize: 15
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                text: "⌄"
                font.pixelSize: 22
                color: "#aebed0"
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: dropdown.visible ? dropdown.close() : picker.showPicker()
        }
    }

    Popup {
        id: dropdown
        parent: Overlay.overlay
        padding: 9
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onOpened: zoneSearch.forceActiveFocus()
        background: Rectangle {
            radius: 9
            color: "#15243a"
            border.color: "#465a74"
        }
        contentItem: ColumnLayout {
            spacing: 7
            TextField {
                id: zoneSearch
                Layout.fillWidth: true
                placeholderText: "Hae esim. hels, madrid, oslo..."
                selectByMouse: true
                color: "#f0f6ff"
                font.pixelSize: 14
                onTextChanged: zoneList.currentIndex = 0
                Keys.onDownPressed: zoneList.forceActiveFocus()
                Keys.onReturnPressed: {
                    if (picker.filteredZones.length > 0)
                        picker.choose(picker.filteredZones[0])
                }
                background: Rectangle {
                    radius: 6
                    color: "#1c3049"
                    border.color: "#5e7794"
                }
            }
            Text {
                Layout.fillWidth: true
                text: picker.filteredZones.length === 0
                      ? "Aikavyöhykkeitä ei löytynyt" :
                        picker.filteredZones.length + " aikavyöhykettä"
                font.pixelSize: 12
                color: "#aebed0"
            }
            ListView {
                id: zoneList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 1
                model: picker.filteredZones
                currentIndex: 0
                keyNavigationWraps: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical: ScrollIndicator { }
                Keys.onReturnPressed: {
                    if (currentIndex >= 0 && currentIndex < picker.filteredZones.length)
                        picker.choose(picker.filteredZones[currentIndex])
                }
                Keys.onSpacePressed: {
                    if (currentIndex >= 0 && currentIndex < picker.filteredZones.length)
                        picker.choose(picker.filteredZones[currentIndex])
                }
                delegate: ItemDelegate {
                    required property int index
                    required property string modelData
                    width: zoneList.width - 4
                    height: 34
                    text: modelData
                    onClicked: picker.choose(modelData)
                    contentItem: Text {
                        text: modelData
                        color: "#eff5fe"
                        font.pixelSize: 14
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                    background: Rectangle {
                        radius: 5
                        color: parent.hovered || zoneList.currentIndex === index
                               ? "#29466b" : "#15243a"
                    }
                }
            }
        }
    }
}
