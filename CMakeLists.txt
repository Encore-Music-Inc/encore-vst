cmake_minimum_required(VERSION 3.22)

# Mac builds run on macOS 11 and newer (set before project() so it sticks)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# The ENCORE. VST plugins. Each plugin lives in plugins/<Name>; the UI pieces they
# share live in shared/. The audio processing code comes from the
# encore-audio-processing submodule (external/encore-audio-processing).
project(EncorePlugins VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Submodules. Initialise them without --recursive: encore-audio-processing has
# its own external/JUCE, which is only for its own tests.
foreach(sub JUCE encore-audio-processing)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/${sub}/CMakeLists.txt"
       AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/${sub}/dsp/MasterChain.h")
        message(FATAL_ERROR "external/${sub} is missing. Run:\n"
            "  git submodule update --init external/JUCE external/encore-audio-processing\n"
            "(not --recursive)")
    endif()
endforeach()

add_subdirectory(external/JUCE)

# Include path for the plugins: shared/ for "ui/..." and "EncoreProcessorBase.h",
# and the encore-audio-processing root for "dsp/...".
set(ENCORE_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)
set(ENCORE_AUDIO_PROCESSING_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/encore-audio-processing)
set(ENCORE_INCLUDE_DIRS ${ENCORE_SHARED_DIR} ${ENCORE_AUDIO_PROCESSING_DIR})

# Plugin formats per platform: VST3 + Standalone everywhere, plus AU on Mac
# (Logic and GarageBand only load AU) and LV2 on Linux (Ardour, Carla, etc.)
set(ENCORE_PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ENCORE_PLUGIN_FORMATS AU)
elseif(UNIX)
    list(APPEND ENCORE_PLUGIN_FORMATS LV2)
endif()

# The ENCORE. typefaces (DM Sans, Space Mono; SIL OFL), built into every editor
juce_add_binary_data(EncoreFonts
    HEADER_NAME EncoreFonts.h
    NAMESPACE EncoreFonts
    SOURCES
        shared/ui/fonts/DMSans-Regular.ttf
        shared/ui/fonts/DMSans-Bold.ttf
        shared/ui/fonts/SpaceMono-Regular.ttf
        shared/ui/fonts/SpaceMono-Bold.ttf)

add_subdirectory(plugins/Vox)
add_subdirectory(plugins/Verb)
add_subdirectory(plugins/EQ)
add_subdirectory(plugins/Master)
add_subdirectory(plugins/Strum)

# Plugin-level tests. The audio processing suites (Pitch, Verb, Eq, Master,
# Lite) live in encore-audio-processing and run there.
option(ENCORE_VST_TESTS "Build the plugin tests" ON)
if(ENCORE_VST_TESTS)
    enable_testing()

    # The shared user-preset system, round trip
    juce_add_console_app(PresetTests PRODUCT_NAME "PresetTests")
    target_sources(PresetTests PRIVATE tests/PresetTests.cpp)
    target_include_directories(PresetTests PRIVATE ${ENCORE_INCLUDE_DIRS})
    target_compile_definitions(PresetTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PresetTests PRIVATE juce::juce_audio_processors PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME PresetTests COMMAND PresetTests)

    # Strum's suite comes from encore-audio-processing. Built here with the plugin's
    # own processor and ENCORE_STRUM_PLUGIN_TESTS=1, it also checks Learn, state and
    # presets end to end the way a host would drive them.
    juce_add_console_app(StrumTests PRODUCT_NAME "StrumTests")
    target_sources(StrumTests PRIVATE ${ENCORE_AUDIO_PROCESSING_DIR}/tests/StrumTests.cpp plugins/Strum/Source/PluginProcessor.cpp plugins/Strum/Source/PluginEditor.cpp)
    target_include_directories(StrumTests PRIVATE ${ENCORE_INCLUDE_DIRS} plugins/Strum/Source)
    target_compile_definitions(StrumTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 ENCORE_STRUM_PLUGIN_TESTS=1 "JucePlugin_Name=\"Encore Strum\"")
    target_link_libraries(StrumTests PRIVATE juce::juce_audio_utils juce::juce_dsp EncoreFonts PUBLIC juce::juce_recommended_config_flags)
    add_test(NAME StrumTests COMMAND StrumTests)

    # Each plugin's editor drawn to a PNG (EditorShot.cpp), for checking the look.
    # The installer uses these for its plugin cards.
    foreach(shot Vox Verb EQ Master Strum)
        juce_add_console_app(Shot${shot} PRODUCT_NAME "Shot${shot}")
        target_sources(Shot${shot} PRIVATE tests/EditorShot.cpp plugins/${shot}/Source/PluginProcessor.cpp plugins/${shot}/Source/PluginEditor.cpp)
        target_include_directories(Shot${shot} PRIVATE ${ENCORE_INCLUDE_DIRS} plugins/${shot}/Source)
        target_compile_definitions(Shot${shot} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 "JucePlugin_Name=\"Encore ${shot}\"")
        target_link_libraries(Shot${shot} PRIVATE juce::juce_audio_utils juce::juce_dsp EncoreFonts PUBLIC juce::juce_recommended_config_flags)
        add_test(NAME Shot${shot} COMMAND Shot${shot} "${CMAKE_BINARY_DIR}/shots/${shot}.png")
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shots")
endif()

# The branded installer (Windows only; needs a payload from installer/package.ps1)
if(WIN32)
    add_subdirectory(installer)
endif()
