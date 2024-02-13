###### it only took 2 years for that rewrite huh, oops
# OMSIPresence
An OMSI 2 Bus Simulator plugin which integrates Discord Rich Presence into the game.

![media](https://github.com/brokenphilip/OMSIPresence/assets/13336890/5ebe1267-df5e-4a44-bd7c-b3727dbeabc8)

# Usage
### Disclaimer
**This is not a finished product.** While stable enough for a public release, the plugin is still in development and further testing is required. Please don't hesitate to give suggestions or report any bugs that might occur!

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
The project has seen major improvements since its initial release as OMSI2Drpc in 2020:
- The plugin uses the `TTimer` [Delphi class](https://docwiki.embarcadero.com/Libraries/Alexandria/en/Vcl.ExtCtrls.TTimer) to update Rich Presence (alongside `Access*Variable` functions, but no longer solely relying on them)
  - This allows the game to update Rich Presence even when the game is "hard-paused" (via ESC (or another open menu) instead of the P key)
  - In theory, it should perform better as well
- The plugin can now also read information directly from memory instead of just through the plugin API
  - It is no longer required to use a custom vehicle script for each bus in order to receive additional data
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