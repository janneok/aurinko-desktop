## Aurinko Desktop 0.8.2 – ensimmäinen julkinen kokeilujulkaisu

Natiivi C++/Qt 6 -aurinkosovellus Linuxille. Ensimmäinen julkinen **x86_64 Flatpak** -jakelupaketti. Lähdekoodi on MIT-lisensoitu.

### Sisältö

- Tänään-, Huomenna- ja kuukausinäkymät; nousu- ja laskuajat sekä päivän pituus
- Päivä- ja yökaari, reaaliaikaisesti päivittyvä aurinkopiste ja neljän valaistustilan maisema
- Omat paikat ja hakutoiminnolla varustettu aikavyöhykevalinta
- Offline-laskenta, asetusten tallennus ja KDE-integraatio
- v0.8.2: huomisen tapahtumakortin sekä huippukorkeuden korjaukset

### Lataa ja asenna

**Tiedosto:** `Aurinko-0.8.2-x86_64.flatpak`

```bash
flatpak remote-add --user --if-not-exists flathub \
  https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user ./Aurinko-0.8.2-x86_64.flatpak
flatpak run fi.oksanen.Aurinko
```

Flatpak/KDE-runtime saattaa vaatia internetyhteyden ensiasennuksessa; itse ohjelmalla ei ole verkkolupaa. Liitteenä on myös SHA256SUMS.

**Yhteensopivuus:** x86_64 Linux + Flatpak. Ei testattu kaikilla jakeluilla. Kuvakaappaus on repositorion `docs/aurinko-desktop.png`-tiedostossa (versiosta 0.8.0, vastaava ulkoasu).

**Testaus:** Flatpak rakennettu, asennettu ja käynnistetty Fedora 44 KDE:ssä. Omat sijainnit säilyivät uudelleenkäynnistyksen jälkeen.

**Lähdekoodi:** [GitHub](https://github.com/janneok/aurinko-desktop) · [MIT-lisenssi](https://github.com/janneok/aurinko-desktop/blob/main/LICENSE)
