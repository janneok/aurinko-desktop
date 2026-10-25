# Aurinko Desktop 0.8.2

Natiivi **Qt 6 + C++ + QML** -työpöytäsovellus Fedora KDE:lle. Aurinkoajan laskenta, omat sijainnit ja auringon päivä-/yörata toimivat paikallisesti ilman verkkoa. Alkuperäiseen `aurinko.oksanen.cc`-verkkosivuun tai VPS:ään ei kosketa.

## Korjaukset v0.8.2:ssa

- **Tänään:** Seuraava tapahtuma tarkoittaa nykyhetkestä seuraavaa auringonnousua tai -laskua; laskuri sekä korkeusaste päivittyvät livenä.
- **Huomenna tai käsin valittu päivä:** kortissa näkyy valitun päivän ensimmäinen todellinen tapahtuma ja **valitun päivän paikallinen kellonaika**, ei kuluvan päivän laskuri. Myös viimeinen kortti on näissä näkymissä "Päivän huippukorkeus" (ei väärin nimetty nykyhetken korkeus).
- Kaamoksen ja yöttömän yön päivinä tapahtumakortti ilmoittaa, ettei kyseiselle päivälle ole horisontin ylityksiä. Erikseen käsitellään muut poikkeukselliset siirtymäpäivät.
- Asetuksista poistettiin käyttäjän pyytämä Astral-vertailua koskeva viimeinen kappale. Testityökalut jäävät ennalleen.
- Flatpak-metadata ja CMake-versionumero on päivitetty versioon **0.8.2**. Jatkossa jakopaketti rakennetaan tästä versiosta, ei 0.8.1:stä.

**Rajaus:** Aurinkolaskennan algoritmia, aurinkokaaren ulkoasua eikä sijaintien tallennustapaa muutettu. Tämä korjaa valitun päivän tapahtumakortin ja korkeusastekortin näyttölogiikan.

## Uutta v0.8:ssa ja v0.8.1:ssä

- **Elävä aurinkopiste:** auringon asema, nykyinen korkeuskulma ja seuraavan tapahtuman laskuri päivittyvät **5 sekunnin välein**, kun sovellus on käynnissä. Kalliimpi koko päivän kiertorata pysyy välimuistissa, eikä sitä luoda uudestaan joka tikillä.
- **Paikallisen päivän ja kuukauden vaihto:** sovellus tarkistaa valitun sijainnin kalenteripäivän viiden sekunnin välein (`QTimeZone`). Kun päivä vaihtuu, Tänään, Huomenna, valittu päivä ja kuukausitaulukko päivittyvät automaattisesti. Kuukauden vaihtuessa siirrytään automaattisesti uuden kuukauden taulukkoon **vain, jos käyttäjä ei ole itse selannut toiseen kuukauteen**. Sama pätee käsin valittuun päivään: sitä ei ylikirjoiteta keskiyöllä. Sovellus tunnistaa myös lepotilasta palatessa muuttuneen päivän seuraavalla tikillä.
- **Dynaaminen maisema aurinkokaaressa:** kirkas sininen päivätaivas ja pehmeät pilvet; lämpimämpi **aamurusko** ja **iltarusko**; yöksi tumma sininen tähtitaivas. Samaa järvi-, metsä- ja linnasiluettia käytetään kaikissa tiloissa, eikä kuvia tai tietoja ladata verkosta. Värit muuttuvat vähitellen auringon korkeuden mukaan. Muuta päivää tarkasteltaessa käytetään kyseisen päivän valoisuutta kuvaavaa esikatselua.
- Versio `0.8.2` generoituu edelleen suoraan CMake-projektiversiosta.
- **Aurinkolaskennan algoritmia ei ole muutettu:** aikaleiman luomiseen käytettävä Qt-kutsu vaihtui uudempaan, saman UTC-hetken ilmaisevaan muotoon. V0.6.2:n vuosivertailu (1095/1095) on aiempi mittaustulos; uutta täyttä auditointia ei ole vielä ajettu.

## Uutta versiossa 0.8.1 / Flatpak-jakelu

- Varoitukset Qt 6.11:n JS-/qmldir-tiedostoista siivottu. Myös vanhentunut `QDateTime::fromMSecsSinceEpoch`-kutsu vaihdettu `QTimeZone`-muotoon. Astronomisen laskennan algoritmia tai esityslogiikkaa ei ole muutettu.
- Flatpak-manifesti, metadata ja rakentamisskripti. Sovellusta ei ole vielä julkaistu Flathubiin.
- Flatpakilla ei ole verkko- eikä kotihakemistooikeuksia. KDE/Qt-ajonaikaiset kirjastot tulevat `org.kde.Platform//6.10`-runtimepaketista.
- Alkuperäinen natiiviasennus ja Flatpak säilyttävät asetukset **erillään**. Halutessasi voit kopioida omat sijainnit erillisellä skriptillä ennen Flatpakin ensimmäistä käynnistystä.

### Tee jaettava Flatpak Fedoralla

```bash
sudo dnf install flatpak flatpak-builder
cd ~/Projects/aurinko-desktop-v0.8.2
./scripts/build-flatpak.sh
```

Skripti hakee tarvittaessa KDE:n Qt 6.10 -SDK:n käyttäjän Flathub-varastosta, kääntää ohjelman Flatpak-ympäristössä ja tekee tiedoston esimerkiksi
`~/Projects/aurinko-flatpak-output/Aurinko-0.8.2-x86_64.flatpak`.
**Yksittäinen `.flatpak` sisältää sovelluksen, mutta ei Qt/KDE-runtimea:** asennuskone tarvitsee Flatpakin sekä verkkoyhteyden runtime-latausta varten (jos se ei ole jo asennettu).

Asenna ja testaa **käsin** (vanhan KDE-käynnistimen rinnalla käynnistyskomento on yksiselitteisin):

```bash
flatpak install --user ~/Projects/aurinko-flatpak-output/Aurinko-0.8.2-x86_64.flatpak
flatpak run fi.oksanen.Aurinko
```

Kaverin asennusohje (Linux-jakeluun asennetaan ensin Flatpak, jos se puuttuu):

```bash
flatpak remote-add --user --if-not-exists flathub \
  https://dl.flathub.org/repo/flathub.flatpakrepo
flatpak install --user ./Aurinko-0.8.2-x86_64.flatpak
flatpak run fi.oksanen.Aurinko
```

Jos haluat tuoda nykyisestä natiivisovelluksesta omat tallennetut sijainnit Flatpakiin, sulje Flatpak ensin ja suorita ennen sen ensimmäistä käynnistystä:

```bash
./scripts/copy-settings-to-flatpak.sh
```

Skripti ei ylikirjoita olemassa olevia Flatpak-asetuksia. Paikallisen KDE-käynnistimen ja Flatpakin `.desktop`-tunnus on sama: jos asennat molemmat, sovellusvalikossa toinen voi peittää toisen. Testaa Flatpak aina ensin `flatpak run fi.oksanen.Aurinko` -komennolla. Vanhaa natiiviasennusta ei tarvitse poistaa testaamista varten.

## Käynnistys Fedorassa

Fedoran Qt/C++-kehityspaketit on jo asennettu aiemmissa vaiheissa.

```bash
cd ~/Projects
unzip "$(xdg-user-dir DOWNLOAD)/aurinko-desktop-v0.8.2.zip"
cd aurinko-desktop-v0.8.2

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/aurinko-desktop
```

Sulje vanha käynnissä oleva Aurinko ennen asennusta. Jos kaikki toimii, päivitä KDE:n sovellusvalikon asennus:

```bash
./scripts/install-user.sh
```

Aiemmat omat sijainnit säilyvät, koska `Oksanen/Aurinko`-asetustunnisteita ei ole muutettu.

## Mitä testata?

1. Hämeenlinnassa päiväsaikaan kaaren taivaan pitäisi näyttää kirkkaammalta ja siniseltä, ja aurinkopisteen pitäisi siirtyä ajan kuluessa. Laskuri päivittyy samalla. Huomaa, että aurinko liikkuu viidessä sekunnissa vain vähän, eikä yksittäistä siirtymää välttämättä erota silmällä.
2. Auringonlaskun lähestyessä värit vaihtuvat iltaruskon kautta tummemmaksi. Yöllä näkyy edelleen himmeä yökaari (päiväkaarta ei näytetä kirkkaana).
3. Pidä ohjelma auki keskiyön yli: Tänään / Huomenna ja kuukauden taulukko päivittyvät ilman uudelleenkäynnistystä. Tätä ei tarvitse odottaa testatakseen pelkkää kalenterilogiikkaa: `node tests/test_live_calendar.js` testaa lokakuun–marraskuun vaihteen, DST-päivän, vuodenvaihteen sekä käyttäjän omat päivä-/kuukausivalinnat.
4. Jos selaat kuukausinuolilla menneeseen tai tulevaan kuukauteen, sitä **ei siirretä keskiyöllä**. Kuukausi-valikon painaminen palauttaa kuluvan kuukauden näyttöön.

## Testaus ja rajaukset

- Tässä rakennusympäristössä ajetut testit: Pythonin `unittest`-testit ja erillinen **Node.js**-testi (kalenterin päiväsiirtymät sekä maiseman väripaletit). `ctest` ajaa Node-testin, jos Node on asennettu. **Node.js ei ole ohjelman ajonaikainen riippuvuus**.
- Varsinaista Qt-käännöstä ja grafiikan näyttöä ei voitu suorittaa tämän ympäristön puuttuvien Qt-kehityskirjastojen vuoksi. Ne varmistetaan Fedorassa yllä olevilla komennoilla.
- Mukana on Flatpak-rakennusresepti ja skripti, mutta **ZIP ei ole valmiiksi käännetty Flatpak**. Varsinainen `.flatpak` tuotetaan paikallisesti Fedora-koneella.
- Laskennan UTC/DST-tarkkuutta ei muutettu. Vuosi 2026 -vertailu on käyttäjän aiemmin Fedorassa suorittama v0.6.2-tulos, ei tässä ympäristössä uusiksi tehty mittaus.
