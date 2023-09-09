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
#include <shellapi.h>


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

struct Contraption
{
    inline static Contraption* GetInstance()
    {
        return *reinterpret_cast<Contraption**>(std::uintptr_t(GetModuleHandle(NULL)) + 0x12A7618);
    }

    private:
        Contraption() = delete;
        ~Contraption() = delete;
};

struct PlayStateManager
{
    inline static PlayStateManager* GetInstance()
    {
		return *reinterpret_cast<PlayStateManager**>(std::uintptr_t(Contraption::GetInstance()) + 0x160);
    }

    private:
        PlayStateManager() = delete;
		~PlayStateManager() = delete;
};

struct PlayState
{
    inline static PlayState* GetInstance()
    {
		return *reinterpret_cast<PlayState**>(std::uintptr_t(PlayStateManager::GetInstance()) + 0x20);
	}

	private:
		PlayState() = delete;
		~PlayState() = delete;
};

struct SteamNetworkClient
{
    inline static SteamNetworkClient* GetInstance()
    {
		return *reinterpret_cast<SteamNetworkClient**>(std::uintptr_t(PlayState::GetInstance()) + 0x130);
	}

	private:
        SteamNetworkClient() = delete;
		~SteamNetworkClient() = delete;
};

enum class ConsoleLogType
{
    Default = 0x1,
    Profile = 0x2,
    Resource = 0x4,
    Shader = 0x8,
    Buffer = 0x10,
    Render = 0x20,
    Network = 0x40,
    System = 0x80,
    Terrain = 0x100,
    World = 0x200,
    Lua = 0x800,
    Print = 0x1000,
    Audio = 0x400,
    UGC = 0x2000,
    Steam = 0x4000,
    Error = 0x80000000,
    Warning = 0x40000000
};

enum class ConsoleLogColors : WORD
{
    Default = 7,
	Network = 8,
    Error = 12,
	Warning = 14
};

using fConsoleLog = std::add_pointer< void(void*, const std::string&, ConsoleLogColors, ConsoleLogType) >::type;

class ConsoleLog__vftable {
    uintptr_t function0;
public:
    fConsoleLog m_Log;
private:
    uintptr_t function1;
};

struct Console {
    ConsoleLog__vftable* __vftable;

    static ConsoleLogColors typeToColor(const ConsoleLogType type) {
        switch (type) {
        case ConsoleLogType::Default:
            return ConsoleLogColors::Default;
        case ConsoleLogType::Network:
            return ConsoleLogColors::Network;
        case ConsoleLogType::Error:
            return ConsoleLogColors::Error;
        case ConsoleLogType::Warning:
            return ConsoleLogColors::Warning;
        default:
            return ConsoleLogColors::Default;
        }
    }

    static Console& getInstancePtr() {
        return **(Console**)(uintptr_t(GetModuleHandle(NULL)) + 0x12A76E0);
    }

    void log(const std::string& message, ConsoleLogType type = ConsoleLogType::Default) {
        __vftable->m_Log(this, message, typeToColor(type), type);
    }
};

// log function with std::format using orginal logger
void log(ConsoleLogType type, const char* format, ...) {
	static char buffer[0xA00000];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, format, args);
	va_end(args);

    Console::getInstancePtr().log(buffer, type);
}

// same for wstring
void log(ConsoleLogType type, const wchar_t* format, ...) {
    static wchar_t buffer[0xA00000];
	va_list args;
	va_start(args, format);
	vswprintf_s(buffer, format, args);
	va_end(args);
	
    // to std:string
    std::wstring ws(buffer);
    std::string str(ws.begin(), ws.end());

    Console::getInstancePtr().log(str, type);
}

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

            // convert data to hex
            std::string hex;
            for (unsigned int i = 0; i < size; i++)
            {
				char buf[4];
				sprintf_s(buf, "%02X ", PBYTE(message->GetData())[i]);
				hex += buf;
			}

            
            const char* string = (
                "\n------------\n"
                "Server > Client\n"
                "PacketID: %u\n"
                "Size: %u\n"
                "Return address : %p\n"
                "Data:\n%s\n"
                "------------"
            );

            log(ConsoleLogType::Network, string, PBYTE(message->GetData())[0], size, (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandle(NULL), hex.c_str());
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

        // convert data to hex
        std::string hex;
        for (unsigned int i = 0; i < cbData; i++)
        {
			char buf[4];
			sprintf_s(buf, "%02X ", data[i]);
			hex += buf;
		}

        const char* string = (
            "\n------------\n"
            "Client > Server\n"
            "PacketID: %u\n"
            "Size: %u\n"
            "Return address : %p\n"
            "Data:\n%s\n"
            "------------"
        );

        log(ConsoleLogType::Network, string, data[0], cbData, (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandle(NULL), hex.c_str());

        /*if (data[0] == 9)
        {
            log("X");
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
int message_count = 1;

COORD coord = {0, 0};

bool logStacker = true;

void hooked_logger(void* self, const char* str, int type) {
	// if last message = str, don't print
    if (last_message == str && logStacker) {
        message_count++;
        std::string stdstr = str;

        SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);

        std::string count = " (x" + std::to_string(message_count) + ")";
        std::string message = str + count;
        original_logger(self, message.c_str(), type);
    }
    else {
        message_count = 1;

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
        log(ConsoleLogType::Default, "BaseCharacter.lua loaded!");
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

        if (lua_isstring(L, -1)) {
            log(ConsoleLogType::Error, lua_tostring(L, -1));
        }

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
        log(ConsoleLogType::Error, "Lua state not initialized yet!");
    }
}

std::queue<std::wstring> files;

void luahook_ExecFileQueue(lua_State* L, lua_Debug* ar)
{
    lua_sethook(L, nullptr, NULL, NULL);

    while (!files.empty())
    {
        // convert to string
        std::string str(files.front().begin(), files.front().end());

        luaL_dofile(L, str.c_str());
        // error handling
        if (lua_isstring(L, -1)) {
            log(ConsoleLogType::Error, lua_tostring(L, -1));
        }

        lua_pop(L, 1);
        files.pop();
    }
}

void runfile(std::wstring str) {
    if (lState != nullptr) {
        files.push(str);

        lua_sethook(lState, luahook_ExecFileQueue, LUA_MASKLINE, NULL);
    }
    else {
        // print no error state
        log(ConsoleLogType::Error, "Lua state not initialized yet!");
    }
}

std::vector<std::wstring> ParseArgs(const std::string_view& input)
{
    LPWSTR* szArglist;
    int nArgs;
    int i;

    // input to lpcwstr
    std::wstring winput(input.begin(), input.end());

    // CommandLineToArgvW
    szArglist = CommandLineToArgvW(winput.c_str(), &nArgs);

    // convert to vector
    std::vector<std::wstring> args;
    for (i = 0; i < nArgs; i++)
    {
		args.push_back(szArglist[i]);
	}

    return args;
}

std::atomic<bool> stop_thread(false);
std::map<std::wstring, std::atomic<bool>> stop_flags;

int watch(std::wstring path, std::wstring file = L"") {
    bool verrystinkyfix = false;

    // Open the directory to watch
    HANDLE directory = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (directory == INVALID_HANDLE_VALUE) {
        log(ConsoleLogType::Error, "Could not open directory");

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
                verrystinkyfix = !verrystinkyfix;

                if (verrystinkyfix) {
					continue;
				}

                log(ConsoleLogType::Default, L"File modified: %s", fullpath.c_str());

                // run file
                runfile(fullpath);
            }

            notification = reinterpret_cast<decltype(notification)>(notification->NextEntryOffset + reinterpret_cast<uintptr_t>(notification));
        }
    }
}

// std::map key: path, value: thread
std::map<std::wstring, std::thread> watches;

// addwatch(path) -> thread
void addwatch(std::wstring path, std::wstring file = L"") {
    // check if already watching
    if (watches.find(path) != watches.end()) {
		log(ConsoleLogType::Warning, L"Already watching %s", path.c_str());
		return;
	}

    watches[path] = std::thread(watch, path, file);
    // log started watching <path+file>
    log(ConsoleLogType::Default, L"Started watching %s%s", path.c_str(), file.c_str());
}

// removewatch(path) -> thread
void removewatch(std::wstring path) {
    stop_flags[path] = true;
    watches[path].detach();

    // remove from map
    watches.erase(path);
    log(ConsoleLogType::Default, L"Stopped watching %s", path.c_str());
}

bool devbypass = false;

void toggleDevBypass() {
    devbypass = !devbypass;

    PBYTE DevFlagAddress = (PBYTE)GetModuleHandle(NULL) + 0x12a7617;
    *DevFlagAddress = devbypass;

    if (devbypass) {
        log(ConsoleLogType::Default, "dev bypass enabled!");

        uint64_t address = (uint64_t)GetModuleHandle(NULL) + 0x45AAAC;
        DWORD oldprotect = 0;
        VirtualProtect((LPVOID)address, 6, PAGE_EXECUTE_READWRITE, &oldprotect);
        memset((LPVOID)address, 0x90, 6);
        VirtualProtect((LPVOID)address, 6, oldprotect, 0);
    }
    else {
        log(ConsoleLogType::Default, "dev bypass disabled!");

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

    log(ConsoleLogType::Default, "Executor Loade!");
    toggleDevBypass();
    log(ConsoleLogType::Default, "Volvo Pointer: %llx", ptr);

    // read input and handle commands
    std::string input;
    while (true) {
		std::getline(std::cin, input);

        last_message = "";
        message_count = 1;

        coord = { 0, 0 };

        if (input == "unload") {
            log(ConsoleLogType::Default, "unloading...");
			break;
		}
        else if (input.starts_with("runstring ")) {
            runstring(input.data() + 10);
        }
        else if (input == "dev") {
            toggleDevBypass();
        }
        else if (input.starts_with("connect ")) {
            // args = steamid, passphrase
            std::string argsStr = input.data() + 8;
            std::vector<std::wstring> args = ParseArgs(argsStr);

            // steamid = 
            std::wstring steamid = args[0];
            std::wstring passphrase = args[1];

            auto playstate = PlayState::GetInstance();
            if (playstate == 0) {
                log(ConsoleLogType::Error, "Not in game!");
                continue;
            }

            // connection info = playstate + 8
            int64_t connectionInfo = *(int64_t*)((uintptr_t)playstate + 0x8);

            *(int64_t*)(connectionInfo + 0x8) = std::stoll(steamid);

            std::string passphraseStr(passphrase.begin(), passphrase.end());
            *(std::string*)(connectionInfo + 0x10) = passphraseStr;

            // make function pointer float *__fastcall sub_140459D10(__int64 PlayState, float *a2)
            typedef float* (__fastcall* sub_140459D10_t)(int64_t, float*);
            sub_140459D10_t sub_140459D10 = (sub_140459D10_t)((uint64_t)GetModuleHandle(NULL) + 0x459D10);

            float bruh[3];

            sub_140459D10((int64)playstate, bruh);
        }
        else if (input.starts_with("watch ")) {
            // remove watch 
            std::string argsStr = input.data() + 6;

            std::vector<std::wstring> args = ParseArgs(argsStr);

            // check if args[0] exists safely
            if (args.size() == 0) {
				log(ConsoleLogType::Error, "No path specified!");
				continue;
			}

            // file = args[1] or ""
            std::wstring file = L"";
            if (args.size() > 1) {
				file = args[1];
            }

			// path = args[0]
			std::wstring path = args[0];
			// add watch
			addwatch(path, file);
        }
        else if (input.starts_with("unwatch ")) {
            // remove watch 
			std::string argsStr = input.data() + 8;
			std::vector<std::wstring> args = ParseArgs(argsStr);

			// removewatch if args[0] exists 
            if (args.size() > 0) {
                removewatch(args[0]);
            }

        }
        else if (input.starts_with("runfile ")) {
            if (lState) {
                std::string argsStr = input.data() + 8;
                std::vector<std::wstring> args = ParseArgs(argsStr);

                // removewatch if args[0] exists 
                if (args.size() > 0) {
                    runfile(args[0]);
                }
            }
            else {
                log(ConsoleLogType::Default, "No lua state!");
            }
        }
        else if (input == "quit") {
            log(ConsoleLogType::Default, "quitting...");
            ExitProcess(0);
            break;
        }
        else if (input == "help") {
			log(ConsoleLogType::Default, "help: show this message");
            log(ConsoleLogType::Default, "quit: quits the game");
            log(ConsoleLogType::Default, "unload: unloads the dll");
            log(ConsoleLogType::Default, "clear: clears console");
            log(ConsoleLogType::Default, "dev: toggle dev bypass");
            log(ConsoleLogType::Default, "runstring: runs a lua string");
            log(ConsoleLogType::Default, "runfile: runs a file containing lua code");
            log(ConsoleLogType::Default, "watch: watches a file or directory for changes");
            log(ConsoleLogType::Default, "unwatch: stops watching a file or directory for changes");
		}
        else if (input == "clear") { // clear console
            FillConsoleOutputCharacter(console, ' ', consoleInfo.dwSize.X * consoleInfo.dwSize.Y, cursorPosition, &numCellsWritten);
            SetConsoleCursorPosition(console, cursorPosition);

            last_message = "";
            message_count = 1;

            coord = { 0, 0 };
        }
        else {
			log(ConsoleLogType::Warning, "Unknown command: %s", input.c_str());
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
		watch.join(); // needs fixing, doesnt unload properly when files are being watched
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

