cmake_minimum_required(VERSION 3.22)

# Sick & Sad plugin collection. Each plugin lives in plugins/<Name>; reusable
# DSP and UI pieces live in shared/.
project(SickAndSad VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

set(SNS_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shared)

add_subdirectory(plugins/Vox)

# Offline DSP tests (not installed anywhere)
juce_add_console_app(PitchTests PRODUCT_NAME "PitchTests")
target_sources(PitchTests PRIVATE tests/PitchTests.cpp)
target_include_directories(PitchTests PRIVATE ${SNS_SHARED_DIR})
target_compile_definitions(PitchTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PitchTests PRIVATE juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
