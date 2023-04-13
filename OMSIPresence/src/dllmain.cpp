#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <minidumpapiset.h>
#include <thread>

#include "shared.h"
#include "discord.h"
#include "../resource.h"

extern "C" __declspec(dllexport)void __stdcall PluginStart(void* aOwner);
extern "C" __declspec(dllexport)void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write);
extern "C" __declspec(dllexport)void __stdcall AccessVariable(unsigned short index, float* value, bool* write);
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

	MessageBoxA(NULL, buffer, PROJECT_NAME " " PROJECT_VERSION, MB_ICONERROR);
}

BOOL APIENTRY DllMain(HMODULE instance, DWORD, LPVOID)
{
	dll_instance = instance;
	return TRUE;
}

Offsets* VersionCheck()
{
	Offsets* o = nullptr;

	// 2.3.004 - Latest Steam version
	if (!strncmp(reinterpret_cast<char*>(0x8BBEE3), ":33333333", 9))
	{
		o = new Offsets();

		o->hard_paused1 = 0x861694;
		o->hard_paused2 = 0x861BCD;

		o->tmap = 0x861588;
		o->tmap_friendlyname = 0x158;

		o->tttman = 0x8614E8;
		o->tttman_lines = 0x18;

		o->trvlist = 0x861508;
		o->trvlist_getmyvehicle = 0x74A43C;

		o->trvinst_sch_info_valid = 0x65C;
		o->trvinst_sch_line = 0x660;
		o->trvinst_sch_next_stop = 0x6AC;
		o->trvinst_sch_delay = 0x6BC;
		o->trvinst_trv = 0x710;
		o->trvinst_target = 0x7BC;

		o->trv_friendlyname = 0x19C;
		o->trv_hersteller = 0x5D8;

		DEBUG("Detected version 2.3.004");
	}

	// 2.2.032 - "Tram patch"
	else if (!strncmp(reinterpret_cast<char*>(0x8BBAE3), ":33333333", 9))
	{
		// TODO find new offsets
		DEBUG("Detected version 2.2.032");
	}

	else
	{
		Error("This version of OMSI 2 is not supported. OMSIPresence cannot continue.");
	}

	return o;
}

bool OplCheck()
{
	// OMSIPresence.opl in this project is linked to OMSIPresence.rc and its resource will be used for the consistency check
	HRSRC resource = FindResource(dll_instance, MAKEINTRESOURCE(IDR_OPL1), "OPL");
	HGLOBAL global = LoadResource(dll_instance, resource);
	LPVOID pointer = LockResource(global);

	char* opl = reinterpret_cast<char*>(pointer);
	int length = SizeofResource(dll_instance, resource);
		
	FILE* file = nullptr;
	fopen_s(&file, "plugins/OMSIPresence.opl", "rb");

	if (!file)
	{
		char error[64];
		strerror_s(error, errno);
		Error("Failed to start a consistency check on OMSIPresence.opl - error opening file. OMSIPresence cannot continue.\n\nError code %d: %s", errno, error);
		return false;
	}

	// Calculate the size of the OPL in our OMSI/plugins folder
	fseek(file, 0, SEEK_END);
	int offset = ftell(file);
	fseek(file, 0, SEEK_SET);

	DEBUG("OPL length: expected %d, got %d", length, offset);

	if (offset == length)
	{
		char* contents = new char[length];
		fread_s(contents, length, 1, length, file);

		// OPL files are of identical lengths and their contents match
		if (!strncmp(opl, contents, length))
		{
			DEBUG("OPL consistency check was successful");

			fclose(file);
			delete[] contents;
			return true;
		}

		DEBUG("OPL files do not match!");
		delete[] contents;
	}
	fclose(file);

	DEBUG("OPL consistency check failed");

	// Try to revert the OPL file to its defaults from our resource
	fopen_s(&file, "plugins/OMSIPresence.opl", "wb");
	if (!file)
	{
		char error[64];
		strerror_s(error, errno);
		Error("Inconsistencies in the OMSIPresence.opl file have been detected. OMSIPresence cannot continue.\n\n"
			"Failed to revert the file's contents to their defaults - error opening file.\n\nError code %d: %s", errno, error);
		return false;
	}

	fwrite(opl, 1, length, file);
	fclose(file);

	Error("Inconsistencies in the OMSIPresence.opl file have been detected. OMSIPresence cannot continue.\n\n"
		"The file's contents have been reverted to their defaults. OMSI 2 must be restarted for these changes to take effect.");
	return false;
}

// Gets called when the game starts
void __stdcall PluginStart(void* aOwner)
{
#ifdef PROJECT_DEBUG
	FILE* console;
	AllocConsole();
	freopen_s(&console, "CONIN$", "r", stdin);
	freopen_s(&console, "CONOUT$", "w", stdout);
	freopen_s(&console, "CONOUT$", "w", stderr);
#endif

	DEBUG("Plugin has started");

	offsets = nullptr;
	if (!OplCheck())
	{
		return;
	}

	offsets = VersionCheck();
	if (!offsets)
	{
		return;
	}

	discord::Setup();
	discord::Update();

	DEBUG("Rich presence initialized");
}

// Gets called each game frame per variable when said system variable receives an update
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
}

// Ditto, but for vehicle variables
void __stdcall AccessVariable(unsigned short index, float* value, bool* write)
{
	// AI
	sysvars::ai = *value == 1.0f;
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

void CreateDump(EXCEPTION_POINTERS* exception_pointers)
{
	MINIDUMP_EXCEPTION_INFORMATION exception_info;
	exception_info.ClientPointers = TRUE;
	exception_info.ExceptionPointers = exception_pointers;
	exception_info.ThreadId = GetCurrentThreadId();

	HANDLE hProcess = GetCurrentProcess();
	HANDLE hFile = CreateFile(PROJECT_NAME ".dmp", GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	MiniDumpWriteDump(hProcess, GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, &exception_info, NULL, NULL);

	Error("Exception %08X at %08X. OMSI 2 cannot continue safely and will be terminated.\n\n"
		"An OMSIPresence.dmp file containing more information about the crash has been created in your game folder. "
		"Please submit this file for developer review.",
		exception_pointers->ExceptionRecord->ExceptionCode, exception_pointers->ExceptionRecord->ExceptionAddress);

	// Normally, we would pass this exception on using EXCEPTION_EXECUTE_HANDLER in our exception handlers which call this function
	// Unfortunately, the game's built-in exception handler will throw it away, so we'll just have to terminate the program here
	TerminateProcess(hProcess, ERROR_UNHANDLED_EXCEPTION);
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
