# Steam Haptics Singer (RtMidi)

This branch attempts to add RtMidi and USB gadget support to SteamHapticsSinger. 

At the moment, it's currently unfinished, but works. The implementation is a little messy as it's mostly used for testing right now. Only the Steam Deck is supported.

**Please note: You must be running BIOS F7A0121 or F7G0109 for DRD.**

## How To

1. Turn on your Steam Deck
2. Run the gadget_enable.sh script
3. Run steam-haptics-singer
4. Plug in and use as any standard USB MIDI device

Make sure to run gadget_disable.sh when finished.

### Current Implementation

Channels 1 and 2 are tied to the right and left trackpads respectively. Velocity is applied to gain.

Channels 3 and 4 are tied to the right and left trackpads as well but use the SteamController_PlayNote function as lower notes are louder. Velocity is applied to gain, but does not account for the signed value.

### Usage from command prompt:
	steam-haptics-singer [-r] [-l DEBUG_LEVEL] [-i INTERVAL]

	-i INTERVAL argument to choose player sleep interval (in microseconds). Lower generally means better song fidelity, but higher cpu usage, and at some point goidn lower won't improve any more. Default value is 10000

	-l DEBUG_LEVEL argument to choose libusb debug level. Default is 0, no debug output. max is 4, max verbosity output
	
	-r to enable repeat mode, which plays continously (restart the song when finished)

## Compiling

You will need libusb(-dev) and pkgconf. If you have them, just type `make`.

It's recommended to build this in a container such as [steam-runtime](https://github.com/ValveSoftware/steam-runtime?tab=readme-ov-file#building-in-the-runtime) or [holo-docker](https://github.com/SteamDeckHomebrew/holo-docker) in order for the packges to line up with the Steam Deck.

If you go the steam-runtime route, make sure to use sniper as scout is outdated.

### For a guide:
	git clone -b rtmidi https://github.com/CrazyCritic89/SteamHapticsSinger.git
	cd SteamHapticsSinger
	podman run --rm -v ./:/src -it registry.gitlab.steamos.cloud/steamrt/sniper/sdk bash
	make
	exit
