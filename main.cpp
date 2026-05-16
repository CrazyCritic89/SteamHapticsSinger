#include <cstdint>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <cstring>

#include <stdint-gcc.h>
#include <unistd.h>
#include <stdint.h>

#include <signal.h>
#include <stdio.h>

#include <libusb.h>
#include "midifile/midifile.h"

#define STEAM_CONTROLLER_MAGIC_PERIOD_RATIO 495483.0
#define CHANNEL_COUNT					   4
#define DEFAULT_INTERVAL_USEC			   10000

#define DURATION_MAX		-1
#define NOTE_STOP		   -1

#define DEFAULT_GAIN 127

using namespace std;

double midiFrequency[128]  = {0, 8.66196, 9.17702, 9.72272, 10.3009, 10.9134, 11.5623, 12.2499, 12.9783, 13.75, 14.5676, 15.4339, 16.3516, 17.3239, 18.354, 19.4454, 20.6017, 21.8268, 23.1247, 24.4997, 25.9565, 27.5, 29.1352, 30.8677, 32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55, 58.2705, 61.7354, 65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.826, 110, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220, 233.082, 246.942, 261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164, 493.883, 523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767, 1046.5, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760, 1864.66, 1975.53, 2093, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520, 3729.31, 3951.07, 4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040, 7458.62, 7902.13, 8372.02, 8869.84, 9397.27, 9956.06, 10548.1, 11175.3, 11839.8, 12543.9};


struct ParamsStruct{
	const char* midiSongOrDevice;
	unsigned int intervalUSec;
	int libusbDebugLevel;
	bool repeatSong;
};

//TEMPORARY, move to ParamsStruct and find a way to reference within playback function
bool legacyInst = false;
bool directVel = false;
bool tritonLimit = false;
bool tritonSwap = false;
int channelCount = 2;

enum class ControllerType {
	None,
	Original,
	Triton,
	Jupiter,
	Galileo
};

struct SteamControllerInfos{
	libusb_device_handle* dev_handle;
	int interfaceNum;

	ControllerType type = ControllerType::None;
	signed char leftGain;
	signed char rightGain;
	int endPoint;
};

SteamControllerInfos steamController1;

bool SteamController_Open(SteamControllerInfos* controller){
	if(!controller)
		return false;

	libusb_device_handle* dev_handle;
	//Open Steam Controller device
	if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1102)) != NULL){ // Wired Steam Controller (2015)
		cout<<"Found wired Steam Controller (2015)"<<endl;
		controller->dev_handle = dev_handle;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1142)) != NULL){ // Steam Controller (2015) dongle //TODO: FIX
		cout<<"Found Steam Dongle, will attempt to use the first Steam Controller (2015)"<<endl;
		controller->dev_handle = dev_handle;
		controller->interfaceNum = 1;
		controller->type = ControllerType::Original;
	} 
	else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1302)) != NULL){ // Steam Controller (2026)
		cout<<"Found wired Steam Controller (2026)"<<endl;
		controller->dev_handle = dev_handle;
		controller->interfaceNum = 0;
		controller->type = ControllerType::Triton;
		controller->endPoint = 0x01;
		if (!tritonLimit) channelCount = 4;
	}
	else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1304)) != NULL){ // Steam Puck
		cout<<"Found Steam Puck, will attempt to use the first Steam Controller (2026)"<<endl;
		controller->dev_handle = dev_handle;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Triton;
		controller->endPoint = 0x02;
		//Workaround for controller disconnecting after puck reconnects on Windows
		#ifdef _WIN32
		cout << "Reconnect controller and press Enter...";
    	cin.get(); 
		#endif
		if (!tritonLimit) channelCount = 4;
	}
	else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1205)) != NULL){ // Steam Deck
		cout<<"Found Steam Deck"<<endl;
		controller->dev_handle = dev_handle;
		controller->interfaceNum = 2;
		controller->type = ControllerType::Jupiter;
	}
	else{
		cout<<"No device found"<<endl;
		return false;
	}

	//On Linux, automatically detach and reattach kernel module
	libusb_set_auto_detach_kernel_driver(controller->dev_handle,1);
	
	return true;
}

bool SteamController_Claim(SteamControllerInfos* controller){
	//Claim the USB interface controlling the haptic actuators
	int r = libusb_claim_interface(controller->dev_handle,controller->interfaceNum);
	if(r < 0) {
		cout<<"Interface claim Error "<<libusb_error_name(r)<<endl;
		std::cin.ignore();
		libusb_close(controller->dev_handle);
		return false;
	}

	return true;
}

void SteamController_Close(SteamControllerInfos* controller){
	int r = libusb_release_interface(controller->dev_handle,controller->interfaceNum);
	if(r < 0) {
		cout<<"Interface release Error "<<libusb_error_name(r)<<endl;
		std::cin.ignore();
		return;
	}
}

//Steam Haptics Playblack
int SteamHaptics_PlayNote(SteamControllerInfos* controller, int channel, int note, int velocity){
	if (channel > 1 && controller->type != ControllerType::Triton) return 1;
	unsigned char dataBlob[16] = {0};
	
	double frequency = midiFrequency[note];
	uint16_t duration = (note == NOTE_STOP) ? 0x0000 : 0x7fff;

	int r;

	double period;
	uint16_t periodCommand;
	uint16_t repeatCommand;
	uint16_t gainCommand;

	switch(controller->type) {
	case ControllerType::Original: //Steam Controller (2015) Playback

		period = 1.0 / frequency;
		periodCommand = period * STEAM_CONTROLLER_MAGIC_PERIOD_RATIO; //Reminder to check if the Steam Controller tuning lines up with the Deck.
		repeatCommand = (note == NOTE_STOP) ? 0x0000 : 0x7fff;
		//gainCommand = (directVel) ? (velocity * 65535) / 127 : 0x0000; //Doesn't work

		dataBlob[0] = 0x8F;
		dataBlob[2] = channel;
		dataBlob[3] = periodCommand % 0xFF;
		dataBlob[4] = periodCommand / 0xFF;
		dataBlob[5] = periodCommand % 0xFF;
		dataBlob[6] = periodCommand / 0xFF;
		dataBlob[7] = repeatCommand % 0xFF;
		dataBlob[8] = repeatCommand / 0xFF;
		//dataBlob[9] = 0x00;
		//dataBlob[10]= 0x00;
		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,controller->interfaceNum,dataBlob,16,1000);
		if(r < 0) {
			cout<<"Command Error "<<libusb_error_name(r)<< endl;
			exit(0);
		}
		break;

	case ControllerType::Triton: //Steam Controller (2026) Playback

		if (note == NOTE_STOP) {
			//This prevents the controller from rebooting when using rumble motors and drifting out of tune
			dataBlob[0] = 0x81;
			dataBlob[1] = (tritonSwap) ?
						  ((channel < 2) ? channel : !(channel-2)+3) :
						  ((channel < 2) ? !channel+3 : channel-2);			  
			//dataBlob[1] = ((channel < 2) != tritonSwap) ? !channel+3 : channel-2;
		} else {
			dataBlob[0] = 0x83;
			dataBlob[1] = (tritonSwap) ?
						  ((channel < 2) ? !channel : !(channel-2)+3) :
						  ((channel < 2) ? !channel+3 : !(channel-2));
			//dataBlob[1] = ((channel < 2) != tritonSwap) ? !channel+3 : !(channel-2);
			dataBlob[2] = (directVel) ? (velocity * 255) / 127 - 128 : 0xFE;
			dataBlob[3] = (int)frequency % 0xFF;
			dataBlob[4] = (int)frequency / 0xFF;
			dataBlob[5] = 0xFF;
			dataBlob[6] = 0x7F;
		}
		
		r = libusb_interrupt_transfer(controller->dev_handle,controller->endPoint,dataBlob,16,NULL,1000);
		if(r < 0) {
			cout<<"Command Error "<<libusb_error_name(r)<< endl;
			exit(0);
		}
		break;

	case ControllerType::Jupiter: //Steam Deck Playback
	
		dataBlob[0] = 0xEA;
		dataBlob[2] = !channel; //Swap haptics to match 2015
		dataBlob[3] = 0x03; 
		dataBlob[5] = (directVel) ? (velocity * 255) / 127 - 128 : 0x00;
		dataBlob[6] = (int)frequency % 0xFF;
		dataBlob[7] = (int)frequency / 0xFF;
		dataBlob[8] = duration % 0xFF;
		dataBlob[9] = duration / 0xFF;
		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,2,dataBlob,16,1000);
		if(r < 0) {
			cout<<"Command Error "<<libusb_error_name(r)<< endl;
			exit(0);
		}
		break;
	
	}

	return 0;
}

float timeElapsedSince(std::chrono::steady_clock::time_point tOrigin){
	using namespace std::chrono;
	steady_clock::time_point tNow = steady_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(tNow - tOrigin);
	return time_span.count();
}


void displayPlayedNotes(int channel, int8_t note){
	static int8_t notePerChannel[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP, NOTE_STOP, NOTE_STOP};
	const char* textPerChannel[CHANNEL_COUNT] = {"LEFT haptic : ",", RIGHT haptic : ",", LEFT haptic : ",", RIGHT haptic : "};
	const char* noteBaseNameArray[12] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

	if(channel >= channelCount)
		return;

	notePerChannel[(channel < 2) ? !channel : !(channel-2)+2] = note;

	for(int i = 0 ; i < channelCount ; i++){
		cout << textPerChannel[i];

		//Write empty string
		if(notePerChannel[i] == NOTE_STOP){
			cout << "OFF ";
		}
		else{
			//Write note name
			cout << noteBaseNameArray[notePerChannel[i]%12];
			int octave = (notePerChannel[i]/12)-1;
			cout << octave;
			if(octave >= 0 ){
				cout << " ";
			}
		}
	}

	cout << "\r" ;
	cout.flush();
}

void playSong(SteamControllerInfos* controller,const ParamsStruct params){

	MidiFile_t midifile;

	//Open Midi File
	midifile = MidiFile_load(params.midiSongOrDevice);

	if(midifile == NULL){
		cout << "Unable to open MIDI file!" << params.midiSongOrDevice << endl;
		return;
	}

	//Check if file contains at least one midi event
	if(MidiFile_getFirstEvent(midifile) == NULL){
		cout << "MIDI file is empty!" << endl;
		return;
	}
	
	if (strstr(params.midiSongOrDevice,"dv")) {
        std::cout << "Found \"dv\" in file name, assuming direct velocity to gain control" << std::endl;
		directVel = true;
    }

	//Waiting for user to press enter; YOURE WRONG, SULFURIC ACID!
	cout << "Starting playback of " << params.midiSongOrDevice  << "... press Ctrl+C anytime to stop" << endl;
	sleep(1);

	//This will contains the previous events accepted for each channel
	MidiFileEvent_t acceptedEventPerChannel[CHANNEL_COUNT] = {0};

	//Get current time point, will be used to know elapsed time
	std::chrono::steady_clock::time_point tOrigin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point tRestart = std::chrono::steady_clock::now();

	//Iterate through events
	MidiFileEvent_t currentEvent = MidiFile_getFirstEvent(midifile);
	
	while(currentEvent != NULL){
		usleep(params.intervalUSec);

		//This will contains the events to play
		MidiFileEvent_t eventsToPlay[CHANNEL_COUNT] = {NULL};

		//We now need to play all events with tick < currentTime
		long currentTick = MidiFile_getTickFromTime(midifile,timeElapsedSince(tOrigin));

		//Iterate through all events until the current time, and selecte potential events to play
		for( ; currentEvent != NULL && MidiFileEvent_getTick(currentEvent) < currentTick ; currentEvent = MidiFileEvent_getNextEventInFile(currentEvent)){

			//Only process note start events or note end events matching previous event
			if (!MidiFileEvent_isNoteStartEvent(currentEvent) && !MidiFileEvent_isNoteEndEvent(currentEvent)) continue;

			//Get channel event
			int eventChannel = MidiFileVoiceEvent_getChannel(currentEvent);

			//If channel is other than 0 or 1, skip this event, we cannot play it with only 1 steam controller
			if(eventChannel < 0 || !(eventChannel < channelCount)) continue;

			//If event is note off and does not match previous played event, skip it
			if(MidiFileEvent_isNoteEndEvent(currentEvent)){
				MidiFileEvent_t previousEvent = acceptedEventPerChannel[eventChannel];

				//Skip if current event is not ending previous event,
				// or if they share the same tick ( end event after start evetn on same tick )
				if(MidiFileNoteStartEvent_getNote(previousEvent) != MidiFileNoteEndEvent_getNote(currentEvent)
				||(MidiFileEvent_getTick(currentEvent) == MidiFileEvent_getTick(previousEvent)))
					continue;
			}

			//If we arrive here, this event is accepted
			eventsToPlay[eventChannel] = currentEvent;
			acceptedEventPerChannel[eventChannel]=currentEvent;
		}

		//Now play the last events found
		for(int currentChannel = 0 ; currentChannel < channelCount ; currentChannel++){
			MidiFileEvent_t selectedEvent = eventsToPlay[currentChannel];

			//If no note event available on the channel, skip it
			if(!MidiFileEvent_isNoteStartEvent(selectedEvent) && !MidiFileEvent_isNoteEndEvent(selectedEvent)) continue;

			//Set note event
			int8_t eventNote = NOTE_STOP;
			int8_t eventVel  = 0;
			if(MidiFileEvent_isNoteStartEvent(selectedEvent)){
				//Send note stop before playing to prevent Steam Controller (2026) rebooting when using motors
				SteamHaptics_PlayNote(controller,currentChannel,NOTE_STOP,0);
				eventNote = MidiFileNoteStartEvent_getNote(selectedEvent);
				eventVel  = MidiFileNoteStartEvent_getVelocity(selectedEvent);
			}

			//Play notes
			SteamHaptics_PlayNote(controller,currentChannel,eventNote,eventVel);

			displayPlayedNotes(currentChannel,eventNote);
		}
	}

	for(int i = 0 ; i < CHANNEL_COUNT ; i++){
		SteamHaptics_PlayNote(&steamController1,i,NOTE_STOP,0); //Wait, this actually references the controller directly, why????????
	}
	
	cout <<endl<< "Playback completed " << endl;
}
#ifdef __linux__
void playDevice(SteamControllerInfos *controller, const ParamsStruct params) {
  FILE *midiDevice = fopen(params.midiSongOrDevice, "r");
  if (!midiDevice) {
    cout << "Unable to open MIDI device!" << params.midiSongOrDevice << endl;
    return;
  }

  cout << "Playing from MIDI device, press Ctrl+C to stop" << endl;

  int currentNote[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP, NOTE_STOP, NOTE_STOP};
  while (true) {
    uint8_t midiEvent[3];
    size_t bytesRead = fread(midiEvent, sizeof(uint8_t), 3, midiDevice);
    if (bytesRead < 3) {
      break;
    }

    uint8_t status = midiEvent[0] & 0xF0; // ignore channel
    uint8_t channel = midiEvent[0] & 0x0F;
    uint8_t note = midiEvent[1];
    uint8_t velocity = midiEvent[2];

    if (status == 0x90) { // Note On
      currentNote[channel] = note;
      SteamHaptics_PlayNote(controller, channel, note, velocity);
      displayPlayedNotes(channel, note);
    } else if (((status == 0x80) || (status == 0x90 && velocity == 0)) && note == currentNote[channel]) { // Note Off
      SteamHaptics_PlayNote(controller, channel, NOTE_STOP, 0);
      displayPlayedNotes(channel, NOTE_STOP);
    }
  }

  fclose(midiDevice);
}
#endif


bool parseArguments(int argc, char** argv, ParamsStruct* params){
	int c;
	while ( (c = getopt(argc, argv, "d:i:pets")) != -1) {
		unsigned long int value;
		switch(c){
		/*case 'l':
			value = strtoul(optarg,NULL,10);
			if(value <= 255 && value > 0){
				params->leftGain = value;
			}
			break;
		case 'r':
			value = strtoul(optarg,NULL,10);
			if(value <= 255 && value > 0){
				params->rightGain = value;
			}
			break;*/
		case 'd':
			value = strtoul(optarg,NULL,10);
			if(value >= LIBUSB_LOG_LEVEL_NONE && value <= LIBUSB_LOG_LEVEL_DEBUG){
				params->libusbDebugLevel = value;
			}
			break;
		case 'i':
			value = strtoul(optarg,NULL,10);
			if(value <= 1000000 && value > 0){
				params->intervalUSec = value;
			}
			break;
		case 'p':
			params->repeatSong = true;
			break;
		case 'e':
			directVel = true;
			break;
		case 't':
			tritonLimit = true;
			break;
		case 's':
			tritonSwap = true;
			break;
		// case 'y':
		// 	legacyInst = true;
		// 	break;
		case '?':
			return false;
			break;
		default:
			break;
		}
	}
	if(optind == argc-1 ){
		params->midiSongOrDevice = argv[optind];
		return true;
	}
	else{
		return false;
	}
}


void abortPlaying(int){
	for(int i = 0 ; i < CHANNEL_COUNT ; i++){
		SteamHaptics_PlayNote(&steamController1,i,NOTE_STOP,0); //Wait, this actually references the controller directly, why????????
	}

	SteamController_Close(&steamController1);

	cout << endl<< "Aborted " << endl;
	cout.flush();
	exit(1);
}

int main(int argc, char** argv)
{
	cout <<"Steam Haptics Singer by Crazy, Steam Controller Singer by Pila"<<endl;

	ParamsStruct params;
	params.intervalUSec = DEFAULT_INTERVAL_USEC;
	params.libusbDebugLevel = LIBUSB_LOG_LEVEL_NONE;
	params.repeatSong = false;
	params.midiSongOrDevice = "\0";
	//params.leftGain = DEFAULT_GAIN;
	//params.rightGain = DEFAULT_GAIN;


	//Parse arguments
	if(!parseArguments(argc, argv, &params)){
		cout << "Usage: steam-haptics-singer [-p] [-y] [-d DEBUG_LEVEL] [-i INTERVAL] MIDI_FILE\n"
			  "\n  -i INTERVAL		Player sleep interval (in microseconds). Lower generally means better song fidelity, but higher cpu usage, and at some point going lower won't improve any more. Default value is 10000"
			  "\n  -d DEBUG_LEVEL	Libusb debug level. Default is 0, no debug output. max is 4, max verbosity output"
		      "\n  -p	Repeat song, plays again after ending"
			  "\n  -e 	Direct velocity to gain control, the MIDI file will set the gain"
			  "\n  -t	(Steam Controller 2026 Only) Limit to only two channels"
			  "\n  -s	(Steam Controller 2026 Only) Swap rumble and trackpad channels"
				"" << endl;
		return 1;
	}


	//Initializing LIBUSB
	libusb_context * ctx = NULL;
	int r = libusb_init(&ctx);
	libusb_set_option(ctx, LIBUSB_OPTION_USE_USBDK);
	if(r < 0) {
		cout<<"LIBUSB Init Error "<<libusb_error_name(r)<<endl;
		std::cin.ignore();
		return 1;
	}

	libusb_set_debug(NULL, params.libusbDebugLevel);

	//Gaining access to Steam Controller
	if(!SteamController_Open(&steamController1)){
		return 1;
	}
	if(!SteamController_Claim(&steamController1)){
		return 1;
	}

	//Set mecanism to stop playing when closing process
	signal(SIGINT, abortPlaying);

	#ifdef __linux__
	if (std::filesystem::is_character_file(params.midiSongOrDevice)) {
		playDevice(&steamController1, params);
	}
	#endif
	//Playing song
	do{
		playSong(&steamController1,params);
	}while(params.repeatSong);

	//Releasing access to Steam Controller
	SteamController_Close(&steamController1);
	
	libusb_close((&steamController1)->dev_handle);

	libusb_exit(NULL);

	return 0;
}
