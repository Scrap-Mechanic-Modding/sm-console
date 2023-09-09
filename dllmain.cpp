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
#include <stdexcept>
#include <utility>
#include <sstream>


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


std::map<unsigned char, std::vector<unsigned char>> editedPackets = {};
const unsigned char checksum[] = "\xF0\xFF\xFF\xFF\xFF\xFF\x5A\x00\x00\x01\x58\x81\x5B\xD7\xA7\xA8\x23\xC3\xD1\xEE\x29\xC2\x4F\xEA\x0B\x36\x9E\xE1\xE6\x2C\x48\x47\x10\x6E\x90\x6C\x41\x6C\x43\xB3\x06\x62\x13\xBB\x14\x6F\xF0\xC7\xC6\xE5\xF8\x05\x02\xF1\xE0\x33\xB5\x2F\x6C\x5B\x36\x73\x9C\x41\xA3\x53\x1B\xCC\x18\x98\x99\x16\x68\xAF\x0E\xC0\x4C\xD7\xA4\xB9\x39\x31\x40\x61\x85\x23\xB9\xE1\x2F\xD8\xE6\x0D\x8B\x90\x82\x42\x80\xE0\x1F\x93\x61\x8D\x26\xEF\x37\xDD\xA1\x91\x78\x5F\xE9\x38\x2D\xC5\xF6\x58\x90\x73\xCD\xAC\x4E\xBA\xD0\xB3\x34\xC7\x35\x11\x0C\x0E\x9A\x88\x9E\x21\x5E\x9C\xA8\x47\x98\xFE\x64\xF2\x05\xCB\x84\x09\x9A\x35\xA8\xCA\x52\xD7\x6F\xF5\xF6\x2C\xEB\x1F\x82\x6A\xDD\x2B\x13\x94\x6F\xC3\xCE\xD3\x45\x50\xC2\x92\x74\x9C\x6E\x7E\xB3\xAD\xFD\x6D\x79\x57\x8B\x21\x48\x5E\xA0\x0B\xDA\x9F\x29\x7F\x7A\x5A\x74\x0D\x5F\x7E\xD8\xF6\x3C\x99\xE7\xDE\x40\xD8\xF5\x98\x88\xBB\xB3\x55\x22\xFA\xDC\x06\x06\x46\xD1\x64\x3F\x99\x59\xE8\x9B\x02\x87\x40\x70\x0D\x54\x0A\x53\xBB\xF8\xB0\x77\x69\x7B\xAA\x3A\xF0\x6F\x12\x1D\x39\xF7\xBB\xBE\x1E\x80\xAF\x3A\xBB\x76\xAF\x4D\x2A\x2E\x02\x11\x20\xA1\x40\x8C\xA1\x71\x7D\x29\xCD\xDB\x01\x20\x5C\x79\xBC\x32\x7C\x72\xC9\x8C\xEE\xAA\x09\xB9\x3C\xC1\x3B\xE4\x27\xA7\xE4\xBA\x6B\x5D\x68\x37\x4F\x93\x74\xF5\xEA\x39\x89\x51\x09\xCC\x02\x56\xF8\xCA\x56\x4B\xBF\xEA\x79\x50\x71\x1F\xC7\x45\xAF\x1F\xD6\x6A\x3F\xC2\x41\xCD\x34\x13\xE8\x68\x39\xA8\x09\x14\x39\x85\x20\xE9\xF5\x3A\xA0\x93\x01\xDE\xD3\xED\xD0\x3D\x2F\xCB\x95\x51\x8B\xC3\xE3\x40\x1D\x51\x4C\x6B\x92\x0B\x90\xFA\x5D\xC8\x0C\xF4\xD8\x67\x99\x21\x4E\x62\xBE\xC3\x6E\xCE\x65\x63\x99\x4D\x1F\x7C\xD2\xD4\x41\x13\xD3\xDA\xA7\xDA\xF3\x4D\xAC\xFA\xB4\x35\x42\x76\xB9\x43\xFA\x2D\xB8\x16\x2D\x2A\x05\x48\xB9\x7A\x6C\x2B\x05\x60\x36\x6F\xD3\xE8\xE3\x62\x7C\x19\xD4\xC6\xAF\xFA\xE0\x89\xBB\x77\x93\x47\xE6\xF5\x90\xF2\x1F\xFA\xFA\x97\x2B\x65\x76\x6C\xB3\x38\x6C\x81\x18\x2C\x36\x7F\x9B\xFE\x75\x6F\x18\xAD\x90\xFF\x10\xDE\x29\x7A\x3D\xE6\x94\x1D\x7D\x67\x7D\xAF\x3A\xFF\x1B\xF0\x88\xEB\x29\x31\xF8\x54\x69\x7E\x0E\x9E\x60\x59\xF6\xCB\xF3\x2E\x77\x21\xD2\x33\xC9\x01\xAE\x02\xC3\x63\x3B\x32\xCF\x4E\x4C\xCF\x7C\x56\x20\xC8\x4C\x37\x2A\xFD\x68\x6A\x4E\x37\xE6\x10\x59\xC3\xF6\x72\x1B\x09\x5C\xCE\xA4\x51\xCC\xDE\xCD\x40\xC7\xE5\x96\x34\x8D\x38\xC5\xD9\xAA\xAF\xCF\xFA\x21\x6A\xA9\xB0\x49\x17\xFE\xB2\x59\x93\xF7\x93\xFE\xF7\x82\x40\x8C\xAD\xE2\x75\x49\x3C\x8E\xD8\x74\x07\x37\x21\x1C\xCC\x1E\x9A\xB0\xD8\x19\x82\xCD\x11\x39\x4E\x6D\xA5\xB0\xE0\xFA\x18\x6D\x62\x51\xF9\xE6\x2D\x23\x40\xAD\xBD\xF4\x5A\x8A\x06\x45\x42\xE2\xB6\xC1\xFB\x12\xC8\xAC\x91\xF1\xCE\x95\x3E\xC9\x3E\xB4\x49\xC4\xAF\x1B\xC5\xBB\xDE\x80\x17\x25\xA5\x3F\x70\x29\x27\x9A\x5C\x7B\xC9\x08\xCF\xA7\xA3\x7D\x8D\x75\x56\x73\x9A\x66\x86\xAE\xE6\x75\x6B\xD9\x35\x64\x0D\xCB\x5A\x9E\x39\xCB\x0E\xB5\xDD\xA8\x0B\x8B\x0A\x79\xBD\x7B\xEA\x47\x38\x38\x8C\x9D\xB3\x97\xF7\x53\xE8\xF3\xE2\x5B\xEE\x15\x33\x8C\xC4\xB3\x9C\x09\x20\x7B\xCC\xDF\x16\x55\xE1\xB2\xE0\xAB\xD6\x7A\xFA\x42\x62\x45\xD5\xE1\x6A\x31\x61\x55\x16\xDF\x92\xAC\x90\xCF\x75\x1F\xE6\x72\x12\xEF\x5D\x94\x43\x85\x07\xCA\xAF\x36\x52\x7F\x28\x23\x9C\xB8\x3B\x1A\x65\x6E\x1A\x70\x47\xB2\xD6\x7B\x09\x7D\x44\xD9\x6D\x34\x0B\xA2\x63\x37\x8B\x86\xC4\xD0\x8E\x2C\x87\xCC\x76\xB4\x27\x15\x64\xC0\xC2\x2B\x7F\xB7\x15\x6F\xD6\x15\x55\x42\x18\x9F\x93\x27\x93\x1B\xD8\x5B\x38\xEE\xE7\x35\x85\x0C\xF7\xEA\x37\x20\x00\xEE\x65\x01\xD1\xF2\x6B\xB2\xD8\x32\x09\xFE\xF6\xDA\xE4\xC1\x60\xBA\x96\x78\x43\x45\xCB\x07\xEE\xB7\x30\x19\x0F\xC3\xDA\x7E\x1B\x55\x38\xC4\x12\x2F\x17\xBC\x9E\x0C\x91\x4A\xE6\x85\x12\x98\x3A\xE7\x6D\x92\x5D\xA9\x74\xEE\xDD\xFD\xB3\xBA\xAF\x7E\x88\xC5\x3B\x49\x41\x4C\xC4\x64\xDA\xD1\xFA\x42\x37\x07\x9B\x1C\x86\x4E\x7B\x26\x07\x23\xB4\xD0\x19\xC5\x85\xBE\xAC\x19\x30\x0C\x8C\x6D\xA2\x7E\xC1\x92\x10\x13\x7F\x45\xEC\x0A\x99\xCE\xAD\x1C\xC1\x35\x54\x46\xE8\x38\x8C\xB6\x76\x50\x2B\x16\xCD\x99\x3D\x67\x69\x87\x7B\x7F\x4C\xAB\xF2\x1A\x66\x9F\x9D\x89\x51\x64\xD1\xD7\x10\xC4\xAD\x3E\x4D\xDF\xBD\xA4\x91\x98\xE7\x93\x92\x82\x2D\x67\xF1\xE1\x06\xCC\x4D\xA2\x49\x6A\xF4\x77\x6B\x82\x28\x3A\x7A\x8A\xCA\xB4\xE4\x55\xCD\x43\xFB\xC0\x76\x57\x04\x29\x48\xD6\xB7\x54\x45\x4A\x6C\xB3\x81\x5D\xB2\xA5\x6C\xF3\xE9\x48\xB2\xC2\x4D\xD2\xCC\x10\xB7\x92\x81\x52\x24\x1B\x0E\x7D\xEE\xAB\xC4\xAC\x32\x99\xCD\x29\xFF\xF0\x41\x70\x70\x4C\x75\xD3\x95\x09\x92\x7D\xB1\x09\xA8\x75\x25\x06\x23\xEF\x52\x54\xA1\x4D\xC5\x94\x57\xF5\xAF\x37\x3C\x05\x8A\x81\x3F\x14\x5A\x61\x4D\x09\xF6\xA0\x59\xC9\xA5\x29\x58\x25\x7B\xD5\xAC\x0A\x3B\x5C\x93\x70\x70\x9A\x1E\xEA\x25\xFF\x3C\xC4\x2D\xD9\x80\x85\x15\x3D\xDC\x98\x8D\xAE\x24\x78\x4F\x51\x5A\x4A\x72\xCE\xB0\x8A\x4F\x24\xCB\xD2\x1F\xD5\x3B\x51\x7A\xF6\x8C\x12\x5B\x97\x13\xB2\x01\x50\x30\x54\x50\x99\xFC\x0B\x0A\xC6\x60\x7A\x6B\x30\x4B\xBA\x2E\x62\x52\xE0\x7B\xBB\x87\x73\x7A\x8A\x75\xF1\x76\xF8\xBA\xF3\x4A\x8D\xDA\xCB\xEE\x53\x0F\x93\x86\x1C\xDD\x85\xAC\x64\xEA\x5B\xD9\xD5\x82\x66\x0A\x44\xF3\xB6\x0F\x32\xCB\x39\x68\x60\x69\x03\xD0\xBC\x5D\x4B\xD3\x61\x38\xC6\xB3\xB4\x32\x5A\xE9\x3E\x43\x8B\x8D\x84\xB0\xA2\x92\x43\xF0\xC0\x44\x08\x77\x56\xAE\x00\xF6\x2B\xFA\x9C\x16\xEE\xAE\x46\xB1\x3C\xEF\x52\x45\x86\xB8\xAF\x5E\xF8\xF5\x5B\xA4\x6D\xAF\xF5\x7B\x23\x2D\x08\x07\xEE\xCE\x93\x22\x7E\xA9\xB8\x77\x82\x7A\x39\xB8\xE0\x6A\x39\x87\x01\x4A\x6E\xB9\x9C\x15\xDF\xB4\x0D\x91\xA8\xF6\x9D\x13\xCA\xE1\x7A\x3B\x4A\x39\x05\x47\xAE\x0F\x4E\xA6\xFC\x7F\x56\x38\x7F\xEC\xC1\x0F\xDD\xE0\xA0\x28\x2A\x1E\x67\xF7\x9B\x30\x98\x7E\x34\x1F\x77\x9D\x1C\xA0\x8F\xFA\xCE\x86\x02\xE1\x9E\x87\x98\xFC\x04\x4E\x8A\x8F\x38\x60\xFF\xE5\x16\x9D\x35\xE8\xC5\xEA\x6F\xDD\x86\x1A\x7F\xAB\xEC\xE6\x02\x70\x66\xC2\xF1\xA7\x60\xC0\xB1\xC4\x73\x71\x56\x49\x25\x36\x4D\xA0\x8F\xB3\xC9\x60\x37\xE0";

bool devbypass = false;
bool checksumBypass = false;

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
	Network = 3,
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

void extract_packet_info(std::vector<std::wstring> args) {

    std::cout << "size " << args.size() << std::endl;

    std::string str(args[1].begin(), args[1].end());

    std::vector<unsigned char> data = {};

    for (int i = 0; i < str.size(); i+=2)
    {
        unsigned char a = (str[i] >= 'A') ? (str[i] >= 'a') ? (str[i] - 'a' + 10) : (str[i] - 'A' + 10) : (str[i] - '0');
        unsigned char b = (str[i + 1] >= 'A') ? (str[i + 1] >= 'a') ? (str[i + 1] - 'a' + 10) : (str[i + 1] - 'A' + 10) : (str[i + 1] - '0');

        data.push_back((a * 16) + b);
    }

    editedPackets[std::stoi(args[0])] = data;
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
            // if packetid == 30||24 return
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

            //log(string, PBYTE(message->GetData())[0], size, (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandle(NULL), hex.c_str());
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
        if (data[0] == 6 && checksumBypass == true) {
            memcpy(&data[1], checksum, sizeof(checksum));
            return o_Steam_SendMessageToConnection(self, conn, data, cbData, nSendFlags, out_msgNum);
        }

        // if packetid == 30||24 return
        if (data[0] == 30) { return o_Steam_SendMessageToConnection(self, conn, data, cbData, nSendFlags, out_msgNum); }
        if (data[0] == 24) { return o_Steam_SendMessageToConnection(self, conn, data, cbData, nSendFlags, out_msgNum); }


        if (!editedPackets[data[0]].empty())
        {
            memcpy(&data[1], editedPackets[data[0]].data(), editedPackets[data[0]].size());
            //cbData = editedPackets[data[0]].size();
        }

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



        //printf(string, data[0], cbData, (uintptr_t)_ReturnAddress() - (uintptr_t)GetModuleHandle(NULL), hex.c_str());
        //printf("\n");

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


bool toggleDevBypass() {
    devbypass = !devbypass;

    PBYTE DevFlagAddress = (PBYTE)GetModuleHandle(NULL) + 0x12a7617;
    *DevFlagAddress = devbypass;

    if (devbypass) {
        uint64_t address = (uint64_t)GetModuleHandle(NULL) + 0x45AAAC;
        DWORD oldprotect = 0;
        VirtualProtect((LPVOID)address, 6, PAGE_EXECUTE_READWRITE, &oldprotect);
        memset((LPVOID)address, 0x90, 6);
        VirtualProtect((LPVOID)address, 6, oldprotect, 0);
    }
    else {
        uint64_t address = (uint64_t)GetModuleHandle(NULL) + 0x45AAAC;
        DWORD oldprotect = 0;
        VirtualProtect((LPVOID)address, 6, PAGE_EXECUTE_READWRITE, &oldprotect);
        memcpy((LPVOID)address, { "\x0F\x85\xA2\x0A\x00\x00" }, 6);
        VirtualProtect((LPVOID)address, 6, oldprotect, 0);
    }

    return devbypass;
}


bool toggleChecksumBypass() {
    checksumBypass = !checksumBypass;
    return checksumBypass;
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
    toggleChecksumBypass();
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
            if (toggleDevBypass()) {
                log(ConsoleLogType::Default, "dev bypass enabled!");
            }
            else {
                log(ConsoleLogType::Default, "dev bypass disabled!");
            }
        }
        else if (input == "checksum") {
            if (toggleChecksumBypass()) {
                log(ConsoleLogType::Default, "checksum bypass enabled!");
            }
            else {
                log(ConsoleLogType::Default, "checksum bypass disabled!");
            }
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
            log(ConsoleLogType::Default, "checksum: toggle checksum bypass");
            log(ConsoleLogType::Default, "runstring: runs a lua string");
            log(ConsoleLogType::Default, "runfile: runs a file containing lua code");
            log(ConsoleLogType::Default, "watch: watches a file or directory for changes");
            log(ConsoleLogType::Default, "unwatch: stops watching a file or directory for changes");
            log(ConsoleLogType::Default, "packet: edit <id> <hex> | revert [id] allows you to edit packets being sent.");
		}
        else if (input == "clear") { // clear console
            FillConsoleOutputCharacter(console, ' ', consoleInfo.dwSize.X * consoleInfo.dwSize.Y, cursorPosition, &numCellsWritten);
            SetConsoleCursorPosition(console, cursorPosition);

            last_message = "";
            message_count = 1;

            coord = { 0, 0 };
        }
        else if (input.starts_with("packet edit ")) {
            std::string argsStr = input.data() + 12;
            std::vector<std::wstring> args = ParseArgs(argsStr);

            extract_packet_info(args);
        }
        else if (input.starts_with("packet revert ")) {
            std::string argsStr = input.data() + 14;
            std::vector<std::wstring> args = ParseArgs(argsStr);

            editedPackets.erase(editedPackets.find(std::stoi(args[0])));
        }
        else if (input.starts_with("packet revert")) {
            editedPackets.clear();
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

