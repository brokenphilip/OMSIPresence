#include <urlmon.h>
#include <comdef.h>
#include <time.h>

#include "shared.h"

#include "discord.h"
#include "veh.h"
#include "../resource.h"

/* === Main project stuff === */

extern "C" __declspec(dllexport) void __stdcall PluginStart(void* aOwner);
extern "C" __declspec(dllexport) void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write);
extern "C" __declspec(dllexport) void __stdcall AccessVariable(unsigned short index, float* value, bool* write);
extern "C" __declspec(dllexport) void __stdcall PluginFinalize();

BOOL APIENTRY DllMain(HMODULE instance, DWORD, LPVOID)
{
	dll_instance = instance;
	return TRUE;
}

bool VersionCheck()
{
	if (!strncmp(reinterpret_cast<char*>(Offsets::version_check_address), Offsets::version_check_string, Offsets::version_check_length))
	{
		Log(LT_INFO, "Detected version " OMSI_VERSION);
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

	Log(LT_INFO, "OPL length: expected %d, got %d", length, offset);

	if (offset == length)
	{
		char* contents = new char[length];
		fread_s(contents, length, 1, length, file);

		// OPL files are of identical lengths and their contents match
		if (!strncmp(opl, contents, length))
		{
			Log(LT_INFO, "OPL consistency check was successful");

			fclose(file);
			delete[] contents;
			return true;
		}

		Log(LT_WARN, "OPL file contents do not match");
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

void UpdateCheck()
{
	Log(LT_INFO, "Checking for updates...");

	/*
	// HACK: Append current unix time as an unused query string to avoid caching
	char url[128] = { 0 };
	sprintf_s(url, 128, "https://api.github.com/repos/brokenphilip/OMSIPresence/tags?%lld", time(NULL));
	*/

	// Attempt to connect
	IStream* stream;
	HRESULT result = URLOpenBlockingStreamA(0, "https://api.github.com/repos/brokenphilip/OMSIPresence/tags", &stream, 0, 0);
	if (FAILED(result))
	{
		Log(LT_WARN, "Connection failed with HRESULT %lX: %s", result, _com_error(result).ErrorMessage());
		return;
	}

	// Read first 32 bytes from the result, we don't need any more
	char buffer[32] = { 0 };
	stream->Read(buffer, 31, nullptr);
	stream->Release();

	// Parse the response using tokens
	auto seps = "\"";
	char* next_token = nullptr;
	char* token = strtok_s(buffer, seps, &next_token);

	for (int i = 0; token != nullptr && i < 3; i++)
	{
		token = strtok_s(NULL, seps, &next_token);

		// First should just be "name"
		if (i == 0 && strncmp(token, "name", 4))
		{
			break;
		}

		// Third should be the latest released version
		if (i == 2)
		{
			Log(LT_INFO, "Version check complete, latest version is %s", token);

			constexpr size_t len = std::char_traits<char>::length(PROJECT_VERSION);
			size_t len2 = strlen(token);

			// If versions do not match, prompt to be taken to the OMSIPresence page
			if (strncmp(token, PROJECT_VERSION, ((len > len2) ? len : len2)))
			{
				char msg[256];
				sprintf_s(msg, 256, "An update to OMSIPresence is available!\n\nYour version: " PROJECT_VERSION "\nLatest version: %s\n\n"
					"Would you like to go to the OMSIPresence page now?", token);

				if (MessageBoxA(NULL, msg, "OMSIPresence " PROJECT_VERSION, MB_ICONINFORMATION | MB_YESNO) == IDYES)
				{
					ShellExecuteA(NULL, NULL, "https://github.com/brokenphilip/OMSIPresence", NULL, NULL, SW_SHOW);
				}
			}

			return;
		}
	}

	Log(LT_WARN, "Version check failed to parse the response");
}

// Gets called when the game starts
void __stdcall PluginStart(void* aOwner)
{
	Discord::presence = nullptr;
	main_thread_id = GetCurrentThreadId();

	debug = strstr(GetCommandLineA(), "-omsipresence_debug");
	if (debug)
	{
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
	}

	Log(LT_INFO, "Plugin has started (%sversion " PROJECT_VERSION ")", (debug? "in DEBUG mode, " : ""));

	if (!VersionCheck())
	{
		Log(LT_WARN, "Version check failed. OMSIPresence will remain dormant");
		return;
	}

	if (!OplCheck())
	{
		Log(LT_WARN, "OPL consistency check failed. OMSIPresence will remain dormant");
		return;
	}

	if (strstr(GetCommandLineA(), "-omsipresence_noupdate"))
	{
		Log(LT_WARN, "Skipping update check due to launch option. "
			"It is highly recommended you stay up-to-date, so please only use this option if you're having issues");
	}
	else
	{
		UpdateCheck();
	}

	Discord::Setup();
	Discord::Update();
	Log(LT_INFO, "Rich presence initialized");

	// Creates an enabled (implicit) TTimer with a 1 second interval (implicit) and OnTimer set to our function
	__asm
	{
		mov     ecx, aOwner
		mov     dl, 1
		mov     eax, Offsets::TTimer_vtable
		call    Offsets::TTimer_Create

		push    aOwner
		push    OnTimer
		call    Offsets::TTimer_SetOnTimer
	}

	Log(LT_INFO, "Update TTimer instantiated");
}

// Gets called as often as each game frame per variable ONLY when a system variable receives an update
void __stdcall AccessSystemVariable(unsigned short index, float* value, bool* write)
{
	// From this point onwards, it is guaranteed that the game will have a map loaded
	g::first_load = true;

	// Pause
	sysvars::pause = *value == 1.0f;
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
	if (Discord::presence)
	{
		Discord::Destroy();
		Log(LT_INFO, "Rich presence destroyed");
	}

	Log(LT_INFO, "Plugin finalized");
}

/* === Utility === */

void Log(LogType log_t, const char* message, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	if (debug)
	{
		char tag[15] = { 0 };
		switch (log_t)
		{
			case LT_FATAL:
			case LT_ERROR: sprintf_s(tag, 15, "\x1B[91mERROR\x1B[0m"); break;

			case LT_WARN: sprintf_s(tag, 15, "\x1B[93m WARN\x1B[0m"); break;

			/* LT_PRINT, LT_INFO */
			default: sprintf_s(tag, 15, "\x1B[97m INFO\x1B[0m"); break;
		}

		SYSTEMTIME time;
		GetLocalTime(&time);
		printf("[%02d:%02d:%02d %s] %s\n", time.wHour, time.wMinute, time.wSecond, tag, buffer);
	}

	int a = 2;

	wchar_t* log = UnicodeString(L"[OMSIPresence] %hs", buffer).string;

	__asm
	{
		mov     eax, log
		mov     dl, log_t
		call    Offsets::AddLogEntry
	}
}

void Error(const char* message, ...)
{
	char buffer[1024];
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	//Log(LT_ERROR, "MBox: %s", buffer);

	MessageBoxA(NULL, buffer, "OMSIPresence " PROJECT_VERSION, MB_ICONERROR);
}

inline int ListLength(uintptr_t list)
{
	return Read<int>(list - 4);
}

// Similar behavior to bound checks with BoundErr, inlined throughout OMSI
inline bool BoundCheck(uintptr_t list, int index)
{
	if (index < 0 || index >= ListLength(list))
	{
		return false;
	}

	return true;
}

/* === Game functions === */

__declspec(naked) void OnTimer()
{
	__asm
	{
		// Skip if we don't have Rich Presence
		mov     eax, Discord::presence
		test    eax, eax
		jg      not_null
		ret

		// Get hard_paused1 & hard_paused2 and update
	not_null:
		mov     eax, Offsets::hard_paused1
		mov     al, [eax]
		mov     g::hard_paused1, al

		mov     eax, Offsets::hard_paused2
		mov     al, [eax]
		mov     g::hard_paused2, al
		
		call    Discord::Update
		ret
	}
}

__declspec(naked) uintptr_t TRVList_GetMyVehicle()
{
	// Delphi's "register" callconv is incompatible with any VC++ callconvs
	// This assembly shows exactly how the game calls this function internally
	__asm
	{
		mov     eax, Offsets::TRVList
		mov     eax, [eax]
		call    Offsets::TRVList_GetMyVehicle
		ret
	}
}

char* TTimeTableMan_GetLineName(int line)
{
	auto tttman = Read<uintptr_t>(Offsets::TTTMan);
	auto lines = Read<uintptr_t>(tttman + Offsets::TTTMan_Lines);

	if (!BoundCheck(lines, line))
	{
		Log(LT_ERROR, "GetLineName: %d is out of bounds (less than 0 or greater than %d)", line, ListLength(lines));
		return nullptr;
	}

	// 0x10 is the size of each element in the list
	return Read<char*>(lines + line * 0x10);
}

int TTimeTableMan_GetBusstopCount(int line, int tour, int tour_entry)
{
	auto tttman = Read<uintptr_t>(Offsets::TTTMan);
	auto lines = Read<uintptr_t>(tttman + Offsets::TTTMan_Lines);
	if (!BoundCheck(lines, line))
	{
		Log(LT_ERROR, "GetBusstopCount: Line %d is out of bounds (less than 0 or greater than %d)", line, ListLength(lines));
		return -1;
	}

	auto tours_for_line = Read<uintptr_t>(lines + line * 0x10 + 0x8);
	if (!BoundCheck(tours_for_line, tour))
	{
		Log(LT_ERROR, "GetBusstopCount: Tour %d is out of bounds (less than 0 or greater than %d)", tour, ListLength(tours_for_line));
		return -1;
	}

	auto entries_for_tour = Read<uintptr_t>(tours_for_line + tour * 0x30 + 0x2C);
	if (!BoundCheck(entries_for_tour, tour_entry))
	{
		Log(LT_ERROR, "GetBusstopCount: TourEntry %d is out of bounds (less than 0 or greater than %d)", tour_entry, ListLength(entries_for_tour));
		return -1;
	}

	auto trip = Read<uintptr_t>(entries_for_tour + tour_entry * 0x18 + 0x4);
	auto trips = Read<uintptr_t>(tttman + Offsets::TTTMan_Trips);
	if (!BoundCheck(trips, trip))
	{
		Log(LT_ERROR, "GetBusstopCount: Trip %d is out of bounds (less than 0 or greater than %d)", trip, ListLength(trips));
		return -1;
	}

	auto stations_for_trip = Read<uintptr_t>(trips + trip * 0x28 + 0x18);
	return ListLength(stations_for_trip) - 1;
}