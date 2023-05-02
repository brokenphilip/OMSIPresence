#include "shared.h"
#include "discord.h"
#include "../resource.h"

/* === Main project stuff === */

extern "C" __declspec(dllexport)void __stdcall PluginStart(void* aOwner);
extern "C" __declspec(dllexport)void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write);
extern "C" __declspec(dllexport)void __stdcall AccessVariable(unsigned short index, float* value, bool* write);
extern "C" __declspec(dllexport)void __stdcall PluginFinalize();

BOOL APIENTRY DllMain(HMODULE instance, DWORD, LPVOID)
{
	dll_instance = instance;
	return TRUE;
}

bool VersionCheck()
{
	if (!strncmp(reinterpret_cast<char*>(offsets::version_check_address), offsets::version_check_string, offsets::version_check_length))
	{
		DEBUG(dbg::ok, "Detected version " OMSI_VERSION);
		return true;
	}

	Error("This version of OMSIPresence does not support this version of OMSI 2.");
	return false;
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

	DEBUG(dbg::info, "OPL length: expected %d, got %d", length, offset);

	if (offset == length)
	{
		char* contents = new char[length];
		fread_s(contents, length, 1, length, file);

		// OPL files are of identical lengths and their contents match
		if (!strncmp(opl, contents, length))
		{
			DEBUG(dbg::ok, "OPL consistency check was successful");

			fclose(file);
			delete[] contents;
			return true;
		}

		DEBUG(dbg::info, "OPL file contents do not match");
		delete[] contents;
	}
	fclose(file);

	// Consistency check failed. Try to revert the OPL file to its defaults from our resource
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
	SetConsoleTitle("OMSIPresence " PROJECT_VERSION " - Debug");

	freopen_s(&console, "CONIN$", "r", stdin);
	freopen_s(&console, "CONOUT$", "w", stdout);
	freopen_s(&console, "CONOUT$", "w", stderr);

	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

	DWORD mode;
	GetConsoleMode(handle, &mode);
	SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
#endif

	DEBUG(dbg::info, "Plugin has started");

	version_ok = VersionCheck();
	if (!version_ok)
	{
		DEBUG(dbg::warn, "Version check failed. OMSIPresence will remain dormant");
		return;
	}

	if (!OplCheck())
	{
		DEBUG(dbg::warn, "OPL consistency check failed. OMSIPresence will remain dormant");
		return;
	}

	discord::Setup();
	discord::Update();
	DEBUG(dbg::ok, "Rich presence initialized");
}

// Gets called each game frame per variable when said system variable receives an update
void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write)
{
	// Version check failed. Remain dormant
	if (!version_ok)
	{
		return;
	}

	// From this point onwards, it is guaranteed that the game will have a map loaded
	in_game = true;

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
	if (!version_ok)
	{
		return;
	}

	discord::Destroy();
	DEBUG(dbg::info, "Rich presence destroyed");
}

/* === Utility === */

#ifdef PROJECT_DEBUG
void Debug(dbg type, const char* message, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	char tag[15] = {0};
	switch (type)
	{
		case dbg::ok: sprintf_s(tag, 15, "\x1B[92m   OK\x1B[0m"); break;
		case dbg::error: sprintf_s(tag, 15, "\x1B[91mERROR\x1B[0m"); break;
		case dbg::warn: sprintf_s(tag, 15, "\x1B[93m WARN\x1B[0m"); break;
		case dbg::info: sprintf_s(tag, 15, "\x1B[97m INFO\x1B[0m"); break;
	}

	SYSTEMTIME time;
	GetLocalTime(&time);
	printf("[%02d:%02d:%02d %s] %s\n", time.wHour, time.wMinute, time.wSecond, tag, buffer);
}
#endif

void Error(const char* message, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	DEBUG(dbg::error, "MBox: %s", buffer);

	MessageBoxA(NULL, buffer, "OMSIPresence " PROJECT_VERSION, MB_ICONERROR);
}

void CreateDump(EXCEPTION_POINTERS* exception_pointers)
{
	// Set up minidump information
	MINIDUMP_EXCEPTION_INFORMATION exception_info;
	exception_info.ClientPointers = TRUE;
	exception_info.ExceptionPointers = exception_pointers;
	exception_info.ThreadId = GetCurrentThreadId();

	// Format minidump name to OMSIPresence_HHMMSS.dmp
	SYSTEMTIME time;
	GetLocalTime(&time);

	char dmp_filename[24];
	sprintf_s(dmp_filename, 24, "OMSIPresence_%02d%02d%02d.dmp", time.wHour, time.wMinute, time.wSecond);

	// Write minidump file
	HANDLE hProcess = GetCurrentProcess();
	HANDLE hFile = CreateFile(dmp_filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	MiniDumpWriteDump(hProcess, GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, &exception_info, NULL, NULL);

	CloseHandle(hFile);

	// Get the name of the module our address is in
	PVOID address = exception_pointers->ExceptionRecord->ExceptionAddress;

	char module_name[MAX_PATH] = { 0 };
	MEMORY_BASIC_INFORMATION memory_info;
	if (VirtualQuery(address, &memory_info, 28UL))
	{
		if (GetModuleFileName((HMODULE)memory_info.AllocationBase, module_name, MAX_PATH))
		{
			sprintf_s(module_name, MAX_PATH, " in module %s", strrchr(module_name, '\\') + 1);
		}
	}

	// If we're in release, error and terminate. If we're in debug, just print a log message
	DWORD code = exception_pointers->ExceptionRecord->ExceptionCode;
	
#ifndef PROJECT_DEBUG
	Error("Exception %08X at %08X%s. OMSI 2 cannot continue safely and will be terminated.\n\n"
		"An %s file containing more information about the crash has been created in your game folder. "
		"Please submit this file for developer review.",
		code, address, module_name, dmp_filename);

	// Normally, we would pass this exception on using EXCEPTION_EXECUTE_HANDLER in our exception handlers which call this function
	// Unfortunately, the game's built-in exception handler will throw it away, so we'll just have to terminate the program here
	TerminateProcess(hProcess, ERROR_UNHANDLED_EXCEPTION);
#else
	DEBUG(dbg::error, "Exception %08X at %08X%s. Saved to %s", code, address, module_name, dmp_filename);
#endif
}

// The length of a dynamic array is located at the list's address minus 4
inline int ListLength(uintptr_t list)
{
	return ReadMemory<int>(list - 4);
}

// Similar behavior to BoundErr
inline bool BoundCheck(uintptr_t list, int index)
{
	if (index < 0 || index >= ListLength(list))
	{
		return false;
	}

	return true;
}

/* === Game function calls === */

__declspec(naked) uintptr_t TRVList_GetMyVehicle()
{
	// Delphi's "register" callconv is incompatible with any VC++ callconvs
	// This assembly shows exactly how the game calls this function internally
	__asm
	{
		mov     eax, offsets::trvlist
		mov     eax, [eax]
		call    offsets::trvlist_getmyvehicle
		ret
	}
}

char* TTimeTableMan_GetLineName(int line)
{
	auto tttman = ReadMemory<uintptr_t>(offsets::tttman);
	auto lines = ReadMemory<uintptr_t>(tttman + offsets::tttman_lines);

	if (!BoundCheck(lines, line))
	{
		DEBUG(dbg::error, "GetLineName: %d is out of bounds (less than 0 or greater than %d)", line, ListLength(lines));
		return nullptr;
	}

	// 0x10 is the size of each element in the list
	return ReadMemory<char*>(lines + line * 0x10);
}

int TTimeTableMan_GetBusstopCount(int line, int tour, int tour_entry)
{
	auto tttman = ReadMemory<uintptr_t>(offsets::tttman);
	auto lines = ReadMemory<uintptr_t>(tttman + offsets::tttman_lines);
	if (!BoundCheck(lines, line))
	{
		DEBUG(dbg::error, "GetBusstopCount: Line %d is out of bounds (less than 0 or greater than %d)", line, ListLength(lines));
		return -1;
	}

	auto tours_for_line = ReadMemory<uintptr_t>(lines + line * 0x10 + 8);
	if (!BoundCheck(tours_for_line, tour))
	{
		DEBUG(dbg::error, "GetBusstopCount: Tour %d is out of bounds (less than 0 or greater than %d)", tour, ListLength(tours_for_line));
		return -1;
	}

	auto entries_for_tour = ReadMemory<uintptr_t>(tours_for_line + tour * 0x24 + 0x2C);
	if (!BoundCheck(entries_for_tour, tour_entry))
	{
		DEBUG(dbg::error, "GetBusstopCount: TourEntry %d is out of bounds (less than 0 or greater than %d)", tour_entry, ListLength(entries_for_tour));
		return -1;
	}

	auto trip = ReadMemory<uintptr_t>(entries_for_tour + tour_entry * 0x18 + 0x4);
	auto trips = ReadMemory<uintptr_t>(tttman + offsets::tttman_trips);
	if (!BoundCheck(trips, trip))
	{
		DEBUG(dbg::error, "GetBusstopCount: Trip %d is out of bounds (less than 0 or greater than %d)", trip, ListLength(trips));
		return -1;
	}

	auto stations_for_trip = ReadMemory<uintptr_t>(trips + trip * 0x28 + 0x18);
	return ListLength(stations_for_trip) - 1;
}