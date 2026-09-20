#ifndef _XBOX
#include "pch.h"
#include "winmain.h"

int main(int argc, char* argv[])
{
    std::string cmdLine;
    for (int i = 1; i < argc; i++)
    {
        if (i > 1) cmdLine += " ";
        cmdLine += argv[i];
    }
    return winmain::WinMain(cmdLine.c_str());
}

#if _WIN32
#include <windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    return winmain::WinMain(lpCmdLine);
}

FILE* fopenu(const char* path, const char* opt)
{
    wchar_t* wideArgs[2]{};
    for (auto& arg : wideArgs)
    {
        auto src = wideArgs[0] ? opt : path;
        auto length = MultiByteToWideChar(CP_UTF8, 0, src, -1, nullptr, 0);
        arg = new wchar_t[length];
        MultiByteToWideChar(CP_UTF8, 0, src, -1, arg, length);
    }
    auto fileHandle = _wfopen(wideArgs[0], wideArgs[1]);
    for (auto arg : wideArgs) delete[] arg;
    return fileHandle;
}
#endif
#endif
