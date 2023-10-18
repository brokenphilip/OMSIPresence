#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <minidumpapiset.h>

#include <string>

/* === Main project stuff ===*/

#define PROJECT_VERSION "1.1"

#define myprintf(dest, size, fmt, ...) _snprintf_s(dest, size, size - 1, fmt, __VA_ARGS__)

/* === Shared data === */

inline HMODULE dll_instance;
inline DWORD main_thread_id;

inline bool debug = false;

namespace g
{
	inline bool first_load = false;
	inline bool hard_paused1 = false;
	inline bool hard_paused2 = false;
}

namespace sysvars
{
	inline bool pause = false;
	inline bool ai = false;
}

/* === Utility === */

struct UnicodeString
{
	uint16_t codepage;
	uint16_t unk_1;
	uint32_t unk_2;
	uint32_t length;
	wchar_t string[1024];

	UnicodeString(const wchar_t* message, ...)
	{
		va_list va;
		va_start(va, message);
		vswprintf_s(string, 1024, message, va);

		codepage = 0x04B0;
		unk_1 = 0x0002;
		unk_2 = 0xFFFFFFFF;
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

char* TTimeTableMan_GetLineName(int index);

int TTimeTableMan_GetBusstopCount(int line, int tour, int tour_entry);

/* === Offsets === */

#define OMSI_VERSION "2.3.004 - Latest Steam version"
namespace Offsets
{
	// String and address of the string that will be used in the version check. Length is automatically calculated at compile time
	constexpr uintptr_t version_check_address = 0x8BBEE3;
	constexpr const char* const version_check_string = ":33333333";
	constexpr size_t version_check_length = std::char_traits<char>::length(version_check_string);

	// Byte which gets set to 7 whenever you enter a menu which stops gameplay
	// and gets set to 0 whenever you return to gameplay (excluding new vehicles, see below)
	constexpr uintptr_t hard_paused1 = 0x861694;

	// Byte which gets set to 3 when you open the new vehicle menu
	// and gets set to 0 when you exit it
	constexpr uintptr_t hard_paused2 = 0x861BCD;

	// Pointer to the map
	constexpr uintptr_t TMap = 0x861588;

	// Offset from TMap, friendlyname
	constexpr uintptr_t TMap_friendlyname = 0x158;

	// Pointer to the timetable manager
	constexpr uintptr_t TTTMan = 0x8614E8;

	// Offset from TTimeTableMan, Trips
	constexpr uintptr_t TTTMan_Trips = 0xC;

	// Offset from TTimeTableMan, Lines
	constexpr uintptr_t TTTMan_Lines = 0x18;

	// Pointer to the roadvehicle list (gets created upon launching OMSI)
	constexpr uintptr_t TRVList = 0x861508;

	// Location of the function TRVList.GetMyVehicle:TRoadVehicleInst
	constexpr uintptr_t TRVList_GetMyVehicle = 0x74A43C;

	// Offset from TRoadVehicleInst, AI_Scheduled_Info_Valid
	constexpr uintptr_t TRVInst_Sch_Info_Valid = 0x65C;

	// Offset from TRoadVehicleInst, AI_Scheduled_Line
	constexpr uintptr_t TRVInst_Sch_line = 0x660;

	// Offset from TRoadVehicleInst, AI_Scheduled_Tour
	constexpr uintptr_t TRVInst_Sch_Tour = 0x664;

	// Offset from TRoadVehicleInst, AI_Scheduled_TourEntry
	constexpr uintptr_t TRVInst_Sch_tourentry = 0x668;

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

	// Location of the function AddLogEntry (custom name, xref " - Fatal Error:       " to find it)
	constexpr uintptr_t AddLogEntry = 0x8022C0;
}