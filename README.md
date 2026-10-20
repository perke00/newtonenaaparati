# Robotic Tune  -  by ProdByPerke  (Engineered by wajfay)

JUCE VST3 (Windows) robotic vocal tuning / drawing plugin for FL Studio.

## Build
Push this folder to a GitHub repo. Actions builds it automatically:
- artifact `RoboticTune-Windows-VST3`  -> `Robotic Tune.vst3` folder
- artifact `RoboticTune-Installer`     -> `RoboticTune-Setup.exe` (installs into Common Files\VST3)

## Get a vocal into the plugin
- **Import Vocal** button, or **drag & drop** a WAV / AIFF / FLAC / MP3 file from Windows Explorer onto the plugin window.
- or **REC**: arm it, press PLAY in FL and let the vocal run (as before).

## Edit
- DRAW (D): with Note Tracking ON it paints detected notes to a pitch; with Note Tracking OFF (Settings) it is free draw: drag to lay down notes yourself, up/down = hard steps.
- SELECT (V), SCISSORS (C), TRILL (T) as before. Trill speeds: 1/8, 1/16, 1/32, 1/64.
- ROBOTIC slider: 100 % = every note is one flat pitch and notes jump instantly (hard-tune steps, robotic trills).
- Space = built-in preview (works with FL stopped). Click / drag the ruler to move the playhead.

## Back to the FL track
- **Drag Audio to FL**: drag the button into the Playlist (or use Save WAV). Place it where the original vocal clip starts.
- Drag MIDI / Save MIDI for the corrected notes.

## Settings (gear icon)
Note tracking on/off, auto-detect key, follow playhead.
