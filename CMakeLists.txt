cmake_minimum_required(VERSION 3.22)
project(TekkMojoVST VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Statically embed mingw runtimes so the plugin runs without extra DLLs.
if(MINGW)
    # Statically embed libgcc and libstdc++ runtimes so those DLLs aren't required.
    # libwinpthread-1.dll can't be statically linked via the posix thread spec, so
    # it is bundled alongside the VST3 DLL via the post-build step below.
    set(_MINGW_STATIC "-static-libgcc -static-libstdc++")
    string(APPEND CMAKE_EXE_LINKER_FLAGS    " ${_MINGW_STATIC}")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " ${_MINGW_STATIC}")
    string(APPEND CMAKE_MODULE_LINKER_FLAGS " ${_MINGW_STATIC}")
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.9
)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(TEKK_FORMATS VST3 AU Standalone)
else()
    set(TEKK_FORMATS VST3 Standalone)
endif()

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
    FORMATS                 ${TEKK_FORMATS}
    PRODUCT_NAME            "Tekk Mojo"
)

# Bundle libwinpthread-1.dll inside the VST3 so it loads without extra installs.
if(MINGW)
    set(_WINPTH_DLL "/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll")
    if(EXISTS "${_WINPTH_DLL}")
        add_custom_command(TARGET TekkMojoVST_VST3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_WINPTH_DLL}"
                "$<TARGET_FILE_DIR:TekkMojoVST_VST3>/libwinpthread-1.dll"
            COMMENT "Bundling libwinpthread-1.dll into VST3 bundle"
        )
    endif()
endif()

target_sources(TekkMojoVST PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/dsp/MojoEngine.cpp
    Source/MeterSource.h  # header-only, listed so IDEs pick it up
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
