import QtQuick
import "SkyPalette.js" as SkyPalette

Rectangle {
    id: root

    required property string sunriseText
    required property string sunsetText
    required property real sunriseFraction
    required property real sunsetFraction
    required property string dayState
    required property var orbitData
    property bool isToday: false
    property real timeFraction: -1
    property real sunElevation: -90
    property string orbitPhase: "timeline"
    property real orbitProgress: 0

    readonly property string mode: (orbitData && orbitData.mode) ? orbitData.mode : "transition"
    readonly property var dayPoints: (orbitData && orbitData.dayPoints) ? orbitData.dayPoints : []
    readonly property var nightPoints: (orbitData && orbitData.nightPoints) ? orbitData.nightPoints : []
    readonly property var timelinePoints: (orbitData && orbitData.timelinePoints) ? orbitData.timelinePoints : []
    // Katsottavan tulevan/menneen päivän tausta näyttää päivän maksimivalon.
    // Nykyiselle päivälle käytetään elävää korkeuskulmaa. Suunta määrää,
    // piirretäänkö aamun vai illan lämmin hämärä.
    readonly property bool rising: !isToday ? true :
        orbitPhase === "night" ? orbitProgress >= 0.5 :
        orbitPhase === "day" ? orbitProgress < 0.5 : timeFraction < 0.5
    readonly property real visualElevation: !isToday ? highest : sunElevation
    readonly property var scene: SkyPalette.forElevation(visualElevation, rising)

    readonly property real horizonAltitude: (orbitData && orbitData.horizon !== undefined)
        ? Number(orbitData.horizon) : -0.7891071353415637
    readonly property real highest: {
        let value = -90
        const all = mode === "loop" ? [dayPoints, nightPoints] : [timelinePoints]
        for (let j = 0; j < all.length; ++j)
            for (let i = 0; i < all[j].length; ++i)
                value = Math.max(value, Number(all[j][i].elevation))
        return value
    }
    readonly property real lowest: {
        let value = 90
        const all = mode === "loop" ? [dayPoints, nightPoints] : [timelinePoints]
        for (let j = 0; j < all.length; ++j)
            for (let i = 0; i < all[j].length; ++i)
                value = Math.min(value, Number(all[j][i].elevation))
        return value
    }

    // Tämä näkymä on Aurinkosivun tapaan havainnollinen: noususta laskuun
    // kuljetaan vasemmalta oikealle ja yön aikana takaisin oikealta
    // vasemmalle. Y-suunnan luvut lasketaan oikeista korkeuskulmista.
    // Kaamos, yötön yö ja siirtymäpäivät piirretään paikalliselta
    // 00.00–00.00 -aikaväliltä, eikä niihin keksitä puuttuvia tapahtumia.
    implicitHeight: 285
    radius: 18
    color: "#091c33"
    clip: true
    border.color: "#2b466b"
    border.width: 1

    Canvas {
        id: sky
        anchors.fill: parent
        antialiasing: true
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        Component.onCompleted: requestPaint()

        onPaint: {
            const ctx = getContext("2d")
            const w = width
            const h = height
            if (!ctx || w < 1 || h < 1) return
            ctx.reset()

            const horizonY = h * 0.505
            const plotLeft = Math.min(82, Math.max(52, w * 0.077))
            const plotRight = w - plotLeft
            const plotWidth = Math.max(1, plotRight - plotLeft)
            const scaleUp = Math.max(5, root.highest - root.horizonAltitude)
            const scaleDown = Math.max(5, root.horizonAltitude - root.lowest)
            const dayHeight = Math.min(horizonY - 39,
                                       Math.max(36, 26 + Math.max(0, root.highest) * 1.8))
            const nightHeight = Math.min(h - horizonY - 63,
                                         Math.max(30, 23 + Math.max(0, -root.lowest) * 1.35))
            const xFromFraction = (fraction, reverse) =>
                reverse ? plotRight - plotWidth * fraction : plotLeft + plotWidth * fraction
            const yFromElevation = elevation => elevation >= root.horizonAltitude
                ? horizonY - Math.max(0, (elevation - root.horizonAltitude) / scaleUp) * dayHeight
                : horizonY + Math.max(0, (root.horizonAltitude - elevation) / scaleDown) * nightHeight

            // Neljä valoisuustilaa ilman verkkokuvia. Sama järvi, metsä ja
            // linna säilyvät koko päivän; vain taivaan, veden ja maiseman
            // värit vaihtuvat auringon oikean korkeusasteen mukaan.
            const scene = root.scene
            const skyGradient = ctx.createLinearGradient(0, 0, 0, horizonY)
            skyGradient.addColorStop(0, scene.skyTop)
            skyGradient.addColorStop(0.58, scene.skyMid)
            skyGradient.addColorStop(1, scene.skyHorizon)
            ctx.fillStyle = skyGradient
            ctx.fillRect(0, 0, w, horizonY + 1)

            // Pehmeä valohuntu horisonttiin; kirkkaampi päivällä.
            ctx.save()
            ctx.globalAlpha = scene.glow
            const glow = ctx.createLinearGradient(0, horizonY - 65, 0, horizonY + 1)
            glow.addColorStop(0, "rgba(255,249,219,0)")
            glow.addColorStop(1, "rgba(255,246,214,0.52)")
            ctx.fillStyle = glow
            ctx.fillRect(0, horizonY - 65, w, 66)
            ctx.restore()

            // Aurinkoisella säällä hennot pilvet tekevät päivätaivaasta
            // valoisamman. Ne katoavat vähitellen illan ja yön tullessa.
            if (scene.clouds > 0.001) {
                function cloud(cx, cy, scale) {
                    ctx.beginPath()
                    ctx.arc(cx - 37 * scale, cy + 5 * scale, 14 * scale, 0, Math.PI * 2)
                    ctx.arc(cx - 18 * scale, cy, 19 * scale, 0, Math.PI * 2)
                    ctx.arc(cx + 3 * scale, cy - 5 * scale, 22 * scale, 0, Math.PI * 2)
                    ctx.arc(cx + 24 * scale, cy + 3 * scale, 15 * scale, 0, Math.PI * 2)
                    ctx.fill()
                }
                ctx.save()
                ctx.globalAlpha = scene.clouds * 0.55
                ctx.fillStyle = "#f5fbff"
                cloud(w * 0.23, horizonY * 0.30, Math.max(0.5, w / 1250))
                cloud(w * 0.80, horizonY * 0.39, Math.max(0.45, w / 1600))
                ctx.restore()
            }

            // Tähdet ovat voimakkaimmillaan yöllä; päivänvalossa ne
            // sammuvat automaattisesti. Paikat pysyvät samoina.
            ctx.save()
            ctx.globalAlpha = scene.stars
            ctx.fillStyle = "#e6efff"
            for (let i = 0; i < 37; ++i) {
                const sx = ((i * 167 + 31) % 107) / 107 * w
                const sy = ((i * 79 + 13) % 77) / 77 * horizonY * 0.80
                ctx.beginPath()
                ctx.arc(sx, sy, i % 4 === 0 ? 1.0 : 0.55, 0, Math.PI * 2)
                ctx.fill()
            }
            ctx.restore()

            ctx.fillStyle = scene.hills
            ctx.beginPath()
            ctx.moveTo(0, horizonY + 1)
            for (let i = 0; i <= 75; ++i) {
                const x = i / 75 * w
                const y = horizonY - 13 - 7 * Math.sin(i * 0.29) - 4 * Math.sin(i * 0.79)
                ctx.lineTo(x, y)
            }
            ctx.lineTo(w, horizonY + 1)
            ctx.closePath()
            ctx.fill()

            const castleX = w * 0.72
            const castleBase = horizonY - 9
            ctx.fillStyle = scene.castle
            ctx.fillRect(castleX - 32, castleBase - 22, 68, 24)
            ctx.fillRect(castleX - 23, castleBase - 38, 17, 40)
            ctx.fillRect(castleX + 12, castleBase - 44, 19, 46)
            ctx.beginPath()
            ctx.moveTo(castleX - 25, castleBase - 38)
            ctx.lineTo(castleX - 15, castleBase - 51)
            ctx.lineTo(castleX - 4, castleBase - 38)
            ctx.lineTo(castleX + 10, castleBase - 44)
            ctx.lineTo(castleX + 22, castleBase - 58)
            ctx.lineTo(castleX + 33, castleBase - 44)
            ctx.closePath()
            ctx.fill()

            const waterGradient = ctx.createLinearGradient(0, horizonY, 0, h)
            waterGradient.addColorStop(0, scene.waterTop)
            waterGradient.addColorStop(1, scene.waterBottom)
            ctx.fillStyle = waterGradient
            ctx.fillRect(0, horizonY, w, h - horizonY)
            ctx.strokeStyle = "rgba(203,225,245,0.38)"
            ctx.lineWidth = 1.2
            ctx.beginPath()
            ctx.moveTo(0, horizonY)
            ctx.lineTo(w, horizonY)
            ctx.stroke()

            // Katkoviivojen ja koron väritys seuraavat fyysistä horisonttia.
            function drawTrack(points, reverse, fraction, strokeColor, weight, dashed) {
                if (!points || points.length < 2) return
                const limit = Math.max(0, Math.min(1, fraction))
                if (limit <= 0) return
                ctx.save()
                ctx.strokeStyle = strokeColor
                ctx.lineWidth = weight
                ctx.lineJoin = "round"
                ctx.lineCap = "round"
                ctx.setLineDash(dashed ? [7, 7] : [])
                ctx.beginPath()
                ctx.moveTo(xFromFraction(Number(points[0].fraction), reverse),
                           yFromElevation(Number(points[0].elevation)))
                let previous = points[0]
                for (let i = 1; i < points.length; ++i) {
                    const next = points[i]
                    const at = Number(next.fraction)
                    if (at <= limit) {
                        ctx.lineTo(xFromFraction(at, reverse),
                                   yFromElevation(Number(next.elevation)))
                    } else {
                        const before = Number(previous.fraction)
                        if (before < limit) {
                            const ratio = (limit - before) / (at - before)
                            const el = Number(previous.elevation)
                                + (Number(next.elevation) - Number(previous.elevation)) * ratio
                            ctx.lineTo(xFromFraction(limit, reverse), yFromElevation(el))
                        }
                        break
                    }
                    previous = next
                }
                ctx.stroke()
                ctx.restore()
            }

            if (root.mode === "loop") {
                // Näytä nykyhetkellä vain aktiivinen kaari selvästi.
                // Yöllä päiväkaari PIILOTETAAN KOKONAAN: myös eilen kuljettu
                // kirkas yläkaari teki näkymästä harhaanjohtavan.
                // Vain yökaari etenee oikealta vasemmalle, auringonlaskusta
                // seuraavaan nousuun. Päiväsaikaan alakaari jää taustalle.
                const liveNight = root.isToday && root.orbitPhase === "night"
                const liveDay = root.isToday && root.orbitPhase === "day"
                if (liveNight) {
                    drawTrack(root.nightPoints, true, 1,
                              "rgba(157,186,226,0.54)", 2.2, true)
                    drawTrack(root.nightPoints, true, root.orbitProgress,
                              "rgba(240,194,133,0.92)", 3.1, false)
                } else {
                    // Päivällä yökaari saa näkyä himmeänä viitteenä koko kierrosta.
                    // Jos katsotaan jotain muuta päivää kuin tätä päivää,
                    // näytetään molemmat kaaret neutraalisti ilman etenemää.
                    drawTrack(root.nightPoints, true, 1,
                              liveDay ? "rgba(157,186,226,0.29)" : "rgba(157,186,226,0.56)",
                              2.2, true)
                    drawTrack(root.dayPoints, false, 1,
                              liveDay ? "rgba(255,204,99,0.49)" : "rgba(255,204,99,0.67)",
                              3, false)
                    if (liveDay)
                        drawTrack(root.dayPoints, false, root.orbitProgress,
                                  "#ffcf66", 3.8, false)
                }
            } else {
                // Napaseudut ja siirtymäpäivät: pysytään 00:00–00:00-akselilla.
                // Yötön yö näkyy kokonaan yläpuolella, kaamos alapuolella.
                drawTrack(root.timelinePoints, false, 1,
                          root.mode === "polar_night" ? "rgba(193,168,142,0.76)" :
                          root.mode === "polar_day" ? "rgba(255,204,99,0.80)" :
                          "rgba(207,216,230,0.78)", 3.0,
                          root.mode === "polar_night")
                if (root.isToday)
                    drawTrack(root.timelinePoints, false, root.timeFraction,
                              root.mode === "polar_night" ? "rgba(237,188,117,0.90)" :
                              "#ffcf66", 3.6, false)
            }

            // Ympyrän paikka saadaan samoista korkeuspisteistä kuin käyrä.
            // Yöllä sitä EI piiloteta. Näin kaamoskin näkyy havainnollisesti.
            function interpolatedElevation(points, fraction) {
                if (!points || points.length === 0) return root.sunElevation
                const f = Math.max(0, Math.min(1, fraction))
                if (f <= Number(points[0].fraction)) return Number(points[0].elevation)
                for (let i = 1; i < points.length; ++i) {
                    const next = points[i]
                    if (Number(next.fraction) >= f) {
                        const prev = points[i - 1]
                        const delta = Number(next.fraction) - Number(prev.fraction)
                        if (delta <= 0) return Number(next.elevation)
                        const ratio = (f - Number(prev.fraction)) / delta
                        return Number(prev.elevation)
                            + (Number(next.elevation) - Number(prev.elevation)) * ratio
                    }
                }
                return Number(points[points.length - 1].elevation)
            }

            if (root.isToday) {
                let points = root.timelinePoints
                let fraction = root.timeFraction
                let reverse = false
                if (root.mode === "loop" && root.orbitPhase === "day") {
                    points = root.dayPoints
                    fraction = root.orbitProgress
                } else if (root.mode === "loop" && root.orbitPhase === "night") {
                    points = root.nightPoints
                    fraction = root.orbitProgress
                    reverse = true
                }
                if (points && points.length && Number.isFinite(fraction)) {
                    const x = xFromFraction(Math.max(0, Math.min(1, fraction)), reverse)
                    const elevation = Number.isFinite(root.sunElevation)
                        ? root.sunElevation : interpolatedElevation(points, fraction)
                    const y = yFromElevation(elevation)
                    const isNight = elevation < root.horizonAltitude
                    const glow = ctx.createRadialGradient(x, y, 2, x, y, isNight ? 24 : 36)
                    glow.addColorStop(0, isNight ? "rgba(245,185,118,0.25)" : "rgba(255,203,78,0.56)")
                    glow.addColorStop(1, "rgba(255,203,78,0)")
                    ctx.fillStyle = glow
                    ctx.beginPath()
                    ctx.arc(x, y, isNight ? 24 : 36, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.fillStyle = isNight ? "rgba(255,202,113,0.58)" : "#ffc347"
                    ctx.beginPath()
                    ctx.arc(x, y, isNight ? 7 : 9, 0, Math.PI * 2)
                    ctx.fill()
                    ctx.strokeStyle = isNight ? "rgba(255,215,155,0.75)" : "#ffecad"
                    ctx.lineWidth = 1.2
                    ctx.stroke()
                }
            }
        }
    }

    onOrbitDataChanged: sky.requestPaint()
    onSceneChanged: sky.requestPaint()
    onTimeFractionChanged: sky.requestPaint()
    onSunElevationChanged: sky.requestPaint()
    onOrbitPhaseChanged: sky.requestPaint()
    onOrbitProgressChanged: sky.requestPaint()
    onIsTodayChanged: sky.requestPaint()

    Text {
        x: 21
        y: 12
        text: root.mode === "polar_night" ? "Kaamos · Aurinko pysyy horisontin alapuolella" :
              root.mode === "polar_day" ? "Yötön yö · Aurinko pysyy horisontin yläpuolella" :
              root.mode === "transition" ? "Siirtymäpäivä · Auringon korkeus vuorokauden aikana" :
              root.isToday && root.orbitPhase === "night" ? "Aurinkorata · yökaari" :
              root.isToday && root.orbitPhase === "day" ? "Aurinkorata · päiväkaari" :
              "Aurinkorata · päivä ja yö"
        color: "#ecf1fc"
        font.pixelSize: 12
    }

    Text {
        text: "Huippu " + Number(root.highest).toLocaleString(Qt.locale("fi_FI"), 'f', 1) + "°"
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 17
        color: "#b5c9de"
        font.pixelSize: 12
    }

    Item {
        anchors.fill: parent
        Text {
            id: riseValue
            visible: root.mode === "loop" && root.sunriseFraction >= 0
            width: 116
            x: 7
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 31
            horizontalAlignment: Text.AlignHCenter
            text: root.sunriseText
            color: "#f8fbff"
            font.pixelSize: 18
            font.bold: true
        }
        Text {
            visible: riseValue.visible
            width: riseValue.width
            x: riseValue.x
            anchors.top: riseValue.bottom
            text: "Auringonnousu"
            horizontalAlignment: Text.AlignHCenter
            color: "#d5e2f2"
            font.pixelSize: 12
        }
        Text {
            id: setValue
            visible: root.mode === "loop" && root.sunsetFraction >= 0
            width: 116
            x: parent.width - width - 7
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 31
            text: root.sunsetText
            horizontalAlignment: Text.AlignHCenter
            color: "#f8fbff"
            font.pixelSize: 18
            font.bold: true
        }
        Text {
            visible: setValue.visible
            width: setValue.width
            x: setValue.x
            anchors.top: setValue.bottom
            text: "Auringonlasku"
            horizontalAlignment: Text.AlignHCenter
            color: "#d5e2f2"
            font.pixelSize: 12
        }
        Text {
            visible: root.mode !== "loop"
            x: 22
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 15
            text: "00:00"
            color: "#afc2dc"
            font.pixelSize: 12
        }
        Text {
            visible: root.mode !== "loop"
            anchors.right: parent.right
            anchors.rightMargin: 22
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 15
            text: "00:00 (+1 pv)"
            color: "#afc2dc"
            font.pixelSize: 12
        }
        Text {
            visible: root.mode === "loop"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 14
            text: root.isToday && root.orbitPhase === "night" ? "Yökaari ↶" :
                  root.isToday && root.orbitPhase === "day" ? "Päiväkaari →" :
                  "Yökaari ↶    •    Päiväkaari →"
            color: "#9eb1c9"
            font.pixelSize: 11
        }
    }
}
