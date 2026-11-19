add_executable (subharm_core_test core_test.cpp)
target_link_libraries (subharm_core_test PRIVATE subharm_core)

if (SUBHARM_BUILD_PLUGIN)
    juce_add_console_app (subharm_host_test PRODUCT_NAME "subharm_host_test")
    target_sources (subharm_host_test PRIVATE
        host_test.cpp
        ../src/plugin/PluginProcessor.cpp
        ../src/plugin/PluginEditor.cpp
        ../src/plugin/TimelineView.cpp)
    target_compile_definitions (subharm_host_test PRIVATE
        JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries (subharm_host_test PRIVATE
        subharm_core juce::juce_audio_utils juce::juce_audio_formats juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
