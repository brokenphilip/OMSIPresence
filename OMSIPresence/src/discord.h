#pragma once
#include "../ext/discord-rpc/discord_rpc.h"
#include "../ext/discord-rpc/discord_register.h"

#include "shared.h"

// Globe With Meridians
#define MAP_EMOJI "\xF0\x9F\x8C\x90"

// Oncoming Bus
#define BUS_EMOJI "\xF0\x9F\x9A\x8D"

// Chequered Flag
#define LINE_EMOJI "\xF0\x9F\x8F\x81"

// Bus Stop
#define BUSSTOP_EMOJI "\xF0\x9F\x9A\x8F"

namespace Discord
{
	// How often in milliseconds to update rich presence
	// Currently, the TTimer is using the implicit 1000ms update rate
	//#define UPDATE_INTERVAL 1000
	//constexpr float update_interval = UPDATE_INTERVAL / 1000;

	// First row of text under the game name (128 bytes max)
	inline char* details;
	#define DETAILS_SIZE 128

	// Second row of text under the game name (128 bytes max)
	inline char* state;
	#define STATE_SIZE 128

	// Small icon (16 bytes max). Possible values are:
	// - menu: just started omsi
	// - camera: "freelook", not in vehicle
	// - driving: "freeroam", in vehicle, no timetable
	// - early: in vehicle, timetable, <= -2min delay
	// - late: in vehicle, timetable, >= 3min delay
	// - ontime: in vehicle, timetable, between the two
	// - paused, p_early, p_late, p_ontime: paused, same as last 4 above
	// - ai, ai_early, ai_late, ai_ontime: in ai controlled vehicle, same as above
	// NOTE: Pause icons have priority over ai icons
	inline char* icon;
	#define ICON_SIZE 16

	// Text which appears when you hover over the small icon (128 bytes max)
	inline char* icon_text;
	#define ICONTEXT_SIZE 128

	// Text which appears when you hover over the main large icon (128 bytes max)
	inline char* credits;
	#define CREDITS_SIZE 128

	inline DiscordRichPresence* presence;

	void Setup();
	void Destroy();
	void Update();
}