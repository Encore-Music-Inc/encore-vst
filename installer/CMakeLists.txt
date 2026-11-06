# ENCORE. VST Setup: the branded Windows installer for the plugins.
# installer/package.ps1 builds the plugins, puts them (zipped) and their
# screenshots in build/installer-payload, then configures again so these
# targets exist. Without a payload they're skipped.

set(ENCORE_VST_PAYLOAD "${CMAKE_BINARY_DIR}/installer-payload")
if(NOT WIN32 OR NOT EXISTS "${ENCORE_VST_PAYLOAD}/plugins.zip")
    message(STATUS "ENCORE. VST Setup: no payload yet (run installer/package.ps1)")
    return()
endif()

# The payload rides in the exe as Windows resources (fast to build, and the exe
# can still be code-signed). The hash makes the .rc change when the payload does.
file(SHA256 "${ENCORE_VST_PAYLOAD}/plugins.zip" ENCORE_VST_PAYLOAD_HASH)
set(rc "${CMAKE_CURRENT_BINARY_DIR}/Payload.rc")
file(WRITE "${rc}.new" "// payload ${ENCORE_VST_PAYLOAD_HASH}\n")
file(APPEND "${rc}.new" "PAYLOAD RCDATA \"${ENCORE_VST_PAYLOAD}/plugins.zip\"\n")
foreach(s VOX VERB EQ MASTER STRUM)
    file(APPEND "${rc}.new" "SHOT_${s} RCDATA \"${ENCORE_VST_PAYLOAD}/shots/${s}.png\"\n")
endforeach()
file(COPY_FILE "${rc}.new" "${rc}" ONLY_IF_DIFFERENT)

# EncoreVstSetup is the real one (asks for admin when it starts). The Dev build
# doesn't, and takes --dest <folder>, --no-register and --shots <folder>.
foreach(t EncoreVstSetup EncoreVstSetupDev)
    juce_add_gui_app(${t}
        PRODUCT_NAME "ENCORE VST Setup"
        COMPANY_NAME "ENCORE. Music"
        VERSION ${PROJECT_VERSION}
        ICON_BIG ${CMAKE_CURRENT_SOURCE_DIR}/icon.png)
    target_sources(${t} PRIVATE Source/Main.cpp Source/InstallCore.cpp Source/Platform.cpp "${rc}")
    target_include_directories(${t} PRIVATE Source ${ENCORE_INCLUDE_DIRS})
    target_compile_definitions(${t} PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0
        ENCORE_VST_VERSION="${PROJECT_VERSION}")
    target_link_libraries(${t} PRIVATE juce::juce_gui_basics EncoreFonts
        PUBLIC juce::juce_recommended_config_flags encore_lto)
endforeach()
target_link_options(EncoreVstSetup PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
target_compile_definitions(EncoreVstSetupDev PRIVATE ENCORE_SETUP_DEV=1)

# Install, replace, remove, in-use and the Windows entry, against a temp folder
juce_add_console_app(SetupTests PRODUCT_NAME "SetupTests")
target_sources(SetupTests PRIVATE ../tests/SetupTests.cpp Source/InstallCore.cpp Source/Platform.cpp)
target_include_directories(SetupTests PRIVATE Source)
target_compile_definitions(SetupTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    "ENCORE_VST_PAYLOAD_ZIP=\"${ENCORE_VST_PAYLOAD}/plugins.zip\"")
target_link_libraries(SetupTests PRIVATE juce::juce_core PUBLIC juce::juce_recommended_config_flags)
