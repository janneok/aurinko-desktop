"""Paikallinen, verkoton Flatpak-julkaisun rakennetesti."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
APP_ID = "fi.oksanen.Aurinko"

class FlatpakPackagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Yaml:n pieni käytetty osajoukko: ei tarvitse PyYAML:ia pakettitestiin.
        cls.manifest = (ROOT / 'flatpak' / (APP_ID + '.yml')).read_text()

    def test_platform_and_minimal_permissions(self):
        manifest = self.manifest
        for key, val in {
            'id': APP_ID,
            'runtime': 'org.kde.Platform',
            'runtime-version': "'6.10'",
            'sdk': 'org.kde.Sdk',
            'command': 'aurinko-desktop',
        }.items():
            self.assertIn(f'{key}: {val}', manifest)
        for permission in ('--share=ipc', '--socket=wayland',
                           '--socket=fallback-x11', '--device=dri'):
            self.assertIn('  - ' + permission, manifest)
        self.assertNotIn('--share=network', manifest)
        self.assertNotIn('--filesystem=', manifest)

    def test_source_stays_local(self):
        manifest = self.manifest
        self.assertIn('buildsystem: cmake-ninja', manifest)
        self.assertIn('-DBUILD_TESTING=OFF', manifest)
        self.assertIn('type: dir\n        path: ..', manifest)
        self.assertTrue((ROOT / 'flatpak' / '..' / 'CMakeLists.txt').exists())

    def test_desktop_and_icon(self):
        desktop = (ROOT / 'resources' / (APP_ID + '.desktop')).read_text()
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        self.assertIn('Exec=aurinko-desktop', desktop)
        self.assertIn('Icon=' + APP_ID, desktop)
        self.assertIn('DESTINATION share/applications', cmake)
        self.assertIn('DESTINATION share/icons/hicolor/scalable/apps', cmake)
        self.assertIn('DESTINATION share/metainfo', cmake)

    def test_metainfo_xml_and_versions(self):
        root = ET.parse(ROOT / 'flatpak' / (APP_ID + '.metainfo.xml')).getroot()
        self.assertEqual(root.findtext('id'), APP_ID)
        self.assertEqual(root.find('launchable').text, APP_ID + '.desktop')
        cmake = (ROOT / 'CMakeLists.txt').read_text()
        expected = re.search(r'project\(AurinkoDesktop VERSION ([0-9.]+)', cmake).group(1)
        self.assertEqual(root.find('releases/release').get('version'), expected)

if __name__ == '__main__':
    unittest.main()
