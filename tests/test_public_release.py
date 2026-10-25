"""Julkisen MIT-julkaisun metadatan ja README:n tarkistus."""
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


class PublicReleaseTests(unittest.TestCase):
    def test_mit_license_is_complete(self):
        license_text = (ROOT / "LICENSE").read_text(encoding="utf-8")
        self.assertIn("MIT License", license_text)
        self.assertIn("Copyright (c) 2026 janneok", license_text)
        self.assertIn("Permission is hereby granted", license_text)
        self.assertIn("THE SOFTWARE IS PROVIDED", license_text)

    def test_metainfo_uses_mit(self):
        path = ROOT / "flatpak" / "fi.oksanen.Aurinko.metainfo.xml"
        component = ET.parse(path).getroot()
        self.assertEqual(component.findtext("project_license"), "MIT")

    def test_readme_is_for_public_repo(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("[MIT-lisenssillä](LICENSE)", readme)
        self.assertNotIn("Huomio yksityisestä repositoriosta", readme)
        self.assertNotIn("Laskennan testaus", readme)
        self.assertNotIn("1 095/1 095", readme)
        self.assertNotIn("LicenseRef-proprietary", readme)

    def test_old_private_publish_script_is_not_distributed(self):
        self.assertFalse((ROOT / "scripts" / "publish-github.sh").exists())


if __name__ == "__main__":
    unittest.main()
