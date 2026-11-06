juce_add_plugin(EQ
    COMPANY_NAME "ENCORE."
    BUNDLE_ID com.encoremusic.eq
    PLUGIN_MANUFACTURER_CODE Encr
    PLUGIN_CODE EnEq
    FORMATS ${ENCORE_PLUGIN_FORMATS}
    LV2URI "https://encoremusic.studio/plugins/eq"
    PRODUCT_NAME "ENCORE Equalize"
    PLUGIN_NAME "ENCORE. Equalize"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EQ
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(EQ PRIVATE Source ${ENCORE_INCLUDE_DIRS})

target_compile_definitions(EQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        EncoreFonts
    PUBLIC
        juce::juce_recommended_config_flags
        encore_lto
        juce::juce_recommended_warning_flags)
