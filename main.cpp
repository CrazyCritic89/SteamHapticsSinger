#include <iostream>
#include <fstream>

#include <chrono>
#include <thread>

#include <cstring>
#include <getopt.h>
#include <csignal>
#include <cstdlib>

#include <hidapi.h>
#include <libusb.h>
#include <libremidi/libremidi.hpp>
#include <libremidi/reader.hpp>

#ifdef _WIN32
#include <conio.h>

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
CONSOLE_SCREEN_BUFFER_INFO csbi;
COORD savedPosition = {-1,-1};
#endif 

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

#ifdef _WIN32 

/*void enableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}*/
#endif

const double midiFrequency[128]  = {0, 8.662, 9.177, 9.723, 10.301, 10.913, 11.562, 12.250, 12.978, 13.750, 14.568, 15.434, 16.352, 17.324, 18.354, 19.445, 20.602, 21.827, 23.125, 24.500, 25.957, 27.500, 29.135, 30.868, 32.703, 34.648, 36.708, 38.891, 41.203, 43.654, 46.249, 48.999, 51.913, 55.000, 58.270, 61.735, 65.406, 69.296, 73.416, 77.782, 82.407, 87.307, 92.499, 97.999, 103.826, 110.000, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220.000, 233.082, 246.942, 261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440.000, 466.164, 493.883, 523.251, 554.365, 587.330, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880.000, 932.328, 987.767, 1046.502, 1108.731, 1174.659, 1244.508, 1318.510, 1396.913, 1479.978, 1567.982, 1661.219, 1760.000, 1864.655, 1975.533, 2093.005, 2217.461, 2349.318, 2489.016, 2637.020, 2793.826, 2959.955, 3135.963, 3322.438, 3520.000, 3729.310, 3951.066, 4186.009, 4434.922, 4698.636, 4978.032, 5274.041, 5587.652, 5919.911, 6271.927, 6644.875, 7040.000, 7458.620, 7902.133, 8372.018, 8869.844, 9397.273, 9956.063, 10548.082, 11175.303, 11839.822, 12543.854};
const uint16_t midiFrequencyDk[128] = {440};
const uint16_t midiFrequencyRb[128] = {0, 10, 10, 11, 11, 12, 13, 13, 14, 15, 16, 16, 17, 18, 19, 20, 22, 23, 24, 25, 27, 29, 30, 32, 34, 36, 38, 40, 42, 45, 47, 50, 53, 56, 59, 63, 66, 70, 75, 80, 84, 89, 94, 100, 107, 113, 120, 126, 134, 142, 151, 160, 169, 179, 189, 200, 213, 226, 239, 253, 267, 283, 300, 318, 336, 357, 377, 399, 423, 449, 477, 505, 535, 566, 598, 636, 674, 713, 756, 800, 848, 898, 951, 1008, 1068, 1131, 1199, 1270, 1345, 1425, 1510, 1600, 1693, 1792, 1897, 2008, 2125, 2249, 2381, 2521, 2669, 2826, 2992, 3168, 3354, 3552, 3761, 3983, 4218, 4467, 4731, 5010, 5306, 5620, 5952, 6304, 6677, 7072, 7491, 7934, 8404, 8902, 9429, 9988, 10580, 11207, 11872, 12576};
const uint16_t midiFrequencyTr[128] = {0, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 19, 21, 22, 23, 24, 26, 28, 29, 31, 33, 35, 37, 39, 41, 44, 46, 49, 52, 55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 117, 123, 131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247, 261, 276, 293, 310, 328, 349, 369, 391, 414, 439, 466, 493, 522, 552, 584, 621, 658, 696, 738, 781, 828, 877, 929, 985, 1043, 1105, 1171, 1240, 1314, 1392, 1475, 1562, 1655, 1754, 1858, 1969, 2085, 2209, 2340, 2480, 2627, 2784, 2949, 3124, 3311, 3507, 3716, 3938, 4173, 4422, 4686, 4965, 5261, 5575, 5907, 6259, 6632, 7027, 7446, 7889, 8359, 8857, 9384, 9943, 10535, 11162, 11827, 12531};

const int8_t gainCurveDk[128] = {DEFAULT_GAIN};
const int8_t gainCurveRb[128] = {DEFAULT_GAIN};
const int8_t gainCurveTr[128] = {DEFAULT_GAIN};

struct ParamsStruct{
	const char* midiInput;
	unsigned int intervalUSec;
	int libusbDebugLevel;
	bool repeatSong;
	bool realTime;
};

//TEMPORARY, move to ParamsStruct and find a way to reference within playback function
bool directVel = false;
bool tritonLimit = false;
bool tritonSwap = false;
bool exitMute = false;
int channelCount = 2;
int gainModifier[5] = {0};
bool noGainCurve = false;
int consoleType = 0; //Specifies what to use, 0 is legacy playback display, 1 is ANSI, 2 is Win32 API

enum class ControllerType {
	None,
	Original,	//Steam Controller (2015)
	Triton,		//Steam Controller (2026)
	Jupiter, 	//Steam Deck (LCD)
	Galileo 	//Steam Deck (OLED)
};

struct SteamControllerInfos{
	libusb_device_handle* dev_handle = NULL;
	hid_device* hid_handle = NULL;
	int interfaceNum = 0;
	ControllerType type = ControllerType::None;
};

SteamControllerInfos steamController1;

hid_device* open_steam_controller_hid(uint16_t pid) {
	unsigned char buf[64];
	struct hid_device_info *devs = hid_enumerate(VALVE_VID, pid);
	if (devs == NULL) return NULL;
	if (pid == STEAM_CONTROLLER_2026) std::cout << "\rAttempting to find wired Steam Controller (2026)... ";
	else if (pid == STEAM_PUCK) std::cout << "\rFound Steam Puck, attempting to find first Steam Controller (2026)... ";
	else std::cout << "\rAttempting to find Valve device...";
	hid_device* handle = NULL;
	int r;
	for (struct hid_device_info *cur = devs; cur != NULL; cur = cur->next) {
		if (cur->usage_page == 0xFF00) {
			handle = hid_open_path(cur->path);
			if (handle) {
				//Check if any data being sent, if valid controller, will always be sending
				r = hid_read_timeout(handle,buf,64,100);
				if (r > 0) break;
			}
		}
		//Ensures that if no controller is found, handle is NULL
		handle = NULL;
	}
	hid_free_enumeration(devs);
	return handle;
}

bool SteamController_Open(SteamControllerInfos* controller){
	if(!controller) return false;

	struct hid_device_info *devs, *cur_dev;
	//Open Steam Controller device
	if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_CONTROLLER)) != NULL){ // A Steam Controller
		std::cout<<"Found a Steam Controller"<<std::endl;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_CONTROLLER_2015)) != NULL){ // Wired Steam Controller (2015)
		std::cout<<"Found wired Steam Controller (2015)"<<std::endl;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_DONGLE)) != NULL){ // Steam Controller (2015) dongle //TODO: FIX
		std::cout<<"Found Steam Dongle, will attempt to use the first Steam Controller (2015)"<<std::endl;
		controller->interfaceNum = 1;
		controller->type = ControllerType::Original;
	} 
	else if((controller->hid_handle = open_steam_controller_hid(STEAM_CONTROLLER_2026)) != NULL) { // Steam Controller (2026)
		std::cout<<"OK"<<std::endl;
		controller->type = ControllerType::Triton;
		if (!tritonLimit) channelCount = 4;
	}
	else if((controller->hid_handle = open_steam_controller_hid(STEAM_PUCK)) != NULL) { // Steam Puck
		std::cout<<"OK"<<std::endl;
		controller->type = ControllerType::Triton;
		if (!tritonLimit) channelCount = 4;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_DECK)) != NULL){ // Steam Deck
		std::cout<<"Found Steam Deck"<<std::endl;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Jupiter;
	}
	else{
		std::cout<<"No device found"<<std::endl;
		std::cin.ignore();
		return false;
	}

	//If dev_handle is NULL, it's using HIDAPI so skip this
	if(controller->dev_handle != NULL) {
		//On Linux, automatically detach and reattach kernel module
		libusb_set_auto_detach_kernel_driver(controller->dev_handle,1);
		//Claim the USB interface
		int r = libusb_claim_interface(controller->dev_handle,controller->interfaceNum);
		if(r < 0) {
			std::cout<<"Interface claim Error "<<libusb_error_name(r)<<std::endl;
			libusb_close(controller->dev_handle);
			std::cin.ignore();
			return false;
		}
	}
	
	return true;
}

void SteamController_Close(SteamControllerInfos* controller){
	if(controller->dev_handle != NULL) {
		int r = libusb_release_interface(controller->dev_handle,controller->interfaceNum);
		if(r < 0) {
			std::cout<<"Interface release Error "<<libusb_error_name(r)<<std::endl;
			std::cin.ignore();
			return;
		}
		libusb_close(controller->dev_handle);
	} else {
		hid_close(controller->hid_handle);
	}
}

uint16_t pitchFrequency_uint16(const uint16_t (&midiFrequencyRef)[128], int note, int pitch_bend, int pitch_range) {
	if (pitch_bend == 0) return midiFrequencyRef[note];
	double range_step = pitch_bend / (16384 / (pitch_range * 2));
	int note_offset = note + (int)range_step - pitch_range;
	double pitch_mul = range_step - (int)range_step;
	return midiFrequencyRef[note_offset] + ((midiFrequencyRef[note_offset+1] - midiFrequencyRef[note_offset]) * pitch_mul);
}

int8_t gainSlide(const int8_t (&gainCurveRef)[128], int note, int pitch_bend) {
	if (pitch_bend == 0) return gainCurveRef[note];
	double range_step = pitch_bend / (16384 / (2 * 2));
	int note_offset = note + (int)range_step - 2;
	double pitch_mul = range_step - (int)range_step;
	return gainCurveRef[note_offset] + ((gainCurveRef[note_offset+1] - gainCurveRef[note_offset]) * pitch_mul);
}

//Steam Haptics Playblack
int SteamHaptics_PlayNote(SteamControllerInfos* controller, int channel, int note, int velocity, int pitch_bend){
	if (channel > 1 && controller->type != ControllerType::Triton) return 1;
	unsigned char dataBlob[64] = {0};
	
	double frequency = midiFrequency[note];

	int r;

	//double period;
	uint16_t periodCommand;
	//uint16_t repeatCommand;
	//uint16_t gainCommand;
	
	int haptic;
	uint16_t freq;
	int8_t gain;

	switch(controller->type) {
	case ControllerType::Original: //Steam Controller (2015) Playback

		//repeatCommand = (note == NOTE_STOP) ? 0x0000 : 0x7fff;
		//gainCommand = (directVel) ? (velocity * 65535) / 127 : 0x0000; //Doesn't work
		if (note == NOTE_STOP) {
			dataBlob[0] = 0x8F;
			dataBlob[2] = channel;
			//dataBlob[8] = 0x80;
		} else {
			//period = 1.0 / frequency;
			periodCommand = STEAM_CONTROLLER_MAGIC_PERIOD_RATIO / frequency; //Reminder to check if the Steam Controller tuning lines up with the Deck.
			dataBlob[0] = 0x8F;
			dataBlob[2] = channel;
			dataBlob[3] = periodCommand % 0xFF;
			dataBlob[4] = periodCommand / 0xFF;
			dataBlob[5] = periodCommand % 0xFF;
			dataBlob[6] = periodCommand / 0xFF;
			dataBlob[7] = 0xFF;
			dataBlob[8] = 0x7F;
			//dataBlob[9] = 0x00;
			//dataBlob[10]= 0x00;
		}
		
		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,controller->interfaceNum,dataBlob,64,1000);
		if(r < 0) {
			std::cout<<"\nCommand Error "<<libusb_error_name(r)<<std::endl;
			exitMute = false;
			std::cin.ignore();
			std::exit(EXIT_FAILURE);
		}
		break;

	case ControllerType::Triton: //Steam Controller (2026) Playback

		//Swap channels to match Steam Controller (2015)
		haptic = channel ^ 1;
		//Swap trackpad and rumble if wanted
		if (!tritonSwap) haptic = haptic ^ 2;
		//Make range match what command expects (0,1,2,3) -> (0,1,3,4)
		haptic = haptic + (haptic >> 1);
		if (note == NOTE_STOP) {
			dataBlob[0] = 0x83;
			dataBlob[1] = haptic;
			dataBlob[2] = 0x80;
			dataBlob[6] = 0x80;
		} else {
			//Get frequency and gain needed depending on haptic
			//const uint16_t* freq[128] = (haptic < 2) ? &midiFrequencyRb : midiFrequencyTr;
			//freq = (pitchpitchFrequency_uint32(midiFrequencyRb, note, pitch_bend)Bend == 0) ? ((haptic < 2) ? midiFrequencyRb[note] : midiFrequencyTr[note]) : pitchFrequency(&midiFrequency);
			freq = (haptic < 2) ? pitchFrequency_uint16(midiFrequencyRb, note, pitch_bend,0) : pitchFrequency_uint16(midiFrequencyTr, note, pitch_bend,0);
			gain = (haptic < 2) ? gainSlide(gainCurveRb,note,pitch_bend) : gainSlide(gainCurveRb,note,pitch_bend);
			dataBlob[0] = 0x83;
			dataBlob[1] = haptic;
			dataBlob[2] = ((directVel) ? (velocity * 255) / 127 - 128 : gain) + gainModifier[haptic];
			dataBlob[3] = freq % 0xFF;
			dataBlob[4] = freq / 0xFF;
			dataBlob[5] = 0xFF;
			dataBlob[6] = 0x7F;
		}
		
		r = hid_write(controller->hid_handle,dataBlob,64);
		if(r < 0) {
			wprintf(L"\nCommand Error %ls\n", hid_error(controller->hid_handle));
			exitMute = false;
			std::cin.ignore();
			std::exit(EXIT_FAILURE);
		}
		break;

	case ControllerType::Jupiter: //Steam Deck Playback

		if (note == NOTE_STOP) {
			dataBlob[0] = 0xEA;
			dataBlob[2] = !channel;
			dataBlob[3] = 0x03;
			dataBlob[5] = 0x80;
			dataBlob[9] = 0x80;
		} else {
			freq = midiFrequency[note];
			dataBlob[0] = 0xEA;
			dataBlob[2] = !channel; //Swap haptics to match 2015
			dataBlob[3] = 0x03; 
			dataBlob[5] = ((directVel) ? (velocity * 255) / 127 - 128 : gainCurveDk[note]) + gainModifier[!channel];
			dataBlob[6] = freq % 0xFF;
			dataBlob[7] = freq / 0xFF;
			dataBlob[8] = 0xFF;
			dataBlob[9] = 0x7F;
		}

		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,2,dataBlob,64,1000);
		if(r < 0) {
			std::cout<<"\nCommand Error "<<libusb_error_name(r)<<std::endl;
			exitMute = false;
			std::cin.ignore();
			std::exit(EXIT_FAILURE);
		}
		break;
	
	}

	return 0;
}

void SteamHaptics_StopNotes(SteamControllerInfos* controller) {
	for (int i = 0 ; i < CHANNEL_COUNT ; i++) {
		SteamHaptics_PlayNote(controller,i,NOTE_STOP,0,0);
	}
}

/*double pitchFrequency_double(int low_note, int base_note, int high_note, int pitch_bend) {

}*/

#ifdef __linux__
	#include <termios.h>

	struct termios orig_termios;
	bool rawMode = false;

	void disableRawMode() {
		if (rawMode) {
			tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
			rawMode = false;
		}
	}

	void enableRawMode() {
		if (!rawMode) {
			tcgetattr(STDIN_FILENO, &orig_termios);
			struct termios raw = orig_termios;
			raw.c_lflag &= ~(ICANON | ECHO);
			tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
			rawMode = true;
		}
	}

	bool isKeyInBuffer() {
		fd_set fds;
		struct timeval tv = {0, 0};

		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);

		return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
	}
#endif

//Retuns 0 - none, -1 - left, 2 - space, 1 - right, 3 - enter
int getPressedKey() {
	char ch,seq1,seq2;
	#ifdef __linux__
		#define KEY_LEFT  'D'
		#define KEY_RIGHT 'C'
		#define KEY_SPACE ' '
		#define KEY_ENTER '\n'
		if (isKeyInBuffer()) {
			std::cin.get(ch);
			if (ch == KEY_SPACE) {
				return 2;
			}
			else if (ch == KEY_ENTER) {
				return 3;
			}
			else if (ch == '\x1b') {
				char seq1, seq2;
				if (std::cin.get(seq1) && std::cin.get(seq2)) {
					if (seq1 == '[') {
						switch (seq2) {
							//Left   
							case KEY_LEFT:
								return -1;
								break;
							//Right
							case KEY_RIGHT:
								return 1;
								break;
						}
					}
				}
			}
		}
	#elif defined(_WIN32)
		
		#define KEY_LEFT  75
		#define KEY_RIGHT 77
		#define KEY_SPACE 32
		#define KEY_ENTER 13
		if (_kbhit()) {
			ch = _getch();
			
			if (ch == KEY_SPACE) {
				return 2;
			}
			else if (ch == KEY_ENTER) {
				return 3;
			}
			else if (ch == 0 || ch == 224) {
				seq1 = _getch();

				switch (seq1) {
					case KEY_LEFT:
						return -1;
					case KEY_RIGHT:
						return 1;

				}
			}
		}
	#endif
	return 0;
}

void displayPlayedNotes(int channel, int note) {
    static int8_t notePerChannel[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP, NOTE_STOP, NOTE_STOP};
	const char* textPerChannel[CHANNEL_COUNT] = {"Left Rumble    : ",
                                                 "Right Rumble   : ",
                                                 "Left Trackpad  : ",
                                                 "Right Trackpad : "};
	const char* noteBaseNameArray[12] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

    if (channel >= 0 && channel < channelCount) {
        notePerChannel[channel ^ 1 ^ (tritonSwap * 2)] = note;
    }
	else if (channel == -1) {
		for (int i = 0; i < channelCount; ++i) {
			notePerChannel[i] = NOTE_STOP;
		}
	}

	

	#ifdef __linux__

	static bool savedPos = false;
	if (!savedPos) {
		//Save position
    	std::cout << "\033[s";
		savedPos = true;
	} else {
		//Go back up
    	std::cout << "\033[u";
	}

	std::cout << "\033[?25l" << std::flush;

	#elif defined(_WIN32)

	if (savedPosition.X == -1) {
		//Save position
		GetConsoleScreenBufferInfo(hConsole, &csbi);
		savedPosition = csbi.dwCursorPosition;
	} else {
		//Go back up
    	SetConsoleCursorPosition(hConsole, savedPosition);
	}

	#endif

    for (int i = 0; i < channelCount; ++i) {
        std::cout << textPerChannel[(i + channelCount) & 3];
        //Write OFF
        if (notePerChannel[i] == NOTE_STOP) {
            std::cout << "OFF";
        } else {
           //Write note name
		    std::cout << noteBaseNameArray[notePerChannel[i]%12];
			int octave = (notePerChannel[i]/12)-1;
			std::cout << octave;
        }
        std::cout << "\n";
    }
}

void displayProgressBar(int currentTick, int endTick, float tempo, bool playing) {
	std::cout << "\nTempo: ";
	if (tempo) {
		std::cout << tempo;
	}
	std::cout << "\n";

    const int pos = PROGRESS_BAR_LENGTH * currentTick / (endTick + 1);
    for (int i = 0; i < PROGRESS_BAR_LENGTH; ++i) {
        if (i < pos) {
            std::cout << "=";
        } 
        else if (i == pos) {
            std::cout << "|";
        }
        else {
            std::cout << "-";
        }
    }

	std::cout << "\n[Left/Right] Seek - [Space] ";
	if (playing) std::cout<<"Pause"; else std::cout<<"Play ";
}

void displayPlayedNotes_old(int channel, int8_t note){
	static int8_t notePerChannel[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP, NOTE_STOP, NOTE_STOP};
	const char* textPerChannel[CHANNEL_COUNT] = {"LEFT haptic : ",", RIGHT haptic : ",", LEFT haptic : ",", RIGHT haptic : "};
	const char* noteBaseNameArray[12] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

	if(channel >= channelCount) return;

	notePerChannel[(channel < 2) ? !channel : !(channel-2)+2] = note;

	for(int i = 0 ; i < channelCount ; i++){
		std::cout << textPerChannel[i];

		//Write empty string
		if(notePerChannel[i] == NOTE_STOP){
			std::cout << "OFF ";
		}
		else{
			//Write note name
			std::cout << noteBaseNameArray[notePerChannel[i]%12];
			int octave = (notePerChannel[i]/12)-1;
			std::cout << octave;
			if(octave >= 0 ){
				std::cout << " ";
			}
		}
	}

	std::cout << "\r" ;
	std::cout.flush();
}

void playSong(SteamControllerInfos* controller,const ParamsStruct* params) {
	//Load MIDI file
	std::ifstream file{params->midiInput, std::ios::binary | std::ios::ate};
    if (!file.is_open())
    {
        std::cout << "Could not open " << params->midiInput << std::endl;
        return;
    }

	//Get size
	std::streamsize size = file.tellg();
	if (size > 1048576) {
		std::cout << "Given file is larger than 1MB! Are you sure this is MIDI?" << std::endl;
		return;
	}

	//Allocate
	file.seekg(0, std::ios::beg);
	std::vector<uint8_t> bytes(size);
    bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	//Initialize reader object
    libremidi::reader r{true};

    //Parse
    libremidi::reader::parse_result result = r.parse(bytes);

    if (result == libremidi::reader::invalid) {
        std::cout << "Invalid MIDI file!" << std::endl;
        return;
    }
	
	if (strstr(params->midiInput,"_dv")) {
        std::cout << "Found \"_dv\" in file name, assuming direct velocity to gain control" << std::endl;
		directVel = true;
    }

	#ifdef __linux__
	enableRawMode();
	#endif

	//Now try to stop notes on exit
	exitMute = true;

	//Hide cursor
	std::cout << "\033[?25l" << std::flush;

	//Pre-start
	std::cout << "Starting playback of " << params->midiInput  << "... press [Enter] anytime to stop\n\n";
	displayPlayedNotes(-1,NOTE_STOP);
	displayProgressBar(0,100,0,true);
    displayPlayedNotes(-1,NOTE_STOP);
	std::cout << "\n\n";
	const char spinner[4] = {'|','/','-','\\'};
	for (int i = 0; i < 9; ++i) {
		std::cout << "\r" << spinner[i & 3] << std::flush;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	displayPlayedNotes(-1,NOTE_STOP);
	

	//Wait a sec TODO: make something a little more fancy here | / - \ | 
	//std::this_thread::sleep_for(std::chrono::seconds(1));

	//Clock setup
	long beatDuration = 60'000'000 / r.startingTempo;
	float tempo = 60 * 1'000'000 / beatDuration;
	//std::cout << "Tempo: " << tempo << std::endl;
    std::chrono::nanoseconds tickDuration(static_cast<long>(beatDuration * 1'000 / r.ticksPerBeat));
	//float tickDuration = 60.0 / r.startingTempo / r.ticksPerBeat;
	int currentTick = 0;
	int endTick = static_cast<int>(r.get_end_time());
	bool playing = true;
	int stuckTick;
	std::chrono::steady_clock::time_point tOrigin = std::chrono::steady_clock::now();
	std::vector<size_t> trackIndex(r.tracks.size(),0);
	const libremidi::track_event* eventOnChannel[CHANNEL_COUNT] = {nullptr};

	while (currentTick <= endTick) {
		std::this_thread::sleep_for(std::chrono::microseconds(params->intervalUSec));

		//Check for input
		int key = getPressedKey();
		switch (key) {
			case -1:
			case 1:
				currentTick += (endTick / PROGRESS_BAR_LENGTH + 1) * key;
				//currentTick += 10 * r.ticksPerBeat * key;
				if (currentTick < 0) currentTick = 0;
				else if (currentTick >= endTick) {
					currentTick = endTick-r.ticksPerBeat;
				}
				for (int i = 0; i < CHANNEL_COUNT; ++i) {
					eventOnChannel[i] = nullptr;
				}
				trackIndex.assign(r.tracks.size(),0);
				stuckTick = currentTick;
				SteamHaptics_StopNotes(controller);
				displayPlayedNotes(-1,NOTE_STOP);
				displayProgressBar(currentTick,endTick,tempo,playing);
				break;
			case 2:
				playing = !playing;
				stuckTick = currentTick;
				displayPlayedNotes(-2,NOTE_STOP);
				displayProgressBar(currentTick,endTick,tempo,playing);
				std::cout<<std::flush;
				break;
			case 3:
				currentTick = endTick+1;
				break;
		}

		//Accumulate tick
		std::chrono::steady_clock::time_point tNow = std::chrono::steady_clock::now();
		if (tNow - tOrigin >= tickDuration) {
			int tickAmount = std::chrono::duration_cast<std::chrono::nanoseconds>(tNow - tOrigin) / tickDuration;
			currentTick += tickAmount;
			tOrigin += tickDuration * tickAmount;
		}

		//Check if we went over, since we're done if we did
		if (currentTick > endTick) break;

		if (playing) {

		//Get MIDI data
		for (size_t i = 0; i < r.tracks.size(); ++i) {
			const libremidi::midi_track& track = r.tracks[i];
			while (trackIndex[i] < track.size()) {
				const libremidi::track_event& event = track[trackIndex[i]];
				//If the current event hasn't happened yet, we can't do it, break out of loop
				if (currentTick < event.tick) break;
				//Increment the index of this track
				trackIndex[i]++;

				//Get message data
				if (event.m.is_meta_event()) {
					//TEMPO CHANGE
					if (event.m.get_meta_event_type() == libremidi::meta_event_type::TEMPO_CHANGE) {
						beatDuration = ((((uint32_t)event.m.bytes[3]) << 16) + (event.m.bytes[4] << 8) + event.m.bytes[5]);
						tickDuration = static_cast<std::chrono::nanoseconds>(static_cast<long>(beatDuration * 1'000 / r.ticksPerBeat));
						tempo = 60 * 1'000'000 / beatDuration;
						//std::cout << "Tempo: " << tempo << std::endl;
					}
				} else {
					//This needs more work, we need to check if the NOTE OFF matches the previous NOTE ON depending on channel, and by default be NOTE OFF until set otherwise
					if (event.m.is_note_on_or_off()) {
						
						//Get Channel
						int channel = event.m.get_channel()-1;

						//Skip if channel out of range
						if (channel >= channelCount) continue;
						
						//Set note
						int note = NOTE_STOP;
						int velocity = 0;
						
						//Check if note off or velocity 0
						if (event.m.bytes[2] == 0 || event.m.get_message_type() == libremidi::message_type::NOTE_OFF) {

							//Get previous event
							//Make sure it's not null
							if (eventOnChannel[channel] == nullptr) continue;
							const libremidi::track_event previousEvent = *eventOnChannel[channel];

							//Skip if the previous event wasn't note on (should always be)
							if (previousEvent.m.get_message_type() != libremidi::message_type::NOTE_ON) continue;

							//Skip if the notes don't match
							if (previousEvent.m.bytes[1] != event.m.bytes[1]) continue;

							//Skip if they're on the same tick
							if (previousEvent.tick == event.tick) continue;
						
						} 
						//Check if note on
						else if (event.m.get_message_type() == libremidi::message_type::NOTE_ON) {
							note = event.m.bytes[1];
							velocity = event.m.bytes[2];
							eventOnChannel[channel] = &event;
						}

						if (currentTick - r.ticksPerBeat < event.tick) {
						SteamHaptics_PlayNote(controller,channel,note,velocity,0);
						displayPlayedNotes(channel,note);
						displayProgressBar(currentTick,endTick,tempo,playing);
						}

					} else if (event.m.get_message_type() == libremidi::message_type::PITCH_BEND) {

						//Get pitch bend from current event
						int pitchBend = event.m.bytes[1] << 7 + event.m.bytes[2];

						//Skip if pitch bend is 0
						if (pitchBend == 0) continue;

						//Get Channel
						int channel = event.m.get_channel()-1;

						//Skip if channel out of range
						if (channel >= channelCount) continue;
						//std::cout << channel << std::endl;

						//Get previous event
						//Make sure it's not null
						if (eventOnChannel[channel] == nullptr) continue;
						const libremidi::track_event previousEvent = *eventOnChannel[channel];

						//Skip if the previous event wasn't note on (should always be)
						if (previousEvent.m.get_message_type() != libremidi::message_type::NOTE_ON) continue;
						//Get data from previous event
						channel = previousEvent.m.get_channel()-1;
						int note = previousEvent.m.bytes[1];
						int velocity = previousEvent.m.bytes[2];
						
						SteamHaptics_PlayNote(controller,channel,note,velocity,pitchBend);
					}
				}
			}
		}
		} else {
			SteamHaptics_StopNotes(controller);
			currentTick = stuckTick;
		}
	}

	//Stop everything just in case
	SteamHaptics_StopNotes(controller);
	
	//if (channelCount - 2) std::cout << "\n\n"; 
	std::cout << "\nPlayback completed, press any key to exit\n";

	return;
}

void playRealTime(SteamControllerInfos* controller, const ParamsStruct* params) {
	std::cout << "Starting real-time MIDI... press [Ctrl+C] anytime to stop\n";

	//enableRawMode();
	
	auto my_callback = [&](const libremidi::message& message) {
		static int noteOnChannel[CHANNEL_COUNT][3] = {{NOTE_STOP,0,-1}};
		if (message.is_note_on_or_off()) {
						
			//Get Channel
			int channel = message.get_channel()-1;

			//Skip if channel out of range
			if (channel >= channelCount) return;
						
			//Set note
			int note = NOTE_STOP;
			int velocity = 0;
						
			//Check if note off or velocity 0
			if (message.bytes[2] == 0 || message.get_message_type() == libremidi::message_type::NOTE_OFF) {

				//Get previous event
				//Make sure it's not null
				if (noteOnChannel[channel][0] == -1) return;
				//const libremidi::message previousMesg = messageOnChannel[channel];

				//Skip if the previous event wasn't note on (should always be)
				//if (message.get_message_type() != libremidi::message_type::NOTE_ON) return;

				//Skip if the notes don't match
				if (noteOnChannel[channel][0] != message.bytes[1]) return;

				//Skip if they're on the same tick
				//if (previousEvent.tick == event.tick) continue;
			
			} 
		//Check if note on
		else if (message.get_message_type() == libremidi::message_type::NOTE_ON) {
			note = message.bytes[1];
			velocity = message.bytes[2];
		}

		noteOnChannel[channel][0] = note;
		noteOnChannel[channel][1] = velocity;
		SteamHaptics_PlayNote(controller,channel,note,velocity,0);
		displayPlayedNotes(channel,note);
		}
	};

	// Create the midi object
	libremidi::midi_in midi{ 
	libremidi::input_configuration{ .on_message = my_callback } 
	};

	libremidi::observer obs;
	// if (!params->midiInput[0]) {
	// 	if (libremidi::input_port port = libremidi::midi1::in_default_port()) {
			
	// 	}
	// }

	for(const libremidi::input_port& port : obs.get_input_ports()) {
		
		#ifdef __linux__
  		if (port.port_name == params->midiInput) {
		#elif defined(_WIN32)
		if (port.port_name == ((params->midiInput[0]) ? params->midiInput : "loopMIDI Port")) {
		#endif
			std::cout << "Using \"" << port.port_name << "\"\n\n";
			displayPlayedNotes(-1,NOTE_STOP);
			midi.open_port(port);
		}
	}
	if (!midi.is_port_open()) {
		std::cout << "MIDI port failure! ";
		if (params->midiInput[0]) {
			std::cout << "Unable to open \"" << params->midiInput << "\"." << std::endl;
		}
		#ifdef __linux__
		else {
			std::cout << "None specified." << std::endl;
		}
		#elif defined(_WIN32) 
		else {
			std::cout << "Attempted to open \"loopMIDI Port\" but failed. Unless you're using a different MIDI device, please launch or install loopMIDI to use real-time." << std::endl;
		}
		#endif
		exitMute = false;
		std::cin.ignore();
		return;
	}
		


	while(true) {
		std::this_thread::sleep_for(std::chrono::microseconds(params->intervalUSec));
	}
}

bool parseArguments(int argc, char** argv, ParamsStruct* params){
	int c;
	bool readyState = false;
	while ( (c = getopt(argc, argv, "l:r:n:m:d:i:pvutseh")) != -1) {
		int32_t value;
		switch(c){
		case 'l':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				gainModifier[0] = value;
			}
			break;
		case 'r':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				gainModifier[1] = value;
			}
			break;
		case 'n':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				gainModifier[3] = value;
			}
			break;
		case 'm':
			value = strtol(optarg,NULL,10);
			if(value >= -64 && value <= 63){
				gainModifier[4] = value;
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
			directVel = true;
			break;
		case 'u':
			noGainCurve = true;
			break;
		case 't':
			tritonLimit = true;
			break;
		case 's':
			tritonSwap = true;
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

	if (exitMute) SteamHaptics_StopNotes(&steamController1);
	SteamController_Close(&steamController1);

	#ifdef __linux__
	disableRawMode();
	#endif

	std::cout << std::flush;

	libusb_exit(NULL);
	hid_exit();
}

int main(int argc, char** argv)
{
	

	/*while (true) {
		std::this_thread::sleep_for(std::chrono::microseconds(10000));
		int a = getPressedKey();
		if (a) std::cout << a << std::endl;
	}*/

	ParamsStruct params;
	params.intervalUSec = DEFAULT_INTERVAL_USEC;
	params.libusbDebugLevel = LIBUSB_LOG_LEVEL_NONE;
	params.repeatSong = false;
	params.midiInput = "\0";
	params.realTime = false;

	//Parse arguments
	if(!parseArguments(argc, argv, &params)){
		std::cout << 
				"Steam Haptics Singer v1.13 by Crazy, based off of Steam Controller Singer by Pila\n"
			    "Usage: steam-haptics-singer [-lMODIFIER] [-rMODIFIER] [-nMODIFIER] [-mMODIFIER] [-iINTERVAL] [-dDEBUG_LEVEL] [-p] [-v] [-u] [-t] [-s] MIDI_FILE\n"
			  "\nThere must be no space for negative gain modifiers"
			  "\n  -lMODIFIER		Left trackpad gain modifier"
			  "\n  -rMODIFIER		Right trackpad gain modifier"
			  "\n  -nMODIFIER		Left rumble gain modifier"
			  "\n  -mMODIFIER		Right rumble gain modifier "
			  "\n  -iINTERVAL		Player sleep interval (in microseconds). Lower generally means better song fidelity, but higher cpu usage, and at some point going lower won't improve any more. Default value is 5000"
			  "\n  -dDEBUG_LEVEL		Libusb debug level. Default is 0, no debug output. Max is 4, max verbosity output"
		      "\n  -p	Repeat song, plays again after ending"
			  "\n  -v 	Direct velocity to gain control, the MIDI file will set the gain"
			  "\n  -u	No gain curve, all notes use (signed) 0x00 gain"
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

	//DWORD dwMode = 0;
	//if (!GetConsoleMode(hConsole,&dwMode)) return 1;
	//dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	//SetConsoleMode(hConsole,dwMode);

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
