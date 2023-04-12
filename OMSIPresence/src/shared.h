#pragma once
#include <cstdint>

// Uncomment to use debug mode
#define PROJECT_DEBUG

#define PROJECT_NAME "OMSIPresence"
#define PROJECT_VERSION "0.1"

#ifdef PROJECT_DEBUG
void Debug(const char* message, ...);
#define DEBUG(...) Debug(__VA_ARGS__)
#else
#define DEBUG(...)
#endif

void Error(const char* message, ...);

uintptr_t TRVList_GetMyVehicle();
char* TTimeTableMan_GetLineName(int index);

template <typename T>
inline T ReadMemory(uintptr_t address)
{
	return *reinterpret_cast<T*>(address);
}

inline bool inGame = false;
inline float timer = 0;

struct Offsets
{
	// Byte which gets set to 7 whenever you enter a menu which stops gameplay
	// and gets set to 0 whenever you return to gameplay (excluding new vehicles, see below)
	uintptr_t hard_paused1 = 0;

	// Byte which gets set to 3 when you open the new vehicle menu
	// and gets set to 0 when you exit it
	uintptr_t hard_paused2 = 0;

	// Pointer to the map
	uintptr_t tmap = 0;

	// Offset from TMap, friendlyname
	uintptr_t tmap_friendlyname = 0;

	// Pointer to the timetable manager
	uintptr_t tttman = 0;

	// Offset from TTimeTableMan, Lines
	uintptr_t tttman_lines = 0;

	// Pointer to the roadvehicle list (gets created upon launching OMSI)
	uintptr_t trvlist = 0;

	// Location of the function TRVList.GetMyVehicle:TRoadVehicleInst
	uintptr_t trvlist_getmyvehicle = 0;

	// Offset from TRoadVehicleInst, AI_Scheduled_Info_Valid
	uintptr_t trvinst_sch_info_valid = 0;

	// Offset from TRoadVehicleInst, AI_Scheduled_Line
	uintptr_t trvinst_sch_line = 0;

	// Offset from TRoadVehicleInst, AI_Schuduled_NextBusstopName
	uintptr_t trvinst_sch_next_stop = 0;

	// Offset from TRoadVehicleInst, AI_Scheduled_Delay
	uintptr_t trvinst_sch_delay = 0;

	// Offset from TRoadVehicleInst, RoadVehicle (pointer to its TRoadVehicle)
	uintptr_t trvinst_trv = 0;

	// Offset from TRoadVehicleInst, target_int_string
	uintptr_t trvinst_target = 0;

	// Offset from TRoadVehicle, friendlyname
	uintptr_t trv_friendlyname = 0;

	// Offset from TRoadVehicle, hersteller (manufacturer)
	uintptr_t trv_hersteller = 0;
};
inline Offsets* offsets;

namespace sysvars
{
	inline bool pause = false;
	inline bool ai = false;
}