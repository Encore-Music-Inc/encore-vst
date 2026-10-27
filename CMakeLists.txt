cmake_minimum_required(VERSION 3.22)

# Encore plugin collection. Each plugin lives in plugins/<Name>; reusable
# DSP and UI pieces live in shared/.
project(EncorePlugins VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

set(SNS_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_subdirectory(plugins/Vox)
add_subdirectory(plugins/Verb)
add_subdirectory(plugins/EQ)
add_subdirectory(plugins/Master)

# Offline DSP tests (not installed anywhere)
juce_add_console_app(PitchTests PRODUCT_NAME "PitchTests")
target_sources(PitchTests PRIVATE tests/PitchTests.cpp)
target_include_directories(PitchTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(PitchTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PitchTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(PresetTests PRODUCT_NAME "PresetTests")
target_sources(PresetTests PRIVATE tests/PresetTests.cpp)
target_include_directories(PresetTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(PresetTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PresetTests PRIVATE juce::juce_audio_processors PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(VerbTests PRODUCT_NAME "VerbTests")
target_sources(VerbTests PRIVATE tests/VerbTests.cpp)
target_include_directories(VerbTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(VerbTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(VerbTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(EqTests PRODUCT_NAME "EqTests")
target_sources(EqTests PRIVATE tests/EqTests.cpp)
target_include_directories(EqTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(EqTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(EqTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(MasterTests PRODUCT_NAME "MasterTests")
target_sources(MasterTests PRIVATE tests/MasterTests.cpp)
target_include_directories(MasterTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(MasterTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MasterTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)

# The browser build's configuration (no JUCE), tested natively
add_executable(LiteMasterTests tests/LiteMasterTests.cpp)
target_include_directories(LiteMasterTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(LiteMasterTests PRIVATE VS_NO_JUCE=1 _USE_MATH_DEFINES)
if (MSVC)
    target_compile_options(LiteMasterTests PRIVATE /O2 /fp:fast)
endif()
