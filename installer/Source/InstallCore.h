#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <functional>

// ENCORE. VST Setup: the part that does the work, with no window, so SetupTests
// can drive it against a temporary folder. The window (Main.cpp) only chooses
// which plugins to keep and shows the progress.
namespace setup
{
    struct PluginInfo
    {
        const char* title;    // what people see: "VOX"
        const char* bundle;   // the VST3 bundle's name, without ".vst3": "Encore Vox"
        const char* legacy;   // its name before the rename ("Sick & Sad Vox"), removed so hosts don't list it twice
        const char* blurb;    // one line on the card
        const char* shot;     // the resource holding its screenshot
    };

    inline constexpr int numPlugins = 5;
    extern const std::array<PluginInfo, numPlugins> plugins;
    using Selection = std::array<bool, numPlugins>;

    // Where every DAW looks for VST3 plugins on Windows
    juce::File defaultVst3Folder();

    // "" when the plugin isn't in vst3Dir; its version from the bundle's moduleinfo.json,
    // or "?" when the bundle is there but doesn't say
    juce::String installedVersion (const juce::File& vst3Dir, const PluginInfo&);

    // A bundle whose plugin file a DAW has open can't be replaced or removed
    bool isInUse (const juce::File& vst3Dir, const PluginInfo&);

    enum class Status { waiting, working, installed, removed, unchanged, inUse, failed };

    struct Options
    {
        juce::File vst3Dir = defaultVst3Folder();
        bool registerWithWindows = true;              // Apps and features entry + a copy of the setup to modify/uninstall with
        juce::String registryKey = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\EncoreVST";
        juce::File setupFolder;                       // where the setup keeps its own copy (default: Program Files\ENCORE\VST Setup)
        juce::File thisSetup = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
        juce::String version;
    };

    // Plugins that keep[i] wants are installed (or replaced); the rest that are
    // installed are removed. report() is called from the calling thread as each
    // plugin starts and ends. Returns what happened to each.
    std::array<Status, numPlugins> apply (juce::ZipFile& payload, const Options&, const Selection& keep,
                                          const std::function<void (int, Status)>& report = {});

    // Which of the plugins the change would touch are open in a DAW right now
    juce::StringArray pluginsInUse (const Options&, const Selection& keep);

    // Windows bits (Platform.cpp)
    namespace platform
    {
        juce::File knownFolder (bool commonFiles);    // Common Files, or Program Files (64-bit)
        bool fileIsLocked (const juce::File&);
        juce::MemoryBlock resource (const char* name);
        void deleteFolderAfterExit (const juce::File&);
        bool isElevated();
    }
}
