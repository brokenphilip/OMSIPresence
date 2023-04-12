#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <thread>

#include "shared.h"
#include "discord.h"

extern "C" __declspec(dllexport)void __stdcall PluginStart(void* aOwner);
extern "C" __declspec(dllexport)void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write);
extern "C" __declspec(dllexport)void __stdcall PluginFinalize();

#ifdef PROJECT_DEBUG
void Debug(const char* message, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	SYSTEMTIME time;
	GetLocalTime(&time);
	printf("[%02d:%02d:%02d] %s\n", time.wHour, time.wMinute, time.wSecond, buffer);
}
#endif

void Error(const char* message, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	MessageBoxA(NULL, buffer, PROJECT_NAME, MB_ICONERROR);
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		offsets = nullptr;

		// 2.3.004 - Latest Steam version
		if (!strncmp(reinterpret_cast<char*>(0x8BBEE3), ":33333333", 9))
		{
			offsets = new Offsets();

			offsets->hard_paused1 = 0x861694;
			offsets->hard_paused2 = 0x861BCD;

			offsets->tmap              = 0x861588;
			offsets->tmap_friendlyname = 0x158;

			offsets->tttman       = 0x8614E8;
			offsets->tttman_lines = 0x18;

			offsets->trvlist              = 0x861508;
			offsets->trvlist_getmyvehicle = 0x74A43C;

			offsets->trvinst_sch_info_valid	= 0x65c;
			offsets->trvinst_sch_line       = 0x660;
			offsets->trvinst_sch_next_stop  = 0x6AC;
			offsets->trvinst_sch_delay      = 0x6BC;
			offsets->trvinst_trv            = 0x710;
			offsets->trvinst_target         = 0x7BC;

			offsets->trv_friendlyname = 0x19C;
			offsets->trv_hersteller   = 0x5D8;

		}

		// 2.2.032 - "Tram patch"
		else if (!strncmp(reinterpret_cast<char*>(0x8BBAE3), ":33333333", 9))
		{
			// TODO find new offsets
		}

		if (!offsets)
		{
			Error("This version of OMSI 2 is not supported.");
			// Can't return FALSE here, otherwise the game will not close properly
			// The plugin will remain dormant instead
		}
	}

	return TRUE;
}

// Gets called when the game starts
void __stdcall PluginStart(void* aOwner)
{
	// Version check failed. Remain dormant
	if (!offsets)
	{
		return;
	}

#ifdef PROJECT_DEBUG
	FILE* console;
	AllocConsole();
	freopen_s(&console, "CONIN$", "r", stdin);
	freopen_s(&console, "CONOUT$", "w", stdout);
	freopen_s(&console, "CONOUT$", "w", stderr);
#endif

	discord::Setup();
	discord::Update();

	DEBUG("Rich presence initialized.");
}

// Gets called each game frame per variable when said variable receives an update
void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write)
{
	// Version check failed. Remain dormant
	if (!offsets)
	{
		return;
	}

	// From this point onwards, it is guaranteed that the game will have a map loaded
	inGame = true;

	// Timegap (delta time)
	if (index == 0)
	{
		timer += *value;
		if (timer > discord::update_interval)
		{
			timer = 0;
			discord::Update();
		}
	}

	// Pause
	if (index == 1)
	{
		sysvars::pause = *value == 1.0f;
	}

	// AI
	else if (index == 2)
	{
		sysvars::ai = *value == 1.0f;
	}
}

// Gets called when the game is exiting
void __stdcall PluginFinalize()
{
	// Version check failed. Remain dormant
	if (!offsets)
	{
		return;
	}

	discord::Destroy();
}

__declspec(naked)
uintptr_t TRVList_GetMyVehicle()
{
	static uintptr_t trvlist = offsets->trvlist;
	static uintptr_t trvlist_getmyvehicle = offsets->trvlist_getmyvehicle;

	// Delphi's "register" callconv is incompatible with any VC++ callconvs
	// This assembly shows exactly how the game calls this function internally
	__asm
	{
		mov     eax, trvlist
		mov     eax, [eax]
		call    trvlist_getmyvehicle
		ret
	}
}

char* TTimeTableMan_GetLineName(int index)
{
	auto tttman = ReadMemory<uintptr_t>(offsets->tttman);
	auto lines = ReadMemory<uintptr_t>(tttman + offsets->tttman_lines);

	// According to TTimeTableMan.IsLineIndexValid. Dynamic array size is stored at array address - 4
	if (index < 0 || index >= ReadMemory<int>(lines - 4))
	{
		return nullptr;
	}

	// 0x10 is the size of each element in the list
	return ReadMemory<char*>(lines + index * 0x10);
}
