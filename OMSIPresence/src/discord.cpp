#include <chrono>

#include "discord.h"
#include "veh.h"

void Discord::Setup()
{
	presence.state = state;
	presence.details = details;
	presence.startTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	presence.endTimestamp = NULL;
	presence.largeImageKey = "icon";
	presence.largeImageText = "OMSIPresence " PROJECT_VERSION;
	presence.smallImageKey = icon;
	presence.smallImageText = icon_text;
	presence.instance = 1;

	DiscordEventHandlers handlers { 0 };
	Discord_Initialize("783688666823524353", &handlers, 1, NULL);
}

void Discord::Destroy()
{
	Discord_ClearPresence();
	Discord_Shutdown();

	presence = { 0 };

	details[0] = '\0';
	state[0] = '\0';
	icon[0] = '\0';
	icon_text[0] = '\0';
	credits[0] = '\0';
}

void Discord::UpdateMap(uintptr_t map, char* map_name, size_t size)
{
	if (map)
	{
		auto map_wide = Read<wchar_t*>(map + Offsets::TMap_friendlyname);
		if (map_wide)
		{
			WideCharToMultiByte(CP_UTF8, 0, map_wide, size, map_name, size, NULL, NULL);
		}
		else
		{
			Log(LT_ERROR, "TMap.mapname was null");
			map_name[0] = '\0';
		}
	}
	else
	{
		Log(LT_ERROR, "TMap was null");
		map_name[0] = '\0';
	}
}

void Discord::UpdateVehicle(uintptr_t vehicle, char* vehicle_name, size_t size)
{
	if (vehicle)
	{
		auto myTRVehicle = Read<uintptr_t>(vehicle + Offsets::TRVInst_TRV);
		if (myTRVehicle)
		{
			auto manufacturer = Read<char*>(myTRVehicle + Offsets::TRV_hersteller);
			auto model = Read<char*>(myTRVehicle + Offsets::TRV_friendlyname);

			if (manufacturer && model)
			{
				myprintf(vehicle_name, size, "%s %s", manufacturer, model);
			}
			else
			{
				Log(LT_ERROR, "TRoadVehicle.hersteller and/or TRoadVehicle.friendlyname were null");
				vehicle_name[0] = '\0';
			}
		}
		else
		{
			Log(LT_ERROR, "TRoadVehicleInst.TRVehicle was null");
			vehicle_name[0] = '\0';
		}
	}
	else // We don't have a vehicle anymore
	{
		vehicle_name[0] = '\0';
	}
}

void Discord::UpdateLine(uintptr_t tttman, int line, char* line_name, size_t size)
{
	char* ttt_line_name = TTimeTableMan_GetLineName(tttman, line);
	if (ttt_line_name)
	{
		mystrcpy(line_name, ttt_line_name, size);
	}
	else
	{
		// Debug is printed in TTimeTableMan_GetLineName
		line_name[0] = '\0';
	}
}

void Discord::Update()
{
	// OMSI's internal exception handler will ignore almost all exceptions coming from our plugin, which is why we're using our own
	VEH::AddHandler();

	if (g::first_load) // Have we loaded into a map this session yet?
	{
		/* 
			PART 1: Get all necessary information from the game
			
			Details: map name, vehicle name
			State: current line and terminus
			Icon & text: if we're in-game, have a vehicle, paused, watching AI drive, have a schedule (if we do, are we late or early or on-time)
		*/

		// Is the game paused?
		bool paused = sysvars::pause || g::hard_paused1 || g::hard_paused2;

		// Map name
		#define MAP_SIZE 32
		static char map[MAP_SIZE] { 0 };

		// Pointer to the currently loaded TMap
		static uintptr_t myTMap = 0;

		// If the currently loaded map has changed, get the new name
		auto myTMap_new = Read<uintptr_t>(Offsets::TMap);
		if (myTMap != myTMap_new)
		{
			Log(LT_INFO, "Found new TMap: %08X -> %08X", myTMap, myTMap_new);
			myTMap = myTMap_new;
			Discord::UpdateMap(myTMap, map, MAP_SIZE);
		}
		else if (paused && myTMap)
		{
			// If we're paused, we can afford to preemptively update the map name
			// This fixes an edge case where different maps can have the same address, thus their name would not properly update
			Discord::UpdateMap(myTMap, map, MAP_SIZE);
		}

		// Vehicle name
		#define VEHICLE_SIZE 64
		static char vehicle[VEHICLE_SIZE] { 0 };

		// Pointer to our currently driven TRoadVehicleInst
		static uintptr_t myTRVInst = 0;

		// If the currently driven vehicle has changed, get the new name
		uintptr_t myTRVInst_new = TRVList_GetMyVehicle();
		if (myTRVInst != myTRVInst_new)
		{
			Log(LT_INFO, "Found new TRVInst: %08X -> %08X", myTRVInst, myTRVInst_new);
			myTRVInst = myTRVInst_new;
			Discord::UpdateVehicle(myTRVInst, vehicle, VEHICLE_SIZE);
		}
		else if (paused)
		{
			// If we're paused, we can afford to preemptively update the vehicle name
			// This fixes an edge case where different vehicles can have the same address, thus their name would not properly update
			Discord::UpdateVehicle(myTRVInst, vehicle, VEHICLE_SIZE);
		}

		// Current line
		#define LINE_SIZE 32
		static char line[LINE_SIZE] { 0 };

		// Index of the current line in our active schedule
		static int schedule_line = -1;

		// How many bus stops are there on this trip?
		int schedule_count = -1;

		// Index of the next bus stop
		int schedule_next = -1;

		// Is timetable valid
		bool schedule_valid = false;

		// Our delay to the next bus stop
		int schedule_delay = 0;

		// Next bus stop name
		#define NEXT_NAME_SIZE 64
		char schedule_next_name[NEXT_NAME_SIZE] { 0 };

		// Destination/terminus
		#define TERMINUS_SIZE 64
		char terminus[TERMINUS_SIZE] { 0 };

		if (myTRVInst) // If we have a vehicle
		{
			// If we have a target (terminus) and it isn't set to $allexit$ (special OMSI terminus which tells all passengers to exit the bus, eg. "Out of Order")
			auto target = Read<char*>(myTRVInst + Offsets::TRVInst_Target);
			if (target && strncmp(target, "$allexit$", 10))
			{
				mystrcpy(terminus, target, TERMINUS_SIZE);
			}
			else
			{
				// In case of $allexit$, try to fetch target (terminus) from the HOF (if it was empty instead of $allexit$, this won't do anything, hence why we don't care for it)
				target = TRVInst_GetTargetFromHof(myTRVInst, Read<int>(myTRVInst + Offsets::TRVInst_Target_Index));
				if (target)
				{
					mystrcpy(terminus, target, TERMINUS_SIZE);
				}
			}

			schedule_valid = Read<uint8_t>(myTRVInst + Offsets::TRVInst_Sch_Info_Valid);
			if (schedule_valid)
			{
				schedule_delay = Read<int>(myTRVInst + Offsets::TRVInst_Sch_Delay);
				schedule_next = Read<int>(myTRVInst + Offsets::TRVInst_Sch_NextStop);
				const wchar_t* schedule_next_name_wide = nullptr;
				const char* schedule_target = nullptr;

				auto tttman = Read<uintptr_t>(Offsets::TTTMan);
				TTimeTableMan_GetTripInfo(tttman, Read<int>(myTRVInst + Offsets::TRVInst_Sch_Trip), schedule_next, &schedule_target, &schedule_next_name_wide, &schedule_count);
				if (schedule_next_name_wide)
				{
					WideCharToMultiByte(CP_UTF8, 0, schedule_next_name_wide, NEXT_NAME_SIZE, schedule_next_name, NEXT_NAME_SIZE, NULL, NULL);
				}
				else
				{
					Log(LT_ERROR, "TTimeTableMan_GetTripInfo returned null for the next bus stop name");
				}

				// If we still don't have a target (terminus) somehow, fall back to the timetable target
				if (strlen(terminus) == 0)
				{
					if (schedule_target)
					{
						mystrcpy(terminus, schedule_target, TERMINUS_SIZE);
					}
					else
					{
						Log(LT_ERROR, "TTimeTableMan_GetTripInfo returned null for the target name");
					}
				}

				// If the currently chosen line index has changed, get the new line
				auto schedule_line_new = Read<int>(myTRVInst + Offsets::TRVInst_Sch_line);
				if (schedule_line != schedule_line_new)
				{
					Log(LT_INFO, "Found new line: %d -> %d", schedule_line, schedule_line_new);
					schedule_line = schedule_line_new;
					Discord::UpdateLine(tttman, schedule_line, line, LINE_SIZE);
				}
				else if (paused)
				{
					// If we're paused, we can afford to preemptively update the line name
					// This fixes an edge case where different lines can have the same index, thus their name would not properly update
					Discord::UpdateLine(tttman, schedule_line, line, LINE_SIZE);
				}
			}
			else // We don't have a (valid) schedule
			{
				if (schedule_line != -1)
				{
					Log(LT_INFO, "Found new line: %d -> -1", schedule_line);
					schedule_line = -1;
				}
				line[0] = '\0';
			}
		}
		else // We don't have a vehicle
		{
			if (schedule_line != -1)
			{
				Log(LT_INFO, "Found new line: %d -> -1", schedule_line);
				schedule_line = -1;
			}
			line[0] = '\0';
		}

		/*
			PEAK LENGTH INFO AS OF 26-JAN-24 - IF CHANGED, UPDATE ACCORDINGLY!!!

			PART 2: Format the data to Rich Presence and send it off

			For the details, we display the current map and/or bus name, whatever is available to us.
			If we're in the main menu, we display that we're getting ready to drive.
			- Peak length: 14 (static + \0) + 32 (map) + 64 (vehicle) = 110, out of 128

			For the state, we display the current line and terminus.
			We can only have a terminus if we have a vehicle. We can only have a line if we have (a vehicle and) a schedule.
			If we don't have a vehicle to begin with, we display nothing.
			- Peak length: 19 (static + \0) + 32 (line) + 64 (terminus) + 2x (x = length of bus stop count) = 115 + 2x (Xmax = 6, so 999999 bus stops), out of 128

			For the icon text, we display what the user is currently doing using text - if they're driving, paused, watching AI drive, in freecam or in the main menu.
			If we have a schedule, for the first three aformentioned actions, we additionally display whether they're late, early or on-time.
			We also additionally display the next bus stop as well as the exact delay formatted in +/-MM:SS, where +/- indicates whether we're late/early.
			If the delay is between -2 minutes and +3 minutes, we are on-time.
			- Peak length: 25 ("Paused, watching AI drive") + 14 (" on-time (, )\0") + 64 (next stop) + 16 (delay) = 119, out of 128

			For the icon, we display what the user is currently doing using an icon and a background color.
			When we're in the main menu, a menu icon will show. When we're freelooking, a camera icon will show. When we're freeroaming, a bus icon will show.
			Additionally, if we've paused the game while freelooking or freeroaming, a pause icon will show.
			If we're letting AI control our vehicle, an AI icon will show. For all 5 of the aformentioned icons, the "default" yellow background will be used.
			When we have multiple states at once, pausing has the highest priority, followed by AI, then everything else.
			When we have a schedule, instead of yellow, we use red (indicating we're late), blue (indicating we're early) and green (indicating we're on time).
			- Peak length: 10 ("ai_ontime\0"), out of 16
		*/

		if (strlen(map) > 0 && strlen(vehicle) > 0)
		{
			myprintf(details, DETAILS_SIZE, MAP_EMOJI " %s | " BUS_EMOJI " %s", map, vehicle);
		}
		else if (strlen(map) > 0)
		{
			myprintf(details, DETAILS_SIZE, MAP_EMOJI " %s", map);
		}
		else if (strlen(vehicle) > 0)
		{
			myprintf(details, DETAILS_SIZE, BUS_EMOJI " %s", vehicle);
		}
		else
		{
			details[0] = '\0';
		}

		if (myTRVInst) // We have a vehicle
		{
			char icon_text_1[ICONTEXT_SIZE] { 0 };
			char icon_text_2[ICONTEXT_SIZE] { 0 };

			// First, handle the left part of the icon state (the second part is handled by the timetable delay)
			if (paused && sysvars::ai)
			{
				mystrcpy(icon_text_1, "Paused, watching AI drive", ICONTEXT_SIZE);
			}
			else if (paused)
			{
				mystrcpy(icon_text_1, "Paused, driving", ICONTEXT_SIZE);
			}
			else if (sysvars::ai)
			{
				mystrcpy(icon_text_1, "Watching AI drive", ICONTEXT_SIZE);
			}
			else
			{
				mystrcpy(icon_text_1, "Driving", ICONTEXT_SIZE);
			}

			if (schedule_valid) // We have a schedule
			{
				char state_1[STATE_SIZE] { 0 };
				char state_2[STATE_SIZE] { 0 };

				// Left part of the state
				if (schedule_next >= 0 && schedule_count > 0)
				{
					myprintf(state_1, STATE_SIZE, BUSSTOP_EMOJI " %d/%d", schedule_next + 1, schedule_count + 1);
				}

				// Right part of the state - only show the line (if there is one) if it's accompanied by a terminus, otherwise don't show anything
				if (strlen(line) > 0 && strlen(terminus) > 0)
				{
					myprintf(state_2, STATE_SIZE, LINE_EMOJI " %s => %s", line, terminus);
				}
				else if (strlen(terminus) > 0)
				{
					myprintf(state_2, STATE_SIZE, LINE_EMOJI " %s", terminus);
				}

				// Combine the left and right parts of the state
				if (strlen(state_1) > 0 && strlen(state_2) > 0)
				{
					myprintf(state, STATE_SIZE, "%s | %s", state_1, state_2);
				}
				else if (strlen(state_1) > 0)
				{
					mystrcpy(state, state_1, STATE_SIZE);
				}
				else if (strlen(state_2) > 0)
				{
					mystrcpy(state, state_2, STATE_SIZE);
				}
				else
				{
					state[0] = '\0';
				}

				#define DELAY_SIZE 16
				char delay[DELAY_SIZE] { 0 };

				if (schedule_delay < 0)
				{
					myprintf(delay, DELAY_SIZE, "-%01d:%02d", -schedule_delay / 60, -schedule_delay % 60);
				}
				else
				{
					myprintf(delay, DELAY_SIZE, "+%01d:%02d", schedule_delay / 60, schedule_delay % 60);
				}

				if (schedule_delay >= 180) // We're late
				{
					if (paused)
					{
						mystrcpy(icon, "p_late", ICON_SIZE);
					}
					else if (sysvars::ai)
					{
						mystrcpy(icon, "ai_late", ICON_SIZE);
					}
					else
					{
						mystrcpy(icon, "late", ICON_SIZE);
					}

					if (strlen(schedule_next_name) > 0)
					{
						myprintf(icon_text_2, ICONTEXT_SIZE, "late (%s, %s)", schedule_next_name, delay);
					}
					else
					{
						myprintf(icon_text_2, ICONTEXT_SIZE, "late (%s)", delay);
					}
				}
				else if (schedule_delay <= -120) // We're early
				{
					if (paused)
					{
						mystrcpy(icon, "p_early", ICON_SIZE);
					}
					else if (sysvars::ai)
					{
						mystrcpy(icon, "ai_early", ICON_SIZE);
					}
					else
					{
						mystrcpy(icon, "early", ICON_SIZE);
					}

					if (strlen(schedule_next_name) > 0)
					{
						myprintf(icon_text_2, ICONTEXT_SIZE, "early (%s, %s)", schedule_next_name, delay);
					}
					else
					{
						myprintf(icon_text_2, ICONTEXT_SIZE, "early (%s)", delay);
					}
				}
				else // We're on time
				{
					if (paused)
					{
						mystrcpy(icon, "p_ontime", ICON_SIZE);
					}
					else if (sysvars::ai)
					{
						mystrcpy(icon, "ai_ontime", ICON_SIZE);
					}
					else
					{
						mystrcpy(icon, "ontime", ICON_SIZE);
					}

					if (strlen(schedule_next_name) > 0)
					{
						myprintf(icon_text_2, ICONTEXT_SIZE, "on-time (%s, %s)", schedule_next_name, delay);
					}
					else
					{
						myprintf(icon_text_2, ICONTEXT_SIZE, "on-time (%s)", delay);
					}
				}
			}
			else // No schedule
			{
				if (strlen(terminus) > 0)
				{
					myprintf(state, STATE_SIZE, LINE_EMOJI " %s", terminus);
				}
				else
				{
					state[0] = '\0';
				}

				if (paused)
				{
					mystrcpy(icon, "paused", ICON_SIZE);
				}
				else if (sysvars::ai)
				{
					mystrcpy(icon, "ai", ICON_SIZE);
				}
				else
				{
					mystrcpy(icon, "driving", ICON_SIZE);
				}
			}

			// Assemble our icon text from the left and right parts
			if (strlen(icon_text_2) > 0)
			{
				myprintf(icon_text, ICONTEXT_SIZE, "%s %s", icon_text_1, icon_text_2);
			}
			else
			{
				mystrcpy(icon_text, icon_text_1, ICONTEXT_SIZE);
			}
		}
		else // No vehicle
		{
			state[0] = '\0';

			if (paused)
			{
				mystrcpy(icon_text, "Paused, freelooking", ICONTEXT_SIZE);
				mystrcpy(icon, "paused", ICON_SIZE);
			}
			else
			{
				mystrcpy(icon_text, "Freelooking", ICONTEXT_SIZE);
				mystrcpy(icon, "camera", ICON_SIZE);
			}
		}
	}
	else // Not in-game
	{
		mystrcpy(icon_text, "In the main menu", ICONTEXT_SIZE);
		mystrcpy(icon, "menu", ICON_SIZE);
	}
	
	Discord_UpdatePresence(&presence);

	VEH::RemoveHandler();
}