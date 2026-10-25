#include "SolarCalculator.h"

#include <QCoreApplication>
#include <QDate>
#include <QLocale>
#include <QDebug>
#include <QTime>
#include <cmath>
#include <algorithm>
#include <limits>
#include <QTemporaryDir>
#include <QSettings>

static int fail(const QString &text) {
    qCritical().noquote() << "FAIL:" << text;
    return 1;
}

static bool within(const QString &actual, const QString &expected, int toleranceSeconds) {
    const QTime a = QTime::fromString(actual, "HH:mm");
    const QTime b = QTime::fromString(expected, "HH:mm");
    return a.isValid() && b.isValid() && std::abs(a.secsTo(b)) <= toleranceSeconds;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    SolarCalculator solar(nullptr, false);

    // v0.7: IANA-aikavyöhykkeet tulevat paikallisesta Qt:n tietokannasta.
    const QStringList zones = solar.availableTimeZones();
    if (zones.size() < 100 || !zones.contains(QStringLiteral("Europe/Helsinki"))
        || !zones.contains(QStringLiteral("Europe/Madrid"))
        || !zones.contains(QStringLiteral("UTC")))
        return fail("aikavyoehykelista puuttuu tai on liian suppea");
    if (zones.first() != QStringLiteral("Europe/Helsinki"))
        return fail("yleisimmat aikavyoehykkeet eivaet ole ensin");
    for (const QString &zone : zones) {
        if (!QTimeZone(zone.toUtf8()).isValid())
            return fail("valitsimen listassa virheellinen aikavyoehyke: " + zone);
    }

    // Riippumattomat vertailuajat sunrise-sunset.org (Hämeenlinna, 2026).
    // Huomio: lähteen ilmoittamat ajat voivat poiketa algoritmeittain 1–2 min.
    const auto saturday = solar.day("2026-10-24");
    const auto sunday = solar.day("2026-10-25");
    if (!within(saturday.value("sunrise").toString(), "08:27", 180))
        return fail("24.10. nousu ei vastaa julkaistua vertailuarvoa (±3 min)");
    if (!within(saturday.value("sunset").toString(), "17:44", 180))
        return fail("24.10. lasku ei vastaa julkaistua vertailuarvoa (±3 min)");
    if (!within(sunday.value("sunrise").toString(), "07:29", 180))
        return fail("25.10. nousu ei vastaa talviaikaan siirtymistä");
    if (!within(sunday.value("sunset").toString(), "16:41", 180))
        return fail("25.10. lasku ei vastaa talviaikaan siirtymistä");

    const int daylightSeconds = saturday.value("daylightSeconds").toInt();
    if (daylightSeconds < 9 * 3600 || daylightSeconds > 10 * 3600)
        return fail("24.10. päiväpituus on mahdoton");
    const QString expectedDuration = QStringLiteral("%1 h %2 min")
        .arg(daylightSeconds / 3600).arg((daylightSeconds % 3600) / 60);
    if (saturday.value("duration").toString() != expectedDuration)
        return fail("pituus ei kaeyta alkuperaeisen Aurinkosivun minuuttien katkaisua");
    // v0.8.2: "Huomenna"-näkymän tapahtuma ja korkeus eivät saa
    // sisältää samaa nykyhetken laskuria tai korkeuskulmaa kuin "Tänään".
    // Käytetään laskimen omaa paikallista päivämäärää, ei koneen aikavyöhykettä.
    const QDate todayDate = QDate::fromString(solar.today(), Qt::ISODate);
    if (!todayDate.isValid()) return fail("paikallinen tanaan-paiva virheellinen");
    const QString todayIso = todayDate.toString(Qt::ISODate);
    const QString tomorrowIso = todayDate.addDays(1).toString(Qt::ISODate);
    const auto todayLive = solar.live(todayIso);
    const auto tomorrowLive = solar.live(tomorrowIso);
    const auto tomorrowDay = solar.day(tomorrowIso);
    if (!todayLive.value("isToday").toBool() || tomorrowLive.value("isToday").toBool())
        return fail("tanaan/huomenna live-paivat sekoittuvat");
    if (tomorrowLive.value("nextName").toString() != QStringLiteral("Auringonnousu")
        || tomorrowLive.value("nextDetail").toString() !=
           QStringLiteral("Huomenna klo ") + tomorrowDay.value("sunrise").toString())
        return fail("huomisen tapahtumakortti ei kayta huomispaivan nousuaikaa");
    if (tomorrowLive.value("nextDetail").toString() == todayLive.value("nextDetail").toString())
        return fail("huomisen korttiin kopioitu tanaan-laskuri");
    const QString laterIso = todayDate.addDays(3).toString(Qt::ISODate);
    if (solar.live(laterIso).value("nextDetail").toString()
        != todayDate.addDays(3).toString(QStringLiteral("dd.MM.yyyy"))
           + QStringLiteral(" klo ") + solar.day(laterIso).value("sunrise").toString())
        return fail("kasivalitun paivan tapahtuma ei seuraa valittua paivaa");
    const QVariantList tomorrowPath = solar.sunPath(tomorrowIso);
    double sampledPeak = -90;
    for (const QVariant &value : tomorrowPath)
        sampledPeak = std::max(sampledPeak, value.toMap().value("elevation").toDouble());
    QString altitudeText = tomorrowLive.value("altitude").toString();
    if (!altitudeText.endsWith(QChar(0x00B0)))
        return fail("huomisen korkeuskulmasta puuttuu aste-merkki");
    altitudeText.chop(1);
    bool altitudeValid = false;
    const double displayedPeak = QLocale(QLocale::Finnish).toDouble(altitudeText, &altitudeValid);
    if (!altitudeValid || std::abs(displayedPeak - sampledPeak) > 0.6)
        return fail("huomisen korkeuskortti nayttaa nykyista korkeutta huipun sijasta");

    if (solar.month(2026, 10).size() != 31)
        return fail("lokakuussa tulee olla 31 päivää");
    if (solar.daysAround("2026-10-24").size() != 7)
        return fail("ympäröivässä näkymässä tulee olla seitsemän päivää");
    if (solar.day("2026-06-21").value("daylightSeconds").toInt() < 18 * 3600)
        return fail("kesäpäivänseisauksen päiväpituus on liian lyhyt");
    // Piirtodatan on oltava järjestyksessä ja fysikaalisesti järkevää.
    const QVariantList path = solar.sunPath("2026-10-24");
    if (path.size() < 140 || path.size() > 160)
        return fail("lokakuun vuorokauden korkeuskayran pisteita puuttuu");
    double previousX = -1.0;
    double highest = -90;
    for (const QVariant &entry : path) {
        const QVariantMap point = entry.toMap();
        const double x = point.value("fraction").toDouble();
        const double alt = point.value("elevation").toDouble();
        if (x < previousX || x > 1.001 || alt < -90 || alt > 90)
            return fail("korkeuskayrassa virheellinen piste");
        previousX = x;
        highest = std::max(highest, alt);
    }
    if (highest < 10 || highest > 30)
        return fail("lokakuun keskipaeivaen korkeus mahdoton");


    // Aurinko Desktop 0.4: päivä- ja yökaari muodostuvat oikeista
    // peräkkäisistä aikaväleistä (ei keksitystä yön kestosta).
    const auto orbit = solar.sunOrbit("2026-10-24");
    if (orbit.value("mode").toString() != "loop")
        return fail("tavallisen vuorokauden kiertokaari puuttuu");
    const QVariantList dayArc = orbit.value("dayPoints").toList();
    const QVariantList nightArc = orbit.value("nightPoints").toList();
    // 24.10. -> 25.10. yo ylittaa talviajan kellonsiirron; noin
    // 14 h 45 min ja siten n. 90 kymmenen minuutin naytetta.
    if (dayArc.size() < 45 || nightArc.size() < 88)
        return fail("paiva- tai yokaaresta puuttuu naytteita");

    auto checkArc = [&](const QVariantList &samples, bool night) -> bool {
        double previousFraction = -1.0;
        double highestElevation = -90.0;
        double lowestElevation = 90.0;
        for (const auto &sample : samples) {
            const auto point = sample.toMap();
            const double fraction = point.value("fraction").toDouble();
            const double elevation = point.value("elevation").toDouble();
            if (!std::isfinite(elevation) || !std::isfinite(fraction)
                || fraction < previousFraction || fraction < 0 || fraction > 1.0 + 1e-12)
                return false;
            previousFraction = fraction;
            highestElevation = std::max(highestElevation, elevation);
            lowestElevation = std::min(lowestElevation, elevation);
        }
        const auto first = samples.first().toMap();
        const auto last = samples.last().toMap();
        if (std::abs(first.value("fraction").toDouble()) > 1e-12
            || std::abs(last.value("fraction").toDouble() - 1.0) > 1e-12)
            return false;
        const double eventHorizon = orbit.value("horizon").toDouble();
        if (std::abs(first.value("elevation").toDouble() - eventHorizon) > 0.05
            || std::abs(last.value("elevation").toDouble() - eventHorizon) > 0.05)
            return false;
        return night ? lowestElevation < -10 : highestElevation > 10;
    };
    if (!checkArc(dayArc, false) || !checkArc(nightArc, true))
        return fail("yhtaikaisen aurinkokierron korkeus tai tapahtumarajat virheellisia");

    // Kaamos: ei keinotekoista ylakaaren nousua, vaan koko 24h alempana.
    // Yötön yö: koko polku pysyy horisontin yläpuolella.
    // Paikkavaihto muuttaa koordinaatit ja aikavyöhykkeen eikä sekoita cachea.
    if (!solar.setLocation(6) || solar.locationName() != QString::fromUtf8("Málaga"))
        return fail("Malagan valinta ei onnistu");
    if (solar.timeZoneName() != "Europe/Madrid")
        return fail("Malagan aikavyohyke vaara");
    const auto malaga = solar.day("2026-10-24");
    if (malaga.value("sunrise").toString() == saturday.value("sunrise").toString()
        || malaga.value("sunset").toString() == saturday.value("sunset").toString())
        return fail("kaupunkien aurinkoajat eivat eroa");
    if (!solar.setLocation(9))
        return fail("Longyearbyen ei asetu");
    if (solar.day("2026-06-21").value("state").toString() != "polar_day")
        return fail("yoton yo ei tunnistu");
    if (solar.day("2026-12-21").value("state").toString() != "polar_night")
        return fail("kaamos ei tunnistu");

    // 0.6.1: Longyearbyen 15.2. oli 0.6:ssa tekaistu 29 min aurinkokäynti.
    // Astral 3.2 käyttää tarkkaa horisonttirajaa (~-0.789107 astetta):
    // päivän maksimi jää n. -0.810 asteeseen, mutta on liian lähellä
    // horisonttia varmaksi polar_night-luokitukseksi -> partial.
    const auto feb = solar.day("2026-02-15");
    if (feb.value("state").toString() != "partial"
        || feb.value("sunrise").toString() != QStringLiteral("—")
        || feb.value("sunset").toString() != QStringLiteral("—")
        || !feb.value("daylightSeconds").isNull())
        return fail("Longyearbyen 15.2: horisonttiraja/partial-luokitus väärä");
    if (solar.sunOrbit("2026-02-15").value("mode").toString() != "transition")
        return fail("Longyearbyen 15.2 piirretään väärin napayönä");

    // 25.10: Longyearbyenin yläraja tuotti v0.6:ssa +/- 3m19s
    // aikaeron ja 398 sekunnin päivänpituuseron. Uusi arvo n. 10:40/12:40.
    const auto oct = solar.day("2026-10-25");
    if (oct.value("state").toString() != "normal"
        || !within(oct.value("sunrise").toString(), "10:40", 180)
        || !within(oct.value("sunset").toString(), "12:40", 180)
        || oct.value("daylightSeconds").toInt() < 7100
        || oct.value("daylightSeconds").toInt() > 7250
        || !oct.value("sunriseIso").toString().endsWith("+01:00"))
        return fail("Longyearbyen 25.10: auringonnousu/lasku tai talviaika väärin");
    // v0.6.2: 25.8.2026 Longyearbyenissä yötön yö päättyy: samana
    // paikallisena päivänä lasku -> nousu -> lasku. Molempia yksittäisiä
    // kellonaikakenttiä EI voi näyttää yksiselitteisenä (kuten API:ssa),
    // mutta kolmesta ylityksestä laskettu päivän pituus on tiedossa ja
    // alkuperäisen Aurinkosivun tila on "normal", ei "partial".
    const auto august = solar.day("2026-08-25");
    const qint64 augustSeconds = august.value("daylightSeconds").toLongLong();
    if (august.value("state").toString() != "normal"
        || august.value("sunrise").toString() != QStringLiteral("—")
        || august.value("sunset").toString() != QStringLiteral("—")
        || august.value("daylightSeconds").isNull()
        || augustSeconds < 22 * 3600 || augustSeconds >= 24 * 3600
        || august.value("duration").toString() == QStringLiteral("—"))
        return fail("Longyearbyen 25.8: kolme tapahtumaa ja tunnettu paivan pituus");
    if (solar.sunOrbit("2026-08-25").value("mode").toString() != "transition")
        return fail("Longyearbyen 25.8: kolmoisylityksen kiertokuva vaarin");

    if (solar.sunPath("2026-12-21").size() < 140)
        return fail("kaamoksen korkeuskayra puuttuu");
    const auto polarNight = solar.sunOrbit("2026-12-21");
    const auto polarDay = solar.sunOrbit("2026-06-21");
    if (polarNight.value("mode").toString() != "polar_night"
        || polarDay.value("mode").toString() != "polar_day")
        return fail("napapaivat eivat saa piirtaa keksittyja nousuja ja laskuja");
    const QVariantList winterPoints = polarNight.value("timelinePoints").toList();
    const QVariantList summerPoints = polarDay.value("timelinePoints").toList();
    if (winterPoints.size() < 140 || summerPoints.size() < 140)
        return fail("napapaivan tai kaamoksen 24h polku puuttuu");
    const double eventHorizon = polarNight.value("horizon").toDouble();
    for (const auto &point : winterPoints)
        if (point.toMap().value("elevation").toDouble() >= eventHorizon)
            return fail("kaamoksen polku ylittaa horisontin");
    for (const auto &point : summerPoints)
        if (point.toMap().value("elevation").toDouble() < eventHorizon)
            return fail("yottoman yon polku alittaa horisontin");
    if (solar.setLocation(-1) || solar.setLocation(9999))
        return fail("virheellinen kaupunkivalinta sallittu");
    if (!solar.setLocation(0))
        return fail("Hameenlinnaan paluu ei onnistu");

    // v0.6: Aito erittäin lyhyt aurinkokäynti napapiirin lähellä.
    // NOAA:n korkeuskäyrä 21.12.2026, lat 67.3511 N, lon 0 E:
    // huippu n. -0,7884°, -0,7891° rajalla aurinko näkyy vain noin 4 minuuttia.
    // Vanhassa 5 min näytteistyksessä sekä 11.55 että 12.00 ovat
    // horisontin alapuolella, joten molemmat ylitykset jäivät piiloon.
    const auto graze = solar.addLocation("Horizon graze", 67.3511, 0.0, "UTC");
    if (!graze.value("ok").toBool()
        || !solar.setLocation(graze.value("index").toInt()))
        return fail("napapiirin rajatestisijainti ei tallentunut");
    const auto brief = solar.day("2026-12-21");
    const qint64 briefSeconds = brief.value("daylightSeconds").toLongLong();
    if (brief.value("state").toString() != "normal"
        || brief.value("sunrise").toString() == QStringLiteral("—")
        || brief.value("sunset").toString() == QStringLiteral("—")
        || briefSeconds < 60 || briefSeconds > 300)
        return fail("lyhyt alle 5 min aurinkokaeynti jai loytymatta");
    if (solar.sunOrbit("2026-12-21").isEmpty())
        return fail("lyhyt aurinkokaeynti rikkoi kiertokaaren");
    if (!solar.removeLocation(graze.value("index").toInt()).value("ok").toBool())
        return fail("napapiirin rajatestisijaintia ei voitu poistaa");

    // Kelloa siirretään talvella taakse ja keväällä eteen. Näytteiden
    // määrän pitää seurata paikallisen vuorokauden todellista kestoa.
    if (solar.sunPath("2026-10-25").size() != 151
        || solar.sunPath("2026-03-29").size() != 139)
        return fail("23/25 tunnin paivan aikavalit ovat vaarin");

    // Versio 0.5: käyttäjän nimeämät koordinaatit, vyöhykkeet ja poisto.
    const auto custom = solar.addLocation(QStringLiteral("Mökki"), 61.3, 24.8,
                                           QStringLiteral("Europe/Helsinki"));
    if (!custom.value("ok").toBool())
        return fail("oman sijainnin luominen epaeonnistui");
    const int cabinIndex = custom.value("index").toInt();
    if (!solar.setLocation(cabinIndex) || solar.locationName() != QStringLiteral("Mökki"))
        return fail("oman sijainnin valinta epaeonnistui");
    if (solar.timeZoneName() != "Europe/Helsinki"
        || solar.day("2026-10-24").value("sunrise").toString() == QStringLiteral("—"))
        return fail("oman sijainnin laskenta on virheellinen");
    if (solar.addLocation("MÖKKI", 61.3, 24.8, "Europe/Helsinki").value("ok").toBool())
        return fail("samanniminen paikka sallittiin");
    if (solar.addLocation("", 61.3, 24.8, "UTC").value("ok").toBool()
        || solar.addLocation("X", 91.0, 24.8, "UTC").value("ok").toBool()
        || solar.addLocation("Y", 61.3, -181.0, "UTC").value("ok").toBool()
        || solar.addLocation("Z", 61.3, 24.8, "Not/AZone").value("ok").toBool()
        || solar.addLocation("NaN", std::numeric_limits<double>::quiet_NaN(), 24.8, "UTC").value("ok").toBool())
        return fail("syotteen validointi on puutteellinen");
    if (solar.removeLocation(0).value("ok").toBool())
        return fail("valmiin kaupungin poisto sallittiin");
    const auto secondCustom = solar.addLocation("Sevilla", 37.3886, -5.9823, "Europe/Madrid");
    if (!secondCustom.value("ok").toBool() || !solar.setLocation(secondCustom.value("index").toInt()))
        return fail("toisen oman sijainnin luonti epaeonnistui");
    if (!solar.removeLocation(cabinIndex).value("ok").toBool()
        || solar.locationName() != "Sevilla"
        || solar.currentLocationIndex() != cabinIndex)
        return fail("vanhemman sijainnin poisto rikkoi valinnan");
    if (!solar.removeLocation(cabinIndex).value("ok").toBool()
        || solar.locationName() != QStringLiteral("Hämeenlinna"))
        return fail("aktiivisen sijainnin poisto ei palauta oletukseen");

    // Asetusten pyoristys uudelleen avatussa olioissa: ei muuta kaeyttaen
    // yleisia asetuksia, vaan tilapaeinen eristetty QSettings-kansio.
    QTemporaryDir directory;
    if (!directory.isValid()) return fail("tilapaeinen testikansio ei avautunut");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QCoreApplication::setOrganizationName("AurinkoDesktopTests");
    QCoreApplication::setApplicationName("PersistenceIsolation");
    {
        SolarCalculator first(nullptr, true);
        const auto result = first.addLocation("Koe", -33.8688, 151.2093,
                                              "Australia/Sydney");
        if (!result.value("ok").toBool() || !first.setLocation(result.value("index").toInt()))
            return fail("oman sijainnin tallennus epaeonnistui");
    }
    {
        SolarCalculator reopened(nullptr, true);
        if (reopened.locationName() != "Koe"
            || reopened.timeZoneName() != "Australia/Sydney"
            || reopened.locations().size() != 11)
            return fail("tallennettu paikka ja valinta eivat palautuneet");
    }

    qInfo().noquote() << "PASS: 24.10" << saturday.value("sunrise").toString()
                       << saturday.value("sunset").toString()
                       << "25.10" << sunday.value("sunrise").toString()
                       << sunday.value("sunset").toString()
                       << "daylight" << saturday.value("duration").toString();
    return 0;
}
