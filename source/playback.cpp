#include <libremidi/libremidi.hpp>
#include <libremidi/reader.hpp>
#include <cstring>
#include <fstream>
#include "playback.hpp"
#include "controller.hpp"
#include "global.hpp"

#ifdef _WIN32
#include <conio.h>

HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
CONSOLE_SCREEN_BUFFER_INFO csbi;
COORD savedPosition = {-1,-1};
#endif 

#ifdef _WIN32 

/*void enableANSI() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}*/
#endif

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

void displayPlayedNotes(int channel, int note, SteamControllerInfos* controller) {
    static int8_t notePerChannel[CHANNEL_COUNT] = {NOTE_STOP, NOTE_STOP, NOTE_STOP, NOTE_STOP};
	const char* textPerChannel[CHANNEL_COUNT] = {"Left Rumble    : ",
                                                 "Right Rumble   : ",
                                                 "Left Trackpad  : ",
                                                 "Right Trackpad : "};
	const char* noteBaseNameArray[12] = {"C-","C#","D-","D#","E-","F-","F#","G-","G#","A-","A#","B-"};

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

	if (channel >= 0 && channel < controller->channelCount) {
        notePerChannel[channel ^ 1 ^ (controller->tritonSwap * 2)] = note;
    }
	else if (channel == -1) {
		for (int i = 0; i < controller->channelCount; ++i) {
			notePerChannel[i] = NOTE_STOP;
		}
	}
	else if (channel == -2) {
		std::cout << "\n\n";
		if (controller->channelCount > 2) std::cout << "\n\n";
		return;
	}

    for (int i = 0; i < controller->channelCount; ++i) {
        std::cout << textPerChannel[(i + controller->channelCount) & 3];
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

    const int pos = (currentTick >= endTick) ? PROGRESS_BAR_LENGTH-1 : PROGRESS_BAR_LENGTH * currentTick / endTick;
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

/*void displayPlayedNotes_old(int channel, int8_t note) {
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
}*/

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
		controller->directVel = true;
    }

	#ifdef __linux__
	enableRawMode();
	#endif

	//Now try to stop notes on exit
	//exitMute = true;

	//Hide cursor
	std::cout << "\033[?25l" << std::flush;

	//Pre-start
	std::cout << "Starting playback of " << params->midiInput  << "... press [Enter] anytime to stop\n\n";
	displayPlayedNotes(-1,NOTE_STOP,controller);
	displayProgressBar(0,100,0,true);
    displayPlayedNotes(-1,NOTE_STOP,controller);
	std::cout << "\n\n";
	const char spinner[4] = {'|','/','-','\\'};
	for (int i = 0; i < 9; ++i) {
		std::cout << "\r" << spinner[i & 3] << std::flush;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	displayPlayedNotes(-1,NOTE_STOP,controller);
	

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
	const int progress_bar_step = endTick / PROGRESS_BAR_LENGTH;

	while (currentTick <= endTick) {
		std::this_thread::sleep_for(std::chrono::microseconds(params->intervalUSec));

		//Check for input
		int key = getPressedKey();
		switch (key) {
			case -1:
			case 1:
				currentTick += progress_bar_step * key;
				//currentTick += 10 * r.ticksPerBeat * key;
				if (currentTick < progress_bar_step) currentTick = 0;
				else if (currentTick >= endTick) {
					currentTick = endTick-progress_bar_step;
				}
				for (int i = 0; i < CHANNEL_COUNT; ++i) {
					eventOnChannel[i] = nullptr;
				}
				trackIndex.assign(r.tracks.size(),0);
				stuckTick = currentTick;
				SteamHaptics_StopNotes(controller);
				displayPlayedNotes(-1,NOTE_STOP,controller);
				//displayProgressBar(currentTick,endTick,tempo,playing);
				break;
			case 2:
				playing = !playing;
				stuckTick = currentTick;
				//displayPlayedNotes(-2,NOTE_STOP);
				//displayProgressBar(currentTick,endTick,tempo,playing);
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
			//This way is bad and should be improved to wait for every progress bar length measure
			displayPlayedNotes(-2,NOTE_STOP,controller);
			displayProgressBar(currentTick,endTick,tempo,playing);
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
						if (channel >= controller->channelCount) continue;
						
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
						displayPlayedNotes(channel,note,controller);
						//displayProgressBar(currentTick,endTick,tempo,playing);
						}

					} else if (event.m.get_message_type() == libremidi::message_type::PITCH_BEND) {

						//Get pitch bend from current event
						int pitchBend = event.m.bytes[1] << 7 + event.m.bytes[2];

						//Skip if pitch bend is 0
						if (pitchBend == 0) continue;

						//Get Channel
						int channel = event.m.get_channel()-1;

						//Skip if channel out of range
						if (channel >= controller->channelCount) continue;
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
			if (channel >= controller->channelCount) return;
						
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
		displayPlayedNotes(channel,note,controller);
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
  		if (port.port_name == params->midiInput)
		#elif defined(_WIN32)
		if (port.port_name == ((params->midiInput[0]) ? params->midiInput : "loopMIDI Port"))
		#endif
        {
			std::cout << "Using \"" << port.port_name << "\"\n\n";
			displayPlayedNotes(-1,NOTE_STOP,controller);
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
		//exitMute = false;
		std::cin.ignore();
		return;
	}
		


	while(true) {
		std::this_thread::sleep_for(std::chrono::microseconds(params->intervalUSec));
	}
}