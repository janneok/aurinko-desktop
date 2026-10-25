#include "SolarCalculator.h"

#include <QLocale>
#include <QByteArray>
#include <QTime>
#include <QtGlobal>
#include <QSettings>
#include <array>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
struct BuiltInPlace {
    const char *name;
    double latitude;
    double longitude;
    const char *timeZone;
};
// Esivalitut paikat: koordinaatit ovat kaupunkikeskustan likiarvoja.
constexpr std::array<BuiltInPlace, 10> places = {{
    {"Hämeenlinna", 60.9959, 24.4643, "Europe/Helsinki"},
    {"Helsinki", 60.1699, 24.9384, "Europe/Helsinki"},
    {"Tampere", 61.4978, 23.7610, "Europe/Helsinki"},
    {"Turku", 60.4518, 22.2666, "Europe/Helsinki"},
    {"Oulu", 65.0121, 25.4651, "Europe/Helsinki"},
    {"Rovaniemi", 66.5039, 25.7294, "Europe/Helsinki"},
    {"Málaga", 36.7213, -4.4214, "Europe/Madrid"},
    {"Valencia", 39.4699, -0.3763, "Europe/Madrid"},
    {"Barcelona", 41.3874, 2.1686, "Europe/Madrid"},
    {"Longyearbyen", 78.2232, 15.6469, "Europe/Oslo"}
}};
constexpr double pi = 3.14159265358979323846;
double rad(double d) { return d * pi / 180.0; }
double deg(double r) { return r * 180.0 / pi; }
double norm(double x, double modulo) {
    double v = std::fmod(x, modulo);
    return v < 0 ? v + modulo : v;
}
QString hhmm(const QDateTime &value) { return value.toString("HH:mm"); }
QString length(qint64 seconds) {
    // Sama esityssääntö kuin alkuperäisen Aurinkosivun _len_hm:
    // täysiä minuutteja, ei pyöristystä lähimpään minuuttiin.
    const qint64 totalMinutes = std::max<qint64>(0, seconds) / 60;
    return QStringLiteral("%1 h %2 min")
        .arg(totalMinutes / 60)
        .arg(totalMinutes % 60);
}
QString weekday(int dayOfWeek) {
    constexpr const char *names[] = {"Ma", "Ti", "Ke", "To", "Pe", "La", "Su"};
    return QString::fromLatin1(names[std::clamp(dayOfWeek, 1, 7) - 1]);
}
} // namespace

SolarCalculator::SolarCalculator(QObject *parent, bool persistSettings)
    : QObject(parent), m_zone(QByteArrayLiteral("Europe/Helsinki")),
      m_persistSettings(persistSettings) {
    for (size_t i = 0; i < places.size(); ++i) {
        const auto &p = places[i];
        m_places.append(Place{
            QStringLiteral("builtin:%1").arg(static_cast<int>(i)), QString::fromUtf8(p.name),
            p.latitude, p.longitude, QString::fromLatin1(p.timeZone), false
        });
    }
    if (m_persistSettings) {
        loadCustomLocations();
        QSettings settings;
        int selected = settings.value(QStringLiteral("location/index"), 0).toInt();
        const QString savedId = settings.value(QStringLiteral("location/id")).toString();
        if (!savedId.isEmpty()) {
            selected = 0; // Poistettua ID:tä ei saa korvata satunnaisella indeksillä.
            for (int i = 0; i < m_places.size(); ++i) {
                if (m_places[i].id == savedId) { selected = i; break; }
            }
        }
        if (!setLocation(selected)) setLocation(0);
    }
}

QString SolarCalculator::locationName() const {
    return m_places[m_locationIndex].name;
}
QString SolarCalculator::timeZoneName() const {
    return QString::fromUtf8(m_zone.id());
}
int SolarCalculator::currentLocationIndex() const {
    return m_locationIndex;
}
QVariantList SolarCalculator::locations() const {
    QVariantList result;
    for (int i = 0; i < m_places.size(); ++i) {
        const auto &p = m_places[i];
        result.push_back(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("latitude"), p.latitude},
            {QStringLiteral("longitude"), p.longitude},
            {QStringLiteral("timeZone"), p.timeZone},
            {QStringLiteral("custom"), p.custom}
        });
    }
    return result;
}

QStringList SolarCalculator::availableTimeZones() const {
    QStringList ids;
    for (const QByteArray &zoneId : QTimeZone::availableTimeZoneIds())
        ids.append(QString::fromUtf8(zoneId));
    ids.removeDuplicates();
    ids.sort(Qt::CaseInsensitive);

    QStringList ordered;
    const QStringList favourites = {
        QStringLiteral("Europe/Helsinki"), QStringLiteral("Europe/Stockholm"),
        QStringLiteral("Europe/Oslo"), QStringLiteral("Europe/Madrid"),
        QStringLiteral("Europe/London"), QStringLiteral("UTC")
    };
    for (const QString &favorite : favourites) {
        if (ids.removeAll(favorite) > 0)
            ordered.append(favorite);
    }
    ordered.append(ids);
    return ordered;
}

void SolarCalculator::persistLocation() const {
    if (!m_persistSettings) return;
    QSettings settings;
    settings.setValue(QStringLiteral("location/index"), m_locationIndex);
    settings.setValue(QStringLiteral("location/id"), m_places[m_locationIndex].id);
}

bool SolarCalculator::setLocation(int index) {
    if (index < 0 || index >= m_places.size()) return false;
    const Place &p = m_places[index];
    const QTimeZone zone(p.timeZone.toUtf8());
    if (!zone.isValid()) return false;
    if (m_locationIndex == index && m_zone == zone) {
        persistLocation();
        return true;
    }
    m_locationIndex = index;
    m_latitude = p.latitude;
    m_longitude = p.longitude;
    m_zone = zone;
    m_cache.clear();
    persistLocation();
    emit locationChanged();
    return true;
}

QString SolarCalculator::validatePlace(const QString &name, double latitude,
                                       double longitude, const QString &timeZoneName) {
    if (name.trimmed().isEmpty() || name.trimmed().size() > 60)
        return QStringLiteral("Anna sijainnille nimi (1–60 merkkiä).");
    if (!std::isfinite(latitude) || latitude < -90.0 || latitude > 90.0)
        return QStringLiteral("Leveysasteen on oltava välillä −90…90.");
    if (!std::isfinite(longitude) || longitude < -180.0 || longitude > 180.0)
        return QStringLiteral("Pituusasteen on oltava välillä −180…180.");
    const QByteArray zoneId = timeZoneName.trimmed().toUtf8();
    if (zoneId.isEmpty() || !QTimeZone(zoneId).isValid())
        return QStringLiteral("Tuntematon aikavyöhyke. Käytä esimerkiksi Europe/Helsinki.");
    return {};
}

void SolarCalculator::loadCustomLocations() {
    QSettings settings;
    QJsonParseError error;
    const QJsonDocument stored = QJsonDocument::fromJson(
        settings.value(QStringLiteral("locations/customJson")).toByteArray(), &error);
    if (error.error != QJsonParseError::NoError || !stored.isArray()) return;
    for (const QJsonValue &value : stored.array()) {
        if (!value.isObject() || m_places.size() >= static_cast<int>(places.size()) + 100) continue;
        const QJsonObject p = value.toObject();
        const QString name = p.value(QStringLiteral("name")).toString().trimmed();
        const QString zone = p.value(QStringLiteral("timeZone")).toString().trimmed();
        const double latitude = p.value(QStringLiteral("latitude")).toDouble(std::numeric_limits<double>::quiet_NaN());
        const double longitude = p.value(QStringLiteral("longitude")).toDouble(std::numeric_limits<double>::quiet_NaN());
        const QString id = p.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || !validatePlace(name, latitude, longitude, zone).isEmpty()) continue;
        bool duplicateId = false;
        for (const Place &existing : m_places) {
            if (existing.id == id) { duplicateId = true; break; }
        }
        if (!duplicateId) m_places.append(Place{id, name, latitude, longitude, zone, true});
    }
}

void SolarCalculator::saveCustomLocations() const {
    if (!m_persistSettings) return;
    QJsonArray entries;
    for (const Place &p : m_places) {
        if (!p.custom) continue;
        entries.append(QJsonObject{
            {QStringLiteral("id"), p.id},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("latitude"), p.latitude},
            {QStringLiteral("longitude"), p.longitude},
            {QStringLiteral("timeZone"), p.timeZone}
        });
    }
    QSettings settings;
    settings.setValue(QStringLiteral("locations/customJson"),
                      QJsonDocument(entries).toJson(QJsonDocument::Compact));
}

QVariantMap SolarCalculator::addLocation(const QString &name, double latitude,
                                          double longitude, const QString &timeZoneName) {
    const QString cleanName = name.trimmed();
    const QString cleanZone = timeZoneName.trimmed();
    const QString error = validatePlace(cleanName, latitude, longitude, cleanZone);
    if (!error.isEmpty()) return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    if (m_places.size() >= static_cast<int>(places.size()) + 100)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Omia sijainteja voi tallentaa enintään 100.")}};
    for (const Place &p : m_places) {
        if (p.name.compare(cleanName, Qt::CaseInsensitive) == 0)
            return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Samanniminen sijainti on jo listassa.")}};
    }
    m_places.append(Place{QUuid::createUuid().toString(QUuid::WithoutBraces),
                          cleanName, latitude, longitude, cleanZone, true});
    saveCustomLocations();
    emit locationsChanged();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("index"), m_places.size() - 1}};
}

QVariantMap SolarCalculator::removeLocation(int index) {
    if (index < 0 || index >= m_places.size() || !m_places[index].custom)
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Valmiita kaupunkeja ei voi poistaa.")}};
    const bool activeRemoved = index == m_locationIndex;
    const bool selectionIndexShifted = index < m_locationIndex;
    m_places.removeAt(index);
    if (activeRemoved) {
        m_locationIndex = 0;
        const Place &p = m_places[0];
        m_latitude = p.latitude;
        m_longitude = p.longitude;
        m_zone = QTimeZone(p.timeZone.toUtf8());
        m_cache.clear();
    } else if (index < m_locationIndex) {
        --m_locationIndex;
    }
    saveCustomLocations();
    persistLocation();
    emit locationsChanged();
    // Jos indeksit muuttuivat, päivitetään myös Comboboxin nykyinen valinta.
    if (activeRemoved || selectionIndexShifted) emit locationChanged();
    return {{QStringLiteral("ok"), true}};
}

QString SolarCalculator::today() const {
    return QDateTime::currentDateTimeUtc().toTimeZone(m_zone).date().toString(Qt::ISODate);
}

QString SolarCalculator::monthTitle(int year, int month) const {
    if (year < 1 || year > 9999 || month < 1 || month > 12)
        return QStringLiteral("Kuukausi");
    QString title = QLocale(QLocale::Finnish).standaloneMonthName(month);
    if (!title.isEmpty()) title[0] = title[0].toUpper();
    return title + QStringLiteral(" %1").arg(year);
}

// NOAA:n aurinkopositiolaskennan geometrista korkeuskaavaa vastaava
// laskenta. Auringon rektaskensio, deklinaatio ja ajantasoitus johdetaan
// ajanhetken juliaanisesta vuosisadasta. Horisontin taittumakorjaus otetaan
// mukaan nousu- ja laskurajan (-0,833 astetta) kautta.
double SolarCalculator::geometricElevation(qint64 utcMillis) const {
    const double jd = static_cast<double>(utcMillis) / 86400000.0 + 2440587.5;
    const double t = (jd - 2451545.0) / 36525.0;
    const double l0 = norm(280.46646 + t * (36000.76983 + 0.0003032 * t), 360.0);
    const double m = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    const double ecc = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    const double c = std::sin(rad(m)) * (1.914602 - t * (0.004817 + 0.000014 * t))
        + std::sin(rad(2 * m)) * (0.019993 - 0.000101 * t)
        + std::sin(rad(3 * m)) * 0.000289;
    const double trueLong = l0 + c;
    const double omega = 125.04 - 1934.136 * t;
    const double lambda = trueLong - 0.00569 - 0.00478 * std::sin(rad(omega));
    const double eps0 = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
    const double eps = eps0 + 0.00256 * std::cos(rad(omega));
    const double decl = std::asin(std::sin(rad(eps)) * std::sin(rad(lambda)));
    const double y = std::pow(std::tan(rad(eps) / 2), 2);
    const double eqTimeMinutes = 4.0 * deg(
        y * std::sin(2 * rad(l0))
        - 2 * ecc * std::sin(rad(m))
        + 4 * ecc * y * std::sin(rad(m)) * std::cos(2 * rad(l0))
        - 0.5 * y * y * std::sin(4 * rad(l0))
        - 1.25 * ecc * ecc * std::sin(2 * rad(m)));

    const QDateTime utc = QDateTime::fromMSecsSinceEpoch(utcMillis, QTimeZone(QTimeZone::UTC));
    const QTime clock = utc.time();
    const double utcMinutes = clock.hour() * 60.0 + clock.minute()
        + clock.second() / 60.0 + clock.msec() / 60000.0;
    const double trueSolarMinutes = norm(utcMinutes + eqTimeMinutes + 4 * m_longitude, 1440.0);
    const double hourAngle = trueSolarMinutes / 4.0 - 180.0;
    const double cosZenith = std::sin(rad(m_latitude)) * std::sin(decl)
        + std::cos(rad(m_latitude)) * std::cos(decl) * std::cos(rad(hourAngle));
    return 90.0 - deg(std::acos(std::clamp(cosZenith, -1.0, 1.0)));
}

// Näytettävää aurinkokorkeutta varten NOAA:n empiirinen taittumakorjaus.
double SolarCalculator::apparentElevation(qint64 utcMillis) const {
    const double elevation = geometricElevation(utcMillis);
    if (elevation > 85.0) return elevation;
    if (elevation < -5.0) return elevation; // Vältetään merkityksetön korjaus kaukana horisontista.
    const double tanEl = std::tan(rad(elevation));
    double arcsec;
    if (elevation > 5.0) {
        arcsec = 58.1 / tanEl - 0.07 / std::pow(tanEl, 3) + 0.000086 / std::pow(tanEl, 5);
    } else if (elevation > -0.575) {
        arcsec = 1735.0 + elevation * (-518.2 + elevation * (103.4 + elevation * (-12.79 + 0.711 * elevation)));
    } else {
        arcsec = -20.774 / tanEl;
    }
    return elevation + arcsec / 3600.0;
}

bool SolarCalculator::isDaylight(qint64 utcMillis) const {
    return geometricElevation(utcMillis) >= horizon;
}

SolarCalculator::DayInfo SolarCalculator::calculate(const QDate &date) const {
    if (!date.isValid()) return {};
    const qint64 key = date.toJulianDay();
    auto found = m_cache.constFind(key);
    if (found != m_cache.constEnd()) return found.value();

    const QDateTime start(date, QTime(0, 0), m_zone);
    const QDateTime end(date.addDays(1), QTime(0, 0), m_zone);
    DayInfo data;
    if (!start.isValid() || !end.isValid() || start >= end) return data;
    qint64 cursor = start.toMSecsSinceEpoch();
    const qint64 endMs = end.toMSecsSinceEpoch();
    data.spanSeconds = (endMs - cursor) / 1000;
    data.aboveAtStart = isDaylight(cursor);
    const double firstElevation = geometricElevation(cursor);
    data.minimumElevation = firstElevation;
    data.maximumElevation = firstElevation;
    bool above = data.aboveAtStart;
    qint64 began = cursor;

    // UTC-vuorokausi käydään läpi viiden minuutin jaksoissa. Jos
    // jakson molemmat päät ovat horisontin samalla puolella, voi välissä
    // silti tapahtua NOPEA nousu ja lasku (polaarialueen sivuaminen).
    // Tunnistetaan jakson sisäinen ääriarvo silloin, kun horisontin
    // läheisyys tekee kahdesta samanmerkkisestä tapahtumasta mahdollisen.
    // Tällöin pilkotaan jakso kahteen osaan ja etsitään molemmat ylitykset.
    auto appendCrossing = [&](qint64 low, qint64 high) {
        const bool wasAbove = isDaylight(low);
        if (wasAbove == isDaylight(high)) return;
        while (high - low > 1000) {
            const qint64 middle = low + (high - low) / 2;
            if (isDaylight(middle) == wasAbove) low = middle;
            else high = middle;
        }
        const QDateTime at = QDateTime::fromMSecsSinceEpoch(high, m_zone);
        data.events.append(SunEvent{at, !wasAbove});
        if (wasAbove) {
            data.litSeconds += (high - began + 500) / 1000;
            if (!data.sunset.isValid()) data.sunset = at;
        } else if (!data.sunrise.isValid()) {
            data.sunrise = at;
        }
        began = high;
    };

    while (cursor < endMs) {
        const qint64 next = std::min(cursor + qint64(300000), endMs);
        const double nextElevation = geometricElevation(next);
        data.minimumElevation = std::min(data.minimumElevation, nextElevation);
        data.maximumElevation = std::max(data.maximumElevation, nextElevation);
        const bool nextAbove = nextElevation >= horizon;
        if (nextAbove != above) {
            appendCrossing(cursor, next);
        } else {
            // Auringon korkeus muuttuu enintään noin 1,26 astetta
            // viidessä minuutissa. Kauempana horisontista piilevä
            // kaksoisylitys ei ole mahdollinen.
            const double startAltitude = geometricElevation(cursor);
            const double endAltitude = geometricElevation(next);
            if (std::min(std::abs(startAltitude - horizon),
                         std::abs(endAltitude - horizon)) <= 1.30
                && next - cursor >= 4000) {
                const double startSlope = geometricElevation(cursor + 1000) - startAltitude;
                const double endSlope = endAltitude - geometricElevation(next - 1000);
                const bool maximumInside = !above && startSlope > 0 && endSlope < 0;
                const bool minimumInside = above && startSlope < 0 && endSlope > 0;
                if (maximumInside || minimumInside) {
                    qint64 low = cursor, high = next;
                    // Ääriarvo tarkennetaan sekunnin tarkkuuteen. Käyrä on
                    // sileä tällä viiden minuutin välillä.
                    while (high - low > 2000) {
                        const qint64 third = (high - low) / 3;
                        const qint64 left = low + third;
                        const qint64 right = high - third;
                        const double leftAltitude = geometricElevation(left);
                        const double rightAltitude = geometricElevation(right);
                        if ((maximumInside && leftAltitude < rightAltitude)
                            || (minimumInside && leftAltitude > rightAltitude)) {
                            low = left;
                        } else {
                            high = right;
                        }
                    }
                    const qint64 extremum = low + (high - low) / 2;
                    const double extremeElevation = geometricElevation(extremum);
                    data.minimumElevation = std::min(data.minimumElevation, extremeElevation);
                    data.maximumElevation = std::max(data.maximumElevation, extremeElevation);
                    if (isDaylight(extremum) != above) {
                        appendCrossing(cursor, extremum);
                        appendCrossing(extremum, next);
                    }
                }
            }
        }
        above = nextAbove;
        cursor = next;
    }
    if (above) data.litSeconds += (endMs - began + 500) / 1000;
    data.aboveAtEnd = above;
    m_cache.insert(key, data);
    return data;
}

// Alkuperäinen Aurinkosivu erottaa tapahtumien esitettävyyden ja päivän
// aurinkotilan toisistaan. Kun yötön yö loppuu, samalla kalenteripäivällä
// voi tapahtua lasku, nousu JA toinen lasku (Longyearbyen 25.8.2026).
// Pythonin _solar_day antaa silloin tilan "normal", koska sekä nousu että
// lasku ovat olemassa, vaikka _legacy_response_events jättää moniselitteiset
// kellonaikakentät tyhjiksi. Päivän pituus lasketaan kaikista kolmesta
// todellisesta ylityksestä eikä sitä saa pyyhkiä pois "partial"-luokituksella.
//
// Ilman tapahtumia käytetään alkuperäisen konservatiivista 0,64°:n
// varmuusmarginaalia. Täsmällisillä navoilla tapahtumapäivät ovat aina
// "partial", kuten Pythonin _solar_day-erikoistapauksessa.
QString SolarCalculator::solarState(const DayInfo &data) const {
    const bool standardPair = data.events.size() == 2
        && data.events[0].sunrise && !data.events[1].sunrise;
    const bool midnightSunEnds = data.events.size() == 3
        && !data.events[0].sunrise && data.events[1].sunrise
        && !data.events[2].sunrise && data.aboveAtStart;
    if (!data.events.isEmpty()) {
        if (std::abs(m_latitude) == 90.0) return QStringLiteral("partial");
        return (standardPair || midnightSunEnds)
            ? QStringLiteral("normal") : QStringLiteral("partial");
    }

    if (std::abs(m_latitude) == 90.0)
        return data.aboveAtStart ? QStringLiteral("polar_day") : QStringLiteral("polar_night");

    constexpr double classificationMargin = 0.64;
    if (data.minimumElevation > horizon + classificationMargin)
        return QStringLiteral("polar_day");
    if (data.maximumElevation < horizon - classificationMargin)
        return QStringLiteral("polar_night");
    return QStringLiteral("partial");
}

QVariantMap SolarCalculator::toMap(const QDate &date, const DayInfo &data) const {
    const bool standardPair = data.events.size() == 2
        && data.events[0].sunrise && !data.events[1].sunrise;
    const bool standardSingle = data.events.size() == 1;
    const bool expressibleEvents = standardPair || standardSingle;
    const QString state = solarState(data);
    const bool unknownDuration = state == QStringLiteral("partial")
        && std::abs(m_latitude) != 90.0;
    const QDateTime dayStart(date, QTime(0, 0), m_zone);
    const QDateTime dayEnd(date.addDays(1), QTime(0, 0), m_zone);
    const qint64 spanMillis = dayStart.msecsTo(dayEnd);
    auto toFraction = [&](const QDateTime &event) -> double {
        return event.isValid() && spanMillis > 0
            ? double(dayStart.msecsTo(event)) / double(spanMillis) : -1.0;
    };
    return {
        {QStringLiteral("iso"), date.toString(Qt::ISODate)},
        {QStringLiteral("date"), date.toString("dd.MM.yyyy")},
        {QStringLiteral("weekday"), weekday(date.dayOfWeek())},
        {QStringLiteral("label"), weekday(date.dayOfWeek()) + QStringLiteral(" ") + date.toString("dd.MM.yyyy")},
        {QStringLiteral("sunrise"), expressibleEvents && data.sunrise.isValid() ? hhmm(data.sunrise) : QStringLiteral("—")},
        {QStringLiteral("sunset"), expressibleEvents && data.sunset.isValid() ? hhmm(data.sunset) : QStringLiteral("—")},
        // ISO-leimat sisältävät paikallisen päivämäärän ja UTC-poikkeaman;
        // auditoinnissa niitä verrataan absoluuttisina ajanhetkinä.
        {QStringLiteral("sunriseIso"), expressibleEvents && data.sunrise.isValid()
            ? data.sunrise.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("sunsetIso"), expressibleEvents && data.sunset.isValid()
            ? data.sunset.toString(Qt::ISODate) : QString{}},
        {QStringLiteral("sunriseFraction"), expressibleEvents ? toFraction(data.sunrise) : -1.0},
        {QStringLiteral("sunsetFraction"), expressibleEvents ? toFraction(data.sunset) : -1.0},
        {QStringLiteral("duration"), unknownDuration ? QStringLiteral("—") : length(data.litSeconds)},
        {QStringLiteral("state"), state},
        {QStringLiteral("daylightSeconds"), unknownDuration ? QVariant{} : QVariant::fromValue(data.litSeconds)},
    };
}

QVariantMap SolarCalculator::day(const QString &isoDate) const {
    const QDate date = QDate::fromString(isoDate, Qt::ISODate);
    return date.isValid() ? toMap(date, calculate(date)) : QVariantMap{};
}

QVariantList SolarCalculator::daysAround(const QString &isoDate) const {
    QVariantList list;
    const QDate date = QDate::fromString(isoDate, Qt::ISODate);
    if (!date.isValid()) return list;
    for (int offset = -2; offset <= 4; ++offset) {
        const QDate selected = date.addDays(offset);
        list.push_back(toMap(selected, calculate(selected)));
    }
    return list;
}

QVariantList SolarCalculator::month(int year, int monthNumber) const {
    QVariantList list;
    const QDate first(year, monthNumber, 1);
    if (!first.isValid()) return list;
    for (int d = 1; d <= first.daysInMonth(); ++d) {
        const QDate date(year, monthNumber, d);
        list.push_back(toMap(date, calculate(date)));
    }
    return list;
}

QVariantMap SolarCalculator::live(const QString &selectedIsoDate) const {
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const qint64 nowMs = now.toMSecsSinceEpoch();
    const QDate current = now.toTimeZone(m_zone).date();
    const QDate selected = QDate::fromString(selectedIsoDate, Qt::ISODate);
    const DayInfo selectedDay = calculate(selected);
    const bool selectedIsToday = selected == current;

    bool showSun = false;
    double sunProgress = 0.5;
    if (selectedIsToday && selectedDay.sunrise.isValid() && selectedDay.sunset.isValid()) {
        const qint64 riseMs = selectedDay.sunrise.toMSecsSinceEpoch();
        const qint64 setMs = selectedDay.sunset.toMSecsSinceEpoch();
        if (setMs > riseMs) {
            showSun = nowMs >= riseMs && nowMs <= setMs;
            sunProgress = std::clamp(double(nowMs - riseMs) / double(setMs - riseMs), 0.0, 1.0);
        }
    }

    QString eventName = QStringLiteral("—");
    QString eventDetail = QStringLiteral("Tapahtumaa etsitään");

    if (selected.isValid() && !selectedIsToday) {
        // Valittua tulevaa tai mennyttä päivää ei pidä esittää nykyhetken
        // "seuraavana tapahtumana": se antaisi Huomenna-välilehdelle
        // samat laskurit kuin Tänään-välilehdelle. Näytä sen sijaan
        // valitun päivän ensimmäinen todellinen horisontin ylitys ja
        // paikallinen kellonaika. Tämä toimii myös DST- ja napapäivissä.
        if (!selectedDay.events.isEmpty()) {
            const SunEvent &first = selectedDay.events.first();
            eventName = first.sunrise ? QStringLiteral("Auringonnousu")
                                      : QStringLiteral("Auringonlasku");
            const QString when = selected == current.addDays(1)
                ? QStringLiteral("Huomenna")
                : selected.toString(QStringLiteral("dd.MM.yyyy"));
            eventDetail = QStringLiteral("%1 klo %2")
                .arg(when, hhmm(first.at));
        } else {
            const QString state = solarState(selectedDay);
            if (state == QStringLiteral("polar_day")) {
                eventName = QStringLiteral("Yötön yö");
                eventDetail = QStringLiteral("Ei nousua tai laskua");
            } else if (state == QStringLiteral("polar_night")) {
                eventName = QStringLiteral("Kaamos");
                eventDetail = QStringLiteral("Ei nousua tai laskua");
            } else {
                eventDetail = QStringLiteral("Ei tapahtumia valittuna päivänä");
            }
        }
    } else {
        // Tänään-välilehdellä näytetään edelleen seuraava tuleva tapahtuma
        // ja siihen jäljellä oleva todellinen aika, myös yöaikaan.
        bool found = false;
        for (int offset = 0; offset <= 370 && !found; ++offset) {
            const DayInfo d = calculate(current.addDays(offset));
            // Aikajärjestys valitaan UTC-leimojen perusteella.
            QList<QPair<qint64, QString>> options;
            for (const SunEvent &event : d.events) {
                if (event.at.toMSecsSinceEpoch() > nowMs)
                    options.push_back({event.at.toMSecsSinceEpoch(),
                        event.sunrise ? QStringLiteral("Auringonnousu") : QStringLiteral("Auringonlasku")});
            }
            if (options.isEmpty()) continue;
            std::sort(options.begin(), options.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
            const auto next = options.first();
            const qint64 minutes = (next.first - nowMs + 30000) / 60000;
            eventName = next.second;
            eventDetail = minutes < 24 * 60
                ? QStringLiteral("%1 h %2 min päästä").arg(minutes / 60).arg(minutes % 60)
                : QStringLiteral("%1 — %2")
                    .arg(QDateTime::fromMSecsSinceEpoch(next.first, m_zone).date().toString("dd.MM.yyyy"))
                    .arg(QDateTime::fromMSecsSinceEpoch(next.first, m_zone).time().toString("HH:mm"));
            found = true;
        }
        if (!found) eventDetail = QStringLiteral("Ei tapahtumaa seuraavan vuoden aikana");
    }

    // "Nyt" on mielekäs vain nykyiselle päivälle. Muilla päivillä
    // kortti näyttää valitun päivän laskettua korkeinta korkeusastetta.
    const double alt = selectedIsToday ? apparentElevation(nowMs)
                                       : selectedDay.maximumElevation;
    const double geometric = geometricElevation(nowMs);
    const QDateTime dayStart(current, QTime(0, 0), m_zone);
    const QDateTime dayEnd(current.addDays(1), QTime(0, 0), m_zone);
    const double timeFraction = double(dayStart.msecsTo(now)) / double(dayStart.msecsTo(dayEnd));

    // Yökierron etenemä muodostetaan *todellisesta* edellisen laskun ja
    // seuraavan nousun ajanjaksosta. Näin alakaarella oleva piste liikkuu
    // oikein myös ennen aamun nousua ja kesä-/talviajan rajalla.
    QString orbitPhase = QStringLiteral("timeline");
    double orbitProgress = std::clamp(timeFraction, 0.0, 1.0);
    if (selectedIsToday && selectedDay.sunrise.isValid() && selectedDay.sunset.isValid()) {
        const qint64 riseMs = selectedDay.sunrise.toMSecsSinceEpoch();
        const qint64 setMs = selectedDay.sunset.toMSecsSinceEpoch();
        if (riseMs < setMs) {
            if (nowMs >= riseMs && nowMs <= setMs) {
                orbitPhase = QStringLiteral("day");
                orbitProgress = double(nowMs - riseMs) / double(setMs - riseMs);
            } else {
                const DayInfo adjacent = calculate(current.addDays(nowMs < riseMs ? -1 : 1));
                const QDateTime previousSet = nowMs < riseMs ? adjacent.sunset : selectedDay.sunset;
                const QDateTime nextRise = nowMs < riseMs ? selectedDay.sunrise : adjacent.sunrise;
                if (previousSet.isValid() && nextRise.isValid()
                    && previousSet.toMSecsSinceEpoch() < nowMs
                    && nextRise.toMSecsSinceEpoch() > nowMs) {
                    orbitPhase = QStringLiteral("night");
                    orbitProgress = double(previousSet.msecsTo(now)) / double(previousSet.msecsTo(nextRise));
                }
            }
        }
    }
    QString altString = QLocale(QLocale::Finnish).toString(alt, 'f', 1);
    if (alt >= 0) altString.prepend('+');
    altString.append(QChar(0x00B0));
    return {
        {QStringLiteral("sunVisible"), showSun},
        {QStringLiteral("sunProgress"), sunProgress},
        {QStringLiteral("nextName"), eventName},
        {QStringLiteral("nextDetail"), eventDetail},
        {QStringLiteral("altitude"), altString},
        {QStringLiteral("geometricElevation"), geometric},
        {QStringLiteral("timeFraction"), timeFraction},
        {QStringLiteral("orbitPhase"), orbitPhase},
        {QStringLiteral("orbitProgress"), std::clamp(orbitProgress, 0.0, 1.0)},
        {QStringLiteral("isToday"), selectedIsToday},
        {QStringLiteral("todayIso"), current.toString(Qt::ISODate)}
    };
}

// Auringon geometrinen korkeus koko paikalliselta vuorokaudelta.
// x-akseli on kulunut osa vuorokauden todellisesta UTC-kestosta:
// kesä-/talviajan vaihdon päivänä se on vastaavasti 23/25 tuntia.
// Kiinteä n. 10 minuutin askel riittää näyttökäyrään (ei tapahtuma-aikoihin).
QVariantList SolarCalculator::sunPath(const QString &isoDate) const {
    QVariantList result;
    const QDate date = QDate::fromString(isoDate, Qt::ISODate);
    if (!date.isValid()) return result;
    const QDateTime start(date, QTime(0, 0), m_zone);
    const QDateTime end(date.addDays(1), QTime(0, 0), m_zone);
    const qint64 spanMs = start.msecsTo(end);
    if (spanMs <= 0) return result;
    const int steps = std::max(2, int(std::ceil(double(spanMs) / 600000.0)));
    for (int i = 0; i <= steps; ++i) {
        const qint64 at = start.toMSecsSinceEpoch() + spanMs * i / steps;
        result.push_back(QVariantMap{
            {QStringLiteral("fraction"), double(i) / steps},
            {QStringLiteral("elevation"), geometricElevation(at)},
        });
    }
    return result;
}

// Piirtoapu: tasaväliset näytteet yhdeltä todelliselta UTC-aikaväliltä.
// Aika muunnetaan paikalliseksi vain kalenteripäivää / tapahtumia varten,
// joten talviajan 25-tuntinen yö käsitellään ilman kellonaikojen arvausta.
QVariantList SolarCalculator::sampleBetween(qint64 startUtcMillis, qint64 endUtcMillis) const {
    QVariantList result;
    if (startUtcMillis >= endUtcMillis) return result;
    const qint64 spanMs = endUtcMillis - startUtcMillis;
    const int steps = std::max(2, int(std::ceil(double(spanMs) / 600000.0)));
    result.reserve(steps + 1);
    for (int i = 0; i <= steps; ++i) {
        const qint64 at = startUtcMillis + (spanMs * i) / steps;
        result.push_back(QVariantMap{
            {QStringLiteral("fraction"), double(i) / steps},
            {QStringLiteral("elevation"), geometricElevation(at)}
        });
    }
    return result;
}

// Havainnollinen 24h auringonkierto: normaalipäivänä noususta laskuun
// piirretään yläkaari vasemmalta oikealle, sitä seuraava yö laskusta
// seuraavaan nousuun alakaareksi oikealta vasemmalle. Näytteiden
// pystysuuntainen arvo on laskettu geometrinen aurinkokorkeus.
//
// Kaamos, yötön yö ja siirtymäpäivät piirretään kronologisena 24h
// korkeuspolkuna, koska puuttuvia tapahtumia ei saa keksiä.
QVariantMap SolarCalculator::sunOrbit(const QString &isoDate) const {
    const QDate date = QDate::fromString(isoDate, Qt::ISODate);
    if (!date.isValid()) return {};
    const DayInfo today = calculate(date);
    const QDateTime start(date, QTime(0, 0), m_zone);
    const QDateTime end(date.addDays(1), QTime(0, 0), m_zone);
    if (!start.isValid() || !end.isValid() || start >= end) return {};
    const bool hasRise = today.sunrise.isValid();
    const bool hasSet = today.sunset.isValid();

    // Alakaari tarvitsee seuraavan päivän nousun. Kun sellaista ei ole,
    // esitetään koko vuorokausi realistisena aikakäyränä.
    if (today.events.size() == 2 && today.events[0].sunrise
        && !today.events[1].sunrise && hasRise && hasSet
        && today.sunrise.toMSecsSinceEpoch() < today.sunset.toMSecsSinceEpoch()) {
        const DayInfo tomorrow = calculate(date.addDays(1));
        if (tomorrow.sunrise.isValid()
            && tomorrow.sunrise.toMSecsSinceEpoch() > today.sunset.toMSecsSinceEpoch()) {
            return {
                {QStringLiteral("mode"), QStringLiteral("loop")},
                {QStringLiteral("dayPoints"), sampleBetween(
                    today.sunrise.toMSecsSinceEpoch(), today.sunset.toMSecsSinceEpoch())},
                {QStringLiteral("nightPoints"), sampleBetween(
                    today.sunset.toMSecsSinceEpoch(), tomorrow.sunrise.toMSecsSinceEpoch())},
                {QStringLiteral("horizon"), horizon}
            };
        }
    }
    QString mode = QStringLiteral("transition");
    const QString state = solarState(today);
    if (state == QStringLiteral("polar_day") || state == QStringLiteral("polar_night"))
        mode = state;
    return {
        {QStringLiteral("mode"), mode},
        {QStringLiteral("dayPoints"), QVariantList{}},
        {QStringLiteral("nightPoints"), QVariantList{}},
        {QStringLiteral("timelinePoints"), sampleBetween(
            start.toMSecsSinceEpoch(), end.toMSecsSinceEpoch())},
        {QStringLiteral("horizon"), horizon}
    };
}
