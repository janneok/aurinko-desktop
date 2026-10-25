"""Ei-Qt-pohjainen regressiotesti versiotiedoille ja release-UI:lle."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class ReleaseMetadataTests(unittest.TestCase):
    def test_project_version_is_single_source(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        version = re.search(r'project\(AurinkoDesktop VERSION ([0-9]+\.[0-9]+\.[0-9]+)', cmake)
        self.assertIsNotNone(version)
        readme = (ROOT / 'README.md').read_text()
        documented = re.search(r'^# Aurinko Desktop ([0-9]+\.[0-9]+\.[0-9]+)', readme)
        self.assertIsNotNone(documented)
        self.assertEqual(version.group(1), documented.group(1))
        self.assertIn('@PROJECT_VERSION@', (ROOT / 'src/Version.h.in').read_text())
        self.assertIn('AURINKO_APP_VERSION', (ROOT / 'src/main.cpp').read_text())
        self.assertIn('setApplicationVersion', (ROOT / 'src/main.cpp').read_text())

    def test_qml_shows_runtime_version_in_two_places(self):
        qml = (ROOT / 'Main.qml').read_text()
        self.assertEqual(qml.count('Application.version'), 2)
        self.assertNotIn('v0.5 · Qt 6', qml)
        self.assertNotIn('Desktop 0.5', qml)
        self.assertNotIn('PAIKALLINEN LASKENTA (BETA)', qml)

    def test_selected_day_cards_and_about_text(self):
        main = (ROOT / 'Main.qml').read_text()
        solar = (ROOT / 'src/SolarCalculator.cpp').read_text()
        self.assertIn('root.liveData.isToday ? "Seuraava tapahtuma" : "Päivän tapahtuma"', main)
        self.assertIn('root.liveData.isToday ? "Aurinkokorkeus nyt" : "Päivän huippukorkeus"', main)
        self.assertNotIn('1 095 vertailupäivää', main)
        self.assertNotIn('Laskentaa on verrattu alkuperäiseen', main)
        self.assertIn('if (selected.isValid() && !selectedIsToday)', solar)
        self.assertIn('selectedDay.maximumElevation', solar)
        self.assertIn('Huomenna', solar)

    def test_time_zone_search_is_included(self):
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        main = (ROOT / 'Main.qml').read_text()
        picker = (ROOT / 'TimeZonePicker.qml').read_text()
        self.assertIn('TimeZonePicker.qml', cmake)
        self.assertIn('availableTimeZones()', main)
        self.assertIn('QTimeZone::availableTimeZoneIds()', (ROOT / 'src/SolarCalculator.cpp').read_text())
        self.assertIn('zoneSearch.text', picker)
        self.assertIn('selectedZone', picker)


if __name__ == '__main__':
    unittest.main()
