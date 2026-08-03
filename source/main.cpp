#include <iostream>

#include <chrono>
#include <thread>

#include <getopt.h>
#include <csignal>
#include <cstdlib>

#include <hidapi.h>
#include <libusb.h>
#include <libremidi/libremidi.hpp>
#include <libremidi/reader.hpp>

#include "controller.hpp"
#include "playback.hpp"
#include "global.hpp"

//This really shouldn't be here tbh, but exit relies on it being global. Might consider a different way of retrieving it?
static SteamControllerInfos *steam_controller_ptr = nullptr;

bool parseArguments(int argc, char** argv, ParamsStruct* params, SteamControllerInfos* controller){
	int c;
	bool readyState = false;
	while ( (c = getopt(argc, argv, "l:r:n:m:d:i:pvutseh")) != -1) {
		int32_t value;
		switch(c){
		case 'l':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				controller->gainModifier[0] = value;
			}
			break;
		case 'r':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				controller->gainModifier[1] = value;
			}
			break;
		case 'n':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				controller->gainModifier[3] = value;
			}
			break;
		case 'm':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				controller->gainModifier[4] = value;
			}
			break;
		case 'd':
			value = strtol(optarg,NULL,10);
			if(value >= LIBUSB_LOG_LEVEL_NONE && value <= LIBUSB_LOG_LEVEL_DEBUG){
				params->libusbDebugLevel = value;
			}
			break;
		case 'i':
			value = strtol(optarg,NULL,10);
			if(value <= 1000000 && value > 0){
				params->intervalUSec = value;
			}
			break;
		case 'p':
			params->repeatSong = true;
			break;
		case 'v':
			controller->directVel = true;
			break;
		case 'u':
			controller->noGainCurve = true;
			break;
		case 't':
			controller->tritonLimit = true;
			break;
		case 's':
			controller->tritonSwap = true;
			break;
		case 'e':
			params->realTime = true;
			readyState = true;
			break;
		case 'h':
		case '?':
			return false;
			break;
		default:
			break;
		}
	}
	if(optind == argc-1 ){
		params->midiInput = argv[optind];
		readyState = true;
	}
	return readyState;
}

void abortSignal(int) {
	std::cout << "\nAborted\n";
	std::cout.flush();
	std::exit(EXIT_FAILURE);
}

void doExit(){
	std::cout << "\033[?25h" << std::flush;

	if (/*exitMute*/ true) SteamHaptics_StopNotes(steam_controller_ptr);
	SteamController_Close(steam_controller_ptr);

	#ifdef __linux__
	disableRawMode();
	#endif

	std::cout << std::flush;

	//std::cin.ignore();

	libusb_exit(NULL);
	hid_exit();
}

int main(int argc, char** argv)
{
	ParamsStruct params;
    SteamControllerInfos steamController1;
    steam_controller_ptr = &steamController1;

	//Parse arguments
	if(!parseArguments(argc, argv, &params, &steamController1)){
		std::cout << 
				"Steam Haptics Singer v1.13 by Crazy, based off of Steam Controller Singer by Pila\n"
			    "Usage: steam-haptics-singer [-lMODIFIER] [-rMODIFIER] [-nMODIFIER] [-mMODIFIER] [-iINTERVAL] [-dDEBUG_LEVEL] [-p] [-v] [-u] [-t] [-s] MIDI_FILE\n"
			  "\nThere must be no space for negative gain modifiers"
			  "\n  -lMODIFIER		Left trackpad gain modifier"
			  "\n  -rMODIFIER		Right trackpad gain modifier"
			  "\n  -nMODIFIER		Left rumble gain modifier"
			  "\n  -mMODIFIER		Right rumble gain modifier "
              "\n  -uGAIN   	No gain curve, all notes use specified gain (-128 - 127)"
			  "\n  -iINTERVAL		Player sleep interval (in microseconds). Lower generally means better song fidelity, but higher cpu usage, and at some point going lower won't improve any more. Default value is 5000"
			  "\n  -dDEBUG_LEVEL		Libusb debug level. Default is 0, no debug output. Max is 4, max verbosity output"
		      "\n  -p	Repeat song, plays again after ending"
			  "\n  -v 	Direct velocity to gain control, the MIDI file will set the gain"
			  "\n  -t	(Steam Controller 2026 Only) Limit to only two channels"
			  "\n  -s	(Steam Controller 2026 Only) Swap rumble and trackpad channels"
			  #ifdef __linux__
			  "\n  -e	Real-time MIDI, will create a virtual MIDI device for usage"
			  #elif defined(_WIN32)
			  "\n  -e	Real-time MIDI, will attempt to use loopMIDI by default"
			  #endif
			  "\n  -q	No re-tuned frequencies"
			  "\n  -x	External real-time MIDI, will use the default MIDI-IN device"
			  "\n  -w   List MIDI-IN devices"
			  "\n";
		return 1;
		//-left=0 -right=0 -tleft -tright -interval -debug -repeat -directvel -nogain -limit -swap -realtime -noretune -
	}

	//std::cout << "\033[?25l\033[2J\033[H" << std::flush;
	std::cout << "\033c" << std::flush;
	std::cout << "Steam Haptics Singer v1.13 by Crazy, based off of Steam Controller Singer by Pila\n";


	//Initializing LIBUSB
	int r = libusb_init(NULL);
	if(r < 0) {
		std::cout<<"LIBUSB Init Error "<<libusb_error_name(r)<<std::endl;
		std::cin.ignore();
		return 1;
	}

	//Initializing HIDAPI
    if (hid_init() != 0) {
        std::cout<<"HIDAPI Init Error "<<std::endl;
		std::cin.ignore();
        return 1;
    }

	libusb_set_debug(NULL, params.libusbDebugLevel);

	//Gaining access to Steam Controller
	if(!SteamController_Open(&steamController1)){
		//return 1;
	}

	//Set mecanism to stop playing when closing process
	std::signal(SIGINT, abortSignal);
	std::atexit(doExit);

	//Playing song
	if (params.realTime) {
		playRealTime(&steamController1,&params);
	} else {
		do{
			playSong(&steamController1,&params);
		}while(params.repeatSong);
	}

	return 0;
}
