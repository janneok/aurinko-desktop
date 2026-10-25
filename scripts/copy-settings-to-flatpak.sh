#!/usr/bin/env bash
# Vapaaehtoinen kertaluonteinen omien paikkojen tuonti native-Qt:stä Flatpakiin.
# Ei ylikirjoita olemassa olevaa Flatpak-konfiguraatiota.
set -euo pipefail
source_file="${XDG_CONFIG_HOME:-$HOME/.config}/Oksanen/Aurinko.conf"
target_file="$HOME/.var/app/fi.oksanen.Aurinko/config/Oksanen/Aurinko.conf"
if [[ ! -f "$source_file" ]]; then
    echo "Natiiviasennuksen asetustiedostoa ei löytynyt: $source_file" >&2
    exit 1
fi
if [[ -e "$target_file" ]]; then
    echo "Flatpakin asetukset ovat jo olemassa: $target_file" >&2
    echo 'Mitään ei ylikirjoitettu.' >&2
    exit 1
fi
if command -v flatpak >/dev/null 2>&1; then
    if flatpak ps --columns=application 2>/dev/null | grep -Fxq 'fi.oksanen.Aurinko'; then
        echo 'Sulje Aurinko Flatpak ennen asetusten tuontia.' >&2
        exit 1
    fi
fi
install -Dm600 "$source_file" "$target_file"
echo 'Omat sijainnit kopioitu Flatpakin asetuksiin, lähde säilyi ennallaan.'
