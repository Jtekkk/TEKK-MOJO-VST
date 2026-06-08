cmake_minimum_required(VERSION 3.22)
project(TekkMojoVST VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TekkMojoVST
    COMPANY_NAME            "TekkMojo"
    PLUGIN_MANUFACTURER_CODE Tmoj
    PLUGIN_CODE             Tmj1
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS                 VST3 Standalone
    PRODUCT_NAME            "Tekk Mojo"
    # The VST3 moduleinfo.json helper (moduleinfotool) #includes <Windows.h>
    # with a capital W, which fails under case-sensitive MinGW cross-builds.
    # The JSON is an optional scan-speed hint; the plugin loads fine without it.
    VST3_AUTO_MANIFEST      FALSE
)

target_sources(TekkMojoVST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/dsp/MojoEngine.cpp
)

target_include_directories(TekkMojoVST PRIVATE Source)

target_compile_definitions(TekkMojoVST PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

juce_generate_juce_header(TekkMojoVST)

target_link_libraries(TekkMojoVST
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# MSVC auto-links the Win32 system libraries via #pragma comment(lib, ...),
# but MinGW ignores those pragmas, so list them explicitly when cross-compiling.
if(MINGW)
    target_link_libraries(TekkMojoVST PRIVATE
        gdi32 user32 kernel32 comdlg32 ole32 oleaut32 uuid winmm wininet
        version ws2_32 shlwapi imm32 comctl32 winhttp rpcrt4 shell32
        advapi32 gdiplus)
endif()

# ---- DSP unit tests ----
enable_testing()

# Standalone test: DSP primitives only — no JUCE dependency
add_executable(dsp_primitives_test tests/dsp_primitives_test.cpp)
target_include_directories(dsp_primitives_test PRIVATE Source)
target_compile_features(dsp_primitives_test PRIVATE cxx_std_17)
add_test(NAME DSPPrimitives COMMAND dsp_primitives_test)

# JUCE-based engine test
juce_add_console_app(engine_test PRODUCT_NAME "EngineTest")
target_sources(engine_test PRIVATE
    tests/engine_test.cpp
    Source/dsp/MojoEngine.cpp
    Source/PresetManager.cpp
)
target_include_directories(engine_test PRIVATE Source)
target_compile_definitions(engine_test PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)
target_link_libraries(engine_test
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
juce_generate_juce_header(engine_test)
add_test(NAME EngineTest COMMAND engine_test)
