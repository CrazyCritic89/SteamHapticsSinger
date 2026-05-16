# Steam Haptics Singer

This project aims to be a continuation of SteamControllerSinger (forked from [Roboron3042's fork](https://github.com/Roboron3042/SteamControllerSinger), originally by [Pila](https://gitlab.com/Pilatomic/SteamControllerSinger)) by fixing issues, adding features, and, most notably, Steam Deck and Steam Controller (2026) support.

I have made a small [Discord server](https://discord.gg/TWpvAxX5GW) for sharing MIDI files and updates easier for anyone interested.

## How To

1. Turn on your Steam Controller (2015 or 2026) or Steam Deck
2. Drag the MIDI file onto the steam-haptics-singer executable
3. If prompted, press Enter
4. Enjoy!

### Where can I find midi songs?

Songs ready to play can be found in the original guy's [personal collection](https://mega.nz/#F!BWpEWKzB!r7WPw5bZ_domN4pk-FJsjg) (as he called it). Otherwise, you can just try a MIDI and see what happens (most of the time it won't work well).

### Usage from command prompt:
	Usage: steam-haptics-singer [-p] [-y] [-d DEBUG_LEVEL] [-i INTERVAL] MIDI_FILE

	  -i INTERVAL		Player sleep interval (in microseconds). Lower generally means better song fidelity, but higher cpu usage, and at some point going lower won't improve any more. Default value is 10000
	  -d DEBUG_LEVEL	Libusb debug level. Default is 0, no debug output. max is 4, max verbosity output
	  -p	Repeat song, plays again after ending
	  -e 	Direct velocity to gain control, the MIDI file will set the gain"
	  -t	(Steam Controller 2026 Only) Limit to only two channels"
	  -s	(Steam Controller 2026 Only) Swap rumble and trackpad channels"

### MIDI files tips:

MIDI files may need to be edited with a software such as [MidiEditor](https://www.midieditor.org/) to be correctly played with Steam Haptics Singer following the next tips:

* Notes from MIDI channel 0 are played on right haptic/rumble
* Notes from MIDI channel 1 are played on left haptic/rumble
* Notes from MIDI channel 2 are played on right haptic (Steam Controller 2026 only)
* Notes from MIDI channel 3 are played on left haptic (Steam Controller 2026 only)
* Notes from others channels are ignored
* **Avoid multiple notes active at the same time on the same channel**, since haptic actuators can only play one note at the time.

On Linux, you can also use MIDI devices such as a MIDI keyboard. To do so, just run the program with the MIDI device as an argument instead of a MIDI file. You can find the MIDI device in `/dev/` as `midiX` or in `/dev/snd/` as `midiCXDY` (where X and Y are numbers).


## Compiling

You will need libusb(-dev) and pkgconf. If you have them, just type `make`.

It's recommended to build this in a container such as [steam-runtime](https://github.com/ValveSoftware/steam-runtime?tab=readme-ov-file#building-in-the-runtime) or [holo-docker](https://github.com/SteamDeckHomebrew/holo-docker) in order for the packges to line up with the Steam Deck.

If you go the steam-runtime route, make sure to use sniper as scout is outdated.

### For a guide:
	git clone -b master https://github.com/CrazyCritic89/SteamHapticsSinger.git
	cd SteamHapticsSinger
	podman run --rm -v ./:/src -it registry.gitlab.steamos.cloud/steamrt/sniper/sdk bash
	cd src
	make
	exit


## Changelog

[v1.11.1]
* Steam Controller (2026) now defaults to using rumble for the first two channels
* Removed -t and -b parameters
* Added -s parameter to swap the rumble and trackpad channels (credit to @Pixel1011 for the idea)
* Added -t parameter to limit to only two channels (can work in tandem with -s)
* Fixed -d paramter
* Renamed "rumble" to "haptic" in playback display (temporary)
* Errors now give names instead of values

[v1.11]
* Back rumble haptic support for the Steam Controller (2026)
* Channel count is now its own variable allowing it to be changed, still not fully dynamic as anything other than 2 or 4 will cause issues
* Note on is now preceded by a note off. This fixes issues specifically with the Steam Controller (2026) where note sequences with no pauses could drift out of tune, and more notably, prevents the controller from rebooting when using the back rumble haptics
* Added direct velocity to gain control, allowing the MIDI to control how intense the haptics are. To make a MIDI file automatically parse as direct velocity, add "dv" to the file name. Otherwise, you can use the -e parameter
* -t parameter added for Steam Controller (2026) to only use the trackpads to prevent playback issues with MIDI files that still have notes in upper channels
* -b parameter added for Steam Controller (2026) to map the first two channels to the back rumble haptics

[v1.10.2]
* Added Steam Puck support
* Steam Dongle connects but still doesn't work
* The haptics are swapped on the Steam Controller (2026) to match the 2015 and Steam Deck
* Gain/volume is now back at medium level as max might cause damage (#11), will need to be tested more
* Reconnect controller message when connecting with the puck on Windows because the puck reconnects

[v1.10.1]
* Added wired Steam Controller (2026) support, wireless support through puck and bluetooth is still WIP
* The original Steam Controller has been renamed to Steam Controller (2015)
* Controllers now have their own type in the code
* Temporarily removed legacy parameter
* Notes now play at full volume (except the new Steam Controller, to prevent distortion)

[v1.10]
* Implemented Steam Deck-specific haptic command. This now makes songs play correctly on it.
* Updated midifile library 
* Note stop now sets the frequency to 0
* Removed reclaim period, no longer needed and was causing issues
* Legacy command toggle with -y
* Repeat song toggle changed from -r to -p
* Changed debug level argument back from -l to -d
* Updated usage to show more info
* Naturals now have a dash in the middle (C5 -> C-5)

[My v1.9 Build :> (EXCLUSIVE)]
* Badly added Deck support. No improvements.

[v1.8]
* User can now define the reclaim period with -c option.

[v1.7]
* Fixed music stopped playing after a few seconds

[v1.6]
* Fixed major bugs in playback algorithm

[v1.5]
* Changed debug level argument from -d to -l
* Added -r argument to enable demo mode
* Enhanced arguments parsing
* Does not rely on Steam Controller duration anymore
* Updated note display
* Now stops playing when interrupting the process ( on Ctrl+C )

[v1.4]
* Fixed a bug in MIDI librairie that would compute a null duration for notes when ON event and previous OFF event had the same timetick

[v1.3]
* Added -iINTERVAL argument
* Added -dDEBUG_LEVEL argument 

[v1.2]
* Fixed being stuck on "Command error" when disconnecting controller while playing. Now continue playing (even if keep failing)
* Removed the now deprecated 20ms note duration reduction
