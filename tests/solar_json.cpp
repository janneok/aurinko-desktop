// Aurinko Desktop 0.6.2 – paikallinen C++-JSON-apuohjelma.
// Yksi päivä: aurinko-solar-json YYYY-MM-DD LAT LON TIMEZONE
// Kokonainen kuukausi: aurinko-solar-json --month YYYY-MM LAT LON TIMEZONE
// Ei verkkoyhteyttä eikä muutoksia käyttäjän asetuksiin.
#include "SolarCalculator.h"
#include <QCoreApplication>
#include <QDate>
#include <QJsonDocument>
#include <QTextStream>
#include <cstdio>
#include <cmath>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const bool monthMode = argc == 6 && QString::fromUtf8(argv[1]) == QStringLiteral("--month");
    if (!(argc == 5 || monthMode)) {
        std::fprintf(stderr, "Usage: aurinko-solar-json YYYY-MM-DD LAT LON TIMEZONE\n"
                             "   or: aurinko-solar-json --month YYYY-MM LAT LON TIMEZONE\n");
        return 2;
    }
    const int arg = monthMode ? 2 : 1;
    const QString selected = QString::fromUtf8(argv[arg]);
    bool latOk = false, lonOk = false;
    const double latitude = QString::fromUtf8(argv[arg + 1]).toDouble(&latOk);
    const double longitude = QString::fromUtf8(argv[arg + 2]).toDouble(&lonOk);
    if (!latOk || !lonOk || !std::isfinite(latitude) || !std::isfinite(longitude)) {
        std::fprintf(stderr, "Invalid coordinates\n");
        return 2;
    }
    SolarCalculator solar(nullptr, false);
    const QVariantMap added = solar.addLocation(QStringLiteral("Reference case"), latitude,
                                                longitude, QString::fromUtf8(argv[arg + 3]));
    if (!added.value(QStringLiteral("ok")).toBool()) {
        std::fprintf(stderr, "%s\n", added.value(QStringLiteral("error")).toString().toUtf8().constData());
        return 2;
    }
    if (!solar.setLocation(added.value(QStringLiteral("index")).toInt())) return 2;

    QVariant data;
    if (monthMode) {
        const QDate first = QDate::fromString(selected + QStringLiteral("-01"), Qt::ISODate);
        if (!first.isValid() || selected.size() != 7) {
            std::fprintf(stderr, "Invalid YYYY-MM month\n");
            return 2;
        }
        data = solar.month(first.year(), first.month());
    } else {
        const QVariantMap day = solar.day(selected);
        if (day.isEmpty()) {
            std::fprintf(stderr, "Invalid ISO date\n");
            return 2;
        }
        data = day;
    }
    const QByteArray json = QJsonDocument::fromVariant(data).toJson(QJsonDocument::Compact);
    std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
    std::fwrite("\n", 1, 1, stdout);
    return 0;
}
