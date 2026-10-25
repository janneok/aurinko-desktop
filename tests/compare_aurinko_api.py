#!/usr/bin/env python3
"""Vertaa Aurinko Desktopin C++-kuukausia alkuperäisen Aurinkosivun API:in.

Verkko on tarpeen VAIN auditoinnissa. Päivä- ja kuukausikyselyt tehdään
paikallisesta C++-binaarista; julkinen API toimii referenssinä.

Pikavertailu:
  python3 tests/compare_aurinko_api.py --binary build/aurinko-solar-json

Vuoden kaikki päivät:
  python3 tests/compare_aurinko_api.py --full-year --year 2026 --report audit-2026.json
"""
from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from datetime import date, datetime, timezone
import json
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlencode
from urllib.request import Request, urlopen


@dataclass(frozen=True)
class Place:
    name: str
    latitude: float
    longitude: float
    timezone: str


PLACES = {
    "hameenlinna": Place("Hämeenlinna", 60.9959, 24.4643, "Europe/Helsinki"),
    "malaga": Place("Málaga", 36.7213, -4.4214, "Europe/Madrid"),
    "longyearbyen": Place("Longyearbyen", 78.2232, 15.6469, "Europe/Oslo"),
    "valencia": Place("Valencia", 39.4699, -0.3763, "Europe/Madrid"),
    "sydney": Place("Sydney", -33.8688, 151.2093, "Australia/Sydney"),
}

QUICK_CASES = [
    ("hameenlinna", "2026-03-29"),  # kesäajan alku, 23 h
    ("hameenlinna", "2026-06-21"),
    ("hameenlinna", "2026-10-24"),
    ("hameenlinna", "2026-10-25"),  # talviajan alku, 25 h
    ("malaga", "2026-03-29"),
    ("malaga", "2026-10-24"),
    ("malaga", "2026-10-25"),
    ("longyearbyen", "2026-02-15"),  # kaamoksen päättyminen
    ("longyearbyen", "2026-04-01"),
    ("longyearbyen", "2026-08-25"),  # kolme ylitystä; normaali pituus, tyhjät nousu-/laskukentät
    ("longyearbyen", "2026-06-21"),
    ("longyearbyen", "2026-10-25"),  # kaamoksen alku
    ("longyearbyen", "2026-12-21"),
    ("valencia", "2027-03-04"),
    ("sydney", "2026-10-04"),        # Australian kesäajan alku
]


def clock_difference(first: str, second: str) -> int:
    """Saman paikallisen päivän HH:mm-kellonaikojen minuutiero."""
    ah, am = map(int, first.split(":"))
    bh, bm = map(int, second.split(":"))
    delta = abs((ah * 60 + am) - (bh * 60 + bm))
    return min(delta, 1440 - delta)


def absolute_minutes_difference(first_iso: str, second_iso: str) -> float:
    first = datetime.fromisoformat(first_iso)
    second = datetime.fromisoformat(second_iso)
    if first.tzinfo is None or second.tzinfo is None:
        raise ValueError("Auditoinnin ISO-aikaleimoissa on oltava aikavyöhyke")
    return abs((first.astimezone(timezone.utc) -
                second.astimezone(timezone.utc)).total_seconds()) / 60.0


def compare_row(cpp: dict, ref: dict, *, minute_tolerance: int = 3,
                duration_tolerance: int = 180) -> tuple[list[dict], dict]:
    """Palauta poikkeamat ja mittarit; referenssin puuttuva ei ole nolla."""
    errors: list[dict] = []
    measures: dict = {"event_minutes": [], "duration_seconds": []}

    for name, key, refkey, iso_key, refiso_key in (
        ("nousu", "sunrise", "sunrise_local", "sunriseIso", "sunrise"),
        ("lasku", "sunset", "sunset_local", "sunsetIso", "sunset")):

        actual = cpp.get(key)
        expected = ref.get(refkey)
        if expected is None:
            if actual != "—":
                errors.append({"field": name, "type": "extra_event", "actual": actual,
                               "expected": None})
        elif actual in (None, "—"):
            errors.append({"field": name, "type": "missing_event", "actual": actual,
                           "expected": expected})
        else:
            actual_iso, expected_iso = cpp.get(iso_key), ref.get(refiso_key)
            # ISO-leima on ensisijainen, sillä pelkkä HH:mm ei havaitse
            # väärää päivämäärää eikä kesä-/talviaikaoffsetia.
            delta = (absolute_minutes_difference(actual_iso, expected_iso)
                     if actual_iso and expected_iso
                     else clock_difference(actual, expected))
            measures["event_minutes"].append(delta)
            if delta > minute_tolerance:
                errors.append({"field": name, "type": "time_delta", "actual": actual,
                               "expected": expected, "actual_iso": actual_iso,
                               "expected_iso": expected_iso,
                               "minutes": round(delta, 2)})

    actual_sec, reference_sec = cpp.get("daylightSeconds"), ref.get("day_length_seconds")
    if (reference_sec is None):
        if actual_sec is not None:
            errors.append({"field": "päivän pituus", "type": "unexpected_duration",
                           "actual": actual_sec, "expected": None})
    elif actual_sec is None:
        errors.append({"field": "päivän pituus", "type": "missing_duration",
                       "actual": None, "expected": reference_sec})
    else:
        difference = abs(int(actual_sec) - int(reference_sec))
        measures["duration_seconds"].append(difference)
        if difference > duration_tolerance:
            errors.append({"field": "päivän pituus", "type": "duration_delta",
                           "actual": int(actual_sec), "expected": int(reference_sec),
                           "seconds": difference})

    source_state = ref.get("solar_state")
    cpp_state = cpp.get("state")
    if source_state and cpp_state and cpp_state != source_state:
        errors.append({"field": "tila", "type": "state_mismatch",
                       "actual": cpp_state, "expected": source_state})
    return errors, measures


def cpp_month(binary: Path, year: int, month: int, place: Place) -> dict[str, dict]:
    output = subprocess.check_output([
        str(binary), "--month", f"{year:04d}-{month:02d}", str(place.latitude),
        str(place.longitude), place.timezone], text=True)
    payload = json.loads(output)
    if not isinstance(payload, list):
        raise ValueError("C++ --month ei palauttanut JSON-taulukkoa")
    return {row["iso"]: row for row in payload}


def reference_month(base: str, year: int, month: int, place: Place) -> dict[str, dict]:
    query = urlencode(dict(year=year, month=month, lat=place.latitude, lon=place.longitude))
    url = base.rstrip("/") + "/api/month?" + query
    req = Request(url, headers={"User-Agent": "AurinkoDesktopAccuracyAudit/0.6"})
    with urlopen(req, timeout=45) as response:
        payload = json.load(response)
    rows = payload.get("series", payload.get("days", []))
    if not isinstance(rows, list) or not rows:
        raise ValueError("Alkuperäinen API ei palauttanut rivejä: " + url)
    return {row["date_local"]: row for row in rows}


def run(args: argparse.Namespace) -> tuple[int, dict]:
    names = [x.strip().lower() for x in args.locations.split(",") if x.strip()]
    if not names or any(name not in PLACES for name in names):
        raise ValueError("Tuntematon sijainti: valitse " + ", ".join(PLACES))

    if args.full_year:
        cases = [(name, f"{args.year:04d}-{month:02d}", None)
                 for name in names for month in range(1, 13)]
    else:
        cases = [(name, iso[:7], iso) for name, iso in QUICK_CASES if name in names]

    seen_cpp: dict[tuple, dict] = {}
    seen_reference: dict[tuple, dict] = {}
    deviations: list[dict] = []
    measures = {"event_minutes": [], "duration_seconds": []}
    checked = 0
    states: Counter[str] = Counter()
    for name, month_iso, one_day in cases:
        place = PLACES[name]
        year, month = map(int, month_iso.split("-"))
        cachekey = (name, year, month)
        if cachekey not in seen_cpp:
            seen_cpp[cachekey] = cpp_month(args.binary, year, month, place)
            seen_reference[cachekey] = reference_month(args.url, year, month, place)
        local = seen_cpp[cachekey]
        source = seen_reference[cachekey]
        days = [one_day] if one_day else sorted(source)
        for iso in days:
            if iso not in local or iso not in source:
                deviations.append({"location": place.name, "date": iso,
                                   "errors": [{"field": "päivä", "type": "missing_row"}]})
                continue
            checked += 1
            ref = source[iso]
            actual = local[iso]
            states[str(ref.get("solar_state", "tuntematon"))] += 1
            errors, metrics = compare_row(actual, ref,
                minute_tolerance=args.tolerance_minutes,
                duration_tolerance=args.duration_tolerance_seconds)
            for kind in measures:
                measures[kind].extend(metrics[kind])
            if errors:
                deviations.append({"location": place.name, "date": iso, "errors": errors})

        if args.full_year:
            print(f"{place.name:15} {month_iso}: {len(days)} päivää tarkistettu", flush=True)

    result = {
        "mode": "full-year" if args.full_year else "quick",
        "year": args.year if args.full_year else None,
        "reference": args.url.rstrip("/") + "/api/month",
        "checked_days": checked,
        "days_with_deviations": len(deviations),
        "max_event_delta_minutes": max(measures["event_minutes"], default=None),
        "max_duration_delta_seconds": max(measures["duration_seconds"], default=None),
        "reference_states": dict(states),
        "deviations": deviations,
    }
    return (1 if deviations else 0), result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=Path("build/aurinko-solar-json"))
    parser.add_argument("--url", default="https://aurinko.oksanen.cc")
    parser.add_argument("--full-year", action="store_true", help="vertaa kaikki 12 kuukautta")
    parser.add_argument("--year", type=int, default=2026)
    parser.add_argument("--locations", default="hameenlinna,malaga,longyearbyen",
                        help="pilkuilla erotetut tunnisteet: " + ", ".join(PLACES))
    parser.add_argument("--tolerance-minutes", type=int, default=3)
    parser.add_argument("--duration-tolerance-seconds", type=int, default=180)
    parser.add_argument("--max-details", type=int, default=20)
    parser.add_argument("--report", type=Path, help="tallenna JSON-raportti haluttuun tiedostoon")
    args = parser.parse_args()
    if not args.binary.is_file():
        parser.error("C++-ohjelma puuttuu: käännä ensin 'build/aurinko-solar-json'")
    if args.year < 2 or args.year > 9998:
        parser.error("Vuoden on oltava välillä 2–9998")
    if args.tolerance_minutes < 0 or args.duration_tolerance_seconds < 0:
        parser.error("Toleranssien on oltava epänegatiivisia")
    args.binary = args.binary.resolve()
    try:
        exit_status, report = run(args)
    except (ValueError, OSError, subprocess.CalledProcessError, KeyError, json.JSONDecodeError) as error:
        print(f"VERTAILU KESKEYTYI (ei tarkoita hyväksyttyä testiä): {error}", file=sys.stderr)
        return 2

    print(f"\nPäiviä: {report['checked_days']}, poikkeavia päiviä: {report['days_with_deviations']}")
    print(f"Suurin nousu-/laskuaikojen ero: {report['max_event_delta_minutes']} min")
    print(f"Suurin päivänpituuden ero: {report['max_duration_delta_seconds']} s")
    for entry in report["deviations"][:args.max_details]:
        details = "; ".join(f"{error['field']}: {error['type']}" for error in entry["errors"])
        print(f"POIKKEAMA: {entry['location']} {entry['date']} — {details}")
    if len(report["deviations"]) > args.max_details:
        print(f"Lisäksi {len(report['deviations']) - args.max_details} poikkeavaa päivää; katso JSON-raportti.")
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"Raportti: {args.report}")
    return exit_status


if __name__ == "__main__":
    sys.exit(main())
