// The few things the setup needs straight from Windows
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>

#include "InstallCore.h"

#pragma comment (lib, "shell32.lib")
#pragma comment (lib, "ole32.lib")
#pragma comment (lib, "advapi32.lib")

namespace setup::platform
{
    juce::File knownFolder (bool commonFiles)
    {
        PWSTR path = nullptr;
        juce::File f;
        if (SUCCEEDED (SHGetKnownFolderPath (commonFiles ? FOLDERID_ProgramFilesCommonX64 : FOLDERID_ProgramFilesX64, 0, nullptr, &path)))
            f = juce::File (juce::String (path));
        CoTaskMemFree (path);
        if (f == juce::File())
            f = juce::File (commonFiles ? "C:\\Program Files\\Common Files" : "C:\\Program Files");
        return f;
    }

    bool fileIsLocked (const juce::File& file)
    {
        // A plugin a DAW has loaded can't be opened for writing by anyone else
        const auto h = CreateFileW (file.getFullPathName().toWideCharPointer(), GENERIC_READ | GENERIC_WRITE, 0,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            const auto e = GetLastError();
            return e == ERROR_SHARING_VIOLATION || e == ERROR_LOCK_VIOLATION || e == ERROR_ACCESS_DENIED;
        }
        CloseHandle (h);
        return false;
    }

    juce::MemoryBlock resource (const char* name)
    {
        if (const auto r = FindResourceA (nullptr, name, MAKEINTRESOURCEA (10) /* RT_RCDATA */))
            if (const auto g = LoadResource (nullptr, r))
                if (const auto data = LockResource (g))
                    return juce::MemoryBlock (data, (size_t) SizeofResource (nullptr, r));
        return {};
    }

    void deleteFolderAfterExit (const juce::File& folder)
    {
        // A hidden cmd waits a few seconds for this program to close, then removes its folder
        auto cmd = "cmd.exe /c ping 127.0.0.1 -n 4 > nul & rmdir /s /q " + folder.getFullPathName().quoted()
                 + " & rmdir " + folder.getParentDirectory().getFullPathName().quoted();
        STARTUPINFOW si {};
        si.cb = sizeof (si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi {};
        std::wstring line (cmd.toWideCharPointer());
        if (CreateProcessW (nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | DETACHED_PROCESS,
                            nullptr, juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName().toWideCharPointer(), &si, &pi))
        {
            CloseHandle (pi.hThread);
            CloseHandle (pi.hProcess);
        }
    }

    bool isElevated()
    {
        HANDLE token = nullptr;
        TOKEN_ELEVATION e {};
        DWORD size = 0;
        bool yes = false;
        if (OpenProcessToken (GetCurrentProcess(), TOKEN_QUERY, &token))
        {
            if (GetTokenInformation (token, TokenElevation, &e, sizeof (e), &size))
                yes = e.TokenIsElevated != 0;
            CloseHandle (token);
        }
        return yes;
    }
}
