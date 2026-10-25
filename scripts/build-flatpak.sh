#!/usr/bin/env bash
# Rakentaa siirrettävän .flatpak-tiedoston omalle koneelle, ei vaadi sudoa.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

for command in flatpak flatpak-builder; do
    if ! command -v "$command" >/dev/null 2>&1; then
        echo "Puuttuva työkalu: $command" >&2
        echo 'Fedora: sudo dnf install flatpak flatpak-builder' >&2
        exit 1
    fi
done

output="${1:-${HOME}/Projects/aurinko-flatpak-output}"
mkdir -p "$output"
output="$(cd "$output" && pwd -P)"
# Lähteen kopiointi buildiin ei saa päätyä kopioimaan samaa kansiota uudelleen.
case "$output/" in
    "$root/"*) echo 'Valitse tuloshakemisto lähdekoodikansion ulkopuolelta.' >&2; exit 1 ;;
esac

app_id='fi.oksanen.Aurinko'
version="$(sed -nE 's/^project\(AurinkoDesktop VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' "$root/CMakeLists.txt")"
if [[ -z "$version" ]]; then
    echo 'CMake-versionumeroa ei löytynyt' >&2; exit 1
fi
arch="$(flatpak --default-arch)"
manifest="$root/flatpak/${app_id}.yml"
repo="$output/repo"
build="$output/build"
state="$output/state"
bundle="$output/Aurinko-${version}-${arch}.flatpak"

# Flathub käyttäjäkohtaiseksi: pakettien riippuvuudet haetaan sieltä.
flatpak remote-add --user --if-not-exists flathub \
    https://dl.flathub.org/repo/flathub.flatpakrepo

flatpak-builder \
    --default-branch=stable \
    --user \
    --force-clean \
    --install-deps-from=flathub \
    --state-dir="$state" \
    --repo="$repo" \
    "$build" "$manifest"

# Korvaa vain saman version aiempi paikallinen paketti uudelleen käännettäessä.
rm -f "$bundle"
flatpak build-bundle \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    "$repo" "$bundle" "$app_id" stable

echo
echo 'Flatpak-paketti valmis:'
echo "$bundle"
echo 'Asenna paikalliseen käyttäjätiliin komennolla:'
echo "flatpak install --user '$bundle'"
echo 'Testaa käynnistys: flatpak run fi.oksanen.Aurinko'
