set (SUBHARM_FORMATS VST3 Standalone)
if (APPLE)
    list (APPEND SUBHARM_FORMATS AU)
endif()

juce_add_plugin (SubHarm
    COMPANY_NAME "SubRoo"
    COMPANY_WEBSITE "https://subroo.xyz"
    BUNDLE_ID "xyz.subroo.subharm"
    PLUGIN_MANUFACTURER_CODE Sbro
    PLUGIN_CODE Shrm
    PRODUCT_NAME "SubHarm"
    VERSION ${PROJECT_VERSION}
    FORMATS ${SUBHARM_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Vocals
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources (SubHarm PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    TimelineView.cpp)

target_compile_definitions (SubHarm PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    SUBHARM_VERSION="${PROJECT_VERSION}")

target_link_libraries (SubHarm
    PRIVATE
        subharm_core
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
