#pragma once

#include <QObject>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QTimeZone>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>
#include <QString>
#include <QStringList>

// Paikallinen NOAA-tyylinen aurinkolaskin. Paikkavaihto toimii offline-tilassa
// esiasetettujen koordinaattien ja IANA-aikavyöhykkeiden avulla.
// Vuoden 2026 kolmesta vertailupaikasta 1 095/1 095 päivää läpäisi auditoinnin v0.6.2:ssa.
class SolarCalculator final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString locationName READ locationName NOTIFY locationChanged)
    Q_PROPERTY(QString timeZoneName READ timeZoneName NOTIFY locationChanged)
    Q_PROPERTY(int currentLocationIndex READ currentLocationIndex NOTIFY locationChanged)

public:
    explicit SolarCalculator(QObject *parent = nullptr, bool persistSettings = true);

    QString locationName() const;
    QString timeZoneName() const;
    int currentLocationIndex() const;
    Q_INVOKABLE QVariantList locations() const;
    // Kaikki koneen Qt/tzdata-tietokannan tunnistamat aikavyöhykkeet.
    // Yleisimmät ensin, muut aakkosjärjestyksessä. Ei verkkopyyntöjä.
    Q_INVOKABLE QStringList availableTimeZones() const;
    Q_INVOKABLE bool setLocation(int index);
    // Käyttäjän paikat ovat omia pysyviä tietueita; aikavyöhyke on aina
    // käyttäjän antama IANA-tunniste, ei verkosta päätelty arvailu.
    Q_INVOKABLE QVariantMap addLocation(const QString &name, double latitude,
                                        double longitude, const QString &timeZoneName);
    Q_INVOKABLE QVariantMap removeLocation(int index);

signals:
    void locationChanged();
    void locationsChanged();

public:

    Q_INVOKABLE QString today() const;
    Q_INVOKABLE QString monthTitle(int year, int month) const;
    Q_INVOKABLE QVariantMap day(const QString &isoDate) const;
    Q_INVOKABLE QVariantList daysAround(const QString &isoDate) const;
    Q_INVOKABLE QVariantList month(int year, int month) const;
    Q_INVOKABLE QVariantMap live(const QString &selectedIsoDate) const;
    Q_INVOKABLE QVariantList sunPath(const QString &isoDate) const;
    Q_INVOKABLE QVariantMap sunOrbit(const QString &isoDate) const;

private:
    struct Place {
        QString id;
        QString name;
        double latitude = 0;
        double longitude = 0;
        QString timeZone;
        bool custom = false;
    };
    void loadCustomLocations();
    void saveCustomLocations() const;
    void persistLocation() const;
    static QString validatePlace(const QString &name, double latitude,
                                 double longitude, const QString &timeZoneName);

    struct SunEvent {
        QDateTime at;
        bool sunrise = false;
    };

    struct DayInfo {
        QVector<SunEvent> events; // Kaikki aidot ylitykset aikajärjestyksessä.
        QDateTime sunrise;
        QDateTime sunset;
        qint64 litSeconds = 0;
        qint64 spanSeconds = 0;
        bool aboveAtStart = false;
        bool aboveAtEnd = false;
        double minimumElevation = 90.0;
        double maximumElevation = -90.0;
    };

    DayInfo calculate(const QDate &date) const;
    QString solarState(const DayInfo &data) const;
    QVariantMap toMap(const QDate &date, const DayInfo &data) const;
    QVariantList sampleBetween(qint64 startUtcMillis, qint64 endUtcMillis) const;
    double geometricElevation(qint64 utcMillis) const;
    double apparentElevation(qint64 utcMillis) const;
    bool isDaylight(qint64 utcMillis) const;

    QVector<Place> m_places;
    QTimeZone m_zone;
    int m_locationIndex = 0;
    double m_latitude = 60.9959;
    double m_longitude = 24.4643;
    bool m_persistSettings = true;
    mutable QHash<qint64, DayInfo> m_cache;
    // Sama horisonttiraja kuin aurinko.oksanen.cc / Astral 3.2:
    // -[SUN_APPARENT_RADIUS + refraction_at_zenith(90 + SUN_APPARENT_RADIUS)],
    // missä SUN_APPARENT_RADIUS = 32 / 120 astetta.
    // Astralin refraktiopolynomi korkeudella -0.266666... antaa rajan
    // -0.78910713534156 astetta (ei yleinen -0.833 astetta).
    static constexpr double horizon = -0.7891071353415637;
};
