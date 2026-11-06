#include "InstallCore.h"

namespace setup
{
    const std::array<PluginInfo, numPlugins> plugins {{
        { "VOX",      "Encore Vox",      "Sick & Sad Vox",    "Your whole vocal chain: clean up, tune, harmony, rider, tone and space.", "SHOT_VOX" },
        { "VERB",     "Encore Verb",     "Sick & Sad Verb",   "Reverb: rooms, chambers, plates, halls, springs and a huge shimmer.",     "SHOT_VERB" },
        { "EQUALIZE", "Encore Equalize", "Sick & Sad EQ",     "8-band EQ with a live spectrum, plus Smart carving and resonance cuts.",  "SHOT_EQ" },
        { "MASTER",   "Encore Master",   "Sick & Sad Master", "One-click mastering: pick a style, it learns your song, sets loudness.",  "SHOT_MASTER" },
        { "STRUM",    "Encore Strum",    "Sick & Sad Strum",  "Acoustic guitar fixer: a DI that sounds miked, less boom, squeak, pick.", "SHOT_STRUM" },
    }};

    static juce::File bundleIn (const juce::File& dir, const char* name)
    {
        return dir.getChildFile (juce::String (name) + ".vst3");
    }

    juce::File defaultVst3Folder()
    {
        return platform::knownFolder (true).getChildFile ("VST3");
    }

    juce::String installedVersion (const juce::File& vst3Dir, const PluginInfo& p)
    {
        const auto b = bundleIn (vst3Dir, p.bundle);
        if (! b.isDirectory())
            return {};
        // JUCE writes moduleinfo.json in a relaxed JSON (trailing commas), so read the one line
        const auto info = b.getChildFile ("Contents/Resources/moduleinfo.json").loadFileAsString();
        const auto at = info.indexOf ("\"Version\"");
        if (at < 0)
            return "?";
        const auto v = info.substring (at + 9).fromFirstOccurrenceOf ("\"", false, false).upToFirstOccurrenceOf ("\"", false, false);
        return v.isNotEmpty() ? v : "?";
    }

    bool isInUse (const juce::File& vst3Dir, const PluginInfo& p)
    {
        const auto b = bundleIn (vst3Dir, p.bundle);
        if (! b.isDirectory())
            return false;
        for (const auto& f : b.findChildFiles (juce::File::findFiles, true))
            if (platform::fileIsLocked (f))
                return true;
        return false;
    }

    juce::StringArray pluginsInUse (const Options& o, const Selection& keep)
    {
        juce::StringArray names;
        // every touched plugin: kept ones get replaced, unkept installed ones removed
        juce::ignoreUnused (keep);
        for (const auto& p : plugins)
            if (isInUse (o.vst3Dir, p))
                names.add (juce::String ("ENCORE. ") + p.title);
        return names;
    }

    // Puts the bundle's files from the payload into vst3Dir. False if the payload
    // has no such bundle or a file couldn't be written.
    static bool extract (juce::ZipFile& zip, const juce::File& vst3Dir, const PluginInfo& p)
    {
        const auto prefix = juce::String (p.bundle) + ".vst3/";
        int found = 0;
        for (int k = 0; k < zip.getNumEntries(); ++k)
        {
            const auto name = zip.getEntry (k)->filename.replaceCharacter ('\\', '/');
            if (! name.startsWith (prefix))
                continue;
            ++found;
            if (zip.uncompressEntry (k, vst3Dir, juce::ZipFile::OverwriteFiles::yes, juce::ZipFile::FollowSymlinks::no).failed())
                return false;
        }
        return found > 0 && bundleIn (vst3Dir, p.bundle).isDirectory();
    }

    static juce::int64 folderSize (const juce::File& dir)
    {
        juce::int64 n = 0;
        for (const auto& f : dir.findChildFiles (juce::File::findFiles, true))
            n += f.getSize();
        return n;
    }

    // The Apps and features entry, and the copy of the setup it runs to change or
    // remove the plugins. Taken away again once no plugin is left.
    static void registerWithWindows (const Options& o)
    {
        const auto folder = o.setupFolder != juce::File() ? o.setupFolder
                                                          : platform::knownFolder (false).getChildFile ("ENCORE").getChildFile ("VST Setup");
        const auto exe = folder.getChildFile ("ENCORE VST Setup.exe");

        juce::int64 bytes = 0;
        for (const auto& p : plugins)
            if (bundleIn (o.vst3Dir, p.bundle).isDirectory())
                bytes += folderSize (bundleIn (o.vst3Dir, p.bundle));

        if (bytes == 0)
        {
            juce::WindowsRegistry::deleteKey (o.registryKey);
            if (o.thisSetup.isAChildOf (folder))
                platform::deleteFolderAfterExit (folder);   // can't delete the program that's running
            else
                folder.deleteRecursively();
            folder.getParentDirectory().deleteFile();      // "ENCORE", if nothing else of ours is in it
            return;
        }

        folder.createDirectory();
        if (o.thisSetup != exe && o.thisSetup.existsAsFile())
            o.thisSetup.copyFileTo (exe);

        const auto key = o.registryKey + "\\";
        const auto quoted = exe.getFullPathName().quoted();
        juce::WindowsRegistry::setValue (key + "DisplayName", "ENCORE. VST");
        juce::WindowsRegistry::setValue (key + "DisplayVersion", o.version);
        juce::WindowsRegistry::setValue (key + "Publisher", "Encore Music, Inc.");
        juce::WindowsRegistry::setValue (key + "DisplayIcon", exe.getFullPathName() + ",0");
        juce::WindowsRegistry::setValue (key + "InstallLocation", o.vst3Dir.getFullPathName());
        juce::WindowsRegistry::setValue (key + "UninstallString", quoted + " --uninstall");
        juce::WindowsRegistry::setValue (key + "ModifyPath", quoted);
        juce::WindowsRegistry::setValue (key + "URLInfoAbout", "https://encoremusic.studio");
        juce::WindowsRegistry::setValue (key + "EstimatedSize", (juce::uint32) (bytes / 1024));
        juce::WindowsRegistry::setValue (key + "NoRepair", (juce::uint32) 1);
    }

    std::array<Status, numPlugins> apply (juce::ZipFile& zip, const Options& o, const Selection& keep,
                                          const std::function<void (int, Status)>& report)
    {
        std::array<Status, numPlugins> out;
        out.fill (Status::waiting);
        const auto tell = [&] (int i, Status s) { out[(size_t) i] = s; if (report) report (i, s); };

        o.vst3Dir.createDirectory();

        for (int i = 0; i < numPlugins; ++i)
        {
            const auto& p = plugins[(size_t) i];
            const auto dst = bundleIn (o.vst3Dir, p.bundle);
            const bool had = dst.isDirectory();

            if (! keep[(size_t) i] && ! had)
            {
                tell (i, Status::unchanged);
                continue;
            }

            tell (i, Status::working);
            if (had && isInUse (o.vst3Dir, p))
            {
                tell (i, Status::inUse);
                continue;
            }

            if (! keep[(size_t) i])
            {
                tell (i, dst.deleteRecursively() ? Status::removed : Status::failed);
                continue;
            }

            // The old copy steps aside first, so a failed write can put it back
            juce::File old;
            if (had)
            {
                old = o.vst3Dir.getNonexistentChildFile (juce::String (p.bundle) + ".old", ".tmp", false);
                if (! dst.moveFileTo (old))
                {
                    tell (i, Status::inUse);
                    continue;
                }
            }

            if (! extract (zip, o.vst3Dir, p))
            {
                dst.deleteRecursively();
                if (had)
                    old.moveFileTo (dst);
                tell (i, Status::failed);
                continue;
            }

            if (had)
                old.deleteRecursively();

            const auto legacy = bundleIn (o.vst3Dir, p.legacy);
            if (legacy.isDirectory())
                legacy.deleteRecursively();

            tell (i, Status::installed);
        }

        if (o.registerWithWindows)
            registerWithWindows (o);
        return out;
    }
}
