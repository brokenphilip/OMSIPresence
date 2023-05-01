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

	WriteJmp(offsets::tapplication_idle, TApplication_IdleHook, 6);
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
	MINIDUMP_EXCEPTION_INFORMATION exception_info;
	exception_info.ClientPointers = TRUE;
	exception_info.ExceptionPointers = exception_pointers;
	exception_info.ThreadId = GetCurrentThreadId();

	HANDLE hProcess = GetCurrentProcess();
	HANDLE hFile = CreateFile("OMSIPresence.dmp", GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	MiniDumpWriteDump(hProcess, GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, &exception_info, NULL, NULL);

	Error("Exception %08X at %08X. OMSI 2 cannot continue safely and will be terminated.\n\n"
		"An OMSIPresence.dmp file containing more information about the crash has been created in your game folder. "
		"Please submit this file for developer review.",
		exception_pointers->ExceptionRecord->ExceptionCode, exception_pointers->ExceptionRecord->ExceptionAddress);

	// Normally, we would pass this exception on using EXCEPTION_EXECUTE_HANDLER in our exception handlers which call this function
	// Unfortunately, the game's built-in exception handler will throw it away, so we'll just have to terminate the program here
	TerminateProcess(hProcess, ERROR_UNHANDLED_EXCEPTION);
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

char* TTimeTableMan_GetLineName(int index)
{
	auto tttman = ReadMemory<uintptr_t>(offsets::tttman);
	auto lines = ReadMemory<uintptr_t>(tttman + offsets::tttman_lines);

	// According to TTimeTableMan.IsLineIndexValid. Dynamic array size is stored at array address - 4
	if (index < 0 || index >= ReadMemory<int>(lines - 4))
	{
		return nullptr;
	}

	// 0x10 is the size of each element in the list
	return ReadMemory<char*>(lines + index * 0x10);
}

/* === Game function hooks === */

constexpr uintptr_t tapplication_idle_return = offsets::tapplication_idle + 6;
void TApplication_IdleHook()
{
	__asm
	{
		push    eax

		// Check hard_paused1 value
		mov     ecx, offsets::hard_paused1
		mov     ah, [ecx]
		test    ah, ah
		jg      is_hp1

		// hard_paused1 = 0
		mov     al, hard_paused1
		test    al, al
		jg      new_hp1
		jmp     hp2

		// hard_paused1 > 0
	is_hp1:
		mov     al, hard_paused1
		test    al, al
		je      new_hp1
		jmp     hp2

		// hard_paused1 doesn't match
	new_hp1:
		mov    hard_paused1, ah
		call   discord::Update

		// Check hard_paused2 value
	hp2:
		mov     ecx, offsets::hard_paused2
		mov     ah, [ecx]
		test    ah, ah
		jg      is_hp2

		// hard_paused2 = 0
		mov     al, hard_paused2
		test    al, al
		jg      new_hp2
		jmp     end

		// hard_paused2 > 0
	is_hp2:
		mov     al, hard_paused2
		test    al, al
		je      new_hp2
		jmp     end

		// hard_paused2 doesn't match
	new_hp2:
		mov    hard_paused2, ah
		call   discord::Update

	end:
		pop     eax
		push    ebp
		mov     ebp, esp
		add     esp, 0xFFFFFFF0
		push    tapplication_idle_return
		retn
	}
	
}
