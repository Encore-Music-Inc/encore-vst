juce_add_plugin(Strum
    COMPANY_NAME "Encore"
    BUNDLE_ID com.encoremusic.strum
    PLUGIN_MANUFACTURER_CODE SnSd
    PLUGIN_CODE SsSt
    FORMATS ${ENCORE_PLUGIN_FORMATS}
    LV2URI "https://encoremusic.studio/plugins/strum"
    PRODUCT_NAME "Encore Strum"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx EQ Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Strum
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(Strum PRIVATE Source ${ENCORE_INCLUDE_DIRS})

target_compile_definitions(Strum
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Strum
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        EncoreFonts
    PUBLIC
        juce::juce_recommended_config_flags
        encore_lto
        juce::juce_recommended_warning_flags)
