# Aurinko Desktopin asentaminen ja jakaminen

Lähdekoodi ja ensimmäinen testattava Flatpak ovat julkisessa GitHub-repositoriossa:

- Projekti: https://github.com/janneok/aurinko-desktop
- Julkaisu: https://github.com/janneok/aurinko-desktop/releases/tag/v0.8.2

## Flatpakin asennus

Lataa julkaisusta `Aurinko-0.8.2-x86_64.flatpak`. Asenna oman Linux-jakelusi Flatpak-tuki tarvittaessa, ja suorita sen jälkeen:

```bash
flatpak remote-add --user --if-not-exists flathub \
  https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user ./Aurinko-0.8.2-x86_64.flatpak
flatpak run fi.oksanen.Aurinko
```

Ensiasennus voi ladata `org.kde.Platform//6.10` -ajoympäristön Flathubista. Aurinko toimii asennuksen jälkeen ilman internetyhteyttä.

## Tiedoston eheys

Lataa myös julkaisun `SHA256SUMS` samaan hakemistoon kuin Flatpak ja tarkista:

```bash
sha256sum -c SHA256SUMS
```

Paketti on rakennettu x86_64-arkkitehtuurille. ARM64/aarch64-laitteelle tarvitaan uusi kyseiselle arkkitehtuurille rakennettu paketti. Kaikkia Linux-jakeluita ei ole testattu.

Lähdekoodi on MIT-lisensoitu, ks. [LICENSE](../LICENSE).
