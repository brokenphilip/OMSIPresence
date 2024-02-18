#include <chrono>

#include "discord.h"
#include "veh.h"

void Discord::Setup()
{
	details = new char[DETAILS_SIZE] {0};
	state = new char[STATE_SIZE] {0};
	icon = new char[ICON_SIZE] {0};
	icon_text = new char[ICONTEXT_SIZE] {0};
	credits = new char[CREDITS_SIZE] {0};

	presence = new DiscordRichPresence();
	presence->state = state;
	presence->details = details;
	presence->startTimestamp = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	presence->endTimestamp = NULL;
	presence->largeImageKey = "icon";
	presence->largeImageText = "OMSIPresence " PROJECT_VERSION;
	presence->smallImageKey = icon;
	presence->smallImageText = icon_text;
	presence->instance = 1;

	DiscordEventHandlers handlers;
	memset(&handlers, 0, sizeof(handlers));
	Discord_Initialize("783688666823524353", &handlers, 1, NULL);
}

void Discord::Destroy()
{
	Discord_ClearPresence();
	Discord_Shutdown();

	delete presence;

	delete[] details;
	delete[] state;
	delete[] icon;
	delete[] icon_text;
	delete[] credits;
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

		// Map name
		#define MAP_SIZE 32
		static char map[MAP_SIZE] = {0};

		// Pointer to the currently loaded TMap
		static uintptr_t myTMap = 0;

		// If the currently loaded map has changed, get the new name
		auto myTMap_new = Read<uintptr_t>(Offsets::TMap);
		if (myTMap != myTMap_new)
		{
			Log(LT_INFO, "Found new TMap: %08X -> %08X", myTMap, myTMap_new);
			myTMap = myTMap_new;
			if (myTMap)
			{
				auto mapname = Read<wchar_t*>(myTMap + Offsets::TMap_friendlyname);

				if (mapname)
				{
					WideCharToMultiByte(CP_UTF8, 0, mapname, MAP_SIZE, map, MAP_SIZE, NULL, NULL);
				}
				else
				{
					Log(LT_ERROR, "TMap.mapname was null");
					myprintf(map, MAP_SIZE, "");
				}
			}
			else
			{
				Log(LT_ERROR, "TMap was null");
				myprintf(map, MAP_SIZE, "");
			}
		}

		// Vehicle name
		#define VEHICLE_SIZE 64
		static char vehicle[VEHICLE_SIZE] = {0};

		// Pointer to our currently driven TRoadVehicleInst
		static uintptr_t myTRVInst = 0;

		// If the currently driven vehicle has changed, get the new name
		uintptr_t myTRVInst_new = TRVList_GetMyVehicle();
		if (myTRVInst != myTRVInst_new)
		{
			Log(LT_INFO, "Found new TRVInst: %08X -> %08X", myTRVInst, myTRVInst_new);
			myTRVInst = myTRVInst_new;
			if (myTRVInst)
			{
				auto myTRVehicle = Read<uintptr_t>(myTRVInst + Offsets::TRVInst_TRV);
				if (myTRVehicle)
				{
					auto manufacturer = Read<char*>(myTRVehicle + Offsets::TRV_hersteller);
					auto model = Read<char*>(myTRVehicle + Offsets::TRV_friendlyname);

					if (manufacturer && model)
					{
						myprintf(vehicle, VEHICLE_SIZE, "%s %s", manufacturer, model);
					}
					else
					{
						Log(LT_ERROR, "TRoadVehicle.hersteller and/or TRoadVehicle.friendlyname were null");
						myprintf(vehicle, VEHICLE_SIZE, "");
					}
				}
				else
				{
					Log(LT_ERROR, "TRoadVehicleInst.TRVehicle was null");
					myprintf(vehicle, VEHICLE_SIZE, "");
				}
			}
			else // We don't have a vehicle anymore
			{
				myprintf(vehicle, VEHICLE_SIZE, "");
			}
		}

		// Is the game paused?
		bool paused = sysvars::pause || g::hard_paused1 || g::hard_paused2;

		// Current line
		#define LINE_SIZE 32
		static char line[LINE_SIZE] = {0};

		// Index of the current line in our active schedule
		static int schedule_line = -1;

		int schedule_tour = -1;
		int schedule_tourentry = -1;
		int schedule_count = -1;
		static int schedule_trip = -1;

		int schedule_next = -1;

		// Is timetable valid & next bus stop delay
		bool schedule_valid = false;
		int schedule_delay = 0;

		// Next bus stop
		#define NEXT_STOP_SIZE 64
		static char schedule_next_stop[NEXT_STOP_SIZE] = {0};

		// Destination/terminus
		#define TERMINUS_SIZE 64
		static char terminus[TERMINUS_SIZE] = {0};

		if (myTRVInst) // If we have a vehicle
		{
			// If we have a target (terminus) and it isn't set to $allexit$ (special OMSI terminus which tells all passengers to exit the bus, eg. "Out of Order")
			auto target = Read<char*>(myTRVInst + Offsets::TRVInst_Target);
			if (target && strncmp(target, "$allexit$", 10))
			{
				myprintf(terminus, TERMINUS_SIZE, "%s", target);
			}
			else
			{
				myprintf(terminus, TERMINUS_SIZE, "");
			}

			schedule_valid = Read<uint8_t>(myTRVInst + Offsets::TRVInst_Sch_Info_Valid);
			if (schedule_valid)
			{
				schedule_delay = Read<int>(myTRVInst + Offsets::TRVInst_Sch_Delay);

				auto next_stop = Read<wchar_t*>(myTRVInst + Offsets::TRVInst_Sch_NextStopName);
				if (next_stop)
				{
					WideCharToMultiByte(CP_UTF8, 0, next_stop, NEXT_STOP_SIZE, schedule_next_stop, NEXT_STOP_SIZE, NULL, NULL);
				}
				else
				{
					// Seems to happen when the bus stop is in an unloaded tile. 
					// TODO: Can we replace this with act_busstop instead? or, better yet, do what TProgMan.Render does and get the info through the TTMan instead
					Log(LT_WARN, "TRoadVehicleInst.AI_Scheduled_NextBusstopName was null");
					myprintf(schedule_next_stop, NEXT_STOP_SIZE, "");
				}

				// If the currently chosen line index has changed, get the new line
				auto schedule_line_new = Read<int>(myTRVInst + Offsets::TRVInst_Sch_line);
				if (schedule_line != schedule_line_new)
				{
					Log(LT_INFO, "Found new line: %d -> %d", schedule_line, schedule_line_new);
					schedule_line = schedule_line_new;

					char* line_name = TTimeTableMan_GetLineName(schedule_line);
					if (line_name)
					{
						myprintf(line, LINE_SIZE, "%s", line_name);
					}
					else
					{
						// Debug is printed in TTimeTableMan_GetLineName
						myprintf(line, LINE_SIZE, "");
					}
				}

				schedule_tour = Read<int>(myTRVInst + Offsets::TRVInst_Sch_Tour);
				schedule_tourentry = Read<int>(myTRVInst + Offsets::TRVInst_Sch_tourentry);
				schedule_count = TTimeTableMan_GetBusstopCount(schedule_line, schedule_tour, schedule_tourentry);

				// Next stop 0 means we haven't even started the route yet
				// Although, when it gets set to 1, shortly after it gets set to 2, so let's assume 0 means it's at the first (1) stop still
				schedule_next = Read<int>(myTRVInst + Offsets::TRVInst_Sch_NextStop);
				if (schedule_next == 0)
				{
					schedule_next = 1;
				}
			}
			else // We don't have a schedule
			{
				schedule_line = -1;
				myprintf(line, LINE_SIZE, "");
			}
		}
		else // We don't have a vehicle
		{
			schedule_line = -1;
			myprintf(line, LINE_SIZE, "");
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
			myprintf(details, DETAILS_SIZE, "");
		}

		if (myTRVInst) // We have a vehicle
		{
			if (paused && sysvars::ai)
			{
				myprintf(icon_text, ICONTEXT_SIZE, "Paused, watching AI drive");
			}
			else if (paused)
			{
				myprintf(icon_text, ICONTEXT_SIZE, "Paused, driving");
			}
			else if (sysvars::ai)
			{
				myprintf(icon_text, ICONTEXT_SIZE, "Watching AI drive");
			}
			else
			{
				myprintf(icon_text, ICONTEXT_SIZE, "Driving");
			}

			if (schedule_valid) // We have a schedule
			{
				if (schedule_next > 0 && schedule_count > 0)
				{
					myprintf(state, STATE_SIZE, BUSSTOP_EMOJI " %d/%d | ", schedule_next, schedule_count);
				}
				else
				{
					myprintf(state, STATE_SIZE, "");
				}

				if (strlen(line) > 0 && strlen(terminus) > 0)
				{
					myprintf(state, STATE_SIZE, "%s" LINE_EMOJI " %s => %s", state, line, terminus);
				}
				else if (strlen(line) > 0)
				{
					myprintf(state, STATE_SIZE, "%s" LINE_EMOJI " %s", state, line);
				}
				else if (strlen(terminus) > 0)
				{
					myprintf(state, STATE_SIZE, "%s" LINE_EMOJI " %s", state, terminus);
				}

				#define DELAY_SIZE 16
				static char* delay = new char[DELAY_SIZE] {0};

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
						myprintf(icon, ICON_SIZE, "p_late");
					}
					else if (sysvars::ai)
					{
						myprintf(icon, ICON_SIZE, "ai_late");
					}
					else
					{
						myprintf(icon, ICON_SIZE, "late");
					}

					if (strlen(schedule_next_stop) > 0)
					{
						myprintf(icon_text, ICONTEXT_SIZE, "%s late (%s, %s)", icon_text, schedule_next_stop, delay);
					}
					else
					{
						myprintf(icon_text, ICONTEXT_SIZE, "%s late (%s)", icon_text, delay);
					}
				}
				else if (schedule_delay <= -120) // We're early
				{
					if (paused)
					{
						myprintf(icon, ICON_SIZE, "p_early");
					}
					else if (sysvars::ai)
					{
						myprintf(icon, ICON_SIZE, "ai_early");
					}
					else
					{
						myprintf(icon, ICON_SIZE, "early");
					}

					if (strlen(schedule_next_stop) > 0)
					{
						myprintf(icon_text, ICONTEXT_SIZE, "%s early (%s, %s)", icon_text, schedule_next_stop, delay);
					}
					else
					{
						myprintf(icon_text, ICONTEXT_SIZE, "%s early (%s)", icon_text, delay);
					}
				}
				else // We're on time
				{
					if (paused)
					{
						myprintf(icon, ICON_SIZE, "p_ontime");
					}
					else if (sysvars::ai)
					{
						myprintf(icon, ICON_SIZE, "ai_ontime");
					}
					else
					{
						myprintf(icon, ICON_SIZE, "ontime");
					}

					if (strlen(schedule_next_stop) > 0)
					{
						myprintf(icon_text, ICONTEXT_SIZE, "%s on-time (%s, %s)", icon_text, schedule_next_stop, delay);
					}
					else
					{
						myprintf(icon_text, ICONTEXT_SIZE, "%s on-time (%s)", icon_text, delay);
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
					myprintf(state, STATE_SIZE, "");
				}

				if (paused)
				{
					myprintf(icon, ICON_SIZE, "paused");
				}
				else if (sysvars::ai)
				{
					myprintf(icon, ICON_SIZE, "ai");
				}
				else
				{
					myprintf(icon, ICON_SIZE, "driving");
				}
			}
		}
		else // No vehicle
		{
			myprintf(state, STATE_SIZE, "");

			if (paused)
			{
				myprintf(icon_text, ICONTEXT_SIZE, "Paused, freelooking");
				myprintf(icon, ICON_SIZE, "paused");
			}
			else
			{
				myprintf(icon_text, ICONTEXT_SIZE, "Freelooking");
				myprintf(icon, ICON_SIZE, "camera");
			}
		}
	}
	else // Not in-game
	{
		myprintf(details, DETAILS_SIZE, "Getting ready to drive");
		myprintf(icon_text, ICONTEXT_SIZE, "In the main menu");
		myprintf(icon, ICON_SIZE, "menu");
	}

	Discord_UpdatePresence(presence);

	VEH::RemoveHandler();
}