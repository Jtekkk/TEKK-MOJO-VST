#!/bin/bash
# Cross-compilation wrapper for juce_vst3_helper.
# The native Linux helper binary cannot load a Windows PE DLL via dlopen,
# so it can't generate moduleinfo.json for cross-compiled targets.
# Since the plugin codes (PLUGIN_CODE / PLUGIN_MANUFACTURER_CODE) are the same
# in both builds, the moduleinfo.json content is identical — we just copy it.
#
# Setup (run once before cross-compiling):
#   cp cmake/juce_vst3_helper_wrapper.sh /usr/local/bin/juce_vst3_helper_wrapper
#   chmod +x /usr/local/bin/juce_vst3_helper_wrapper
#   ln -sf /usr/local/bin/juce_vst3_helper_wrapper /usr/local/bin/juce_vst3_helper

LINUX_JSON="$(dirname "$0")/../../build/TekkMojoVST_artefacts/Release/VST3/Tekk Mojo.vst3/Contents/Resources/moduleinfo.json"
LINUX_JSON_ABS="/home/user/TEKK-MOJO-VST/build/TekkMojoVST_artefacts/Release/VST3/Tekk Mojo.vst3/Contents/Resources/moduleinfo.json"

output_path=""
prev=""
for arg in "$@"; do
    if [ "$prev" = "-output" ]; then
        output_path="$arg"
    fi
    prev="$arg"
done

if [ -n "$output_path" ] && [ -f "$LINUX_JSON_ABS" ]; then
    mkdir -p "$(dirname "$output_path")"
    cp "$LINUX_JSON_ABS" "$output_path"
    exit 0
fi

exec /home/user/TEKK-MOJO-VST/build/juce_vst3_helper "$@"
