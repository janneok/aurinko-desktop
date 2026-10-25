"""Verkottomat, riippumattomat unittest-testit auditoinnin tulkinnalle."""
import unittest
from compare_aurinko_api import compare_row, clock_difference


class CompareAuditTests(unittest.TestCase):
    def test_same(self):
        cpp = {"sunrise": "08:27", "sunset": "17:44", "daylightSeconds": 33420, "state": "normal"}
        ref = {"sunrise_local": "08:27", "sunset_local": "17:44", "day_length_seconds": 33420,
               "solar_state": "normal"}
        errors, metrics = compare_row(cpp, ref)
        self.assertEqual(errors, [])
        self.assertEqual(metrics["event_minutes"], [0, 0])

    def test_missing_event_is_error(self):
        cpp = {"sunrise": "—", "sunset": "17:44", "daylightSeconds": 0, "state": "partial"}
        ref = {"sunrise_local": "08:27", "sunset_local": "17:44", "day_length_seconds": 33420,
               "solar_state": "normal"}
        errors, _ = compare_row(cpp, ref)
        self.assertEqual({e["type"] for e in errors}, {"missing_event", "duration_delta", "state_mismatch"})

    def test_polar_day(self):
        cpp = {"sunrise": "—", "sunset": "—", "daylightSeconds": 86400, "state": "polar_day"}
        ref = {"sunrise_local": None, "sunset_local": None, "day_length_seconds": 86400,
               "solar_state": "polar_day"}
        errors, _ = compare_row(cpp, ref)
        self.assertEqual(errors, [])

    def test_partial_duration_absent(self):
        cpp = {"sunrise": "08:27", "sunset": "—", "daylightSeconds": None, "state": "partial"}
        ref = {"sunrise_local": "08:27", "sunset_local": None, "day_length_seconds": None,
               "solar_state": "partial"}
        errors, _ = compare_row(cpp, ref)
        self.assertEqual(errors, [])

    def test_an_extra_event_is_error(self):
        cpp = {"sunrise": "08:27", "sunset": "—", "daylightSeconds": 0, "state": "polar_night"}
        ref = {"sunrise_local": None, "sunset_local": None, "day_length_seconds": 0,
               "solar_state": "polar_night"}
        errors, _ = compare_row(cpp, ref)
        self.assertEqual(errors[0]["type"], "extra_event")

    def test_minute_tolerance(self):
        cpp = {"sunrise": "08:31", "sunset": "17:44", "daylightSeconds": 33420, "state": "normal"}
        ref = {"sunrise_local": "08:27", "sunset_local": "17:44", "day_length_seconds": 33420,
               "solar_state": "normal"}
        errors, _ = compare_row(cpp, ref, minute_tolerance=3)
        self.assertEqual(len(errors), 1)
        self.assertEqual(clock_difference("08:31", "08:27"), 4)

    def test_iso_timestamp_detects_wrong_utc_offset(self):
        # Näytetty HH:mm on sama, mutta UTC-aika väärin tunnilla.
        cpp = {"sunrise": "07:29", "sunset": "16:41",
               "sunriseIso": "2026-10-25T07:29:00+03:00",
               "sunsetIso": "2026-10-25T16:41:00+02:00",
               "daylightSeconds": 33120, "state": "normal"}
        ref = {"sunrise_local": "07:29", "sunset_local": "16:41",
               "sunrise": "2026-10-25T07:29:00+02:00",
               "sunset": "2026-10-25T16:41:00+02:00",
               "day_length_seconds": 33120, "solar_state": "normal"}
        errors, _ = compare_row(cpp, ref)
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0]["field"], "nousu")
        self.assertEqual(errors[0]["minutes"], 60)

    def test_iso_timestamp_detects_wrong_date(self):
        cpp = {"sunrise": "08:27", "sunset": "17:44",
               "sunriseIso": "2026-10-25T08:27:00+02:00",
               "sunsetIso": "2026-10-24T17:44:00+03:00",
               "daylightSeconds": 33420, "state": "normal"}
        ref = {"sunrise_local": "08:27", "sunset_local": "17:44",
               "sunrise": "2026-10-24T08:27:00+03:00",
               "sunset": "2026-10-24T17:44:00+03:00",
               "day_length_seconds": 33420, "solar_state": "normal"}
        errors, _ = compare_row(cpp, ref)
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0]["field"], "nousu")
        self.assertEqual(errors[0]["minutes"], 1500)

    def test_daylight_difference(self):
        cpp = {"sunrise": "08:27", "sunset": "17:44", "daylightSeconds": 33300, "state": "normal"}
        ref = {"sunrise_local": "08:27", "sunset_local": "17:44", "day_length_seconds": 33600,
               "solar_state": "normal"}
        errors, _ = compare_row(cpp, ref, duration_tolerance=180)
        self.assertEqual(len(errors), 1)
        self.assertEqual(errors[0]["seconds"], 300)


if __name__ == "__main__":
    unittest.main()
