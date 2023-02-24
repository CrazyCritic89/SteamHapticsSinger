#include <iostream>
#include <chrono>

#include <stdint-gcc.h>
#include <unistd.h>
#include <stdint.h>

#include <signal.h>
#include <stdio.h>

#include <libusb.h>
#include "midifile/midifile.h"

#define STEAM_CONTROLLER_MAGIC_PERIOD_RATIO 495483.0
#define CHANNEL_COUNT                       2
#define DEFAULT_INTERVAL_USEC               10000

#define DURATION_MAX        -1
#define NOTE_STOP           -1

#define DEFAULT_RECLAIM_PERIOD 2

using namespace std;


struct ParamsStruct{
    const char* midiSong;
    unsigned int intervalUSec;
    int libusbDebugLevel;
    bool repeatSong;
    int reclaimPeriod;
};

struct SteamControllerInfos{
    libusb_device_handle* dev_handle;
    int interfaceNum;
};

SteamControllerInfos steamController1;


bool SteamController_Open(SteamControllerInfos* controller){
    if(!controller)
        return false;

    libusb_device_handle* dev_handle;
    //Open Steam Controller device
    if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1102)) != NULL){ // Wired Steam Controller
        cout<<"Found wired Steam Controller"<<endl;
        controller->dev_handle = dev_handle;
        controller->interfaceNum = 2;
    }
    else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1142)) != NULL){ // Steam Controller dongle
        cout<<"Found Steam Dongle, will attempt to use the first Steam Controller"<<endl;
        controller->dev_handle = dev_handle;
        controller->interfaceNum = 1;
    } else if((dev_handle = libusb_open_device_with_vid_pid(NULL, 0x28DE, 0x1205)) != NULL){ // Steam Deck
        cout<<"Found Steam Deck"<<endl;
        controller->dev_handle = dev_handle;
        controller->interfaceNum = 2;
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
        cout<<"Interface claim Error "<<r<<endl;
        std::cin.ignore();
        libusb_close(controller->dev_handle);
        return false;
    }

    return true;
}

void SteamController_Close(SteamControllerInfos* controller){
    int r = libusb_release_interface(controller->dev_handle,controller->interfaceNum);
    if(r < 0) {
        cout<<"Interface release Error "<<r<<endl;
        std::cin.ignore();
        return;
    }
}


int SteamController_PlayNote(SteamControllerInfos* controller, int haptic, int note,double duration = DURATION_MAX){
    double midiFrequency[128]  = {0, 8.66196, 9.17702, 9.72272, 10.3009, 10.9134, 11.5623, 12.2499, 12.9783, 13.75, 14.5676, 15.4339, 16.3516, 17.3239, 18.354, 19.4454, 20.6017, 21.8268, 23.1247, 24.4997, 25.9565, 27.5, 29.1352, 30.8677, 32.7032, 34.6478, 36.7081, 38.8909, 41.2034, 43.6535, 46.2493, 48.9994, 51.9131, 55, 58.2705, 61.7354, 65.4064, 69.2957, 73.4162, 77.7817, 82.4069, 87.3071, 92.4986, 97.9989, 103.826, 110, 116.541, 123.471, 130.813, 138.591, 146.832, 155.563, 164.814, 174.614, 184.997, 195.998, 207.652, 220, 233.082, 246.942, 261.626, 277.183, 293.665, 311.127, 329.628, 349.228, 369.994, 391.995, 415.305, 440, 466.164, 493.883, 523.251, 554.365, 587.33, 622.254, 659.255, 698.456, 739.989, 783.991, 830.609, 880, 932.328, 987.767, 1046.5, 1108.73, 1174.66, 1244.51, 1318.51, 1396.91, 1479.98, 1567.98, 1661.22, 1760, 1864.66, 1975.53, 2093, 2217.46, 2349.32, 2489.02, 2637.02, 2793.83, 2959.96, 3135.96, 3322.44, 3520, 3729.31, 3951.07, 4186.01, 4434.92, 4698.64, 4978.03, 5274.04, 5587.65, 5919.91, 6271.93, 6644.88, 7040, 7458.62, 7902.13, 8372.02, 8869.84, 9397.27, 9956.06, 10548.1, 11175.3, 11839.8, 12543.9};

    unsigned char dataBlob[64] = {0x8f,
                                  0x07,
                                  0x00, //Trackpad select : 0x01 = left, 0x00 = right
                                  0x00, //LSB Pulse High Duration
                                  0x00, //MSB Pulse High Duration
                                  0x00, //LSB Pulse Low Duration
                                  0x00, //MSB Pulse Low Duration
                                  0x00, //LSB Pulse repeat count
                                  0x00, //MSB Pulse repeat count
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if(note == NOTE_STOP){
        note = 0;
        duration = 0.0;
    }

    double frequency = midiFrequency[note];
    double period = 1.0 / frequency;
    uint16_t periodCommand = period * STEAM_CONTROLLER_MAGIC_PERIOD_RATIO;

    //Compute number of repeat. If duration < 0, set to maximum
    uint16_t repeatCount = (duration >= 0.0) ? (duration / period) : 0x7FFF;

    //cout << "Frequency : " <<frequency << ", Period : "<<periodCommand << ", Repeat : "<< repeatCount <<"\n";

    dataBlob[2] = haptic;
    dataBlob[3] = periodCommand % 0xff;
    dataBlob[4] = periodCommand / 0xff;
    dataBlob[5] = periodCommand % 0xff;
    dataBlob[6] = periodCommand / 0xff;
    dataBlob[7] = repeatCount % 0xff;
    dataBlob[8] = repeatCount / 0xff;

    int r;
    r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,2,dataBlob,64,1000);
    if(r < 0) {
        cout<<"Command Error "<<r<< endl;
        exit(0);
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
    static int8_t notePerChannel[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP};
    const char* textPerChannel[CHANNEL_COUNT] = {"LEFT haptic : ",", RIGHT haptic : "};
    const char* noteBaseNameArray[12] = {" C","C#"," D","D#"," E"," F","F#"," G","G#"," A","A#"," B"};

    if(channel >= CHANNEL_COUNT)
        return;

    notePerChannel[CHANNEL_COUNT-1-channel] = note;

    for(int i = 0 ; i < CHANNEL_COUNT ; i++){
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
    midifile = MidiFile_load(params.midiSong);

    if(midifile == NULL){
        cout << "Unable to open song file " << params.midiSong << endl;
        return;
    }

    //Check if file contains at least one midi event
    if(MidiFile_getFirstEvent(midifile) == NULL){
        cout << "Song file is empty !!" << endl;
        return;
    }

    //Waiting for user to press enter
    cout << "Starting playback of " <<params.midiSong  << endl;
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

        //Every reclaimPeriod seconds, reclaim the controller to avoid timeouts
        if(timeElapsedSince(tRestart) > params.reclaimPeriod){
            tRestart = std::chrono::steady_clock::now();
            SteamController_Close(&steamController1);
            SteamController_Claim(&steamController1);
        }

        //Iterate through all events until the current time, and selecte potential events to play
        for( ; currentEvent != NULL && MidiFileEvent_getTick(currentEvent) < currentTick ; currentEvent = MidiFileEvent_getNextEventInFile(currentEvent)){

            //Only process note start events or note end events matching previous event
            if (!MidiFileEvent_isNoteStartEvent(currentEvent) && !MidiFileEvent_isNoteEndEvent(currentEvent)) continue;

            //Get channel event
            int eventChannel = MidiFileVoiceEvent_getChannel(currentEvent);

            //If channel is other than 0 or 1, skip this event, we cannot play it with only 1 steam controller
            if(eventChannel < 0 || !(eventChannel < CHANNEL_COUNT)) continue;

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
        for(int currentChannel = 0 ; currentChannel < CHANNEL_COUNT ; currentChannel++){
            MidiFileEvent_t selectedEvent = eventsToPlay[currentChannel];

            //If no note event available on the channel, skip it
            if(!MidiFileEvent_isNoteEvent(selectedEvent)) continue;

            //Set note event
            int8_t eventNote = NOTE_STOP;
            if(MidiFileEvent_isNoteStartEvent(selectedEvent)){
                eventNote = MidiFileNoteStartEvent_getNote(selectedEvent);
            }

            //Play notes
            SteamController_PlayNote(controller,currentChannel,eventNote);
            displayPlayedNotes(currentChannel,eventNote);
        }
    }

    cout <<endl<< "Playback completed " << endl;
}





bool parseArguments(int argc, char** argv, ParamsStruct* params){
    int c;
    while ( (c = getopt(argc, argv, "c:l:i:r")) != -1) {
        unsigned long int value;
	switch(c){
        case 'c':
	    value = strtoul(optarg,NULL,10);
            if(value <= 1000000 && value > 0){
                params->reclaimPeriod = value;
            }
            break;
        case 'l':
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
        case 'r':
            params->repeatSong = true;
            break;
        case '?':
            return false;
            break;
        default:
            break;
        }
    }
    if(optind == argc-1 ){
        params->midiSong = argv[optind];
        return true;
    }
    else{
        return false;
    }
}


void abortPlaying(int){
    for(int i = 0 ; i < CHANNEL_COUNT ; i++){
        SteamController_PlayNote(&steamController1,i,NOTE_STOP);
    }

    SteamController_Close(&steamController1);

    cout << endl<< "Aborted " << endl;
    cout.flush();
    exit(1);
}

int main(int argc, char** argv)
{
    cout <<"Steam Controller Singer by Pila, Deck support by Crazy"<<endl;

    ParamsStruct params;
    params.intervalUSec = DEFAULT_INTERVAL_USEC;
    params.libusbDebugLevel = LIBUSB_LOG_LEVEL_NONE;
    params.repeatSong = false;
    params.midiSong = "\0";
    params.reclaimPeriod = DEFAULT_RECLAIM_PERIOD;


    //Parse arguments
    if(!parseArguments(argc, argv, &params)){
        cout << "Usage : steam-haptics-singer [-r][-lDEBUG_LEVEL] [-iINTERVAL] [-cRECLAIM_PERIOD] MIDI_FILE" << endl;
        return 1;
    }


    //Initializing LIBUSB
    int r = libusb_init(NULL);
    if(r < 0) {
        cout<<"LIBUSB Init Error "<<r<<endl;
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
