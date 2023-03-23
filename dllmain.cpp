// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <lua.hpp>
#include <minhook.h>
#include <string>
#include <regex>

/*
main goal :
1. console commands for scrap mechanic
2. commands like:
    runfile <path> for running lua files
    runstring <lua> for running lua code
    watch <path> for watching a file and automatically running it when it changes
    unwatch <path> for stopping watching a file
3. watching for changes in a file
*/

/*
todo:
file watching
fix crash and stuff idk
bindings?
quack
*/

int (*original) (lua_State* L, const char* buff, size_t size, const char* name) = nullptr;
lua_State* lState = nullptr;

int hooked_luaL_loadbuffer(lua_State* L, const char* buff, size_t size, const char* name) {
    if (std::string(name) == "...A/Scripts/game/characters/BaseCharacter.lua") {
        printf("BaseCharacter.lua loaded!\n");
		lState = L;
    }

    return original(L, buff, size, name);
}

bool devbypass = false;

void toggleDevBypass() {
    devbypass = !devbypass;
    if (devbypass) {
        printf("dev bypass enabled!\n");

        uint64_t address = (uint64_t)GetModuleHandle(NULL) + 0x45AAAC;
        DWORD oldprotect = 0;
        VirtualProtect((LPVOID)address, 6, PAGE_EXECUTE_READWRITE, &oldprotect);
        memset((LPVOID)address, 0x90, 6);
        VirtualProtect((LPVOID)address, 6, oldprotect, 0);
    }
    else {
        printf("dev bypass disabled!\n");

        uint64_t address = (uint64_t)GetModuleHandle(NULL) + 0x45AAAC;
        DWORD oldprotect = 0;
        VirtualProtect((LPVOID)address, 6, PAGE_EXECUTE_READWRITE, &oldprotect);
        memcpy((LPVOID)address, { "\x0F\x85\xA2\x0A\x00\x00" }, 6);
        VirtualProtect((LPVOID)address, 6, oldprotect, 0);
    }
}

void main(HMODULE hModule) {
    FILE* fstdin = stdin;
    FILE* fstdout = stdout;
    FILE* fstderr = stderr;

    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    COORD cursorPosition = { 0, 0 }; // set the cursor position to (0, 0)
    DWORD numCellsWritten;
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(console, &consoleInfo);

    FILE* fstdin_new, * fstdout_new, * fstderr_new;
    freopen_s(&fstdin_new, "CONIN$", "r", stdin);
    freopen_s(&fstdout_new, "CONOUT$", "w", stdout);
    freopen_s(&fstderr_new, "CONOUT$", "w", stderr);

    // hook luaL_loadbuffer with minhook using create hook api
    //luaL_loadbuffer(L, buff, size, name);
    MH_Initialize();
    MH_CreateHookApi(L"lua51.dll", "luaL_loadbuffer", hooked_luaL_loadbuffer, (LPVOID*) &original);
    MH_EnableHook(MH_ALL_HOOKS);

    toggleDevBypass();

    printf("Executor Loade!\n");

    // read input and handle commands
    std::string input;
    while (true) {
		std::getline(std::cin, input);
        if (input == "unload") {
            printf("unloading...\n");
			break;
		}
        else if (input.starts_with("runstring ")) {
            if (lState) {
                luaL_dostring(lState, input.data() + 10);
            }
            else {
                printf("No lua state!\n");
            }
        }
        else if (input == "dev") {
            toggleDevBypass();
        }
        else if (input.starts_with("watch ")) {
            //WatchFileOrDirectory(s2ws(input.data() + 6));
        }
        else if (input.starts_with("unwatch ")) {
            //StopWatchingFileOrDirectory(s2ws(input.data() + 8));
        }
        else if (input.starts_with("runfile ")) {
            if (lState) {
                std::string_view path = input.data() + 8;
                std::string modifiedPath(path);
                std::replace(modifiedPath.begin(), modifiedPath.end(), '\\', '/');

                luaL_dofile(lState, modifiedPath.c_str());
            }
            else {
                printf("No lua state!\n");
            }
        }
        else if (input == "quit") {
            printf("quitting...\n");
            ExitProcess(0);
            break;
        }
        else if (input == "help") {
			printf("help: show this message\n");
            printf("quit: quits the game\n");
            printf("unload: unloads the dll\n");
            printf("clear: clears console\n");
            printf("runstring: runs a lua string\n");
            printf("runfile: runs a file containing lua code\n");
            printf("watch: watches a file or directory for changes\n");
            printf("unwatch: stops watching a file or directory for changes\n");
		}
        else if (input == "clear") { // clear console
            FillConsoleOutputCharacter(console, ' ', consoleInfo.dwSize.X * consoleInfo.dwSize.Y, cursorPosition, &numCellsWritten);
            SetConsoleCursorPosition(console, cursorPosition);
        }
        else {
			printf("Unknown command: %s\n", input.c_str());
		}
	}

    // redirect back to original
    freopen_s(&fstdin, "CONIN$", "r", stdin);
    freopen_s(&fstdout, "CONOUT$", "w", stdout);
    freopen_s(&fstderr, "CONOUT$", "w", stderr);

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        // create a thread on main function
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, hModule, 0, NULL);
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

