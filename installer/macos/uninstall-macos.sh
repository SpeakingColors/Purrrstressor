#!/bin/sh
# Removes the Purrrstressor plugin installed by the package. Your presets are kept unless you say yes.
set -e
sudo rm -rf "/Library/Audio/Plug-Ins/VST3/Purrrstressor.vst3"
sudo pkgutil --forget com.studio87.purrrstressor.vst3 >/dev/null 2>&1 || true
echo "Removed the plugin."
data="$HOME/Library/Application Support/Studio 87/Purrrstressor"
if [ -d "$data" ]; then
  printf "Also delete your presets in %s ? [y/N] " "$data"
  read -r answer || answer=n
  case "$answer" in
    y|Y|yes|YES) rm -rf "$data"; echo "Presets deleted." ;;
    *) echo "Presets kept." ;;
  esac
fi
