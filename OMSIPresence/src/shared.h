#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <minidumpapiset.h>

#include <string>

/* === Main project stuff ===*/

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! MAKE SURE TO UPDATE THE VERSION RESOURCE AS WELL !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#define PROJECT_VERSION "1.2"

#define myprintf(dest, size, fmt, ...) _snprintf_s(dest, size, size - 1, fmt, __VA_ARGS__)

/* === Shared data === */

namespace g
{
	// Handle to this dll, used for the OPL resource
	inline HMODULE dll_instance;

	// ID of the main thread, used for exception handling
	inline DWORD main_thread_id;

	// Is debug mode (launch option) enabled?
	inline bool debug = false;

	// Current version name
	inline const char* version = nullptr;

	// Have we already loaded a map at some point?
	inline bool first_load = false;

	// Is the game paused (by ESC or simulation-pausing dialogs)?
	inline bool hard_paused1 = false;
	inline bool hard_paused2 = false;
}

namespace sysvars
{
	inline bool pause = false;
	inline bool ai = false;
}

/* === Utility === */

template <size_t size>
struct UnicodeString
{
	uint16_t code_page;
	uint16_t element_size;
	uint32_t reference_count;
	uint32_t length;
	wchar_t string[size];

	UnicodeString(const wchar_t* message, ...)
	{
		va_list va;
		va_start(va, message);
		vswprintf_s(string, size, message, va);

		code_page = CP_WINUNICODE;
		element_size = 2;
		reference_count = -1;
		length = std::char_traits<wchar_t>::length(string);
	}
};

enum LogType : char
{
	LT_PRINT = 0x0, // Prefixless log entries. Only written to logfile.txt if OMSI is started with "-logall". Not used by OMSIPresence
	LT_INFO  = 0x1, // "Information" log entries. Used by OMSIPresence to log its standard procedures
	LT_WARN  = 0x2, // "Warning" log entries. Used by OMSIPresence to log generally unwanted (but not critical) states
	LT_ERROR = 0x3, // "Error" log entries. Used by OMSIPresence to log illegal states, some of which unrecoverable
	LT_FATAL = 0x4, // "Fatal Error" log entries. Shows a "OMSI will be closed" message box. Not used by OMSIPresence
};

void Log(LogType type, const char* message, ...);
void Error(const char* message, ...);

template <typename T>
inline T Read(uintptr_t address)
{
	return *reinterpret_cast<T*>(address);
}

int ListLength(uintptr_t list);
bool BoundCheck(uintptr_t list, int index);

/* === Game functions === */

void OnTimer();

uintptr_t TRVList_GetMyVehicle();

char* TTimeTableMan_GetLineName(uintptr_t tttman, int index);
void TTimeTableMan_GetTripInfo(uintptr_t tttman, int trip, int busstop_index, const wchar_t** busstop_name, int* busstop_count);

/* === Offsets === */

// No offset macro
#define NO_OF(id) 0x0//0x00DEAD00 + 0x01000000 * id
namespace Offsets
{
	// Byte which gets set to 7 whenever you enter a menu which stops gameplay
	// and gets set to 0 whenever you return to gameplay (excluding new vehicles, see below)
	inline uintptr_t hard_paused1 = NO_OF(0);

	// Byte which gets set to 3 when you open the new vehicle menu
	// and gets set to 0 when you exit it
	inline uintptr_t hard_paused2 = NO_OF(1);

	// Pointer to the map
	inline uintptr_t TMap = NO_OF(2);

	// Offset from TMap, friendlyname
	constexpr uintptr_t TMap_friendlyname = 0x158;

	// Pointer to the timetable manager
	inline uintptr_t TTTMan = NO_OF(3);

	// Offset from TTimeTableMan, Trips
	constexpr uintptr_t TTTMan_Trips = 0xC;

	// Offset from TTimeTableMan, Lines
	constexpr uintptr_t TTTMan_Lines = 0x18;

	// Pointer to the roadvehicle list (gets created upon launching OMSI)
	inline uintptr_t TRVList = NO_OF(4);

	// Location of the function TRVList.GetMyVehicle:TRoadVehicleInst
	inline uintptr_t TRVList_GetMyVehicle = NO_OF(5);

	// Offset from TRoadVehicleInst, AI_Scheduled_Info_Valid
	constexpr uintptr_t TRVInst_Sch_Info_Valid = 0x65C;

	// Offset from TRoadVehicleInst, AI_Scheduled_Line
	constexpr uintptr_t TRVInst_Sch_line = 0x660;

	// Offset from TRoadVehicleInst, AI_Scheduled_Trip
	constexpr uintptr_t TRVInst_Sch_Trip = 0x66C;

	// Offset from TRoadVehicleInst, AI_Scheduled_NextBusstop
	constexpr uintptr_t TRVInst_Sch_NextStop = 0x680;

	// Offset from TRoadVehicleInst, AI_Schuduled_NextBusstopName
	constexpr uintptr_t TRVInst_Sch_NextStopName = 0x6AC;

	// Offset from TRoadVehicleInst, AI_Scheduled_Delay
	constexpr uintptr_t TRVInst_Sch_Delay = 0x6BC;

	// Offset from TRoadVehicleInst, RoadVehicle (pointer to its TRoadVehicle)
	constexpr uintptr_t TRVInst_TRV = 0x710;

	// Offset from TRoadVehicleInst, target_int_string
	constexpr uintptr_t TRVInst_Target = 0x7BC;

	// Offset from TRoadVehicle, friendlyname
	constexpr uintptr_t TRV_friendlyname = 0x19C;

	// Offset from TRoadVehicle, hersteller (manufacturer)
	constexpr uintptr_t TRV_hersteller = 0x5D8;

	// Location of TTimer's VTable
	constexpr uintptr_t TTimer_vtable = 0x4F98C4;

	// Location of the function TTimer.Create
	constexpr uintptr_t TTimer_Create = 0x4FD044;

	// Location of the function TTimer.SetOnTimer
	constexpr uintptr_t TTimer_SetOnTimer = 0x4FD1F8;

	// Location of the function AddLogEntry (custom name I gave the function, xref " - Fatal Error:       " to find it)
	inline uintptr_t AddLogEntry = NO_OF(6);

	// String that will be used in the version check. Length is automatically calculated at compile time
	constexpr const char* const str = ":3";
	constexpr int len = std::char_traits<char>::length(str);

	inline const char* CheckVersion()
	{
		if (!strncmp(reinterpret_cast<char*>(0x7C9780), str, len))
		{
			hard_paused1 = 0x861694;
			hard_paused2 = 0x861BCD;
			TMap = 0x861588;
			TTTMan = 0x8614E8;
			TRVList = 0x861508;
			TRVList_GetMyVehicle = 0x74A43C;
			AddLogEntry = 0x8022C0;

			return "2.3.004 - Latest Steam version";
		}

		if (!strncmp(reinterpret_cast<char*>(0x7C9668), str, len))
		{
			hard_paused1 = 0x861690;
			hard_paused2 = 0x861BC9;
			TMap = 0x861584;
			TTTMan = 0x8614E4;
			TRVList = 0x861504;
			TRVList_GetMyVehicle = 0x74A338;
			AddLogEntry = 0x801FD4;

			return "2.2.032 - Tram patch";
		}

		return nullptr;
	}
}
#undef NO_OF