#!/usr/bin/env bash
set -euo pipefail
prefix="${XDG_DATA_HOME:-$HOME/.local/share}"
rm -f "$HOME/.local/bin/aurinko-desktop" \
  "$prefix/applications/fi.oksanen.Aurinko.desktop" \
  "$prefix/icons/hicolor/scalable/apps/fi.oksanen.Aurinko.svg"
if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$prefix/applications" || true
fi
echo 'Aurinko poistettu sovellusvalikosta. Tallennettuja sijainteja ei poistettu.'
