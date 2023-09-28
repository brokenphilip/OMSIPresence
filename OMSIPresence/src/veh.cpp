#include "veh.h"

void VEH::AddHandler()
{
	if (!handler)
	{
		handler = AddVectoredExceptionHandler(TRUE, ExceptionHandler);
		return;
	}

	Log(LT_ERROR, "Tried to veh::AddHandler() when the handler already exists");
}

void VEH::RemoveHandler()
{
	if (handler)
	{
		RemoveVectoredExceptionHandler(handler);
		handler = nullptr;
		return;
	}

	Log(LT_ERROR, "Tried to veh::RemoveHandler() when the handler doesn't exist");
}

// Exceptions in our code aren't the *worst* thing to happen to us (OMSI has them all the time), but there should be zero tolerance for something so simple to break
// Since we're entering a heavily unwanted state, our exception handler creates a dump and either terminates the game, or just adds a log entry if we're in debug
LONG CALLBACK VEH::ExceptionHandler(EXCEPTION_POINTERS* exception_pointers)
{
	// Since this is a Vectored Exception Handler (VEH) and not a Structured Exception Handler (SEH), because we need our exception handler to have priority
	// We must check for exceptions coming from other (non-main) threads and reject them
	DWORD thread_id = GetCurrentThreadId();
	if (thread_id != main_thread_id)
	{
		// TODO: remove this warning for the next release version
		Log(LT_WARN, "VEH called from thread %lu when main thread is %lu", thread_id, main_thread_id);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	VEH::RemoveHandler();

	// Set up minidump information
	MINIDUMP_EXCEPTION_INFORMATION exception_info;
	exception_info.ClientPointers = TRUE;
	exception_info.ExceptionPointers = exception_pointers;
	exception_info.ThreadId = thread_id;

	// Format minidump name to OMSIPresence_HHMMSS.dmp
	SYSTEMTIME time;
	GetLocalTime(&time);

	// Write minidump file
	char dmp_filename[24];
	sprintf_s(dmp_filename, 24, "OMSIPresence_%02d%02d%02d.dmp", time.wHour, time.wMinute, time.wSecond);

	HANDLE hProcess = GetCurrentProcess();
	HANDLE hFile = CreateFile(dmp_filename, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	MiniDumpWriteDump(hProcess, GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, &exception_info, NULL, NULL);
	CloseHandle(hFile);

	// Try to get the name of the module our address is in
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

	DWORD code = exception_pointers->ExceptionRecord->ExceptionCode;

	if (debug)
	{
		Log(LT_ERROR, "Exception %08X at %08X%s. Saved to %s", code, address, module_name, dmp_filename);
	}
	else
	{
		Error("Exception %08X at %08X%s. OMSI 2 cannot continue safely and will be terminated.\n\n"
			"An %s file containing more information about the crash has been created in your game folder. Please submit this file for developer review.",
			code, address, module_name, dmp_filename);

		// Normally, we would pass this exception on, but the game's built-in exception handler will throw it away, so we'll just have to terminate the program here
		TerminateProcess(hProcess, ERROR_UNHANDLED_EXCEPTION);
	}

	return EXCEPTION_CONTINUE_SEARCH;
}