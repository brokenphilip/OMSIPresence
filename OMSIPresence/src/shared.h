#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <minidumpapiset.h>
#include <thread>

#include <cstdint>

/* === Main project stuff ===*/

// Uncomment to use debug mode
#define PROJECT_DEBUG

#define PROJECT_VERSION "0.4"

inline HMODULE dll_instance;
inline float timer = 0;

/* === Shared data === */

inline bool version_ok = false;
inline bool in_game = false;
inline uint8_t hard_paused1 = 0;
inline uint8_t hard_paused2 = 0;

namespace sysvars
{
	inline bool pause = false;
	inline bool ai = false;
}

/* === Utility === */

enum class dbg : uint8_t
{
	ok,
	error,
	warn,
	info
};

#ifdef PROJECT_DEBUG
void Debug(dbg type, const char* message, ...);
#define DEBUG(...) Debug(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

void Error(const char* message, ...);

void CreateDump(EXCEPTION_POINTERS* exception_pointers);

template <typename T>
inline T ReadMemory(uintptr_t address)
{
	return *reinterpret_cast<T*>(address);
}

template <typename T>
inline void WriteMemory(uintptr_t address, T value)
{
	DWORD protection_flags = 0;
	VirtualProtect(reinterpret_cast<LPVOID>(address), sizeof(T), PAGE_EXECUTE_READWRITE, &protection_flags);

	T* memory = reinterpret_cast<T*>(address);
	*memory = value;

	VirtualProtect(reinterpret_cast<LPVOID>(address), sizeof(T), protection_flags, &protection_flags);
}

inline void WriteNop(uintptr_t address, int count = 1)
{
	DWORD protection_flags = 0;
	VirtualProtect(reinterpret_cast<LPVOID>(address), count, PAGE_EXECUTE_READWRITE, &protection_flags);

	memset(reinterpret_cast<void*>(address), 0x90, count);

	VirtualProtect(reinterpret_cast<LPVOID>(address), count, protection_flags, &protection_flags);
}

// NOTE: The jump instruction is 5 bytes
inline void WriteJmp(uintptr_t address, void* jump_to, size_t length = 5)
{
	ptrdiff_t relative = reinterpret_cast<uintptr_t>(jump_to) - address - 5;

	// Write the jump instruction
	WriteMemory<uint8_t>(address, 0xE9);

	// Write the relative address to jump to
	WriteMemory<ptrdiff_t>(address + 1, relative);

	// Write nops until we've reached length
	WriteNop(address + 5, length - 5);
}

int ListLength(uintptr_t list);
bool BoundCheck(uintptr_t list, int index);

/* === Game function calls === */

uintptr_t TRVList_GetMyVehicle();

char* TTimeTableMan_GetLineName(int index);

int TTimeTableMan_GetBusstopCount(int line, int tour, int tour_entry);

/* === Offsets === */

#define OMSI_VERSION "2.3.004 - Latest Steam version"
namespace offsets
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
	constexpr uintptr_t tmap = 0x861588;

	// Offset from TMap, friendlyname
	constexpr uintptr_t tmap_friendlyname = 0x158;

	// Pointer to the timetable manager
	constexpr uintptr_t tttman = 0x8614E8;

	// Offset from TTimeTableMan, Trips
	constexpr uintptr_t tttman_trips = 0xC;

	// Offset from TTimeTableMan, Lines
	constexpr uintptr_t tttman_lines = 0x18;

	// Pointer to the roadvehicle list (gets created upon launching OMSI)
	constexpr uintptr_t trvlist = 0x861508;

	// Location of the function TRVList.GetMyVehicle:TRoadVehicleInst
	constexpr uintptr_t trvlist_getmyvehicle = 0x74A43C;

	// Offset from TRoadVehicleInst, AI_Scheduled_Info_Valid
	constexpr uintptr_t trvinst_sch_info_valid = 0x65C;

	// Offset from TRoadVehicleInst, AI_Scheduled_Line
	constexpr uintptr_t trvinst_sch_line = 0x660;

	// Offset from TRoadVehicleInst, AI_Scheduled_Tour
	constexpr uintptr_t trvinst_sch_tour = 0x664;

	// Offset from TRoadVehicleInst, AI_Scheduled_TourEntry
	constexpr uintptr_t trvinst_sch_tourentry = 0x668;

	// Offset from TRoadVehicleInst, AI_Scheduled_Trip
	constexpr uintptr_t trvinst_sch_trip = 0x66C;

	// Offset from TRoadVehicleInst, AI_Scheduled_NextBusstop
	constexpr uintptr_t trvinst_sch_next = 0x680;

	// Offset from TRoadVehicleInst, AI_Schuduled_NextBusstopName
	constexpr uintptr_t trvinst_sch_next_stop = 0x6AC;

	// Offset from TRoadVehicleInst, AI_Scheduled_Delay
	constexpr uintptr_t trvinst_sch_delay = 0x6BC;

	// Offset from TRoadVehicleInst, RoadVehicle (pointer to its TRoadVehicle)
	constexpr uintptr_t trvinst_trv = 0x710;

	// Offset from TRoadVehicleInst, target_int_string
	constexpr uintptr_t trvinst_target = 0x7BC;

	// Offset from TRoadVehicle, friendlyname
	constexpr uintptr_t trv_friendlyname = 0x19C;

	// Offset from TRoadVehicle, hersteller (manufacturer)
	constexpr uintptr_t trv_hersteller = 0x5D8;
}