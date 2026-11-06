// Checks for ENCORE. VST Setup's install logic, against a temporary folder and a
// test registry key (never the real VST3 folder or Apps and features).
// Run: build\installer\SetupTests_artefacts\Release\SetupTests.exe
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include "InstallCore.h"

namespace
{
    int failures = 0;
    void expect (const juce::String& name, bool ok, const juce::String& detail = {})
    {
        std::printf ("%s %-62s %s\n", ok ? "PASS" : "FAIL", name.toRawUTF8(), detail.toRawUTF8());
        if (! ok) ++failures;
    }
    juce::File bundle (const juce::File& dir, int i) { return dir.getChildFile (juce::String (setup::plugins[(size_t) i].bundle) + ".vst3"); }
    juce::File dll (const juce::File& dir, int i)
    {
        const juce::String n = setup::plugins[(size_t) i].bundle;
        return bundle (dir, i).getChildFile ("Contents/x86_64-win/" + n + ".vst3");
    }
}

int main()
{
    juce::ZipFile zip { juce::File (ENCORE_VST_PAYLOAD_ZIP) };
    expect ("payload has entries", zip.getNumEntries() > 0, juce::String (zip.getNumEntries()) + " entries");

    const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("encore-setup-test");
    root.deleteRecursively();
    const auto vst = root.getChildFile ("VST3");
    setup::Options o;
    o.vst3Dir = vst;
    o.version = "9.9.9";
    o.registryKey = "HKEY_CURRENT_USER\\Software\\EncoreVstSetupTestEntry";
    o.setupFolder = root.getChildFile ("Setup");
    o.thisSetup = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::WindowsRegistry::deleteKey (o.registryKey);

    // 1. a fresh install of Vox and Strum
    std::vector<std::pair<int, setup::Status>> reports;
    auto r = setup::apply (zip, o, { true, false, false, false, true }, [&] (int i, setup::Status s) { reports.push_back ({ i, s }); });
    expect ("fresh: Vox and Strum installed", r[0] == setup::Status::installed && r[4] == setup::Status::installed);
    expect ("fresh: the others left alone", r[1] == setup::Status::unchanged && r[2] == setup::Status::unchanged && r[3] == setup::Status::unchanged);
    expect ("fresh: Vox's plugin file is there", dll (vst, 0).getSize() > 1000000, juce::String (dll (vst, 0).getSize()) + " bytes");
    expect ("fresh: moduleinfo gives a version", setup::installedVersion (vst, setup::plugins[0]).containsChar ('.'), setup::installedVersion (vst, setup::plugins[0]));
    expect ("fresh: Verb reads as not installed", setup::installedVersion (vst, setup::plugins[1]).isEmpty());
    expect ("fresh: progress reported working then installed", reports.size() >= 4 && reports[0].second == setup::Status::working);
    expect ("fresh: no leftover .old folders", vst.findChildFiles (juce::File::findDirectories, false, "*.tmp").isEmpty());

    // 2. the Windows entry
    const auto key = o.registryKey + "\\";
    expect ("registry: DisplayName", juce::WindowsRegistry::getValue (key + "DisplayName") == "ENCORE. VST");
    expect ("registry: version", juce::WindowsRegistry::getValue (key + "DisplayVersion") == "9.9.9");
    expect ("registry: uninstall runs the kept setup", juce::WindowsRegistry::getValue (key + "UninstallString").contains ("--uninstall"));
    expect ("registry: setup copied for later", o.setupFolder.getChildFile ("ENCORE VST Setup.exe").existsAsFile());
    expect ("registry: size counted", juce::WindowsRegistry::getValue (key + "EstimatedSize").getIntValue() > 1000,
            juce::WindowsRegistry::getValue (key + "EstimatedSize") + " KB");

    // 3. a change: keep Vox, add Verb, drop Strum; a stray file in Vox's old copy must go
    bundle (vst, 0).getChildFile ("Contents/stray.txt").replaceWithText ("x");
    r = setup::apply (zip, o, { true, true, false, false, false });
    expect ("change: Vox replaced", r[0] == setup::Status::installed && ! bundle (vst, 0).getChildFile ("Contents/stray.txt").exists());
    expect ("change: Verb added", r[1] == setup::Status::installed && dll (vst, 1).existsAsFile());
    expect ("change: Strum removed", r[4] == setup::Status::removed && ! bundle (vst, 4).exists());

    // 4. a DAW holding Vox open: nothing of Vox changes, it says so
    {
        const auto h = CreateFileW (dll (vst, 0).getFullPathName().toWideCharPointer(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        expect ("in use: test can hold the file", h != INVALID_HANDLE_VALUE);
        expect ("in use: detected", setup::isInUse (vst, setup::plugins[0]));
        expect ("in use: named for the window", setup::pluginsInUse (o, { true, true, false, false, false }).contains ("ENCORE. VOX"));
        const auto before = dll (vst, 0).getSize();
        r = setup::apply (zip, o, { false, true, false, false, false });
        expect ("in use: reported, not removed", r[0] == setup::Status::inUse && dll (vst, 0).getSize() == before);
        r = setup::apply (zip, o, { true, true, false, false, false });
        expect ("in use: not replaced either", r[0] == setup::Status::inUse && dll (vst, 0).getSize() == before);
        expect ("in use: Verb still done", r[1] == setup::Status::installed);
        CloseHandle (h);
        expect ("in use: free again", ! setup::isInUse (vst, setup::plugins[0]));
    }

    // 5. uninstall everything: plugins, the entry and the kept setup all go
    r = setup::apply (zip, o, { false, false, false, false, false });
    expect ("uninstall: Vox and Verb removed", r[0] == setup::Status::removed && r[1] == setup::Status::removed);
    expect ("uninstall: VST3 folder holds none of ours", vst.findChildFiles (juce::File::findDirectories, false, "ENCORE *.vst3").isEmpty());
    expect ("uninstall: registry entry gone", ! juce::WindowsRegistry::keyExists (o.registryKey));
    expect ("uninstall: kept setup gone", ! o.setupFolder.exists());

    // 6. a bundle without moduleinfo (installed by hand) reads as "?"
    bundle (vst, 2).getChildFile ("Contents").createDirectory();
    expect ("hand-installed bundle reads \"?\"", setup::installedVersion (vst, setup::plugins[2]) == "?");

    juce::WindowsRegistry::deleteKey (o.registryKey);
    root.deleteRecursively();
    std::printf ("\n%s: %d failure(s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
