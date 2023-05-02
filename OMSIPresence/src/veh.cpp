#include "veh.h"

void veh::AddHandler()
{
	if (!handler)
	{
		handler = AddVectoredExceptionHandler(TRUE, ExceptionHandler);
		return;
	}

	DEBUG(dbg::error, "Tried to veh::AddHandler() when the handler already exists");
}

void veh::RemoveHandler()
{
	if (handler)
	{
		RemoveVectoredExceptionHandler(handler);
		handler = nullptr;
		return;
	}

	DEBUG(dbg::error, "Tried to veh::RemoveHandler() when the handler doesn't exist");
}

LONG CALLBACK veh::ExceptionHandler(EXCEPTION_POINTERS* exception_pointers)
{
	veh::RemoveHandler();

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

	return EXCEPTION_EXECUTE_HANDLER;
}