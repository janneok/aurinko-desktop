#!/usr/bin/env bash
# Asenna vain tämän käyttäjän KDE/työpöydälle, ei sudoa.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${1:-$root/build/aurinko-desktop}"
if [[ ! -x "$binary" ]]; then
  echo "Ohjelmaa ei löydy: $binary" >&2
  echo 'Käännä ensin: cmake -S . -B build -G Ninja && cmake --build build --parallel' >&2
  exit 1
fi
prefix="${XDG_DATA_HOME:-$HOME/.local/share}"
install -Dm755 "$binary" "$HOME/.local/bin/aurinko-desktop"
install -Dm644 "$root/resources/fi.oksanen.Aurinko.svg" \
  "$prefix/icons/hicolor/scalable/apps/fi.oksanen.Aurinko.svg"
# Käytetään absoluuttista polkua: desktop-käynnistimien PATH ei aina sisällä ~/.local/bin.
exec_escaped="${HOME//\\/\\\\}/.local/bin/aurinko-desktop"
exec_escaped="${exec_escaped//\"/\\\"}"
mkdir -p "$prefix/applications"
cat > "$prefix/applications/fi.oksanen.Aurinko.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Aurinko
Comment=Auringonnousu, -lasku ja päivän pituus
Exec="$exec_escaped"
Icon=fi.oksanen.Aurinko
Categories=Utility;Education;
Terminal=false
StartupNotify=true
EOF
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$prefix/applications" || true
fi
echo 'Asennettu käyttäjälle. Hae KDE:n sovellusvalikosta: Aurinko'
echo "Ohjelma: $HOME/.local/bin/aurinko-desktop"
