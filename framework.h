#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>

// Disable warnings for steam_api
#pragma warning(disable:4996)
#include <Steam/steam_api.h>
#include <Steam/steam_gameserver.h>


#pragma comment(lib, "steam_api64.lib")