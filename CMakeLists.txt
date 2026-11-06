cmake_minimum_required(VERSION 3.22)

# Mac builds run on macOS 11 and newer (set before project() so it sticks)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# The ENCORE. VST plugins. Each plugin lives in plugins/<Name>; the UI pieces they
# share live in shared/. The audio processing code comes from the
# encore-audio-processing submodule (external/encore-audio-processing).
project(EncorePlugins VERSION 0.6.0)

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

# Plugin formats. The default is VST3 + Standalone everywhere, plus AU on Mac
# (Logic and GarageBand only load AU) and LV2 on Linux (Ardour, Carla, etc.).
# CI builds only what it ships, e.g. -DENCORE_PLUGIN_FORMATS=VST3 or "VST3;LV2".
set(_encore_default_formats VST3 Standalone)
if(APPLE)
    list(APPEND _encore_default_formats AU)
elseif(UNIX)
    list(APPEND _encore_default_formats LV2)
endif()
set(ENCORE_PLUGIN_FORMATS "${_encore_default_formats}" CACHE STRING "Plugin formats to build (VST3, LV2, AU, Standalone)")

# Link-time optimisation for the plugins and the installer. Off by default: it
# made no measurable difference to Master's CPU use (see README, "Build speed")
# and costs a lot of link time. THIN is Clang's ThinLTO, or MSVC's /GL with
# incremental LTCG. JUCE's juce_recommended_lto_flags (full LTO) are not used.
set(ENCORE_LTO "OFF" CACHE STRING "OFF or THIN")
set_property(CACHE ENCORE_LTO PROPERTY STRINGS OFF THIN)
add_library(encore_lto INTERFACE)
if(ENCORE_LTO STREQUAL "THIN")
    if(MSVC)
        target_compile_options(encore_lto INTERFACE /GL)
        target_link_options(encore_lto INTERFACE /LTCG:INCREMENTAL)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(encore_lto INTERFACE -flto=thin)
        target_link_options(encore_lto INTERFACE -flto=thin)
    else()
        message(FATAL_ERROR "ENCORE_LTO=THIN needs Clang or MSVC")
    endif()
elseif(NOT ENCORE_LTO STREQUAL "OFF")
    message(FATAL_ERROR "ENCORE_LTO must be OFF or THIN")
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
#
# JUCE is compiled once per plugin, inside each plugin's shared-code library
# (juce_add_plugin does that). The tests link those libraries instead of
# compiling JUCE and the plugin sources again: they take the plugin's include
# directories and definitions, and link its library (which already holds JUCE
# and the plugin's processor and editor).
function(encore_plugin_test name plugin)
    add_executable(${name} ${ARGN})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/${name}_artefacts/$<CONFIG>")
    target_include_directories(${name} PRIVATE $<TARGET_PROPERTY:${plugin},INCLUDE_DIRECTORIES>)
    target_compile_definitions(${name} PRIVATE $<TARGET_PROPERTY:${plugin},COMPILE_DEFINITIONS>)
    target_link_libraries(${name} PRIVATE ${plugin} EncoreFonts juce::juce_recommended_config_flags)
endfunction()

option(ENCORE_VST_TESTS "Build the plugin tests" ON)
if(ENCORE_VST_TESTS)
    enable_testing()

    # The shared user-preset system, round trip (any plugin's library has what it needs)
    encore_plugin_test(PresetTests Master tests/PresetTests.cpp)
    add_test(NAME PresetTests COMMAND PresetTests)

    # Strum's suite comes from encore-audio-processing. Built with the plugin and
    # ENCORE_STRUM_PLUGIN_TESTS=1, it also checks Learn, state and presets end to
    # end the way a host would drive them.
    encore_plugin_test(StrumTests Strum ${ENCORE_AUDIO_PROCESSING_DIR}/tests/StrumTests.cpp)
    target_compile_definitions(StrumTests PRIVATE ENCORE_STRUM_PLUGIN_TESTS=1)
    add_test(NAME StrumTests COMMAND StrumTests)

    # Each plugin's editor drawn to a PNG (EditorShot.cpp), for checking the look.
    # The installer uses these for its plugin cards.
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/shots")
    foreach(shot Vox:Vox Verb:Verb EQ:EQ Master:Master Strum:Strum)
        string(REPLACE ":" ";" shot "${shot}")
        list(GET shot 0 name)
        list(GET shot 1 plugin)
        encore_plugin_test(Shot${name} ${plugin} tests/EditorShot.cpp)
        add_test(NAME Shot${name} COMMAND Shot${name} "${CMAKE_BINARY_DIR}/shots/${name}.png")
    endforeach()
endif()

# MasterBench: Master's CPU use on a fixed test file (for the LTO measurement)
option(ENCORE_VST_BENCH "Build MasterBench" OFF)
if(ENCORE_VST_BENCH)
    encore_plugin_test(MasterBench Master tests/MasterBench.cpp)
endif()

# The branded installer (Windows only; needs a payload from installer/package.ps1)
if(WIN32)
    add_subdirectory(installer)
endif()
