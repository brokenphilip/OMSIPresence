#include <chrono>

#include "discord.h"
#include "veh.h"

void discord::Setup()
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

void discord::Destroy()
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

void discord::Update()
{
	// OMSI's internal exception handler will ignore almost all exceptions coming from our plugin, which is why we're using our own
	veh::AddHandler();

	if (in_game) // Have we loaded into a map this session yet?
	{
		/* 
			PART 1: Get all necessary information from the game
			
			Details: map name, vehicle name
			State: current line and terminus
			Icon & text: if we're in-game, have a vehicle, paused, watching AI drive, have a schedule (if we do, are we late or early or on-time)
		*/

		// Map name
		#define MAP_SIZE 32
		static char* map = new char[MAP_SIZE] {0};

		// Pointer to the currently loaded TMap
		static uintptr_t myTMap = 0;

		// If the currently loaded map has changed, get the new name
		auto myTMap_new = ReadMemory<uintptr_t>(offsets::tmap);
		if (myTMap != myTMap_new)
		{
			DEBUG(dbg::info, "Found new TMap: %08X -> %08X", myTMap, myTMap_new);
			myTMap = myTMap_new;
			if (myTMap)
			{
				auto mapname = ReadMemory<wchar_t*>(myTMap + offsets::tmap_friendlyname);

				if (mapname)
				{
					WideCharToMultiByte(CP_UTF8, 0, mapname, MAP_SIZE, map, MAP_SIZE, NULL, NULL);
				}
				else
				{
					DEBUG(dbg::error, "TMap.mapname was null");
					sprintf_s(map, MAP_SIZE, "");
				}
			}
			else
			{
				DEBUG(dbg::error, "TMap was null");
				sprintf_s(map, MAP_SIZE, "");
			}
		}

		// Vehicle name
		#define VEHICLE_SIZE 64
		static char* vehicle = new char[VEHICLE_SIZE] {0};

		// Pointer to our currently driven TRoadVehicleInst
		static uintptr_t myTRVInst = 0;

		// If the currently driven vehicle has changed, get the new name
		uintptr_t myTRVInst_new = TRVList_GetMyVehicle();
		if (myTRVInst != myTRVInst_new)
		{
			DEBUG(dbg::info, "Found new TRVInst: %08X -> %08X", myTRVInst, myTRVInst_new);
			myTRVInst = myTRVInst_new;
			if (myTRVInst)
			{
				auto myTRVehicle = ReadMemory<uintptr_t>(myTRVInst + offsets::trvinst_trv);
				if (myTRVehicle)
				{
					auto manufacturer = ReadMemory<char*>(myTRVehicle + offsets::trv_hersteller);
					auto model = ReadMemory<char*>(myTRVehicle + offsets::trv_friendlyname);

					if (manufacturer && model)
					{
						sprintf_s(vehicle, VEHICLE_SIZE, "%s %s", manufacturer, model);
					}
					else
					{
						DEBUG(dbg::error, "TRoadVehicle.hersteller and/or TRoadVehicle.friendlyname were null");
						sprintf_s(vehicle, VEHICLE_SIZE, "");
					}
				}
				else
				{
					DEBUG(dbg::error, "TRoadVehicleInst.TRVehicle was null");
					sprintf_s(vehicle, VEHICLE_SIZE, "");
				}
			}
			else // We don't have a vehicle anymore
			{
				sprintf_s(vehicle, VEHICLE_SIZE, "");
			}
		}

		// Is the game paused?
		bool paused = sysvars::pause || hard_paused1 || hard_paused2;

		// Current line
		#define LINE_SIZE 8
		static char* line = new char[LINE_SIZE] {0};

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
		#define NEXT_STOP_SIZE 32
		static char* schedule_next_stop = new char[NEXT_STOP_SIZE] {0};

		// Destination/terminus
		#define TERMINUS_SIZE 32
		static char* terminus = new char[TERMINUS_SIZE] {0};

		if (myTRVInst) // If we have a vehicle
		{
			auto target = ReadMemory<char*>(myTRVInst + offsets::trvinst_target);
			if (target && strncmp(target, "$allexit$", 10))
			{
				sprintf_s(terminus, TERMINUS_SIZE, "%s", target);
			}
			else
			{
				sprintf_s(terminus, TERMINUS_SIZE, "");
			}

			schedule_valid = ReadMemory<uint8_t>(myTRVInst + offsets::trvinst_sch_info_valid);
			if (schedule_valid)
			{
				schedule_delay = ReadMemory<int>(myTRVInst + offsets::trvinst_sch_delay);

				auto next_stop = ReadMemory<wchar_t*>(myTRVInst + offsets::trvinst_sch_next_stop);
				if (next_stop)
				{
					WideCharToMultiByte(CP_UTF8, 0, next_stop, NEXT_STOP_SIZE, schedule_next_stop, NEXT_STOP_SIZE, NULL, NULL);
				}
				else
				{
					// TODO: Might be ok? When does it happen?
					DEBUG(dbg::warn, "TRoadVehicleInst.AI_Scheduled_NextBusstopName was null");
					sprintf_s(schedule_next_stop, NEXT_STOP_SIZE, "");
				}

				// If the currently chosen line index has changed, get the new line
				auto schedule_line_new = ReadMemory<int>(myTRVInst + offsets::trvinst_sch_line);
				if (schedule_line != schedule_line_new)
				{
					DEBUG(dbg::info, "Found new line: %d -> %d", schedule_line, schedule_line_new);
					schedule_line = schedule_line_new;

					char* line_name = TTimeTableMan_GetLineName(schedule_line);
					if (line_name)
					{
						sprintf_s(line, LINE_SIZE, "%s", line_name);
					}
					else
					{
						// Debug is printed in TTimeTableMan_GetLineName
						sprintf_s(line, LINE_SIZE, "");
					}
				}

				schedule_tour = ReadMemory<int>(myTRVInst + offsets::trvinst_sch_tour);
				schedule_tourentry = ReadMemory<int>(myTRVInst + offsets::trvinst_sch_tourentry);
				schedule_count = TTimeTableMan_GetBusstopCount(schedule_line, schedule_tour, schedule_tourentry);

				// Next stop 0 means we haven't even started the route yet
				// Although, when it gets set to 1, shortly after it gets set to 2, so let's assume 0 means it's at the first (1) stop still
				schedule_next = ReadMemory<int>(myTRVInst + offsets::trvinst_sch_next);
				if (schedule_next == 0)
				{
					schedule_next = 1;
				}
			}
			else // We don't have a schedule
			{
				schedule_line = -1;
				sprintf_s(line, LINE_SIZE, "");
			}
		}
		else // We don't have a vehicle
		{
			schedule_line = -1;
			sprintf_s(line, LINE_SIZE, "");
		}

		/*
			PART 2: Format the data to Rich Presence and send it off

			For the details, we display the current map and/or bus name, whatever is available to us.
			If we're in the main menu, we display that we're getting ready to drive.

			For the state, we display the current line and terminus.
			We can only have a terminus if we have a vehicle. We can only have a line if we have (a vehicle and) a schedule.
			If we don't have a vehicle to begin with, we display nothing.

			For the icon text, we display what the user is currently doing using text - if they're driving, paused, watching AI drive, in freecam or in the main menu.
			If we have a schedule, for the first three aformentioned actions, we additionally display whether they're late, early or on-time.
			We also additionally display the next bus stop as well as the exact delay formatted in +/-MM:SS, where +/- indicates whether we're late/early.
			If the delay is between -2 minutes and +3 minutes, we are on-time.

			For the icon, we display what the user is currently doing using an icon and a background color.
			When we're in the main menu, a menu icon will show. When we're freelooking, a camera icon will show. When we're freeroaming, a bus icon will show.
			Additionally, if we've paused the game while freelooking or freeroaming, a pause icon will show.
			If we're letting AI control our vehicle, an AI icon will show. For all 5 of the aformentioned icons, the "default" yellow background will be used.
			When we have multiple states at once, pausing has the highest priority, followed by AI, then everything else.
			When we have a schedule, instead of yellow, we use red (indicating we're late), blue (indicating we're early) and green (indicating we're on time).
		*/

		if (strlen(map) > 0 && strlen(vehicle) > 0)
		{
			sprintf_s(details, DETAILS_SIZE, MAP_EMOJI " %s | " BUS_EMOJI " %s", map, vehicle);
		}
		else if (strlen(map) > 0)
		{
			sprintf_s(details, DETAILS_SIZE, MAP_EMOJI " %s", map);
		}
		else if (strlen(vehicle) > 0)
		{
			sprintf_s(details, DETAILS_SIZE, BUS_EMOJI " %s", vehicle);
		}
		else
		{
			sprintf_s(details, DETAILS_SIZE, "");
		}

		if (myTRVInst) // We have a vehicle
		{
			if (paused && sysvars::ai)
			{
				sprintf_s(icon_text, ICONTEXT_SIZE, "Paused, watching AI drive");
			}
			else if (paused)
			{
				sprintf_s(icon_text, ICONTEXT_SIZE, "Paused, driving");
			}
			else if (sysvars::ai)
			{
				sprintf_s(icon_text, ICONTEXT_SIZE, "Watching AI drive");
			}
			else
			{
				sprintf_s(icon_text, ICONTEXT_SIZE, "Driving");
			}

			if (schedule_valid) // We have a schedule
			{
				if (schedule_next > 0 && schedule_count > 0)
				{
					sprintf_s(state, STATE_SIZE, BUSSTOP_EMOJI " %d/%d | ", schedule_next, schedule_count);
				}
				else
				{
					sprintf_s(state, STATE_SIZE, "");
				}

				if (strlen(line) > 0 && strlen(terminus) > 0)
				{
					sprintf_s(state, STATE_SIZE, "%s" LINE_EMOJI " %s => %s", state, line, terminus);
				}
				else if (strlen(line) > 0)
				{
					sprintf_s(state, STATE_SIZE, "%s" LINE_EMOJI " %s", state, line);
				}
				else if (strlen(terminus) > 0)
				{
					sprintf_s(state, STATE_SIZE, "%s" LINE_EMOJI " %s", state, terminus);
				}

				#define DELAY_SIZE 16
				static char* delay = new char[DELAY_SIZE] {0};

				if (schedule_delay < 0)
				{
					sprintf_s(delay, DELAY_SIZE, "-%01d:%02d", -schedule_delay / 60, -schedule_delay % 60);
				}
				else
				{
					sprintf_s(delay, DELAY_SIZE, "+%01d:%02d", schedule_delay / 60, schedule_delay % 60);
				}

				if (schedule_delay >= 180) // We're late
				{
					if (paused)
					{
						sprintf_s(icon, ICON_SIZE, "p_late");
					}
					else if (sysvars::ai)
					{
						sprintf_s(icon, ICON_SIZE, "ai_late");
					}
					else
					{
						sprintf_s(icon, ICON_SIZE, "late");
					}

					if (strlen(schedule_next_stop) > 0)
					{
						sprintf_s(icon_text, ICONTEXT_SIZE, "%s late (%s, %s)", icon_text, schedule_next_stop, delay);
					}
					else
					{
						sprintf_s(icon_text, ICONTEXT_SIZE, "%s late (%s)", icon_text, delay);
					}
				}
				else if (schedule_delay <= -120) // We're early
				{
					if (paused)
					{
						sprintf_s(icon, ICON_SIZE, "p_early");
					}
					else if (sysvars::ai)
					{
						sprintf_s(icon, ICON_SIZE, "ai_early");
					}
					else
					{
						sprintf_s(icon, ICON_SIZE, "early");
					}

					if (strlen(schedule_next_stop) > 0)
					{
						sprintf_s(icon_text, ICONTEXT_SIZE, "%s early (%s, %s)", icon_text, schedule_next_stop, delay);
					}
					else
					{
						sprintf_s(icon_text, ICONTEXT_SIZE, "%s early (%s)", icon_text, delay);
					}
				}
				else // We're on time
				{
					if (paused)
					{
						sprintf_s(icon, ICON_SIZE, "p_ontime");
					}
					else if (sysvars::ai)
					{
						sprintf_s(icon, ICON_SIZE, "ai_ontime");
					}
					else
					{
						sprintf_s(icon, ICON_SIZE, "ontime");
					}

					if (strlen(schedule_next_stop) > 0)
					{
						sprintf_s(icon_text, ICONTEXT_SIZE, "%s on-time (%s, %s)", icon_text, schedule_next_stop, delay);
					}
					else
					{
						sprintf_s(icon_text, ICONTEXT_SIZE, "%s on-time (%s)", icon_text, delay);
					}
				}
			}
			else // No schedule
			{
				if (strlen(terminus) > 0)
				{
					sprintf_s(state, STATE_SIZE, LINE_EMOJI " %s", terminus);
				}
				else
				{
					sprintf_s(state, STATE_SIZE, "");
				}

				if (paused)
				{
					sprintf_s(icon, ICON_SIZE, "paused");
				}
				else if (sysvars::ai)
				{
					sprintf_s(icon, ICON_SIZE, "ai");
				}
				else
				{
					sprintf_s(icon, ICON_SIZE, "driving");
				}
			}
		}
		else // No vehicle
		{
			sprintf_s(state, STATE_SIZE, "");

			if (paused)
			{
				sprintf_s(icon_text, ICONTEXT_SIZE, "Paused, freelooking");
				sprintf_s(icon, ICON_SIZE, "paused");
			}
			else
			{
				sprintf_s(icon_text, ICONTEXT_SIZE, "Freelooking");
				sprintf_s(icon, ICON_SIZE, "camera");
			}
		}
	}
	else // Not in-game
	{
		sprintf_s(details, DETAILS_SIZE, "Getting ready to drive");
		sprintf_s(icon_text, ICONTEXT_SIZE, "In the main menu");
		sprintf_s(icon, ICON_SIZE, "menu");
	}

	Discord_UpdatePresence(presence);

	veh::RemoveHandler();
}