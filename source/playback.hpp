#pragma once
#include "controller.hpp"

#ifdef __linux__
void disableRawMode();
void enableRawMode();
bool isKeyInBuffer();
#endif

int getPressedKey();

void displayPlayedNotes(int channel, int note);

void displayProgressBar(int currentTick, int endTick, float tempo, bool playing);

void displayPlayedNotes_old(int channel, int8_t note);

void playSong(SteamControllerInfos* controller,const ParamsStruct* params);

void playRealTime(SteamControllerInfos* controller, const ParamsStruct* params);