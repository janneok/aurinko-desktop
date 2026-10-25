import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "CalendarLogic.js" as CalendarLogic

ApplicationWindow {
    id: root
    width: 1300
    height: 890
    minimumWidth: 1080
    minimumHeight: 720
    visible: true
    title: "Aurinko"
    color: "#08111e"

    // Aika- ja paikkatieto C++-laskimelta. Kaikki päiväkohtaiset arvot
    // tulevat paikallisesti; Aurinkosivun palvelinta ei kutsuta.
    property string todayIso: solar.today()
    property string selectedIso: todayIso
    property int displayedYear: Number(todayIso.slice(0, 4))
    property int displayedMonth: Number(todayIso.slice(5, 7))
    property int currentPage: 0 // 0 tänään, 1 huomenna, 2 kuukausi, 3 asetukset
    // Live-arvo päivittyy 5 sekunnin välein. Kalenterit ja kiertokäyrä
    // lasketaan uudelleen vain sijainnin tai päivän vaihtuessa.
    property int liveTick: 0
    property int dataRevision: 0
    property bool followSelectedDate: true
    property bool followDisplayedMonth: true
    property int locationsRevision: 0
    readonly property var locationItems: {
        const revision = locationsRevision
        return solar.locations()
    }
    readonly property var customPlaces: {
        const result = []
        for (let i = 0; i < locationItems.length; ++i)
            if (locationItems[i].custom) result.push(locationItems[i])
        return result
    }
    property string locationMessage: ""
    property bool locationMessageError: false
    readonly property var timeZoneItems: solar.availableTimeZones()

    readonly property var selectedDay: {
        const updated = dataRevision
        return solar.day(selectedIso)
    }
    readonly property var shownDays: {
        const updated = dataRevision
        return currentPage === 2
               ? solar.month(displayedYear, displayedMonth)
               : solar.daysAround(selectedIso)
    }
    readonly property var sunOrbit: {
        const updated = dataRevision
        return solar.sunOrbit(selectedIso)
    }
    readonly property var liveData: {
        const tick = liveTick // käytetään myös aurinkopisteen ajan päivitykseen
        const revision = dataRevision
        return solar.live(selectedIso)
    }

    function navigate(index) {
        currentPage = index
        if (index === 0) {
            followSelectedDate = true
            selectedIso = todayIso
        } else if (index === 1) {
            followSelectedDate = true
            selectedIso = CalendarLogic.addDays(todayIso, 1)
        } else if (index === 2) {
            // Kuukausi-painike palauttaa nykyiseen kalenterikuukauteen.
            // Vasta kuukausinuolet siirtävät selauksen pois automaattitilasta.
            followDisplayedMonth = true
            displayedYear = Number(todayIso.slice(0, 4))
            displayedMonth = Number(todayIso.slice(5, 7))
        }
    }

    function shiftMonth(delta) {
        let m = displayedMonth + delta
        let y = displayedYear
        if (m < 1) { m = 12; y-- }
        if (m > 12) { m = 1; y++ }
        if (y < 1 || y > 9999) return
        followDisplayedMonth = false
        displayedYear = y
        displayedMonth = m
        currentPage = 2
    }

    function pollClock() {
        // QTimeZone tuntee myös kesä-/talviaikaan siirtymisen ja eri
        // sijaintien paikallisen päivämäärän. Timer korjaa tilan myös
        // lepotilasta paluun jälkeen, kun seuraava tikki suoritetaan.
        const changes = CalendarLogic.rollover({
            todayIso: todayIso,
            selectedIso: selectedIso,
            displayedYear: displayedYear,
            displayedMonth: displayedMonth,
            currentPage: currentPage,
            followSelectedDate: followSelectedDate,
            followDisplayedMonth: followDisplayedMonth
        }, solar.today())
        if (changes) {
            todayIso = changes.todayIso
            if (changes.selectedIso !== undefined)
                selectedIso = changes.selectedIso
            if (changes.displayedYear !== undefined) {
                displayedYear = changes.displayedYear
                displayedMonth = changes.displayedMonth
            }
            dataRevision++
        }
        liveTick++
    }

    Connections {
        target: solar
        function onLocationsChanged() {
            root.locationsRevision++
        }
        function onLocationChanged() {
            newTimeZone.selectedZone = solar.timeZoneName
            root.todayIso = solar.today()
            root.selectedIso = root.todayIso
            root.displayedYear = Number(root.todayIso.slice(0, 4))
            root.displayedMonth = Number(root.todayIso.slice(5, 7))
            root.followSelectedDate = true
            root.followDisplayedMonth = true
            root.currentPage = 0
            root.dataRevision++
            root.liveTick++
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true
        onTriggered: root.pollClock()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: "#101b2c"
            border.color: "#20324b"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 9

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 9
                    Layout.topMargin: 16
                    Layout.bottomMargin: 28
                    spacing: 10
                    Text { text: "☀"; color: "#ffcb50"; font.pixelSize: 31 }
                    Text { text: "Aurinko"; color: "#f7faff"; font.pixelSize: 24; font.bold: true }
                }
                SidebarEntry {
                    Layout.fillWidth: true
                    itemText: "Tänään"
                    glyph: "⌂"
                    active: root.currentPage === 0
                    onClicked: root.navigate(0)
                }
                SidebarEntry {
                    Layout.fillWidth: true
                    itemText: "Huomenna"
                    glyph: "◷"
                    active: root.currentPage === 1
                    onClicked: root.navigate(1)
                }
                SidebarEntry {
                    Layout.fillWidth: true
                    itemText: "Kuukausi"
                    glyph: "▦"
                    active: root.currentPage === 2
                    onClicked: root.navigate(2)
                }
                SidebarEntry {
                    Layout.fillWidth: true
                    itemText: "Asetukset"
                    glyph: "⚙"
                    active: root.currentPage === 3
                    onClicked: root.navigate(3)
                }
                Item { Layout.fillHeight: true }
                Text {
                    Layout.fillWidth: true
                    Layout.leftMargin: 11
                    Layout.bottomMargin: 11
                    text: "v" + Application.version + " · Qt 6 / C++"
                    color: "#8998ae"
                    font.pixelSize: 12
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#091527" }
                GradientStop { position: 1.0; color: "#0b1523" }
            }

            ScrollView {
                id: scroller
                anchors.fill: parent
                clip: true
                contentWidth: availableWidth
                contentHeight: contentColumn.implicitHeight + 66
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                ColumnLayout {
                    id: contentColumn
                    x: 24
                    y: 28
                    width: Math.max(0, scroller.availableWidth - 48)
                    spacing: 17

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.bottomMargin: 5
                        ColumnLayout {
                            spacing: 3
                            Text {
                                text: "Aurinko"
                                color: "#f2f6ff"
                                font.pixelSize: 43
                                font.bold: true
                            }
                            Text {
                                text: root.currentPage === 3 ? "Asetukset" :
                                      root.currentPage === 2 ? solar.monthTitle(root.displayedYear, root.displayedMonth) :
                                      root.selectedDay.label
                                color: "#b5c5d9"
                                font.pixelSize: 18
                            }
                        }
                        Item { Layout.fillWidth: true }
                        ComboBox {
                            id: locationChooser
                            Layout.preferredWidth: 184
                            Layout.preferredHeight: 44
                            model: root.locationItems
                            textRole: "name"
                            currentIndex: solar.currentLocationIndex
                            onActivated: function(index) { solar.setLocation(index) }
                            contentItem: Text {
                                text: "⌖  " + locationChooser.displayText
                                color: "#edf2fa"
                                font.pixelSize: 15
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 12
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                color: "#17273b"
                                radius: 10
                                border.color: "#354960"
                            }
                            popup: Popup {
                                y: locationChooser.height + 2
                                width: locationChooser.width + 24
                                implicitHeight: Math.min(contentItem.implicitHeight + 8, 350)
                                padding: 4
                                background: Rectangle {
                                    color: "#17273b"
                                    radius: 10
                                    border.color: "#354960"
                                }
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: locationChooser.popup.visible ? locationChooser.delegateModel : null
                                    currentIndex: locationChooser.highlightedIndex
                                    ScrollIndicator.vertical: ScrollIndicator { }
                                }
                            }
                            delegate: ItemDelegate {
                                required property int index
                                required property var model
                                width: locationChooser.popup.width - 8
                                contentItem: Text {
                                    text: model.name
                                    color: "#edf2fa"
                                    font.pixelSize: 14
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: locationChooser.highlightedIndex === index ? "#254575" : "#17273b"
                                }
                            }
                        }
                    }

                    SunChart {
                        visible: root.currentPage < 2
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 285 : 0
                        sunriseText: root.selectedDay.sunrise
                        sunsetText: root.selectedDay.sunset
                        sunriseFraction: root.selectedDay.sunriseFraction ?? -1
                        sunsetFraction: root.selectedDay.sunsetFraction ?? -1
                        orbitData: root.sunOrbit
                        dayState: root.selectedDay.state ?? "normal"
                        isToday: root.liveData.isToday ?? false
                        timeFraction: root.liveData.timeFraction ?? -1
                        sunElevation: root.liveData.geometricElevation ?? -90
                        orbitPhase: root.liveData.orbitPhase ?? "timeline"
                        orbitProgress: root.liveData.orbitProgress ?? 0
                    }

                    RowLayout {
                        visible: root.currentPage < 2
                        Layout.fillWidth: true
                        spacing: 10
                        MetricCard {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            heading: "Auringonnousu"
                            glyph: "☀"
                            value: root.selectedDay.sunrise
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            heading: "Auringonlasku"
                            glyph: "◕"
                            glyphColor: "#ff805a"
                            value: root.selectedDay.sunset
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            heading: "Päivän pituus"
                            glyph: "◷"
                            value: root.selectedDay.duration
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            heading: root.liveData.isToday ? "Seuraava tapahtuma" : "Päivän tapahtuma"
                            glyph: "➜"
                            value: root.liveData.nextName
                            detail: root.liveData.nextDetail
                        }
                        MetricCard {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            heading: root.liveData.isToday ? "Aurinkokorkeus nyt" : "Päivän huippukorkeus"
                            glyph: "∠"
                            value: root.liveData.altitude
                        }
                    }

                    Rectangle {
                        visible: root.currentPage !== 3
                        Layout.fillWidth: true
                        implicitHeight: monthLayout.implicitHeight + 32
                        color: "#121d2c"
                        radius: 14
                        border.color: "#304056"

                        ColumnLayout {
                            id: monthLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 9

                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: solar.monthTitle(root.displayedYear, root.displayedMonth)
                                    font.pixelSize: 20
                                    font.bold: true
                                    color: "#eef4ff"
                                }
                                Item { Layout.fillWidth: true }
                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 30
                                    radius: 6
                                    color: prevMonth.containsMouse ? "#2b476c" : "#1c3048"
                                    Text { anchors.centerIn: parent; text: "‹"; font.pixelSize: 24; color: "#e0ecff" }
                                    MouseArea {
                                        id: prevMonth
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.shiftMonth(-1)
                                    }
                                }
                                Text {
                                    text: "Selaa kuukausia"
                                    color: "#aabbce"
                                    font.pixelSize: 12
                                }
                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 30
                                    radius: 6
                                    color: nextMonth.containsMouse ? "#2b476c" : "#1c3048"
                                    Text { anchors.centerIn: parent; text: "›"; font.pixelSize: 24; color: "#e0ecff" }
                                    MouseArea {
                                        id: nextMonth
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: root.shiftMonth(1)
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 34
                                color: "#263247"
                                radius: 5
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8
                                    Repeater {
                                        model: ["Päivä", "Auringonnousu", "Auringonlasku", "Päivän pituus"]
                                        delegate: Text {
                                            required property string modelData
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            text: modelData
                                            color: "#d8e3f1"
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                            Repeater {
                                model: root.shownDays.length
                                delegate: Rectangle {
                                    id: dayRow
                                    required property int index
                                    readonly property var d: root.shownDays[index]
                                    Layout.fillWidth: true
                                    implicitHeight: 34
                                    radius: 5
                                    color: root.selectedIso === dayRow.d.iso ? "#254575" :
                                           dayMouse.containsMouse ? "#1d3048" : "#131f2d"
                                    border.color: root.selectedIso === dayRow.d.iso ? "#3769aa" : "transparent"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 8
                                        Repeater {
                                            model: [dayRow.d.weekday + " " + dayRow.d.date,
                                                    dayRow.d.sunrise,
                                                    dayRow.d.sunset,
                                                    dayRow.d.duration]
                                            delegate: Text {
                                                required property string modelData
                                                Layout.fillWidth: true
                                                Layout.preferredWidth: 1
                                                text: modelData
                                                color: "#ecf4ff"
                                                font.pixelSize: 13
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }
                                    MouseArea {
                                        id: dayMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.followSelectedDate = false
                                            root.selectedIso = dayRow.d.iso
                                            root.displayedYear = Number(root.selectedIso.slice(0, 4))
                                            root.displayedMonth = Number(root.selectedIso.slice(5, 7))
                                            root.followDisplayedMonth = false
                                            root.currentPage = 0
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: root.currentPage === 3
                        Layout.fillWidth: true
                        implicitHeight: settingsLayout.implicitHeight + 48
                        radius: 14
                        color: "#121d2c"
                        border.color: "#34465c"

                        ColumnLayout {
                            id: settingsLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 24
                            spacing: 13

                            Text {
                                text: "Omat sijainnit"
                                color: "#f0f6ff"
                                font.pixelSize: 23
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Anna sijainnin koordinaatit desimaaliasteina ja valitse aikavyöhyke. " +
                                      "Tiedot tallentuvat vain tälle koneelle. Toimii ilman verkkoyhteyttä."
                                color: "#c2d1e3"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text { text: "Sijainnin nimi"; color: "#c9d8eb"; font.pixelSize: 13 }
                                    TextField {
                                        id: newPlaceName
                                        Layout.fillWidth: true
                                        placeholderText: "Esim. Oma mökki"
                                        color: "#f0f6ff"
                                        font.pixelSize: 15
                                        background: Rectangle { color: "#182b43"; radius: 7; border.color: "#465a74" }
                                    }
                                }
                                ColumnLayout {
                                    Layout.preferredWidth: 170
                                    spacing: 5
                                    Text { text: "Leveysaste (N +)"; color: "#c9d8eb"; font.pixelSize: 13 }
                                    TextField {
                                        id: newLatitude
                                        Layout.fillWidth: true
                                        placeholderText: "60,9959"
                                        color: "#f0f6ff"
                                        font.pixelSize: 15
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        background: Rectangle { color: "#182b43"; radius: 7; border.color: "#465a74" }
                                    }
                                }
                                ColumnLayout {
                                    Layout.preferredWidth: 170
                                    spacing: 5
                                    Text { text: "Pituusaste (E +)"; color: "#c9d8eb"; font.pixelSize: 13 }
                                    TextField {
                                        id: newLongitude
                                        Layout.fillWidth: true
                                        placeholderText: "24,4643"
                                        color: "#f0f6ff"
                                        font.pixelSize: 15
                                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                                        background: Rectangle { color: "#182b43"; radius: 7; border.color: "#465a74" }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    Text { text: "Aikavyöhyke"; color: "#c9d8eb"; font.pixelSize: 13 }
                                    TimeZonePicker {
                                        id: newTimeZone
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 42
                                        timeZones: root.timeZoneItems
                                        selectedZone: solar.timeZoneName
                                    }
                                }
                                Button {
                                    id: addPlaceButton
                                    Layout.preferredWidth: 215
                                    Layout.preferredHeight: 42
                                    Layout.alignment: Qt.AlignBottom
                                    text: "Tallenna ja valitse"
                                    onClicked: {
                                        const latText = newLatitude.text.trim().replace(",", ".")
                                        const lonText = newLongitude.text.trim().replace(",", ".")
                                        if (latText.length === 0 || lonText.length === 0 ||
                                            !Number.isFinite(Number(latText)) || !Number.isFinite(Number(lonText))) {
                                            root.locationMessage = "Anna kelvolliset leveys- ja pituusasteet."
                                            root.locationMessageError = true
                                            return
                                        }
                                        const result = solar.addLocation(newPlaceName.text, Number(latText),
                                                                          Number(lonText), newTimeZone.selectedZone)
                                        if (result.ok) {
                                            root.locationMessage = "Sijainti tallennettu."
                                            root.locationMessageError = false
                                            newPlaceName.text = ""
                                            newLatitude.text = ""
                                            newLongitude.text = ""
                                            solar.setLocation(result.index)
                                        } else {
                                            root.locationMessage = result.error
                                            root.locationMessageError = true
                                        }
                                    }
                                    contentItem: Text {
                                        text: addPlaceButton.text
                                        color: "#101827"
                                        font.bold: true
                                        font.pixelSize: 14
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: addPlaceButton.down ? "#e5ad45" : "#ffcc66"
                                        radius: 8
                                    }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Valitse aikavyöhyke listasta tai hae kirjoittamalla esimerkiksi hels, madrid tai oslo. " +
                                      "Koordinaatit eivät automaattisesti määritä aikavyöhykettä."
                                color: "#9badc6"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                Layout.fillWidth: true
                                visible: root.locationMessage !== ""
                                text: root.locationMessage
                                color: root.locationMessageError ? "#ff9595" : "#a5e4b4"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: "#31425b"
                            }
                            Text {
                                text: "Tallennetut omat sijainnit (" + root.customPlaces.length + ")"
                                color: "#f0f6ff"
                                font.pixelSize: 18
                                font.bold: true
                            }
                            Text {
                                visible: root.customPlaces.length === 0
                                text: "Ei vielä omia sijainteja."
                                color: "#9badc6"
                                font.pixelSize: 14
                            }
                            Repeater {
                                model: root.customPlaces.length
                                delegate: Rectangle {
                                    id: customRow
                                    required property int index
                                    readonly property var placeInfo: root.customPlaces[index]
                                    Layout.fillWidth: true
                                    implicitHeight: 48
                                    radius: 8
                                    color: "#17283b"
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 8
                                        spacing: 10
                                        Text {
                                            Layout.fillWidth: true
                                            text: customRow.placeInfo.name + "  (" +
                                                  Number(customRow.placeInfo.latitude).toFixed(4) + ", " +
                                                  Number(customRow.placeInfo.longitude).toFixed(4) + ") · " +
                                                  customRow.placeInfo.timeZone
                                            color: "#dce6f4"
                                            elide: Text.ElideRight
                                            font.pixelSize: 14
                                        }
                                        Button {
                                            id: removePlaceButton
                                            text: "Poista"
                                            onClicked: {
                                                const result = solar.removeLocation(customRow.placeInfo.index)
                                                root.locationMessageError = !result.ok
                                                root.locationMessage = result.ok ? "Sijainti poistettu." : result.error
                                            }
                                            contentItem: Text {
                                                text: removePlaceButton.text
                                                color: "#ffd0d0"
                                                font.pixelSize: 13
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                radius: 6
                                                color: removePlaceButton.down ? "#5b3039" : "#3a2c38"
                                                border.color: "#754a59"
                                            }
                                        }
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 1
                                color: "#31425b"
                            }
                            Text {
                                text: "Tietoja sovelluksesta"
                                color: "#f0f6ff"
                                font.pixelSize: 18
                                font.bold: true
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Aurinko Desktop " + Application.version +
                                      " · Qt 6 / C++ / QML. Auringonnousut, -laskut ja aurinkokaaret " +
                                      "lasketaan paikallisesti ilman internetyhteyttä. " +
                                      "Aikavyöhykkeet luetaan laitteen IANA-aikavyöhyketiedoista."
                                color: "#b8c9dd"
                                wrapMode: Text.WordWrap
                                font.pixelSize: 14
                            }
                        }
                    }
                    Item { Layout.preferredHeight: 15 }
                }
            }
        }
    }
}
