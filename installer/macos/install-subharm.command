#!/bin/bash
# Installs SubHarm VST3 + AU for the current user. Double-click to run.
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/SubHarm.vst3 ~/Library/Audio/Plug-Ins/Components/SubHarm.component
cp -R SubHarm.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R SubHarm.component ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/SubHarm.vst3 ~/Library/Audio/Plug-Ins/Components/SubHarm.component 2>/dev/null
killall -9 AudioComponentRegistrar 2>/dev/null
echo "SubHarm installed. Restart your DAW (Logic: it rescans Audio Units on launch)."
