#pragma once
#include <libusb.h>

//THIS FILE IS TEMPORARY!! Variables should be definied elsewhere, but until then this will have to do.

#define STEAM_CONTROLLER_MAGIC_PERIOD_RATIO 	495483.0
#define CHANNEL_COUNT						  	4
#define DEFAULT_INTERVAL_USEC				   	5000

#define DURATION_MAX			-1
#define NOTE_STOP		   		-1
#define PROGRESS_BAR_LENGTH     40

#define DEFAULT_GAIN 0

#define VALVE_VID	 			0x28DE
#define STEAM_CONTROLLER 		0x1101
#define STEAM_CONTROLLER_2015 	0x1102
#define STEAM_DONGLE 			0x1142
#define STEAM_CONTROLLER_2026	0x1302
#define STEAM_PUCK				0x1304
#define STEAM_DECK				0x1205

struct ParamsStruct{
	const char* midiInput = "\0";
	unsigned int intervalUSec = DEFAULT_INTERVAL_USEC;
	int libusbDebugLevel = LIBUSB_LOG_LEVEL_NONE;
	bool repeatSong = false;
	bool realTime = false;
    //bool exitMute = false;
    int consoleType = 0; //Specifies what to use, 0 is legacy playback display, 1 is ANSI, 2 is Win32 API
};