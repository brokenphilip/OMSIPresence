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
	g::dll_instance = instance;
	return TRUE;
}

bool OplCheck()
{
	// OMSIPresence.opl in this project is linked to OMSIPresence.rc and its resource will be used for the consistency check
	HRSRC resource = FindResource(g::dll_instance, MAKEINTRESOURCE(IDR_OPL1), "OPL");
	HGLOBAL global = LoadResource(g::dll_instance, resource);
	LPVOID pointer = LockResource(global);

	char* opl = reinterpret_cast<char*>(pointer);
	int length = SizeofResource(g::dll_instance, resource);

	FILE* file = nullptr;
	fopen_s(&file, "plugins/OMSIPresence.opl", "rb");

	if (!file)
	{
		char error[64] { 0 };
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
		char error[64] { 0 };
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
	char url[128] { 0 };
	myprintf(url, 128, "https://api.github.com/repos/brokenphilip/OMSIPresence/tags?%lld", time(NULL));
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
	char buffer[32] { 0 };
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
				char msg[256] { 0 };
				myprintf(msg, 256, "An update to OMSIPresence is available!\n\nYour version: " PROJECT_VERSION "\nLatest version: %s\n\n"
					"Would you like to go to the OMSIPresence page now?", token);

				if (MessageBoxA(NULL, msg, "OMSIPresence " PROJECT_VERSION, MB_ICONINFORMATION | MB_YESNO | MB_SYSTEMMODAL) == IDYES)
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
	g::main_thread_id = GetCurrentThreadId();

	g::debug = strstr(GetCommandLineA(), "-omsipresence_debug");
	if (g::debug)
	{
		FILE* console;
		if (!AllocConsole())
		{
			Log(LT_WARN, "OMSITextureManager has detected that there is already a console allocated for this program. "
				"Please note that this may disrupt console logging output - consider disabling other plugins which use the console.");
		}
		SetConsoleTitle("OMSIPresence " PROJECT_VERSION " - Debug");

		freopen_s(&console, "CONIN$", "r", stdin);
		freopen_s(&console, "CONOUT$", "w", stdout);
		freopen_s(&console, "CONOUT$", "w", stderr);

		HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

		DWORD mode;
		GetConsoleMode(handle, &mode);
		SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
	}

	g::version = Offsets::CheckVersion();
	if (!g::version)
	{
		Error("OMSIPresence " PROJECT_VERSION " does not support this version of OMSI 2.");
		Log(LT_WARN, "Version check failed. OMSIPresence will remain dormant");
		return;
	}
	Log(LT_INFO, "Detected version %s", g::version);

	if (!OplCheck())
	{
		Log(LT_WARN, "OPL consistency check failed. OMSIPresence will remain dormant");
		return;
	}

	Log(LT_INFO, "Plugin has started (%sversion " PROJECT_VERSION ")", (g::debug ? "in DEBUG mode, " : ""));

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
	g::discord = true;

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
	if (g::discord)
	{
		Discord::Destroy();
		Log(LT_INFO, "Rich presence destroyed");
	}

	Log(LT_INFO, "Plugin finalized");
}

/* === Utility === */

void Log(LogType log_t, const char* message, ...)
{
	char buffer[1024] { 0 };
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	if (g::debug)
	{
		char tag[15] { 0 };
		switch (log_t)
		{
			case LT_FATAL:
			case LT_ERROR: mystrcpy(tag, "\x1B[91mERROR\x1B[0m", 15); break;

			case LT_WARN: mystrcpy(tag, "\x1B[93m WARN\x1B[0m", 15); break;

			/* LT_PRINT, LT_INFO */
			default: mystrcpy(tag, "\x1B[97m INFO\x1B[0m", 15); break;
		}

		SYSTEMTIME time;
		GetLocalTime(&time);
		printf("[%02d:%02d:%02d %s] %s\n", time.wHour, time.wMinute, time.wSecond, tag, buffer);
	}

	// Only write to logfile.txt if we know AddLogEntry is supported
	if (g::version)
	{
		wchar_t* log = UnicodeString<1024>(L"[OMSIPresence] %hs", buffer).string;

		__asm
		{
			mov     eax, log
			mov     dl, log_t
			call    Offsets::AddLogEntry
		}
	}
}

void Error(const char* message, ...)
{
	char buffer[1024] { 0 };
	va_list va;
	va_start(va, message);
	vsprintf_s(buffer, 1024, message, va);

	MessageBoxA(NULL, buffer, "OMSIPresence " PROJECT_VERSION, MB_ICONERROR | MB_SYSTEMMODAL);
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
		jz      no_rp

		// Get hard_paused1 & hard_paused2 and update
		mov     eax, Offsets::hard_paused1
		mov     al, [eax]
		mov     g::hard_paused1, al

		mov     eax, Offsets::hard_paused2
		mov     al, [eax]
		mov     g::hard_paused2, al
		
		call    Discord::Update
	no_rp:
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

char* TRVInst_GetTargetFromHof(uintptr_t vehicle, int target_index)
{
	auto myhof = Read<int>(vehicle + Offsets::TRVInst_myhof);
	auto myTRVehicle = Read<uintptr_t>(vehicle + Offsets::TRVInst_TRV);
	if (myTRVehicle)
	{
		auto hoefe = Read<uintptr_t>(myTRVehicle + Offsets::TRV_hoefe);
		if (hoefe && BoundCheck(hoefe, myhof))
		{
			auto hof = Read<uintptr_t>(hoefe + myhof * 4);
			if (hof)
			{
				auto targets = Read<uintptr_t>(hof + 0x14);
				if (targets && BoundCheck(targets, target_index))
				{
					auto target_string = Read<char*>(targets + 0x18 * target_index + 8);
					if (target_string)
					{
						return target_string;
					}
					else
					{
						Log(LT_ERROR, "GetTargetFromHof: Target string was null");
					}
				}
				else
				{
					if (targets)
					{
						Log(LT_ERROR, "GetTargetFromHof: Target index %d is out of bounds (Size: %d)", target_index, ListLength(targets));
					}
					else
					{
						Log(LT_ERROR, "GetTargetFromHof: Target list was null");
					}
				}
			}
			else
			{
				Log(LT_ERROR, "GetTargetFromHof: Hof was null");
			}
		}
		else
		{
			if (hoefe)
			{
				Log(LT_ERROR, "GetTargetFromHof: Hof index %d is out of bounds (Size: %d)", myhof, ListLength(hoefe));
			}
			else
			{
				Log(LT_ERROR, "GetTargetFromHof: Hof list was null");
			}
		}
	}
	else
	{
		Log(LT_ERROR, "GetTargetFromHof: TRoadVehicle was null");
	}

	return nullptr;
}

char* TTimeTableMan_GetLineName(uintptr_t tttman, int line)
{
	auto lines = Read<uintptr_t>(tttman + Offsets::TTTMan_Lines);

	if (!BoundCheck(lines, line))
	{
		Log(LT_ERROR, "GetLineName: Line %d is out of bounds (Size: %d)", line, ListLength(lines));
		return nullptr;
	}

	// 0x10 is the size of each element in the list
	return Read<char*>(lines + line * 0x10);
}

void TTimeTableMan_GetTripInfo(uintptr_t tttman, int trip, int busstop_index, const char** target_name, const wchar_t** busstop_name, int* busstop_count)
{
	auto trips = Read<uintptr_t>(tttman + Offsets::TTTMan_Trips);
	if (!BoundCheck(trips, trip))
	{
		Log(LT_ERROR, "GetTripInfo: Trip %d is out of bounds (Size: %d)", trip, ListLength(trips));
		return;
	}

	*target_name = Read<char*>(trips + trip * 0x28 + 0x8);

	// Taken from TRoadVehicleInst_ScriptCallback, case 10 (GetTTBusstopCount macro)
	auto busstops_for_trip = Read<uintptr_t>(trips + trip * 0x28 + 0x18);
	if (busstop_count)
	{
		*busstop_count = ListLength(busstops_for_trip) - 1;
	}

	if (!BoundCheck(busstops_for_trip, busstop_index))
	{
		// TODO: watching AI drive out of bounds causes this. should we care?
		Log(LT_ERROR, "GetTripInfo: Bus stop %d is out of bounds (Size: %d)", busstop_index, ListLength(busstops_for_trip));
		return;
	}
	if (busstop_name)
	{
		// Taken from TProgMan.Render ("next Stop:" in Shift+Y overlay)
		*busstop_name = Read<const wchar_t*>(busstops_for_trip + (busstop_index << 6));
	}
}