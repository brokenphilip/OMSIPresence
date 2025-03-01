﻿# OMSIPresence
An OMSI 2 Bus Simulator plugin which integrates Discord Rich Presence into the game.

![media](https://github.com/brokenphilip/OMSIPresence/assets/13336890/5ebe1267-df5e-4a44-bd7c-b3727dbeabc8)

# Usage
### Requirements
You must be running one of the following versions of OMSI 2 (you can check which version you have in your `logfile.txt`):
- `2.3.004` - Latest Steam version
- `2.2.032` - "Tram patch"

### Installation
1. Head over to [Releases](https://github.com/brokenphilip/OMSIPresence/releases) and download the latest `OMSIPresence.zip`
2. Navigate to your OMSI 2 installation folder
   - Open your Steam Library, right click on **OMSI 2** and select **Properties**
   - Select the **Local Files** tab on your left, and then click the **Browse** button
3. Extract the contents of the zip archive to the `plugins` folder inside your OMSI 2 installation folder
   - If the folder does not exist, create it
4. Upon launching the game, Rich Presence status should show on your Discord profile
   - Make sure to disable all other Rich Presences for OMSI 2, if any

### Troubleshooting
If you encounter any issues during installation or usage, please refer to the [issue tracker](https://github.com/brokenphilip/OMSIPresence/issues?q=). If you haven't found your issue, feel free to create a new one; ideally submitting your OMSI 2 version (and OMSIPresence version, although I'd encourage only reporting issues regarding the latest version), your log file and a crash dump if any, as well as a general idea of what you were doing around the time of the issue (ie. spawning a bus, changing maps, freelooking etc...).

Please make sure your crash dumps and log file match the date and time of the incident, by comparing when the files have been last modified. Crash dumps (usually just one is made per crash, but there can be both (or neither) as well) can be found in these locations:
- Your OMSI 2 installation folder - as `OMSIPresence_XXXXXX.dmp` (created by OMSIPresence)
- `%USERPROFILE%\AppData\Local\CrashDumps` - as `Omsi.exe.XXXXX.dmp` (created by Windows Error Reporting (WER))

If you have any further questions about the project, or if (understandably) using the issue tracker is too confusing, feel free to add me on Discord (`brokenphilip`) and I will try to get back to you as soon as possible. :)

# Features
OMSIPresence (originally named OMSI2Drpc, "OMSI 2 Discord Rich Presence", back in 2020) only displays important information about the game in order to provide sufficient context as to what the user is currently doing, such as the map being played, the bus being driven, as well as the current timetable/route information (line, terminus, delay and current bus stop).

In order to display said information in the most compact, universally comprehensible way possible, OMSIPresence uses emotes:
- 🌐 indicates the currently played map
- 🚍 indicates the currently driven bus
- 🚏 indicates the current stop index, as well as the total number of stops for the driven route
- 🏁 indicates the current line number and terminus

Additionally, the small icons (provided by [Font Awesome](https://fontawesome.com/)) contain additional information about the current state of the game:
1. ☰ **Yellow menu icon** - In the main menu, just launched the game
2. 🎥 **Yellow camera icon** - "Freelooking", loaded the map but not in a vehicle
3. 🟡 **Yellow bus icon** - "Freeroaming", in a vehicle but no timetable active
4. 🔵 **Blue bus icon** - In a vehicle, with the timetable active, and a delay of -2 minutes or less (ie. driving early)
5. 🔴 **Red bus icon** - Same as above, but with a delay of 3 minutes or more (ie. driving late)
6. 🟢 **Green bus icon** - Same as above, but with a delay between -2 and 3 minutes (ie. driving on-time)
7. ⏸ **Yellow/Blue/Red/Green pause icon** - Same as 3-6, but the game is paused
8. #️⃣ **Yellow/Blue/Red/Green microchip icon** - Same as 3-6, but the vehicle is controlled by AI

Pause icons have priority over AI icons - that is, if the game is paused and the vehicle is controlled by AI, pause icons are shown. Additionally, hovering over the small icon displays a short explanation of these icons, as well as the current bus stop and timetable delay, if any.

Information that was previously shown in OMSI2Drpc, such as the current bus speed, fuel, passenger count, weather etc... is deemed mostly irrelevant and only serves to clutter the already limited space of Discord Rich Presence, thus it was removed.

Apart from the features listed above, the project has seen major improvements since its initial release as OMSI2Drpc:
- The plugin uses the `TTimer` [Delphi class](https://docwiki.embarcadero.com/Libraries/Alexandria/en/Vcl.ExtCtrls.TTimer) to update Rich Presence (alongside `Access*Variable` functions, but no longer solely relying on them)
  - This allows the game to update Rich Presence even when the game is "hard-paused" (via ESC (or another open menu) instead of the P key)
  - In theory, it should perform better as well
- The plugin now also reads information directly from memory instead of just through the plugin API
  - It is no longer required to use a custom vehicle script for each bus in order to receive additional data (such as the name of the bus)
  - The plugin no longer relies on IBIS data (in case buses don't use IBIS for example)
  - There is now an easy and proper way to detect if the player is in a vehicle or not
  - It is now possible to get the name of the current vehicle as well
  - Map name is no longer read from your vehicle's chosen depot
- The plugin now has a debug mode and logging functionality
  - Debug mode can be activated using the `-omsipresence_debug` launch option - the game will continue to run after exceptions and a console is created for live logging output
  - Log entries are written directly to the `logfile.txt` (unless `-nolog` is active) using the game's own function, prefixed with `[OMSIPresence]`
- The plugin automatically checks for updates on game start
  - Depending on your internet connection quality, the game might take noticeably longer to start (protip: check your `logfile.txt` for an accurate measurement)
  - Can be disabled using the `-omsipresence_noupdate` launch option. **This is not recommended**, only use if you're having issues - please stay up-to-date
  - This **doesn't** automatically download updates - you are prompted to go to this page instead
- Unicode characters are now fully supported
- Rewritten from scratch and open-sourced
