#!/bin/sh
# Installs Purrrstressor for your user only (no admin rights needed).
# The VST3 goes to ~/.vst3, the folder most Linux hosts scan. Set VST3_DIR to use another folder.
set -e
here=$(cd "$(dirname "$0")" && pwd)
dest="${VST3_DIR:-$HOME/.vst3}"
if [ ! -d "$here/Purrrstressor.vst3" ]; then
  echo "Purrrstressor.vst3 was not found next to this script." >&2
  exit 1
fi
mkdir -p "$dest"
rm -rf "$dest/Purrrstressor.vst3"
cp -R "$here/Purrrstressor.vst3" "$dest/"
echo "Installed: $dest/Purrrstressor.vst3"
echo "Rescan plugins in your host. To remove it later, run uninstall.sh."
