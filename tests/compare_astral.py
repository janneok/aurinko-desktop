#!/usr/bin/env python3
"""Compare Aurinko Desktop's C++ result with Astral 3.2 (original site's library).

Uses an offline local C++ executable; Astral is needed only for this optional
reference test, never for the desktop app. Non-boundary cases are chosen to
avoid making false claims of exact parity for polar transition days.
"""
from __future__ import annotations
import argparse
from datetime import date
import json
from pathlib import Path
import subprocess
import sys
from zoneinfo import ZoneInfo

try:
    from astral import Observer
    from astral.sun import sunrise, sunset
except ImportError:
    sys.exit("Astral puuttuu. Luo .venv ja asenna: python -m pip install 'astral==3.2'")

# Leveysaste N+, pituusaste E+. Vertailut myös DST-rajan yli.
CASES = [
    ("Hämeenlinna, lokakuu", "2026-10-24", 60.9959, 24.4643, "Europe/Helsinki"),
    ("Hämeenlinna, talviaika", "2026-10-25", 60.9959, 24.4643, "Europe/Helsinki"),
    ("Hämeenlinna, kesäaika", "2026-03-29", 60.9959, 24.4643, "Europe/Helsinki"),
    ("Hämeenlinna, kesäpäivä", "2026-06-21", 60.9959, 24.4643, "Europe/Helsinki"),
    ("Málaga", "2026-10-24", 36.7213, -4.4214, "Europe/Madrid"),
    ("Valencia", "2027-03-04", 39.4699, -0.3763, "Europe/Madrid"),
    ("Longyearbyen, kaamos", "2026-12-21", 78.2232, 15.6469, "Europe/Oslo"),
    ("Longyearbyen, yötön yö", "2026-06-21", 78.2232, 15.6469, "Europe/Oslo"),
]


def astral_time(fun, observer: Observer, iso: str, zone: str) -> str | None:
    try:
        return fun(observer, date=date.fromisoformat(iso), tzinfo=ZoneInfo(zone)).strftime("%H:%M")
    except ValueError as error:
        # Astral 3.2 uses ValueError for cases with no daily crossing.
        if any(phrase in str(error).lower() for phrase in
               ("always above", "always below", "unable to find a sunrise", "unable to find a sunset")):
            return None
        raise


def clock_difference(a: str, b: str) -> int:
    ah, am = map(int, a.split(":"))
    bh, bm = map(int, b.split(":"))
    delta = abs(ah * 60 + am - bh * 60 - bm)
    return min(delta, 1440 - delta)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=Path("build/aurinko-solar-json"))
    parser.add_argument("--tolerance-minutes", type=int, default=3)
    args = parser.parse_args()
    if not args.binary.is_file():
        parser.error(f"C++-vertailuohjelma puuttuu: {args.binary}. Käännä projekti ensin.")
    binary = str(args.binary.resolve())
    failed = 0
    print("Aurinko Desktop C++ vs Astral 3.2 (paikalliset kellonajat)")
    for title, iso, latitude, longitude, zone in CASES:
        data = json.loads(subprocess.check_output([
            binary, iso, str(latitude), str(longitude), zone], text=True))
        observer = Observer(latitude=latitude, longitude=longitude)
        checks = [
            ("nousu", data["sunrise"], astral_time(sunrise, observer, iso, zone)),
            ("lasku", data["sunset"], astral_time(sunset, observer, iso, zone)),
        ]
        results = []
        for label, actual, expected in checks:
            if expected is None:
                ok = actual == "—"
                detail = f"{label}: {actual} vs ei tapahtumaa"
            elif actual == "—":
                ok = False
                detail = f"{label}: puuttuu, Astral {expected}"
            else:
                delta = clock_difference(actual, expected)
                ok = delta <= args.tolerance_minutes
                detail = f"{label}: C++ {actual}, Astral {expected}, ero {delta} min"
            if not ok:
                failed += 1
            results.append(("OK" if ok else "FAIL") + " " + detail)
        print(f"{title} ({iso}): " + "; ".join(results))
    print(f"Vertailu: {len(CASES)} päivää, poikkeamia {failed} / {len(CASES) * 2} tapahtumasta")
    if failed:
        print("Huom. yli toleranssin erot edellyttävät tarkempaa algoritmivertailua.")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
