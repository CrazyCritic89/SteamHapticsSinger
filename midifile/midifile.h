#ifndef MIDI_FILE_INCLUDED
#define MIDI_FILE_INCLUDED

/*
 * Div's Standard MIDI File API
 * Copyright 2003-2021 by David G. Slomin
 * Provided under the terms of the BSD license
 *
 * Usage notes:
 *
 * 1.  Running status is eliminated from messages at load time; it should not
 *     be used at any other time.
 *
 * 2.  MIDI files in formats 0, 1, and 2 are supported, but the caller is
 *     responsible for placing events into the appropriate tracks.  Format
 *     0 files should only use a single track.  Format 1 files use their
 *     first track as a "conductor track" for meta events like tempo and
 *     meter changes.
 *
 * 3.  Any data passed into these functions is memory-managed by the caller.
 *     Any data returned from these functions is memory-managed by the API.
 *     Don't forget to call MidiFile_free().
 *
 * 4.  This API is not thread-safe.
 *
 * 5.  All numbers in this API are zero-based, to correspond with the actual
 *     byte values of the MIDI protocol, rather than one-based, as they are
 *     commonly displayed to the user.  Channels range from 0 to 15, notes
 *     range from 0 to 127, etc.  The only exception is in the musical time
 *     functions.
 *
 * 6.  You can navigate through events one track at a time, or through all
 *     tracks at once in an interwoven, time-sorted manner.
 *
 * 7.  MidiFile_iterateEvents() and MidiFileTrack_iterateEvents() are specially
 *     designed so that you can add, delete, or change the tick of the current
 *     event (thereby modifying the sorting order) without upsetting the
 *     iterator.  Keep calling them until they return null.
 *
 * 8.  Because a note on event with a velocity of zero is functionally
 *     equivalent to a note off event, you cannot simply look at the type of
 *     an event to see whether it signifies the start or the end of a note.
 *     To handle this problem, convenience wrappers are provided for pseudo
 *     "note start" and "note end" events.
 *
 * 9.  Alternatively, it's often useful to think of a note as a single event
 *     with a duration rather than separate events for the start and end.
 *     Use MidiFile_convertStandardEventsToNoteEvents() to switch to this
 *     representation.  Similar conversions are provided for fine (14 bit)
 *     control change events, RPN events, and NRPN events.  Don't forget
 *     to convert back to standard events before saving.
 *
 * 10. Convenience functions are provided for working with tempo, absolute
 *     time, musical time, and SMPTE time in files of format 1 or 0.
 *
 * 11. Events other than sysex and meta (i.e., events which have a channel)
 *     are considered "voice events".  For interaction with other APIs, it
 *     is sometimes useful to pack their messages into 32 bit integers.
 *
 * 12. Key signatures are expressed as a negative number of flats or a
 *     positive number of sharps.
 *
 * 13. An event can be "detached", which means that it is no longer part of
 *     a track or the file, but retains the rest of its properties.  It can
 *     be reattached to the same or a different file by setting its track
 *     property, or explicitly deleted.
 *
 * 14. Events can be marked as "selected" but this is only meaningful in
 *     memory; it is not persisted to disk.
 */

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct MidiFile *MidiFile_t;
typedef struct MidiFileTrack *MidiFileTrack_t;
typedef struct MidiFileEvent *MidiFileEvent_t;
typedef void (*MidiFileEventVisitorCallback_t)(MidiFileEvent_t event, void *user_data);
typedef struct MidiFileMeasureBeat *MidiFileMeasureBeat_t;
typedef struct MidiFileMeasureBeatTick *MidiFileMeasureBeatTick_t;
typedef struct MidiFileHourMinuteSecond *MidiFileHourMinuteSecond_t;
typedef struct MidiFileHourMinuteSecondFrame *MidiFileHourMinuteSecondFrame_t;

typedef enum
{
	MIDI_FILE_DIVISION_TYPE_INVALID = -1,
	MIDI_FILE_DIVISION_TYPE_PPQ,
	MIDI_FILE_DIVISION_TYPE_SMPTE24,
	MIDI_FILE_DIVISION_TYPE_SMPTE25,
	MIDI_FILE_DIVISION_TYPE_SMPTE30DROP,
	MIDI_FILE_DIVISION_TYPE_SMPTE30
}
MidiFileDivisionType_t;

typedef enum
{
	MIDI_FILE_EVENT_TYPE_INVALID = -1,
	MIDI_FILE_EVENT_TYPE_NOTE_OFF,
	MIDI_FILE_EVENT_TYPE_NOTE_ON,
	MIDI_FILE_EVENT_TYPE_KEY_PRESSURE,
	MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE,
	MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE,
	MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE,
	MIDI_FILE_EVENT_TYPE_PITCH_WHEEL,
	MIDI_FILE_EVENT_TYPE_SYSEX,
	MIDI_FILE_EVENT_TYPE_META,
	MIDI_FILE_EVENT_TYPE_NOTE,
	MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE,
	MIDI_FILE_EVENT_TYPE_RPN,
	MIDI_FILE_EVENT_TYPE_NRPN
}
MidiFileEventType_t;

MidiFile_t MidiFile_load(char *filename);
int MidiFile_save(MidiFile_t midi_file, const char* filename);
MidiFile_t MidiFile_loadFromBuffer(unsigned char *buffer);
int MidiFile_saveToBuffer(MidiFile_t midi_file, unsigned char *buffer);
int MidiFile_getFileSize(MidiFile_t midi_file);

MidiFile_t MidiFile_new(int file_format, MidiFileDivisionType_t division_type, int resolution);
MidiFile_t MidiFile_newFromTemplate(MidiFile_t template_midi_file);
int MidiFile_free(MidiFile_t midi_file);
int MidiFile_getFileFormat(MidiFile_t midi_file);
int MidiFile_setFileFormat(MidiFile_t midi_file, int file_format);
MidiFileDivisionType_t MidiFile_getDivisionType(MidiFile_t midi_file);
int MidiFile_setDivisionType(MidiFile_t midi_file, MidiFileDivisionType_t division_type);
int MidiFile_getResolution(MidiFile_t midi_file);
int MidiFile_setResolution(MidiFile_t midi_file, int resolution);
float MidiFile_getNumberOfFramesPerSecond(MidiFile_t midi_file);
int MidiFile_setNumberOfFramesPerSecond(MidiFile_t midi_file, float number_of_frames_per_second);
MidiFileTrack_t MidiFile_createTrack(MidiFile_t midi_file);
int MidiFile_getNumberOfTracks(MidiFile_t midi_file);
MidiFileTrack_t MidiFile_getTrackByNumber(MidiFile_t midi_file, int number, int create);
MidiFileTrack_t MidiFile_getFirstTrack(MidiFile_t midi_file);
MidiFileTrack_t MidiFile_getLastTrack(MidiFile_t midi_file);
MidiFileEvent_t MidiFile_getFirstEvent(MidiFile_t midi_file);
MidiFileEvent_t MidiFile_getLastEvent(MidiFile_t midi_file);
MidiFileEvent_t MidiFile_iterateEvents(MidiFile_t midi_file);
int MidiFile_visitEvents(MidiFile_t midi_file, MidiFileEventVisitorCallback_t visitor_callback, void *user_data);
int MidiFile_convertSelectionFlagsToTextEvents(MidiFile_t midi_file, char *label);
int MidiFile_convertTextEventsToSelectionFlags(MidiFile_t midi_file, char *label);
int MidiFile_convertStandardEventsToNoteEvents(MidiFile_t midi_file);
int MidiFile_convertNoteEventsToStandardEvents(MidiFile_t midi_file);
int MidiFile_convertStandardEventsToFineControlChangeEvents(MidiFile_t midi_file);
int MidiFile_convertFineControlChangeEventsToStandardEvents(MidiFile_t midi_file);
int MidiFile_convertStandardEventsToRpnAndNrpnEvents(MidiFile_t midi_file);
int MidiFile_convertRpnAndNrpnEventsToStandardEvents(MidiFile_t midi_file);

float MidiFile_getBeatFromTick(MidiFile_t midi_file, long tick);
long MidiFile_getTickFromBeat(MidiFile_t midi_file, float beat);
float MidiFile_getTimeFromTick(MidiFile_t midi_file, long tick); /* time is in seconds */
long MidiFile_getTickFromTime(MidiFile_t midi_file, float time);
float MidiFile_getMeasureFromTick(MidiFile_t midi_file, long tick);
long MidiFile_getTickFromMeasure(MidiFile_t midi_file, float measure);
int MidiFile_setMeasureBeatFromTick(MidiFile_t midi_file, long tick, MidiFileMeasureBeat_t measure_beat);
long MidiFile_getTickFromMeasureBeat(MidiFile_t midi_file, MidiFileMeasureBeat_t measure_beat);
int MidiFile_setMeasureBeatTickFromTick(MidiFile_t midi_file, long tick, MidiFileMeasureBeatTick_t measure_beat_tick);
long MidiFile_getTickFromMeasureBeatTick(MidiFile_t midi_file, MidiFileMeasureBeatTick_t measure_beat_tick);
int MidiFile_setHourMinuteSecondFromTick(MidiFile_t midi_file, long tick, MidiFileHourMinuteSecond_t hour_minute_second);
long MidiFile_getTickFromHourMinuteSecond(MidiFile_t midi_file, MidiFileHourMinuteSecond_t hour_minute_second);
int MidiFile_setHourMinuteSecondFrameFromTick(MidiFile_t midi_file, long tick, MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
long MidiFile_getTickFromHourMinuteSecondFrame(MidiFile_t midi_file, MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
char *MidiFile_getMeasureBeatStringFromTick(MidiFile_t midi_file, long tick);
int MidiFile_getTickFromMeasureBeatString(MidiFile_t midi_file, char *measure_beat_string);
char *MidiFile_getMeasureBeatTickStringFromTick(MidiFile_t midi_file, long tick);
int MidiFile_getTickFromMeasureBeatTickString(MidiFile_t midi_file, char *measure_beat_tick_string);
char *MidiFile_getHourMinuteSecondStringFromTick(MidiFile_t midi_file, long tick);
int MidiFile_getTickFromHourMinuteSecondString(MidiFile_t midi_file, char *hour_minute_second_string);
char *MidiFile_getHourMinuteSecondFrameStringFromTick(MidiFile_t midi_file, long tick);
int MidiFile_getTickFromHourMinuteSecondFrameString(MidiFile_t midi_file, char *hour_minute_second_frame_string);
long MidiFile_getTickFromMarker(MidiFile_t midi_file, char *marker);
long MidiFile_getTickFromTimeString(MidiFile_t midi_file, char *time_string);

MidiFileEvent_t MidiFile_getFirstEventForTick(MidiFile_t midi_file, long tick);
MidiFileEvent_t MidiFile_getLastEventForTick(MidiFile_t midi_file, long tick);
MidiFileEvent_t MidiFile_getLatestTempoEventForTick(MidiFile_t midi_file, long tick);
MidiFileEvent_t MidiFile_getLatestTimeSignatureEventForTick(MidiFile_t midi_file, long tick);
MidiFileEvent_t MidiFile_getLatestKeySignatureEventForTick(MidiFile_t midi_file, long tick);

int MidiFileTrack_delete(MidiFileTrack_t track);
MidiFile_t MidiFileTrack_getMidiFile(MidiFileTrack_t track);
int MidiFileTrack_getNumber(MidiFileTrack_t track);
long MidiFileTrack_getEndTick(MidiFileTrack_t track);
int MidiFileTrack_setEndTick(MidiFileTrack_t track, long end_tick);
MidiFileEvent_t MidiFileTrack_getFirstEventForTick(MidiFileTrack_t track, long tick);
MidiFileEvent_t MidiFileTrack_getLastEventForTick(MidiFileTrack_t track, long tick);
MidiFileTrack_t MidiFileTrack_createTrackBefore(MidiFileTrack_t track);
MidiFileTrack_t MidiFileTrack_getPreviousTrack(MidiFileTrack_t track);
MidiFileTrack_t MidiFileTrack_getNextTrack(MidiFileTrack_t track);
MidiFileEvent_t MidiFileTrack_createNoteOffEvent(MidiFileTrack_t track, long tick, int channel, int note, int velocity);
MidiFileEvent_t MidiFileTrack_createNoteOnEvent(MidiFileTrack_t track, long tick, int channel, int note, int velocity);
MidiFileEvent_t MidiFileTrack_createKeyPressureEvent(MidiFileTrack_t track, long tick, int channel, int note, int amount);
MidiFileEvent_t MidiFileTrack_createControlChangeEvent(MidiFileTrack_t track, long tick, int channel, int number, int value);
MidiFileEvent_t MidiFileTrack_createProgramChangeEvent(MidiFileTrack_t track, long tick, int channel, int number);
MidiFileEvent_t MidiFileTrack_createChannelPressureEvent(MidiFileTrack_t track, long tick, int channel, int amount);
MidiFileEvent_t MidiFileTrack_createPitchWheelEvent(MidiFileTrack_t track, long tick, int channel, int value);
MidiFileEvent_t MidiFileTrack_createSysexEvent(MidiFileTrack_t track, long tick, int data_length, unsigned char *data_buffer);
MidiFileEvent_t MidiFileTrack_createMetaEvent(MidiFileTrack_t track, long tick, int number, int data_length, unsigned char *data_buffer);
MidiFileEvent_t MidiFileTrack_createNoteEvent(MidiFileTrack_t track, long tick, long duration_ticks, int channel, int note, int start_velocity, int end_velocity);
MidiFileEvent_t MidiFileTrack_createFineControlChangeEvent(MidiFileTrack_t track, long tick, int channel, int coarse_number, int value);
MidiFileEvent_t MidiFileTrack_createRpnEvent(MidiFileTrack_t track, long tick, int channel, int number, int value);
MidiFileEvent_t MidiFileTrack_createNrpnEvent(MidiFileTrack_t track, long tick, int channel, int number, int value);
MidiFileEvent_t MidiFileTrack_createNoteStartAndEndEvents(MidiFileTrack_t track, long start_tick, long end_tick, int channel, int note, int start_velocity, int end_velocity); /* returns the start event */
MidiFileEvent_t MidiFileTrack_createTextEvent(MidiFileTrack_t track, long tick, char *text);
MidiFileEvent_t MidiFileTrack_createLyricEvent(MidiFileTrack_t track, long tick, char *text);
MidiFileEvent_t MidiFileTrack_createMarkerEvent(MidiFileTrack_t track, long tick, char *text);
MidiFileEvent_t MidiFileTrack_createPortEvent(MidiFileTrack_t track, long tick, char *name);
MidiFileEvent_t MidiFileTrack_createTempoEvent(MidiFileTrack_t track, long tick, float tempo); /* tempo is in BPM */
MidiFileEvent_t MidiFileTrack_createTimeSignatureEvent(MidiFileTrack_t track, long tick, int numerator, int denominator);
MidiFileEvent_t MidiFileTrack_createKeySignatureEvent(MidiFileTrack_t track, long tick, int number, int minor);
MidiFileEvent_t MidiFileTrack_createVoiceEvent(MidiFileTrack_t track, long tick, unsigned long data);
MidiFileEvent_t MidiFileTrack_copyEvent(MidiFileTrack_t track, MidiFileEvent_t event);
MidiFileEvent_t MidiFileTrack_getFirstEvent(MidiFileTrack_t track);
MidiFileEvent_t MidiFileTrack_getLastEvent(MidiFileTrack_t track);
MidiFileEvent_t MidiFileTrack_iterateEvents(MidiFileTrack_t track);
int MidiFileTrack_visitEvents(MidiFileTrack_t track, MidiFileEventVisitorCallback_t visitor_callback, void *user_data);

int MidiFileEvent_delete(MidiFileEvent_t event);
int MidiFileEvent_detach(MidiFileEvent_t event);
MidiFileTrack_t MidiFileEvent_getTrack(MidiFileEvent_t event);
int MidiFileEvent_setTrack(MidiFileEvent_t event, MidiFileTrack_t track);
MidiFileEvent_t MidiFileEvent_getPreviousEvent(MidiFileEvent_t event); /* deprecated:  use MidiFileEvent_getPreviousEventInTrack() */
MidiFileEvent_t MidiFileEvent_getNextEvent(MidiFileEvent_t event); /* deprecated:  use MidiFileEvent_getNextEventInTrack() */
int MidiFileEvent_setPreviousEvent(MidiFileEvent_t event, MidiFileEvent_t previous_event);
int MidiFileEvent_setNextEvent(MidiFileEvent_t event, MidiFileEvent_t next_event);
MidiFileEvent_t MidiFileEvent_getPreviousEventInTrack(MidiFileEvent_t event);
MidiFileEvent_t MidiFileEvent_getNextEventInTrack(MidiFileEvent_t event);
MidiFileEvent_t MidiFileEvent_getPreviousEventInFile(MidiFileEvent_t event);
MidiFileEvent_t MidiFileEvent_getNextEventInFile(MidiFileEvent_t event);
long MidiFileEvent_getTick(MidiFileEvent_t event);
int MidiFileEvent_setTick(MidiFileEvent_t event, long tick);
MidiFileEventType_t MidiFileEvent_getType(MidiFileEvent_t event);
int MidiFileEvent_isSelected(MidiFileEvent_t event);
int MidiFileEvent_setSelected(MidiFileEvent_t event, int is_selected);
int MidiFileEvent_isNoteStartEvent(MidiFileEvent_t event);
int MidiFileEvent_isNoteEndEvent(MidiFileEvent_t event);
int MidiFileEvent_isPressureEvent(MidiFileEvent_t event);
int MidiFileEvent_isTextEvent(MidiFileEvent_t event);
int MidiFileEvent_isLyricEvent(MidiFileEvent_t event);
int MidiFileEvent_isMarkerEvent(MidiFileEvent_t event);
int MidiFileEvent_isPortEvent(MidiFileEvent_t event);
int MidiFileEvent_isTempoEvent(MidiFileEvent_t event);
int MidiFileEvent_isTimeSignatureEvent(MidiFileEvent_t event);
int MidiFileEvent_isKeySignatureEvent(MidiFileEvent_t event);
int MidiFileEvent_isVoiceEvent(MidiFileEvent_t event);

int MidiFileNoteOffEvent_getChannel(MidiFileEvent_t event);
int MidiFileNoteOffEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileNoteOffEvent_getNote(MidiFileEvent_t event);
int MidiFileNoteOffEvent_setNote(MidiFileEvent_t event, int note);
int MidiFileNoteOffEvent_getVelocity(MidiFileEvent_t event);
int MidiFileNoteOffEvent_setVelocity(MidiFileEvent_t event, int velocity);

int MidiFileNoteOnEvent_getChannel(MidiFileEvent_t event);
int MidiFileNoteOnEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileNoteOnEvent_getNote(MidiFileEvent_t event);
int MidiFileNoteOnEvent_setNote(MidiFileEvent_t event, int note);
int MidiFileNoteOnEvent_getVelocity(MidiFileEvent_t event);
int MidiFileNoteOnEvent_setVelocity(MidiFileEvent_t event, int velocity);

int MidiFileKeyPressureEvent_getChannel(MidiFileEvent_t event);
int MidiFileKeyPressureEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileKeyPressureEvent_getNote(MidiFileEvent_t event);
int MidiFileKeyPressureEvent_setNote(MidiFileEvent_t event, int note);
int MidiFileKeyPressureEvent_getAmount(MidiFileEvent_t event);
int MidiFileKeyPressureEvent_setAmount(MidiFileEvent_t event, int amount);

int MidiFileControlChangeEvent_getChannel(MidiFileEvent_t event);
int MidiFileControlChangeEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileControlChangeEvent_getNumber(MidiFileEvent_t event);
int MidiFileControlChangeEvent_setNumber(MidiFileEvent_t event, int number);
int MidiFileControlChangeEvent_getValue(MidiFileEvent_t event);
int MidiFileControlChangeEvent_setValue(MidiFileEvent_t event, int value);

int MidiFileProgramChangeEvent_getChannel(MidiFileEvent_t event);
int MidiFileProgramChangeEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileProgramChangeEvent_getNumber(MidiFileEvent_t event);
int MidiFileProgramChangeEvent_setNumber(MidiFileEvent_t event, int number);

int MidiFileChannelPressureEvent_getChannel(MidiFileEvent_t event);
int MidiFileChannelPressureEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileChannelPressureEvent_getAmount(MidiFileEvent_t event);
int MidiFileChannelPressureEvent_setAmount(MidiFileEvent_t event, int amount);

int MidiFilePitchWheelEvent_getChannel(MidiFileEvent_t event);
int MidiFilePitchWheelEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFilePitchWheelEvent_getValue(MidiFileEvent_t event);
int MidiFilePitchWheelEvent_setValue(MidiFileEvent_t event, int value);

int MidiFileSysexEvent_getDataLength(MidiFileEvent_t event);
unsigned char *MidiFileSysexEvent_getData(MidiFileEvent_t event);
int MidiFileSysexEvent_setData(MidiFileEvent_t event, int data_length, unsigned char *data_buffer);

int MidiFileMetaEvent_getNumber(MidiFileEvent_t event);
int MidiFileMetaEvent_setNumber(MidiFileEvent_t event, int number);
int MidiFileMetaEvent_getDataLength(MidiFileEvent_t event);
unsigned char *MidiFileMetaEvent_getData(MidiFileEvent_t event);
int MidiFileMetaEvent_setData(MidiFileEvent_t event, int data_length, unsigned char *data_buffer);

long MidiFileNoteEvent_getDurationTicks(MidiFileEvent_t event);
int MidiFileNoteEvent_setDurationTicks(MidiFileEvent_t event, long duration_ticks);
int MidiFileNoteEvent_getChannel(MidiFileEvent_t event);
int MidiFileNoteEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileNoteEvent_getNote(MidiFileEvent_t event);
int MidiFileNoteEvent_setNote(MidiFileEvent_t event, int note);
int MidiFileNoteEvent_getVelocity(MidiFileEvent_t event);
int MidiFileNoteEvent_setVelocity(MidiFileEvent_t event, int velocity);
int MidiFileNoteEvent_getEndVelocity(MidiFileEvent_t event);
int MidiFileNoteEvent_setEndVelocity(MidiFileEvent_t event, int end_velocity);

int MidiFileFineControlChangeEvent_getChannel(MidiFileEvent_t event);
int MidiFileFineControlChangeEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileFineControlChangeEvent_getCoarseNumber(MidiFileEvent_t event);
int MidiFileFineControlChangeEvent_setCoarseNumber(MidiFileEvent_t event, int coarse_number);
int MidiFileFineControlChangeEvent_getFineNumber(MidiFileEvent_t event);
int MidiFileFineControlChangeEvent_setFineNumber(MidiFileEvent_t event, int fine_number);
int MidiFileFineControlChangeEvent_getValue(MidiFileEvent_t event);
int MidiFileFineControlChangeEvent_setValue(MidiFileEvent_t event, int value);
int MidiFileFineControlChangeEvent_getCoarseValue(MidiFileEvent_t event);
int MidiFileFineControlChangeEvent_setCoarseValue(MidiFileEvent_t event, int coarse_value);
int MidiFileFineControlChangeEvent_getFineValue(MidiFileEvent_t event);
int MidiFileFineControlChangeEvent_setFineValue(MidiFileEvent_t event, int fine_value);

int MidiFileRpnEvent_getChannel(MidiFileEvent_t event);
int MidiFileRpnEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileRpnEvent_getNumber(MidiFileEvent_t event);
int MidiFileRpnEvent_setNumber(MidiFileEvent_t event, int number);
int MidiFileRpnEvent_getCoarseNumber(MidiFileEvent_t event);
int MidiFileRpnEvent_setCoarseNumber(MidiFileEvent_t event, int coarse_number);
int MidiFileRpnEvent_getFineNumber(MidiFileEvent_t event);
int MidiFileRpnEvent_setFineNumber(MidiFileEvent_t event, int fine_number);
int MidiFileRpnEvent_getValue(MidiFileEvent_t event);
int MidiFileRpnEvent_setValue(MidiFileEvent_t event, int value);
int MidiFileRpnEvent_getCoarseValue(MidiFileEvent_t event);
int MidiFileRpnEvent_setCoarseValue(MidiFileEvent_t event, int coarse_value);
int MidiFileRpnEvent_getFineValue(MidiFileEvent_t event);
int MidiFileRpnEvent_setFineValue(MidiFileEvent_t event, int fine_value);

int MidiFileNrpnEvent_getChannel(MidiFileEvent_t event);
int MidiFileNrpnEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileNrpnEvent_getNumber(MidiFileEvent_t event);
int MidiFileNrpnEvent_setNumber(MidiFileEvent_t event, int number);
int MidiFileNrpnEvent_getCoarseNumber(MidiFileEvent_t event);
int MidiFileNrpnEvent_setCoarseNumber(MidiFileEvent_t event, int coarse_number);
int MidiFileNrpnEvent_getFineNumber(MidiFileEvent_t event);
int MidiFileNrpnEvent_setFineNumber(MidiFileEvent_t event, int fine_number);
int MidiFileNrpnEvent_getValue(MidiFileEvent_t event);
int MidiFileNrpnEvent_setValue(MidiFileEvent_t event, int value);
int MidiFileNrpnEvent_getCoarseValue(MidiFileEvent_t event);
int MidiFileNrpnEvent_setCoarseValue(MidiFileEvent_t event, int coarse_value);
int MidiFileNrpnEvent_getFineValue(MidiFileEvent_t event);
int MidiFileNrpnEvent_setFineValue(MidiFileEvent_t event, int fine_value);

int MidiFileNoteStartEvent_getChannel(MidiFileEvent_t event);
int MidiFileNoteStartEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileNoteStartEvent_getNote(MidiFileEvent_t event);
int MidiFileNoteStartEvent_setNote(MidiFileEvent_t event, int note);
int MidiFileNoteStartEvent_getVelocity(MidiFileEvent_t event);
int MidiFileNoteStartEvent_setVelocity(MidiFileEvent_t event, int velocity);
MidiFileEvent_t MidiFileNoteStartEvent_getNoteEndEvent(MidiFileEvent_t event);

int MidiFileNoteEndEvent_getChannel(MidiFileEvent_t event);
int MidiFileNoteEndEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFileNoteEndEvent_getNote(MidiFileEvent_t event);
int MidiFileNoteEndEvent_setNote(MidiFileEvent_t event, int note);
int MidiFileNoteEndEvent_getVelocity(MidiFileEvent_t event);
int MidiFileNoteEndEvent_setVelocity(MidiFileEvent_t event, int velocity); /* caution:  will replace a note on event with a note off */
MidiFileEvent_t MidiFileNoteEndEvent_getNoteStartEvent(MidiFileEvent_t event);

int MidiFilePressureEvent_getChannel(MidiFileEvent_t event);
int MidiFilePressureEvent_setChannel(MidiFileEvent_t event, int channel);
int MidiFilePressureEvent_getNote(MidiFileEvent_t event); /* -1 for channel pressure */
int MidiFilePressureEvent_setNote(MidiFileEvent_t event, int note); /* will replace channel pressure with key pressure and vice versa as needed */
int MidiFilePressureEvent_getAmount(MidiFileEvent_t event);
int MidiFilePressureEvent_setAmount(MidiFileEvent_t event, int amount);

char *MidiFileTextEvent_getText(MidiFileEvent_t event);
int MidiFileTextEvent_setText(MidiFileEvent_t event, char *text);

char *MidiFileLyricEvent_getText(MidiFileEvent_t event);
int MidiFileLyricEvent_setText(MidiFileEvent_t event, char *text);

char *MidiFileMarkerEvent_getText(MidiFileEvent_t event);
int MidiFileMarkerEvent_setText(MidiFileEvent_t event, char *text);

char *MidiFilePortEvent_getName(MidiFileEvent_t event);
int MidiFilePortEvent_setName(MidiFileEvent_t event, char *name);

float MidiFileTempoEvent_getTempo(MidiFileEvent_t event);
int MidiFileTempoEvent_setTempo(MidiFileEvent_t event, float tempo);

int MidiFileTimeSignatureEvent_getNumerator(MidiFileEvent_t event);
int MidiFileTimeSignatureEvent_getDenominator(MidiFileEvent_t event);
int MidiFileTimeSignatureEvent_setTimeSignature(MidiFileEvent_t event, int numerator, int denominator);

int MidiFileKeySignatureEvent_getNumber(MidiFileEvent_t event);
int MidiFileKeySignatureEvent_isMinor(MidiFileEvent_t event);
int MidiFileKeySignatureEvent_setKeySignature(MidiFileEvent_t event, int number, int minor);

int MidiFileVoiceEvent_getChannel(MidiFileEvent_t event);
int MidiFileVoiceEvent_getDataLength(MidiFileEvent_t event);
unsigned long MidiFileVoiceEvent_getData(MidiFileEvent_t event);
int MidiFileVoiceEvent_setData(MidiFileEvent_t event, unsigned long data);

MidiFileMeasureBeat_t MidiFileMeasureBeat_new(void);
int MidiFileMeasureBeat_free(MidiFileMeasureBeat_t measure_beat);
long MidiFileMeasureBeat_getMeasure(MidiFileMeasureBeat_t measure_beat);
int MidiFileMeasureBeat_setMeasure(MidiFileMeasureBeat_t measure_beat, long measure);
float MidiFileMeasureBeat_getBeat(MidiFileMeasureBeat_t measure_beat);
int MidiFileMeasureBeat_setBeat(MidiFileMeasureBeat_t measure_beat, float beat);
char *MidiFileMeasureBeat_toString(MidiFileMeasureBeat_t measure_beat);
int MidiFileMeasureBeat_parse(MidiFileMeasureBeat_t measure_beat, char *string);

MidiFileMeasureBeatTick_t MidiFileMeasureBeatTick_new(void);
int MidiFileMeasureBeatTick_free(MidiFileMeasureBeatTick_t measure_beat_tick);
long MidiFileMeasureBeatTick_getMeasure(MidiFileMeasureBeatTick_t measure_beat_tick);
int MidiFileMeasureBeatTick_setMeasure(MidiFileMeasureBeatTick_t measure_beat_tick, long measure);
long MidiFileMeasureBeatTick_getBeat(MidiFileMeasureBeatTick_t measure_beat_tick);
int MidiFileMeasureBeatTick_setBeat(MidiFileMeasureBeatTick_t measure_beat_tick, long beat);
float MidiFileMeasureBeatTick_getTick(MidiFileMeasureBeatTick_t measure_beat_tick);
int MidiFileMeasureBeatTick_setTick(MidiFileMeasureBeatTick_t measure_beat_tick, float tick);
char *MidiFileMeasureBeatTick_toString(MidiFileMeasureBeatTick_t measure_beat_tick);
int MidiFileMeasureBeatTick_parse(MidiFileMeasureBeatTick_t measure_beat_tick, char *string);

MidiFileHourMinuteSecond_t MidiFileHourMinuteSecond_new(void);
int MidiFileHourMinuteSecond_free(MidiFileHourMinuteSecond_t hour_minute_second);
long MidiFileHourMinuteSecond_getHour(MidiFileHourMinuteSecond_t hour_minute_second);
int MidiFileHourMinuteSecond_setHour(MidiFileHourMinuteSecond_t hour_minute_second, long hour);
long MidiFileHourMinuteSecond_getMinute(MidiFileHourMinuteSecond_t hour_minute_second);
int MidiFileHourMinuteSecond_setMinute(MidiFileHourMinuteSecond_t hour_minute_second, long minute);
float MidiFileHourMinuteSecond_getSecond(MidiFileHourMinuteSecond_t hour_minute_second);
int MidiFileHourMinuteSecond_setSecond(MidiFileHourMinuteSecond_t hour_minute_second, float second);
char *MidiFileHourMinuteSecond_toString(MidiFileHourMinuteSecond_t hour_minute_second);
int MidiFileHourMinuteSecond_parse(MidiFileHourMinuteSecond_t hour_minute_second, char *string);

MidiFileHourMinuteSecondFrame_t MidiFileHourMinuteSecondFrame_new(void);
int MidiFileHourMinuteSecondFrame_free(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
long MidiFileHourMinuteSecondFrame_getHour(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
int MidiFileHourMinuteSecondFrame_setHour(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, long hour);
long MidiFileHourMinuteSecondFrame_getMinute(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
int MidiFileHourMinuteSecondFrame_setMinute(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, long minute);
long MidiFileHourMinuteSecondFrame_getSecond(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
int MidiFileHourMinuteSecondFrame_setSecond(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, long second);
float MidiFileHourMinuteSecondFrame_getFrame(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
int MidiFileHourMinuteSecondFrame_setFrame(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, float frame);
char *MidiFileHourMinuteSecondFrame_toString(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame);
int MidiFileHourMinuteSecondFrame_parse(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, char *string);

#ifdef __cplusplus
}
#endif

#endif
