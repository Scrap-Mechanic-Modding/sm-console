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
#include <queue>
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

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
reserve last line for input
be able to get state at any point in time
bindings?
quack
*/

struct VolvoStructure
{
    void* m_functions[0xF];
};

using fGetNetworkingSocketInterface = std::add_pointer< uint64_t(VolvoStructure***) >::type;
using fReceiveMessagesOnPollGroup = std::add_pointer< int(void*, void*, SteamNetworkingMessage_t**, int) >::type;

fReceiveMessagesOnPollGroup o_Steam_ReceiveMessagesOnPollGroup = nullptr;
int hk_Steam_ReceiveMessagesOnPollGroup(void* self, void* poll_group, SteamNetworkingMessage_t** out_msg, int msg_max)
{
    int numMessages = o_Steam_ReceiveMessagesOnPollGroup(self, poll_group, out_msg, msg_max);
    SteamNetworkingMessage_t** outMsg = out_msg;
    for (unsigned int i = 0; i < numMessages; i++)
    {
        SteamNetworkingMessage_t* message = outMsg[i];
        unsigned int size = message->GetSize();
        constexpr unsigned char Receiving = 1;

        if (size > 0 && size != -1 && message->GetData() && message->m_usecTimeReceived > 0)
        {
            // if packetid == 30 return
            if (PBYTE(message->GetData())[0] == 30) { return numMessages; }
            if (PBYTE(message->GetData())[0] == 24) { return numMessages; }

            printf("------------\n");
            printf("Server > Client\n");
            printf("PacketID: %u\n", PBYTE(message->GetData())[0]);
            printf("Size: %u\n", size);
            printf("Return address: %p\n", (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandle(NULL));

            for (unsigned int i = 0; i < size; i++)
            {
                printf("%02X ", PBYTE(message->GetData())[i]);
            }

            printf("\n------------\n");
        }
    }

    return numMessages;
}

using fSendMessageToConnection = std::add_pointer< EResult(void*, HSteamNetConnection, PBYTE, uint32, int, int64*) >::type;

fSendMessageToConnection o_Steam_SendMessageToConnection = nullptr;
int hk_Steam_SendMessageToConnection(void* self, HSteamNetConnection conn, PBYTE data, uint32 cbData, int nSendFlags, int64* out_msgNum)
{
    constexpr unsigned char Receiving = 0;
    if (cbData > 0 && cbData != -1 && data)
    {
        // if packetid == 30 return
        if (data[0] == 30) { return o_Steam_SendMessageToConnection(self, conn, data, cbData, nSendFlags, out_msgNum); }
        if (data[0] == 24) { return o_Steam_SendMessageToConnection(self, conn, data, cbData, nSendFlags, out_msgNum); }

        printf("------------\n");
        printf("Client > Server\n");
        printf("PacketID: %u\n", data[0]);
        printf("Size: %u\n", cbData);
        printf("Return address: %p\n",  (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandle(NULL));

        for (unsigned int i = 0; i < cbData; i++)
        {
            printf("%02X ", data[i]);
        }

        printf("\n------------\n");

        /*if (data[0] == 9)
        {
            printf("X\n");
        }*/

        // Crashes server
        /*if (data[0] == 26)
        {
            cbData = 0;
            data[0] = 0;
        }*/
    }
    return o_Steam_SendMessageToConnection(self, conn, data, cbData, nSendFlags, out_msgNum);
}

int (*original_logger) (void* self, const char* str, int type) = nullptr;

// last message
std::string last_message = "";
// message count
int message_count = 1;

COORD coord = {0, 0};

void hooked_logger(void* self, const char* str, int type) {
	// if last message = str, don't print
    if (last_message == str) {
        message_count++;
        std::string stdstr = str;

        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);

        std::string count = " (x" + std::to_string(message_count) + ")";
        std::string message = str + count;
        original_logger(self, message.c_str(), type);
    }
    else {
        message_count = 0;

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        coord = csbi.dwCursorPosition;

        original_logger(self, str, type);
    }

    last_message = str;
}

int (*original) (lua_State* L, const char* buff, size_t size, const char* name) = nullptr;
lua_State* lState = nullptr;

int hooked_luaL_loadbuffer(lua_State* L, const char* buff, size_t size, const char* name) {
    if (std::string(name) == "...A/Scripts/game/characters/BaseCharacter.lua") {
        printf("BaseCharacter.lua loaded!\n");
        lState = L;
    }

    return original(L, buff, size, name);
}

std::queue<std::string> scripts;
void luahook_ExecQueue(lua_State* L, lua_Debug* ar)
{
    lua_sethook(L, nullptr, NULL, NULL);

    while (!scripts.empty())
    {
        luaL_dostring(L, scripts.front().c_str());
        lua_pop(L, 1);
        scripts.pop();
    }
}

void runstring(std::string str) {
    if (lState != nullptr) {
        scripts.push(str);

        lua_sethook(lState, luahook_ExecQueue, LUA_MASKLINE, NULL);
	}
    else {
        // print no error state
        printf("ERROR: Lua state not initialized yet!\n");
    }
}

std::queue<std::string> files;

void luahook_ExecFileQueue(lua_State* L, lua_Debug* ar)
{
    lua_sethook(L, nullptr, NULL, NULL);

    while (!files.empty())
    {
        luaL_dofile(L, files.front().c_str());
        lua_pop(L, 1);
        files.pop();
    }
}

void runfile(std::string str) {
    if (lState != nullptr) {
        files.push(str);

        lua_sethook(lState, luahook_ExecFileQueue, LUA_MASKLINE, NULL);
    }
    else {
        // print no error state
        printf("ERROR: Lua state not initialized yet!\n");
    }
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
                // run file
                runfile(std::string(fullpath.begin(), fullpath.end()));
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

    PBYTE DevFlagAddress = (PBYTE)GetModuleHandle(NULL) + 0x12a7617;
    *DevFlagAddress = devbypass;

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

    const PBYTE pBaseAddress = PBYTE(GetModuleHandle(NULL));
    const fGetNetworkingSocketInterface pfnGetNetworkingSocketInterface = fGetNetworkingSocketInterface(pBaseAddress + 0x44B560);

    VolvoStructure** ptr = nullptr;
    void* funcPtr = nullptr;
    DWORD oldProtect = 0;

    pfnGetNetworkingSocketInterface(&ptr);
    funcPtr = &(*ptr)->m_functions[14];

    // Hook ReceiveMessagesOnPollGroup
    oldProtect = 0;
    VirtualProtect(funcPtr, 8, PAGE_READWRITE, &oldProtect);
    o_Steam_ReceiveMessagesOnPollGroup = fReceiveMessagesOnPollGroup((*ptr)->m_functions[14]);
    o_Steam_SendMessageToConnection = fSendMessageToConnection((*ptr)->m_functions[11]);
    (*ptr)->m_functions[14] = &hk_Steam_ReceiveMessagesOnPollGroup;
    (*ptr)->m_functions[11] = &hk_Steam_SendMessageToConnection;
    VirtualProtect(funcPtr, 8, oldProtect, NULL);

    // hook luaL_loadbuffer with minhook using create hook api
    //luaL_loadbuffer(L, buff, size, name);
    MH_Initialize();
    MH_CreateHookApi(L"lua51.dll", "luaL_loadbuffer", hooked_luaL_loadbuffer, (LPVOID*) &original);
    MH_CreateHook(LPVOID((uint64_t)GetModuleHandle(NULL) + 0x543A40), hooked_logger, (LPVOID*) &original_logger);
    MH_EnableHook(MH_ALL_HOOKS);


    printf("Executor Loade!\n");
    toggleDevBypass();
    printf("Volvo Pointer: %llx\n", ptr);

    // read input and handle commands
    std::string input;
    while (true) {
		std::getline(std::cin, input);
        if (input == "unload") {
            printf("unloading...\n");
			break;
		}
        else if (input.starts_with("runstring ")) {
            runstring(input.data() + 10);
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

                runfile(modifiedPath.c_str());
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
            printf("dev: toggle dev bypass\n");
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


    // Hook ReceiveMessagesOnPollGroup
    oldProtect = 0;
    VirtualProtect(funcPtr, 8, PAGE_READWRITE, &oldProtect);
    (*ptr)->m_functions[14] = o_Steam_ReceiveMessagesOnPollGroup;
    (*ptr)->m_functions[11] = o_Steam_SendMessageToConnection;
    VirtualProtect(funcPtr, 8, oldProtect, NULL);

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

