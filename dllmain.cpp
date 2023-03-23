// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include <iostream>
#include <stdio.h>
#include <lua.hpp>
#include <minhook.h>
#include <string>
#include <regex>
#include <map>
#include <thread>

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
fix crash and stuff idk
reserve last line for input
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

std::vector<std::string> ParseArgs(const std::string_view& input)
{
    const static std::regex regex(R"X("([\w\s\/\.]+)"|([\w\/\.\\]+))X");
    std::vector<std::string> args;

    bool inQuotes = false;
    std::string currentArg;
    for (const auto& c : input)
    {
        if (c == '"')
        {
            if (inQuotes)
            {
                args.push_back(currentArg);
                currentArg = "";
                inQuotes = false;
            }
            else
                inQuotes = true;
        }
        else if (isspace(c))
        {
            if (inQuotes)
                currentArg += c;
            else if (currentArg.length() > 0)
            {
                args.push_back(currentArg);
                currentArg = "";
            }
        }
        else
            currentArg += c;
    }
    if (currentArg.length() > 0)
        args.push_back(currentArg);

    return args;
}

std::atomic<bool> stop_thread(false);
std::map<std::wstring, std::atomic<bool>> stop_flags;

int watch(std::wstring path, std::wstring file = L"") {
    // Open the directory to watch
    HANDLE directory = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (directory == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: could not open directory" << std::endl;
        return 1;
    }

    while (!stop_thread && !stop_flags[path]) {
        // Wait for a change notification to occur
        const DWORD buffer_size = 1024;
        BYTE buffer[buffer_size];
        DWORD bytes_returned;

        if (ReadDirectoryChangesW(directory, buffer, buffer_size, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &bytes_returned, NULL, NULL)) {
            // Loop through all change notifications in the buffer
            FILE_NOTIFY_INFORMATION* notification = (FILE_NOTIFY_INFORMATION*)buffer;

            std::wstring filename(notification->FileName, notification->FileNameLength / sizeof(wchar_t));
            // full path
            std::wstring fullpath = path + filename;

            // if file modified, print content and if file is null or filename = file
            if (notification->Action == FILE_ACTION_MODIFIED && (file.length() <= 0 || filename == file)) {
                std::wcout << "File modified: " << fullpath << std::endl;
                // open file
                HANDLE file = CreateFileW(fullpath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (file == INVALID_HANDLE_VALUE) {
                    std::cerr << "Error: could not open file" << std::endl;
                    return 1;
                }

                // read file without a char vector
                DWORD size = GetFileSize(file, NULL);
                char* content = new char[size];
                DWORD bytes_read;
                if (ReadFile(file, content, size, &bytes_read, NULL)) {
                    //terminate string
                    content[size] = '\0';
                    // check if lState is valid
                    if (lState != nullptr) {
						// run string
                        luaL_dostring(lState, content);
					}
                }

                CloseHandle(file);
            }

            notification = (FILE_NOTIFY_INFORMATION*)((BYTE*)notification + notification->NextEntryOffset);
        }
    }
}

// std::map key: path, value: thread
std::map<std::wstring, std::thread> watches;

// addwatch(path) -> thread
void addwatch(std::wstring path, std::wstring file = L"") {
    watches[path] = std::thread(watch, path, file);
    // wprintf started watching <path+file>
    wprintf(L"Started watching %s%s\n", path.c_str(), file.c_str());
}

// removewatch(path) -> thread
void removewatch(std::wstring path) {
    stop_flags[path] = true;
    watches[path].detach();

    // remove from map
    watches.erase(path);
    wprintf(L"Stopped watching %s\n", path.c_str());
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
            // remove watch 
            std::string argsStr = input.data() + 6;

            std::vector<std::string> args = ParseArgs(argsStr);

            // check if args[0] exists safely
            if (args.size() == 0) {
				printf("Error: no path specified!\n");
				continue;
			}

            // file = args[1] or ""
            std::wstring file = L"";
            if (args.size() > 1) {
				file = std::wstring(args[1].begin(), args[1].end());
            }

			// path = args[0]
			std::wstring path = std::wstring(args[0].begin(), args[0].end());
			// add watch
			addwatch(path, file);
        }
        else if (input.starts_with("unwatch ")) {
            // remove watch 
			std::string argsStr = input.data() + 8;
			std::vector<std::string> args = ParseArgs(argsStr);

			// removewatch if args[0] exists 
            if (args.size() > 0) {
                std::wstring path = std::wstring(args[0].begin(), args[0].end());
                removewatch(path);
            }

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

    stop_thread = true;
    // detach and unload all threads in watches
    for (auto& [path, watch] : watches) {
		watch.join();
	}

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

