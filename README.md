# Steam Haptics Singer
<img align="left" height="90" alt="shs_icon_new" src="https://github.com/user-attachments/assets/0c7629e8-b289-47d1-b0f1-560037f4139e" />
<svg
   viewBox="0 0 36 36"
   fill="none"
   version="1.1"
   id="svg4"
   sodipodi:docname="shs_icon_b.svg"
   inkscape:export-filename="shs_icon.png"
   inkscape:export-xdpi="2730.6667"
   inkscape:export-ydpi="2730.6667"
   inkscape:version="1.4.4 (dcaf3e7d9e, 2026-05-05)"
   xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape"
   xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
   xmlns="http://www.w3.org/2000/svg"
   xmlns:svg="http://www.w3.org/2000/svg">
  <defs
     id="defs4" />
  <g
     id="g10"
     transform="matrix(0.9558466,0,0,0.9558466,0.7947612,0.47193656)"
     style="fill:#ffffff">
    <path
       fill="currentColor"
       d="M 6.68629,12.972521 C 3.79086,15.867951 2,19.867921 2,24.286221 c 0,4.4183 1.79086,8.4183 4.68629,11.3137 l 2.82843,-2.8284 C 7.34315,30.599921 6,27.599921 6,24.286221 c 0,-3.3137 1.34315,-6.3137 3.51472,-8.48527 z"
       id="path1"
       style="fill:#ffffff" />
    <path
       fill="currentColor"
       d="m 26.4853,15.800951 2.8284,-2.82843 c 2.8954,2.89543 4.6863,6.8954 4.6863,11.3137 0,4.4183 -1.7909,8.4183 -4.6863,11.3137 l -2.8284,-2.8284 c 2.1716,-2.1716 3.5147,-5.1716 3.5147,-8.4853 0,-3.3137 -1.3431,-6.3137 -3.5147,-8.48527 z"
       id="path2"
       style="fill:#ffffff" />
    <circle
       fill="currentColor"
       cx="18"
       cy="23.972523"
       r="2"
       id="circle2"
       style="fill:#ffffff" />
    <path
       fill="currentColor"
       d="m 17.8891,15.021171 c -2.2288,0.05461 -4.4412,0.93239 -6.1422,2.63335 -1.81373,1.8138 -2.69151,4.2091 -2.63327,6.5858 h 4.00217 c -0.0597,-1.3525 0.4268,-2.7247 1.4596,-3.7574 0.9199,-0.9199 2.109,-1.4064 3.3137,-1.4596 z"
       id="path3"
       style="fill:#ffffff" />
    <path
       fill="currentColor"
       d="m 23.106,24.240321 h 4.0022 c -0.0546,2.2288 -0.9324,4.4412 -2.6333,6.1421 -1.8138,1.8138 -4.2092,2.6916 -6.5858,2.6333 v -4.0022 c 1.3525,0.0597 2.7246,-0.4268 3.7573,-1.4595 0.9199,-0.9199 1.4064,-2.109 1.4596,-3.3137 z"
       id="path4"
       style="fill:#ffffff" />
    <path
       style="fill:none;stroke:#ffffff;stroke-width:4;stroke-linecap:butt;stroke-linejoin:miter;stroke-miterlimit:4;stroke-opacity:1"
       d="m 18,23.972521 c 0,0 12.841818,-8.831348 -4.049088,-21.2167022 7.389848,4.7708025 11.898585,2.1119279 11.898585,2.1119279"
       id="path5"
       sodipodi:nodetypes="ccc" />
  </g>
</svg>

This project aims to be a continuation of SteamControllerSinger (forked from [Roboron3042's fork](https://github.com/Roboron3042/SteamControllerSinger), originally by [Pila](https://gitlab.com/Pilatomic/SteamControllerSinger)) by fixing issues, adding features, and, most notably, Steam Deck and Steam Controller (2026) support.

I have made a small [Discord server](https://discord.gg/TWpvAxX5GW) for sharing MIDI files and updates easier for anyone interested.

NOTE: For Steam Controller (2015), make sure the BLE firmware is NOT installed.

## How To

1. [Download the latest release](https://github.com/CrazyCritic89/SteamHapticsSinger/releases) (the Linux build is the one with no file extension)
2. Turn on your Steam Controller (2015 or 2026) or Steam Deck

#### On Linux
1. Right click inside the folder
2. Click "Open in Terminal" 
3. Type `chmod +x steam-haptics-singer` to make the program executable
4. Type `./steam-haptics-singer [name of your midi file]` to run
5. Enjoy!
#### On Windows
1. Click the open space in the folder path at the top in file explorer
2. Type in "cmd"
3. Type `steam-haptics-singer.exe [name of your midi file]` to run
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

You will need libusb(-dev), hidapi-hidraw, and pkgconf. If you have them, just type `make`.

### For a guide:
	git clone -b master https://github.com/CrazyCritic89/SteamHapticsSinger.git
	cd SteamHapticsSinger
	make

## Changelog

[v1.11.2]
* The program now looks through the connected devices on a Steam Puck to find the first Steam Controller (2026)
* Steam Controller (2026) is now handled by HIDAPI

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
