#include <iostream>
#include <cstdint>
#include <hidapi.h>
#include <libusb.h>
#include <cstring>
#include <fstream>
#include "controller.hpp"
#include "global.hpp"

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

bool Check_SteamDeckOLED() {
	std::ifstream file("/sys/class/dmi/id/product_name");
	char buffer[8];
	if (!file) {
		return false;
	}
	while(file.getline(buffer, sizeof(buffer))) {
		if( strncmp(buffer,"Galileo",sizeof(buffer))==0 ) return true;
	}
	return false;
}

bool SteamController_Open(SteamControllerInfos* controller){
	if(!controller) return false;

	struct hid_device_info *devs, *cur_dev;
	//Open Steam Controller device
	if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_CONTROLLER)) != NULL){ // A Steam Controller
		std::cout<<"Found a Steam Controller\n";
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_CONTROLLER_2015)) != NULL){ // Wired Steam Controller (2015)
		std::cout<<"Found wired Steam Controller (2015)\n";
		controller->interfaceNum = 2;
		controller->type = ControllerType::Original;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_DONGLE)) != NULL){ // Steam Controller (2015) dongle //TODO: FIX
		std::cout<<"Found Steam Dongle, will attempt to use the first Steam Controller (2015)\n";
		controller->interfaceNum = 1;
		controller->type = ControllerType::Original;
	} 
	else if((controller->hid_handle = open_steam_controller_hid(STEAM_CONTROLLER_2026)) != NULL) { // Steam Controller (2026)
		std::cout<<"OK\n";
		controller->type = ControllerType::Triton;
		if (!controller->tritonLimit) controller->channelCount = 4;
	}
	else if((controller->hid_handle = open_steam_controller_hid(STEAM_PUCK)) != NULL) { // Steam Puck
		std::cout<<"OK\n";
		controller->type = ControllerType::Triton;
		if (!controller->tritonLimit) controller->channelCount = 4;
	}
	else if((controller->dev_handle = libusb_open_device_with_vid_pid(NULL, VALVE_VID, STEAM_DECK)) != NULL){ // Steam Deck
		std::cout<<"Found Steam Deck ";
		controller->interfaceNum = 2;
		controller->type = ControllerType::Jupiter;
		if(Check_SteamDeckOLED()) {
			std::cout<<"(OLED)\n";
			controller->isOled = true;
		}
		else std::cout << "(LCD)\n";
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

void SteamController_Close(SteamControllerInfos* controller) {
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
int SteamHaptics_PlayNote(SteamControllerInfos* controller, int channel, int note, int velocity, int pitch_bend) {
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
			//exitMute = false;
			std::cin.ignore();
			std::exit(EXIT_FAILURE);
		}
		break;

	case ControllerType::Triton: //Steam Controller (2026) Playback

		//Swap channels to match Steam Controller (2015)
		haptic = channel ^ 1;
		//Swap trackpad and rumble if wanted
		if (!controller->tritonSwap) haptic = haptic ^ 2;
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
			dataBlob[2] = ((controller->directVel) ? (velocity * 255) / 127 - 128 : gain) + controller->gainModifier[haptic];
			dataBlob[3] = freq % 0xFF;
			dataBlob[4] = freq / 0xFF;
			dataBlob[5] = 0xFF;
			dataBlob[6] = 0x7F;
		}
		
		r = hid_write(controller->hid_handle,dataBlob,64);
		if(r < 0) {
			wprintf(L"\nCommand Error %ls\n", hid_error(controller->hid_handle));
			//exitMute = false;
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
			dataBlob[5] = ((controller->directVel) ? (velocity * 255) / 127 - 128 : gainCurveJp[note]) + controller->gainModifier[!channel];
			dataBlob[6] = freq % 0xFF;
			dataBlob[7] = freq / 0xFF;
			dataBlob[8] = 0xFF;
			dataBlob[9] = 0x7F;
		}

		r = libusb_control_transfer(controller->dev_handle,0x21,9,0x0300,2,dataBlob,64,1000);
		if(r < 0) {
			std::cout<<"\nCommand Error "<<libusb_error_name(r)<<std::endl;
			//exitMute = false;
			std::cin.ignore();
			std::exit(EXIT_FAILURE);
		}
		break;
	
	}

	return 0;
}

void SteamHaptics_StopNotes(SteamControllerInfos* controller) {
	for (int i = 0 ; i < controller->channelCount ; i++) {
		SteamHaptics_PlayNote(controller,i,NOTE_STOP,0,0);
	}
}

/*double pitchFrequency_double(int low_note, int base_note, int high_note, int pitch_bend) {

}*/