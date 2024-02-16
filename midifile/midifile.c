
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "midifile.h"

/*
 * Data Types
 */

struct MidiFile
{
	int file_format;
	MidiFileDivisionType_t division_type;
	int resolution;
	float number_of_frames_per_second;
	int number_of_tracks;
	struct MidiFileTrack *first_track;
	struct MidiFileTrack *last_track;
	struct MidiFileEvent *first_event;
	struct MidiFileEvent *last_event;
	struct MidiFileMeasureBeat *measure_beat;
	struct MidiFileMeasureBeatTick *measure_beat_tick;
	struct MidiFileHourMinuteSecond *hour_minute_second;
	struct MidiFileHourMinuteSecondFrame *hour_minute_second_frame;
	struct MidiFileEvent *event_iterator_current;
	struct MidiFileEvent *event_iterator_next;
};

struct MidiFileTrack
{
	struct MidiFile *midi_file;
	int number;
	long end_tick;
	struct MidiFileTrack *previous_track;
	struct MidiFileTrack *next_track;
	struct MidiFileEvent *first_event;
	struct MidiFileEvent *last_event;
	struct MidiFileEvent *event_iterator_current;
	struct MidiFileEvent *event_iterator_next;
};

struct MidiFileEvent
{
	struct MidiFileTrack *track;
	struct MidiFileEvent *previous_event_in_track;
	struct MidiFileEvent *next_event_in_track;
	struct MidiFileEvent *previous_event_in_file;
	struct MidiFileEvent *next_event_in_file;
	long tick;
	MidiFileEventType_t type;

	union
	{
		struct
		{
			int channel;
			int note;
			int velocity;
		}
		note_off;

		struct
		{
			int channel;
			int note;
			int velocity;
		}
		note_on;

		struct
		{
			int channel;
			int note;
			int amount;
		}
		key_pressure;

		struct
		{
			int channel;
			int number;
			int value;
		}
		control_change;

		struct
		{
			int channel;
			int number;
		}
		program_change;

		struct
		{
			int channel;
			int amount;
		}
		channel_pressure;

		struct
		{
			int channel;
			int value;
		}
		pitch_wheel;

		struct
		{
			int data_length;
			unsigned char *data_buffer;
		}
		sysex;

		struct
		{
			int number;
			int data_length;
			unsigned char *data_buffer;
		}
		meta;

		struct
		{
			long duration_ticks;
			int channel;
			int note;
			int velocity;
			int end_velocity;
		}
		note;

		struct
		{
			int channel;
			int coarse_number;
			int value;
		}
		fine_control_change;

		struct
		{
			int channel;
			int number;
			int value;
		}
		rpn;

		struct
		{
			int channel;
			int number;
			int value;
		}
		nrpn;
	}
	u;

	int should_be_visited;
	int is_selected;
};

struct MidiFileMeasureBeat
{
	long measure;
	float beat;
	char string[32];
};

struct MidiFileMeasureBeatTick
{
	long measure;
	long beat;
	float tick;
	char string[32];
};

struct MidiFileHourMinuteSecond
{
	long hour;
	long minute;
	float second;
	char string[32];
};

struct MidiFileHourMinuteSecondFrame
{
	long hour;
	long minute;
	long second;
	float frame;
	char string[32];
};

typedef struct MidiFileIO *MidiFileIO_t;

typedef enum 
{
	MIDI_FILE_IO_TYPE_INVALID = -1,
	MIDI_FILE_IO_TYPE_FILE,
	MIDI_FILE_IO_TYPE_BUFFER
}
MidiFileIOType_t;

struct MidiFileIO
{
	MidiFileIOType_t type;

	union
	{
		struct
		{
			FILE *file;
		}
		file;

		struct
		{
			long offset;
			unsigned char *buffer;
		}
		buffer;
	}
	u;
};

/*
 * Helpers
 */

static MidiFileIO_t MidiFileIO_newFromFile(FILE *file)
{
	MidiFileIO_t io = (MidiFileIO_t)(malloc(sizeof(struct MidiFileIO)));
	io->type = MIDI_FILE_IO_TYPE_FILE;
	io->u.file.file = file;
	return io;
}

static MidiFileIO_t MidiFileIO_newFromBuffer(unsigned char *buffer)
{
	MidiFileIO_t io = (MidiFileIO_t)(malloc(sizeof(struct MidiFileIO)));
	io->type = MIDI_FILE_IO_TYPE_BUFFER;
	io->u.buffer.offset = 0;
	io->u.buffer.buffer = buffer;
	return io;
}

static void MidiFileIO_free(MidiFileIO_t io)
{
	free(io);
}

static int MidiFileIO_getc(MidiFileIO_t io)
{
	switch (io->type)
	{
		case MIDI_FILE_IO_TYPE_FILE:
		{
			return fgetc(io->u.file.file);
		}
		case MIDI_FILE_IO_TYPE_BUFFER:
		{
			long offset = io->u.buffer.offset++;

			if (io->u.buffer.buffer == NULL)
			{
				return 0;
			}
			else
			{
				return io->u.buffer.buffer[offset];
			}
		}
		default:
		{
			return -1;
		}
	}
}

static int MidiFileIO_putc(MidiFileIO_t io, int c)
{
	switch (io->type)
	{
		case MIDI_FILE_IO_TYPE_FILE:
		{
			return fputc(c, io->u.file.file);
		}
		case MIDI_FILE_IO_TYPE_BUFFER:
		{
			long offset = io->u.buffer.offset++;
			if (io->u.buffer.buffer != NULL) io->u.buffer.buffer[offset] = (unsigned char)(c);
			return 0;
		}
		default:
		{
			return -1;
		}
	}
}

static size_t MidiFileIO_read(MidiFileIO_t io, size_t length, unsigned char *buffer)
{
	switch (io->type)
	{
		case MIDI_FILE_IO_TYPE_FILE:
		{
			return fread(buffer, 1, length, io->u.file.file);
		}
		case MIDI_FILE_IO_TYPE_BUFFER:
		{
			if (io->u.buffer.buffer == NULL)
			{
				memset(buffer, 0, length);
			}
			else
			{
				memcpy(buffer, io->u.buffer.buffer + io->u.buffer.offset, length);
			}

			io->u.buffer.offset += length;
			return length;
		}
		default:
		{
			return 0;
		}
	}
}

static size_t MidiFileIO_write(MidiFileIO_t io, size_t length, unsigned char *buffer)
{
	switch (io->type)
	{
		case MIDI_FILE_IO_TYPE_FILE:
		{
			return fwrite(buffer, 1, length, io->u.file.file);
		}
		case MIDI_FILE_IO_TYPE_BUFFER:
		{
			if (io->u.buffer.buffer != NULL) memcpy(io->u.buffer.buffer + io->u.buffer.offset, buffer, length);
			io->u.buffer.offset += length;
			return length;
		}
		default:
		{
			return 0;
		}
	}
}

static long MidiFileIO_tell(MidiFileIO_t io)
{
	switch (io->type)
	{
		case MIDI_FILE_IO_TYPE_FILE:
		{
			return ftell(io->u.file.file);
		}
		case MIDI_FILE_IO_TYPE_BUFFER:
		{
			return io->u.buffer.offset;
		}
		default:
		{
			return -1;
		}
	}
}

static int MidiFileIO_seek(MidiFileIO_t io, long offset, int whence)
{
	switch (io->type)
	{
		case MIDI_FILE_IO_TYPE_FILE:
		{
			return fseek(io->u.file.file, offset, whence);
		}
		case MIDI_FILE_IO_TYPE_BUFFER:
		{
			switch (whence)
			{
				case SEEK_SET:
				{
					io->u.buffer.offset = offset;
					return 0;
				}
				case SEEK_CUR:
				{
					io->u.buffer.offset += offset;
					return 0;
				}
				default:
				{
					return -1;
				}
			}
		}
		default:
		{
			return -1;
		}
	}
}

static unsigned short interpret_uint16(unsigned char *buffer)
{
	return ((unsigned short)(buffer[0]) << 8) | (unsigned short)(buffer[1]);
}

static unsigned short read_uint16(MidiFileIO_t io)
{
	unsigned char buffer[2];
	MidiFileIO_read(io, 2, buffer);
	return interpret_uint16(buffer);
}

static void write_uint16(MidiFileIO_t io, unsigned short value)
{
	unsigned char buffer[2];
	buffer[0] = (unsigned char)((value >> 8) & 0xFF);
	buffer[1] = (unsigned char)(value & 0xFF);
	MidiFileIO_write(io, 2, buffer);
}

static unsigned long interpret_uint32(unsigned char *buffer)
{
	return ((unsigned long)(buffer[0]) << 24) | ((unsigned long)(buffer[1]) << 16) | ((unsigned long)(buffer[2]) << 8) | (unsigned long)(buffer[3]);
}

static unsigned long read_uint32(MidiFileIO_t io)
{
	unsigned char buffer[4];
	MidiFileIO_read(io, 4, buffer);
	return interpret_uint32(buffer);
}

static void write_uint32(MidiFileIO_t io, unsigned long value)
{
	unsigned char buffer[4];
	buffer[0] = (unsigned char)(value >> 24);
	buffer[1] = (unsigned char)((value >> 16) & 0xFF);
	buffer[2] = (unsigned char)((value >> 8) & 0xFF);
	buffer[3] = (unsigned char)(value & 0xFF);
	MidiFileIO_write(io, 4, buffer);
}

static unsigned long read_variable_length_quantity(MidiFileIO_t io)
{
	unsigned char b;
	unsigned long value = 0;

	do
	{
		b = MidiFileIO_getc(io);
		value = (value << 7) | (b & 0x7F);
	}
	while ((b & 0x80) == 0x80);

	return value;
}

static void write_variable_length_quantity(MidiFileIO_t io, unsigned long value)
{
	unsigned char buffer[4];
	int offset = 3;

	while (1)
	{
		buffer[offset] = (unsigned char)(value & 0x7F);
		if (offset < 3) buffer[offset] |= 0x80;
		value >>= 7;
		if ((value == 0) || (offset == 0)) break;
		offset--;
	}

	MidiFileIO_write(io, 4 - offset, buffer + offset);
}

static void add_event_before(MidiFileEvent_t new_event, MidiFileEvent_t next_event)
{
	/* Add in proper sorted order.  Search forwards to optimize for inserting. */

	MidiFileEvent_t event;

	for (event = new_event->track->first_event; (event != NULL) && (event->tick < new_event->tick); event = event->next_event_in_track) {}

	if ((event != NULL) && (next_event != NULL) && (event->track == next_event->track) && (event->tick == next_event->tick))
	{
		while (event != next_event) event = event->next_event_in_track;
	}

	new_event->next_event_in_track = event;

	if (event == NULL)
	{
		new_event->previous_event_in_track = new_event->track->last_event;
		new_event->track->last_event = new_event;
	}
	else
	{
		new_event->previous_event_in_track = event->previous_event_in_track;
		event->previous_event_in_track = new_event;
	}

	if (new_event->previous_event_in_track == NULL)
	{
		new_event->track->first_event = new_event;
	}
	else
	{
		new_event->previous_event_in_track->next_event_in_track = new_event;
	}

	for (event = new_event->track->midi_file->first_event; (event != NULL) && (new_event->tick > event->tick); event = event->next_event_in_file) {}

	new_event->next_event_in_file = event;

	if (event == NULL)
	{
		new_event->previous_event_in_file = new_event->track->midi_file->last_event;
		new_event->track->midi_file->last_event = new_event;
	}
	else
	{
		new_event->previous_event_in_file = event->previous_event_in_file;
		event->previous_event_in_file = new_event;
	}

	if (new_event->previous_event_in_file == NULL)
	{
		new_event->track->midi_file->first_event = new_event;
	}
	else
	{
		new_event->previous_event_in_file->next_event_in_file = new_event;
	}

	if (new_event->tick > new_event->track->end_tick) new_event->track->end_tick = new_event->tick;
}

static void add_event_after(MidiFileEvent_t new_event, MidiFileEvent_t previous_event)
{
	/* Add in proper sorted order.  Search backwards to optimize for appending. */

	MidiFileEvent_t event;

	for (event = new_event->track->last_event; (event != NULL) && (event->tick > new_event->tick); event = event->previous_event_in_track) {}

	if ((event != NULL) && (previous_event != NULL) && (event->track == previous_event->track) && (event->tick == previous_event->tick))
	{
		while (event != previous_event) event = event->previous_event_in_track;
	}

	new_event->previous_event_in_track = event;

	if (event == NULL)
	{
		new_event->next_event_in_track = new_event->track->first_event;
		new_event->track->first_event = new_event;
	}
	else
	{
		new_event->next_event_in_track = event->next_event_in_track;
		event->next_event_in_track = new_event;
	}

	if (new_event->next_event_in_track == NULL)
	{
		new_event->track->last_event = new_event;
	}
	else
	{
		new_event->next_event_in_track->previous_event_in_track = new_event;
	}

	for (event = new_event->track->midi_file->last_event; (event != NULL) && (new_event->tick < event->tick); event = event->previous_event_in_file) {}

	new_event->previous_event_in_file = event;

	if (event == NULL)
	{
		new_event->next_event_in_file = new_event->track->midi_file->first_event;
		new_event->track->midi_file->first_event = new_event;
	}
	else
	{
		new_event->next_event_in_file = event->next_event_in_file;
		event->next_event_in_file = new_event;
	}

	if (new_event->next_event_in_file == NULL)
	{
		new_event->track->midi_file->last_event = new_event;
	}
	else
	{
		new_event->next_event_in_file->previous_event_in_file = new_event;
	}

	if (new_event->tick > new_event->track->end_tick) new_event->track->end_tick = new_event->tick;
}

static void add_event(MidiFileEvent_t new_event)
{
	add_event_after(new_event, NULL);
}

static void remove_event(MidiFileEvent_t event)
{
	if (event->previous_event_in_track == NULL)
	{
		event->track->first_event = event->next_event_in_track;
	}
	else
	{
		event->previous_event_in_track->next_event_in_track = event->next_event_in_track;
	}

	if (event->next_event_in_track == NULL)
	{
		event->track->last_event = event->previous_event_in_track;
	}
	else
	{
		event->next_event_in_track->previous_event_in_track = event->previous_event_in_track;
	}

	if (event->previous_event_in_file == NULL)
	{
		event->track->midi_file->first_event = event->next_event_in_file;
	}
	else
	{
		event->previous_event_in_file->next_event_in_file = event->next_event_in_file;
	}

	if (event->next_event_in_file == NULL)
	{
		event->track->midi_file->last_event = event->previous_event_in_file;
	}
	else
	{
		event->next_event_in_file->previous_event_in_file = event->previous_event_in_file;
	}
}

static MidiFile_t load_midi_file(MidiFileIO_t io)
{
	MidiFile_t midi_file;
	unsigned char chunk_id[4], division_type_and_resolution[4];
	long chunk_size, chunk_start;
	int file_format, number_of_tracks, number_of_tracks_read = 0;

	MidiFileIO_read(io, 4, chunk_id);
	chunk_size = read_uint32(io);
	chunk_start = MidiFileIO_tell(io);

	/* check for the RMID variation on SMF */

	if (memcmp(chunk_id, "RIFF", 4) == 0)
	{
		MidiFileIO_read(io, 4, chunk_id); /* technically this one is a type id rather than a chunk id, but we'll reuse the buffer anyway */

		if (memcmp(chunk_id, "RMID", 4) != 0)
		{
			return NULL;
		}

		MidiFileIO_read(io, 4, chunk_id);
		chunk_size = read_uint32(io);

		if (memcmp(chunk_id, "data", 4) != 0)
		{
			return NULL;
		}

		MidiFileIO_read(io, 4, chunk_id);
		chunk_size = read_uint32(io);
		chunk_start = MidiFileIO_tell(io);
	}

	if (memcmp(chunk_id, "MThd", 4) != 0)
	{
		return NULL;
	}

	file_format = read_uint16(io);
	number_of_tracks = read_uint16(io);
	MidiFileIO_read(io, 2, division_type_and_resolution);

	switch ((signed char)(division_type_and_resolution[0]))
	{
		case -24:
		{
			midi_file = MidiFile_new(file_format, MIDI_FILE_DIVISION_TYPE_SMPTE24, division_type_and_resolution[1]);
			break;
		}
		case -25:
		{
			midi_file = MidiFile_new(file_format, MIDI_FILE_DIVISION_TYPE_SMPTE25, division_type_and_resolution[1]);
			break;
		}
		case -29:
		{
			midi_file = MidiFile_new(file_format, MIDI_FILE_DIVISION_TYPE_SMPTE30DROP, division_type_and_resolution[1]);
			break;
		}
		case -30:
		{
			midi_file = MidiFile_new(file_format, MIDI_FILE_DIVISION_TYPE_SMPTE30, division_type_and_resolution[1]);
			break;
		}
		default:
		{
			midi_file = MidiFile_new(file_format, MIDI_FILE_DIVISION_TYPE_PPQ, interpret_uint16(division_type_and_resolution));
			break;
		}
	}

	/* forwards compatibility:  skip over any extra header data */
	MidiFileIO_seek(io, chunk_start + chunk_size, SEEK_SET);

	while (number_of_tracks_read < number_of_tracks)
	{
		MidiFileIO_read(io, 4, chunk_id);
		chunk_size = read_uint32(io);
		chunk_start = MidiFileIO_tell(io);

		if (memcmp(chunk_id, "MTrk", 4) == 0)
		{
			MidiFileTrack_t track = MidiFile_createTrack(midi_file);
			long tick, previous_tick = 0;
			unsigned char status, running_status = 0;
			int at_end_of_track = 0;

			while ((MidiFileIO_tell(io) < chunk_start + chunk_size) && !at_end_of_track)
			{
				tick = read_variable_length_quantity(io) + previous_tick;
				previous_tick = tick;

				status = MidiFileIO_getc(io);

				if ((status & 0x80) == 0x00)
				{
					status = running_status;
					MidiFileIO_seek(io, -1, SEEK_CUR);
				}
				else
				{
					running_status = status;
				}

				switch (status & 0xF0)
				{
					case 0x80:
					{
						int channel = status & 0x0F;
						int note = MidiFileIO_getc(io);
						int velocity = MidiFileIO_getc(io);
						MidiFileTrack_createNoteOffEvent(track, tick, channel, note, velocity);
						break;
					}
					case 0x90:
					{
						int channel = status & 0x0F;
						int note = MidiFileIO_getc(io);
						int velocity = MidiFileIO_getc(io);
						MidiFileTrack_createNoteOnEvent(track, tick, channel, note, velocity);
						break;
					}
					case 0xA0:
					{
						int channel = status & 0x0F;
						int note = MidiFileIO_getc(io);
						int amount = MidiFileIO_getc(io);
						MidiFileTrack_createKeyPressureEvent(track, tick, channel, note, amount);
						break;
					}
					case 0xB0:
					{
						int channel = status & 0x0F;
						int number = MidiFileIO_getc(io);
						int value = MidiFileIO_getc(io);
						MidiFileTrack_createControlChangeEvent(track, tick, channel, number, value);
						break;
					}
					case 0xC0:
					{
						int channel = status & 0x0F;
						int number = MidiFileIO_getc(io);
						MidiFileTrack_createProgramChangeEvent(track, tick, channel, number);
						break;
					}
					case 0xD0:
					{
						int channel = status & 0x0F;
						int amount = MidiFileIO_getc(io);
						MidiFileTrack_createChannelPressureEvent(track, tick, channel, amount);
						break;
					}
					case 0xE0:
					{
						int channel = status & 0x0F;
						int value = MidiFileIO_getc(io);
						value = (MidiFileIO_getc(io) << 7) | value;
						MidiFileTrack_createPitchWheelEvent(track, tick, channel, value);
						break;
					}
					case 0xF0:
					{
						switch (status)
						{
							case 0xF0:
							case 0xF7:
							{
								int data_length = read_variable_length_quantity(io) + 1;
								unsigned char *data_buffer = malloc(data_length);
								data_buffer[0] = status;
								MidiFileIO_read(io, data_length - 1, data_buffer + 1);
								MidiFileTrack_createSysexEvent(track, tick, data_length, data_buffer);
								free(data_buffer);
								break;
							}
							case 0xFF:
							{
								int number = MidiFileIO_getc(io);
								int data_length = read_variable_length_quantity(io);
								unsigned char *data_buffer = malloc(data_length);
								MidiFileIO_read(io, data_length, data_buffer);

								if (number == 0x2F)
								{
									MidiFileTrack_setEndTick(track, tick);
									at_end_of_track = 1;
								}
								else
								{
									MidiFileTrack_createMetaEvent(track, tick, number, data_length, data_buffer);
								}

								free(data_buffer);
								break;
							}
						}

						break;
					}
				}
			}

			number_of_tracks_read++;
		}

		/* forwards compatibility:  skip over any unrecognized chunks, or extra data at the end of tracks */
		MidiFileIO_seek(io, chunk_start + chunk_size, SEEK_SET);
	}

	return midi_file;
}

static void save_midi_file(MidiFile_t midi_file, MidiFileIO_t io)
{
	MidiFileTrack_t track;

	MidiFileIO_write(io, 4, (unsigned char *)("MThd"));
	write_uint32(io, 6);
	write_uint16(io, (unsigned short)(MidiFile_getFileFormat(midi_file)));
	write_uint16(io, (unsigned short)(MidiFile_getNumberOfTracks(midi_file)));

	switch (MidiFile_getDivisionType(midi_file))
	{
		case MIDI_FILE_DIVISION_TYPE_PPQ:
		{
			write_uint16(io, (unsigned short)(MidiFile_getResolution(midi_file)));
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			MidiFileIO_putc(io, -24);
			MidiFileIO_putc(io, MidiFile_getResolution(midi_file));
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			MidiFileIO_putc(io, -25);
			MidiFileIO_putc(io, MidiFile_getResolution(midi_file));
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			MidiFileIO_putc(io, -29);
			MidiFileIO_putc(io, MidiFile_getResolution(midi_file));
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			MidiFileIO_putc(io, -30);
			MidiFileIO_putc(io, MidiFile_getResolution(midi_file));
			break;
		}
		default:
		{
			break;
		}
	}

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		MidiFileEvent_t event;
		long track_size_offset, track_start_offset, track_end_offset, tick, previous_tick;

		MidiFileIO_write(io, 4, (unsigned char *)("MTrk"));

		track_size_offset = MidiFileIO_tell(io);
		write_uint32(io, 0);

		track_start_offset = MidiFileIO_tell(io);

		previous_tick = 0;

		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			tick = MidiFileEvent_getTick(event);
			write_variable_length_quantity(io, tick - previous_tick);

			switch (MidiFileEvent_getType(event))
			{
				case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
				{
					MidiFileIO_putc(io, 0x80 | (MidiFileNoteOffEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, MidiFileNoteOffEvent_getNote(event) & 0x7F);
					MidiFileIO_putc(io, MidiFileNoteOffEvent_getVelocity(event) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_NOTE_ON:
				{
					MidiFileIO_putc(io, 0x90 | (MidiFileNoteOnEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, MidiFileNoteOnEvent_getNote(event) & 0x7F);
					MidiFileIO_putc(io, MidiFileNoteOnEvent_getVelocity(event) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
				{
					MidiFileIO_putc(io, 0xA0 | (MidiFileKeyPressureEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, MidiFileKeyPressureEvent_getNote(event) & 0x7F);
					MidiFileIO_putc(io, MidiFileKeyPressureEvent_getAmount(event) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE:
				{
					MidiFileIO_putc(io, 0xB0 | (MidiFileControlChangeEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, MidiFileControlChangeEvent_getNumber(event) & 0x7F);
					MidiFileIO_putc(io, MidiFileControlChangeEvent_getValue(event) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE:
				{
					MidiFileIO_putc(io, 0xC0 | (MidiFileProgramChangeEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, MidiFileProgramChangeEvent_getNumber(event) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
				{
					MidiFileIO_putc(io, 0xD0 | (MidiFileChannelPressureEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, MidiFileChannelPressureEvent_getAmount(event) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_PITCH_WHEEL:
				{
					int value = MidiFilePitchWheelEvent_getValue(event);
					MidiFileIO_putc(io, 0xE0 | (MidiFilePitchWheelEvent_getChannel(event) & 0x0F));
					MidiFileIO_putc(io, value & 0x7F);
					MidiFileIO_putc(io, (value >> 7) & 0x7F);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_SYSEX:
				{
					int data_length = MidiFileSysexEvent_getDataLength(event);
					unsigned char *data = MidiFileSysexEvent_getData(event);
					MidiFileIO_putc(io, data[0]);
					write_variable_length_quantity(io, data_length - 1);
					MidiFileIO_write(io, data_length - 1, data + 1);
					break;
				}
				case MIDI_FILE_EVENT_TYPE_META:
				{
					int data_length = MidiFileMetaEvent_getDataLength(event);
					unsigned char *data = MidiFileMetaEvent_getData(event);
					MidiFileIO_putc(io, 0xFF);
					MidiFileIO_putc(io, MidiFileMetaEvent_getNumber(event) & 0x7F);
					write_variable_length_quantity(io, data_length);
					MidiFileIO_write(io, data_length, data);
					break;
				}
				default:
				{
					break;
				}
			}

			previous_tick = tick;
		}

		write_variable_length_quantity(io, MidiFileTrack_getEndTick(track) - previous_tick);
		MidiFileIO_write(io, 3, (unsigned char *)("\xFF\x2F\x00"));

		track_end_offset = MidiFileIO_tell(io);

		MidiFileIO_seek(io, track_size_offset, SEEK_SET);
		write_uint32(io, track_end_offset - track_start_offset);

		MidiFileIO_seek(io, track_end_offset, SEEK_SET);
	}
}

/*
 * Public API
 */

MidiFile_t MidiFile_load(char *filename)
{
	FILE *in;
	MidiFileIO_t io;
	MidiFile_t midi_file;

	if ((filename == NULL) || ((in = fopen(filename, "rb")) == NULL)) return NULL;

	io = MidiFileIO_newFromFile(in);
	midi_file = load_midi_file(io);
	MidiFileIO_free(io);
	fclose(in);
	return midi_file;
}

int MidiFile_save(MidiFile_t midi_file, const char* filename)
{
	FILE *out;
	MidiFileIO_t io;

	if ((midi_file == NULL) || (filename == NULL) || ((out = fopen(filename, "wb")) == NULL)) return -1;

	io = MidiFileIO_newFromFile(out);
	save_midi_file(midi_file, io);
	MidiFileIO_free(io);
	fclose(out);
	return 0;
}

MidiFile_t MidiFile_loadFromBuffer(unsigned char *buffer)
{
	MidiFileIO_t io;
	MidiFile_t midi_file;

	if (buffer == NULL) return NULL;

	io = MidiFileIO_newFromBuffer(buffer);
	midi_file = load_midi_file(io);
	MidiFileIO_free(io);
	return midi_file;
}

int MidiFile_saveToBuffer(MidiFile_t midi_file, unsigned char *buffer)
{
	MidiFileIO_t io;

	if ((midi_file == NULL) || (buffer == NULL)) return -1;

	io = MidiFileIO_newFromBuffer(buffer);
	save_midi_file(midi_file, io);
	MidiFileIO_free(io);
	return 0;
}

int MidiFile_getFileSize(MidiFile_t midi_file)
{
	MidiFileIO_t io;
	int file_size;

	if (midi_file == NULL) return -1;

	io = MidiFileIO_newFromBuffer(NULL);
	save_midi_file(midi_file, io);
	file_size = (int)(MidiFileIO_tell(io));
	MidiFileIO_free(io);
	return file_size;
}

MidiFile_t MidiFile_new(int file_format, MidiFileDivisionType_t division_type, int resolution)
{
	MidiFile_t midi_file = (MidiFile_t)(malloc(sizeof(struct MidiFile)));
	midi_file->file_format = file_format;
	midi_file->division_type = division_type;
	midi_file->resolution = resolution;

	switch (division_type)
	{
		case MIDI_FILE_DIVISION_TYPE_PPQ:
		{
			midi_file->number_of_frames_per_second = 30.0;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			midi_file->number_of_frames_per_second = 24.0;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			midi_file->number_of_frames_per_second = 25.0;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			midi_file->number_of_frames_per_second = 29.97;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			midi_file->number_of_frames_per_second = 30.0;
			break;
		}
		default:
		{
			midi_file->number_of_frames_per_second = 30.0;
			break;
		}
	}

	midi_file->number_of_tracks = 0;
	midi_file->first_track = NULL;
	midi_file->last_track = NULL;
	midi_file->first_event = NULL;
	midi_file->last_event = NULL;
	midi_file->measure_beat = MidiFileMeasureBeat_new();
	midi_file->measure_beat_tick = MidiFileMeasureBeatTick_new();
	midi_file->hour_minute_second = MidiFileHourMinuteSecond_new();
	midi_file->hour_minute_second_frame = MidiFileHourMinuteSecondFrame_new();
	midi_file->event_iterator_current = NULL;
	midi_file->event_iterator_next = NULL;
	return midi_file;
}

MidiFile_t MidiFile_newFromTemplate(MidiFile_t template_midi_file)
{
	if (template_midi_file == NULL) return MidiFile_new(1, MIDI_FILE_DIVISION_TYPE_PPQ, 960);
	return MidiFile_new(MidiFile_getFileFormat(template_midi_file), MidiFile_getDivisionType(template_midi_file), MidiFile_getResolution(template_midi_file));
}

int MidiFile_free(MidiFile_t midi_file)
{
	MidiFileTrack_t track, next_track;

	if (midi_file == NULL) return -1;
	MidiFileHourMinuteSecondFrame_free(midi_file->hour_minute_second_frame);
	MidiFileHourMinuteSecond_free(midi_file->hour_minute_second);
	MidiFileMeasureBeatTick_free(midi_file->measure_beat_tick);
	MidiFileMeasureBeat_free(midi_file->measure_beat);

	for (track = midi_file->first_track; track != NULL; track = next_track)
	{
		next_track = track->next_track;
		MidiFileTrack_delete(track);
	}

	free(midi_file);
	return 0;
}

int MidiFile_getFileFormat(MidiFile_t midi_file)
{
	if (midi_file == NULL) return -1;
	return midi_file->file_format;
}

int MidiFile_setFileFormat(MidiFile_t midi_file, int file_format)
{
	if (midi_file == NULL) return -1;
	midi_file->file_format = file_format;
	return 0;
}

MidiFileDivisionType_t MidiFile_getDivisionType(MidiFile_t midi_file)
{
	if (midi_file == NULL) return MIDI_FILE_DIVISION_TYPE_INVALID;
	return midi_file->division_type;
}

int MidiFile_setDivisionType(MidiFile_t midi_file, MidiFileDivisionType_t division_type)
{
	if (midi_file == NULL) return -1;
	midi_file->division_type = division_type;

	switch (division_type)
	{
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			midi_file->number_of_frames_per_second = 24.0;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			midi_file->number_of_frames_per_second = 25.0;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			midi_file->number_of_frames_per_second = 29.97;
			break;
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			midi_file->number_of_frames_per_second = 30.0;
			break;
		}
		default:
		{
			break;
		}
	}

	return 0;
}

int MidiFile_getResolution(MidiFile_t midi_file)
{
	if (midi_file == NULL) return -1;
	return midi_file->resolution;
}

int MidiFile_setResolution(MidiFile_t midi_file, int resolution)
{
	if (midi_file == NULL) return -1;
	midi_file->resolution = resolution;
	return 0;
}

float MidiFile_getNumberOfFramesPerSecond(MidiFile_t midi_file)
{
	if (midi_file == NULL) return -1;
	return midi_file->number_of_frames_per_second;
}

int MidiFile_setNumberOfFramesPerSecond(MidiFile_t midi_file, float number_of_frames_per_second)
{
	if (midi_file == NULL) return -1;
	midi_file->number_of_frames_per_second = number_of_frames_per_second;
	return 0;
}

MidiFileTrack_t MidiFile_createTrack(MidiFile_t midi_file)
{
	MidiFileTrack_t new_track;

	if (midi_file == NULL) return NULL;

	new_track = (MidiFileTrack_t)(malloc(sizeof(struct MidiFileTrack)));
	new_track->midi_file = midi_file;
	new_track->number = midi_file->number_of_tracks;
	new_track->end_tick = 0;
	new_track->previous_track = midi_file->last_track;
	new_track->next_track = NULL;
	midi_file->last_track = new_track;

	if (new_track->previous_track == NULL)
	{
		midi_file->first_track = new_track;
	}
	else
	{
		new_track->previous_track->next_track = new_track;
	}

	(midi_file->number_of_tracks)++;

	new_track->first_event = NULL;
	new_track->last_event = NULL;
	new_track->event_iterator_current = NULL;
	new_track->event_iterator_next = NULL;

	return new_track;
}

int MidiFile_getNumberOfTracks(MidiFile_t midi_file)
{
	if (midi_file == NULL) return -1;
	return midi_file->number_of_tracks;
}

MidiFileTrack_t MidiFile_getTrackByNumber(MidiFile_t midi_file, int number, int create)
{
	int current_track_number;
	MidiFileTrack_t track = NULL;

	for (current_track_number = 0; current_track_number <= number; current_track_number++)
	{
		if (track == NULL)
		{
			track = MidiFile_getFirstTrack(midi_file);
		}
		else
		{
			track = MidiFileTrack_getNextTrack(track);
		}

		if ((track == NULL) && create)
		{
			track = MidiFile_createTrack(midi_file);
		}
	}

	return track;
}

MidiFileTrack_t MidiFile_getFirstTrack(MidiFile_t midi_file)
{
	if (midi_file == NULL) return NULL;
	return midi_file->first_track;
}

MidiFileTrack_t MidiFile_getLastTrack(MidiFile_t midi_file)
{
	if (midi_file == NULL) return NULL;
	return midi_file->last_track;
}

MidiFileEvent_t MidiFile_getFirstEvent(MidiFile_t midi_file)
{
	if (midi_file == NULL) return NULL;
	return midi_file->first_event;
}

MidiFileEvent_t MidiFile_getLastEvent(MidiFile_t midi_file)
{
	if (midi_file == NULL) return NULL;
	return midi_file->last_event;
}

MidiFileEvent_t MidiFile_iterateEvents(MidiFile_t midi_file)
{
	if (midi_file == NULL) return NULL;

	if (midi_file->event_iterator_current == NULL)
	{
		for (midi_file->event_iterator_current = MidiFile_getFirstEvent(midi_file); midi_file->event_iterator_current != NULL; midi_file->event_iterator_current = MidiFileEvent_getNextEventInFile(midi_file->event_iterator_current)) midi_file->event_iterator_current->should_be_visited = 1;
		midi_file->event_iterator_current = MidiFile_getFirstEvent(midi_file);
	}
	else
	{
		midi_file->event_iterator_current = midi_file->event_iterator_next;
	}

	while ((midi_file->event_iterator_current != NULL) && !(midi_file->event_iterator_current->should_be_visited))
	{
		midi_file->event_iterator_current = MidiFileEvent_getNextEventInFile(midi_file->event_iterator_current);
	}

	if (midi_file->event_iterator_current == NULL)
	{
		midi_file->event_iterator_next = NULL;
	}
	else
	{
		midi_file->event_iterator_current->should_be_visited = 0;
		midi_file->event_iterator_next = MidiFileEvent_getNextEventInFile(midi_file->event_iterator_current);
	}

	return midi_file->event_iterator_current;
}

int MidiFile_visitEvents(MidiFile_t midi_file, MidiFileEventVisitorCallback_t visitor_callback, void *user_data)
{
	MidiFileEvent_t event;

	if ((midi_file == NULL) || (visitor_callback == NULL)) return -1;

	for (event = MidiFile_iterateEvents(midi_file); event != NULL; event = MidiFile_iterateEvents(midi_file))
	{
		(*visitor_callback)(event, user_data);
	}

	return 0;
}

int MidiFile_convertSelectionFlagsToTextEvents(MidiFile_t midi_file, char *label)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event;

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isSelected(event))
			{
				MidiFileEvent_setNextEvent(MidiFileTrack_createTextEvent(MidiFileEvent_getTrack(event), MidiFileEvent_getTick(event), label), event);
				MidiFileEvent_setSelected(event, 0);
			}
		}
	}

	return 0;
}

int MidiFile_convertTextEventsToSelectionFlags(MidiFile_t midi_file, char *label)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event, next_event;

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = next_event)
		{
			next_event = MidiFileEvent_getNextEventInTrack(event);

			if (MidiFileEvent_isTextEvent(event) && (strcmp(MidiFileTextEvent_getText(event), label) == 0))
			{
				MidiFileEvent_setSelected(next_event, 1);
				MidiFileEvent_delete(event);
			}
		}
	}

	return 0;
}

int MidiFile_convertStandardEventsToNoteEvents(MidiFile_t midi_file)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event;

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			MidiFileEvent_t end_event = MidiFileNoteStartEvent_getNoteEndEvent(event);

			if (end_event != NULL)
			{
				MidiFileEvent_t new_event = MidiFileTrack_createNoteEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileEvent_getTick(end_event) - MidiFileEvent_getTick(event),
					MidiFileNoteStartEvent_getChannel(event),
					MidiFileNoteStartEvent_getNote(event),
					MidiFileNoteStartEvent_getVelocity(event),
					MidiFileNoteEndEvent_getVelocity(end_event));

				MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setPreviousEvent(new_event, event);
				MidiFileEvent_delete(event);
				MidiFileEvent_delete(end_event);
				event = new_event;
			}
		}
	}

	return 0;
}

int MidiFile_convertNoteEventsToStandardEvents(MidiFile_t midi_file)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event;

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_NOTE)
			{
				MidiFileEvent_t note_start_event = MidiFileTrack_createNoteStartAndEndEvents(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileEvent_getTick(event) + MidiFileNoteEvent_getDurationTicks(event),
					MidiFileNoteEvent_getChannel(event),
					MidiFileNoteEvent_getNote(event),
					MidiFileNoteEvent_getVelocity(event),
					MidiFileNoteEvent_getEndVelocity(event));

				MidiFileEvent_setSelected(note_start_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(MidiFileNoteStartEvent_getNoteEndEvent(note_start_event), MidiFileEvent_isSelected(event));
				MidiFileEvent_setPreviousEvent(note_start_event, event);
				MidiFileEvent_delete(event);
				event = note_start_event;
			}
		}
	}

	return 0;
}

int MidiFile_convertStandardEventsToFineControlChangeEvents(MidiFile_t midi_file)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event, next_event;
	int values[16][64];

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		memset(values, 0, sizeof (int) * 16 * 64);

		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = next_event)
		{
			next_event = MidiFileEvent_getNextEventInTrack(event);

			if (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)
			{
				if (MidiFileControlChangeEvent_getNumber(event) < 32)
				{
					if ((MidiFileEvent_getType(next_event) == MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)
						&& (MidiFileControlChangeEvent_getChannel(next_event) == MidiFileControlChangeEvent_getChannel(event))
						&& (MidiFileControlChangeEvent_getNumber(next_event) == MidiFileControlChangeEvent_getNumber(event) + 32))
					{
						MidiFileEvent_t new_event = MidiFileTrack_createFineControlChangeEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							MidiFileControlChangeEvent_getNumber(event),
							0);

						MidiFileFineControlChangeEvent_setCoarseValue(new_event, MidiFileControlChangeEvent_getValue(event));
						MidiFileFineControlChangeEvent_setFineValue(new_event, MidiFileControlChangeEvent_getValue(next_event));
						values[MidiFileControlChangeEvent_getChannel(event)][MidiFileControlChangeEvent_getNumber(event)] = MidiFileControlChangeEvent_getValue(event);
						values[MidiFileControlChangeEvent_getChannel(next_event)][MidiFileControlChangeEvent_getNumber(next_event)] = MidiFileControlChangeEvent_getValue(next_event);
						MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
						MidiFileEvent_setPreviousEvent(new_event, next_event);
						MidiFileEvent_delete(event);
						MidiFileEvent_delete(next_event);
						next_event = MidiFileEvent_getNextEventInTrack(new_event);
					}
					else
					{
						MidiFileEvent_t new_event = MidiFileTrack_createFineControlChangeEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							MidiFileControlChangeEvent_getNumber(event),
							0);

						MidiFileFineControlChangeEvent_setCoarseValue(new_event, MidiFileControlChangeEvent_getValue(event));
						MidiFileFineControlChangeEvent_setFineValue(new_event, values[MidiFileControlChangeEvent_getChannel(event)][MidiFileControlChangeEvent_getNumber(event) + 32]);
						values[MidiFileControlChangeEvent_getChannel(event)][MidiFileControlChangeEvent_getNumber(event)] = MidiFileControlChangeEvent_getValue(event);
						MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
						MidiFileEvent_setPreviousEvent(new_event, event);
						MidiFileEvent_delete(event);
						next_event = MidiFileEvent_getNextEventInTrack(new_event);
					}
				}
				else if (MidiFileControlChangeEvent_getNumber(event) < 64)
				{
					MidiFileEvent_t new_event = MidiFileTrack_createFineControlChangeEvent(
						track,
						MidiFileEvent_getTick(event),
						MidiFileControlChangeEvent_getChannel(event),
						MidiFileControlChangeEvent_getNumber(event),
						0);

					MidiFileFineControlChangeEvent_setCoarseValue(new_event, values[MidiFileControlChangeEvent_getChannel(event)][MidiFileControlChangeEvent_getNumber(event) - 32]);
					MidiFileFineControlChangeEvent_setFineValue(new_event, MidiFileControlChangeEvent_getValue(event));
					values[MidiFileControlChangeEvent_getChannel(event)][MidiFileControlChangeEvent_getNumber(event)] = MidiFileControlChangeEvent_getValue(event);
					MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
					MidiFileEvent_setPreviousEvent(new_event, event);
					MidiFileEvent_delete(event);
					next_event = MidiFileEvent_getNextEventInTrack(new_event);
				}
			}
		}
	}

	return 0;
}

int MidiFile_convertFineControlChangeEventsToStandardEvents(MidiFile_t midi_file)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event;

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)
			{
				MidiFileEvent_t coarse_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileFineControlChangeEvent_getChannel(event),
					MidiFileFineControlChangeEvent_getCoarseNumber(event),
					MidiFileFineControlChangeEvent_getCoarseValue(event));

				MidiFileEvent_t fine_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileFineControlChangeEvent_getChannel(event),
					MidiFileFineControlChangeEvent_getFineNumber(event),
					MidiFileFineControlChangeEvent_getFineValue(event));

				MidiFileEvent_setSelected(coarse_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(fine_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setPreviousEvent(coarse_event, event);
				MidiFileEvent_setPreviousEvent(fine_event, coarse_event);
				MidiFileEvent_delete(event);
				event = fine_event;
			}
		}
	}

	return 0;
}

int MidiFile_convertStandardEventsToRpnAndNrpnEvents(MidiFile_t midi_file)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event, next_event;
	int is_nrpn[16];
	int numbers[16];
	int values[16];

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		memset(is_nrpn, 0, sizeof (int) * 16);
		memset(numbers, 0, sizeof (int) * 16);
		memset(values, 0, sizeof (int) * 16);

		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = next_event)
		{
			next_event = MidiFileEvent_getNextEventInTrack(event);

			if (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)
			{
				if (MidiFileControlChangeEvent_getNumber(event) == 101)
				{
					if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
					{
						is_nrpn[MidiFileControlChangeEvent_getChannel(event)] = 0;
						numbers[MidiFileControlChangeEvent_getChannel(event)] = MidiFileControlChangeEvent_getValue(event) << 7;
					}
					else
					{
						numbers[MidiFileControlChangeEvent_getChannel(event)] = (MidiFileControlChangeEvent_getValue(event) << 7) | (numbers[MidiFileControlChangeEvent_getChannel(event)] & 0x7F) ;
					}

					values[MidiFileControlChangeEvent_getChannel(event)] = 0;
					MidiFileEvent_delete(event);
				}
				else if (MidiFileControlChangeEvent_getNumber(event) == 100)
				{
					if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
					{
						is_nrpn[MidiFileControlChangeEvent_getChannel(event)] = 0;
						numbers[MidiFileControlChangeEvent_getChannel(event)] = MidiFileControlChangeEvent_getValue(event);
					}
					else
					{
						numbers[MidiFileControlChangeEvent_getChannel(event)] = (numbers[MidiFileControlChangeEvent_getChannel(event)] & ~0x7F) | MidiFileControlChangeEvent_getValue(event);
					}

					values[MidiFileControlChangeEvent_getChannel(event)] = 0;
					MidiFileEvent_delete(event);
				}
				else if (MidiFileControlChangeEvent_getNumber(event) == 6)
				{
					if ((MidiFileEvent_getType(next_event) == MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)
						&& (MidiFileControlChangeEvent_getChannel(next_event) == MidiFileControlChangeEvent_getChannel(event))
						&& (MidiFileControlChangeEvent_getNumber(next_event) == 38))
					{
						MidiFileEvent_t new_event;

						values[MidiFileControlChangeEvent_getChannel(event)] = (MidiFileControlChangeEvent_getValue(event) << 7) | MidiFileControlChangeEvent_getValue(next_event);

						if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
						{
							new_event = MidiFileTrack_createNrpnEvent(
								track,
								MidiFileEvent_getTick(event),
								MidiFileControlChangeEvent_getChannel(event),
								numbers[MidiFileControlChangeEvent_getChannel(event)],
								values[MidiFileControlChangeEvent_getChannel(event)]);
						}
						else
						{
							new_event = MidiFileTrack_createRpnEvent(
								track,
								MidiFileEvent_getTick(event),
								MidiFileControlChangeEvent_getChannel(event),
								numbers[MidiFileControlChangeEvent_getChannel(event)],
								values[MidiFileControlChangeEvent_getChannel(event)]);
						}

						MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
						MidiFileEvent_setPreviousEvent(new_event, next_event);
						MidiFileEvent_delete(event);
						MidiFileEvent_delete(next_event);
						next_event = MidiFileEvent_getNextEventInTrack(new_event);
					}
					else
					{
						MidiFileEvent_t new_event;

						values[MidiFileControlChangeEvent_getChannel(event)] = (MidiFileControlChangeEvent_getValue(event) << 7) | (values[MidiFileControlChangeEvent_getChannel(event)] & 0x7F);

						if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
						{
							new_event = MidiFileTrack_createNrpnEvent(
								track,
								MidiFileEvent_getTick(event),
								MidiFileControlChangeEvent_getChannel(event),
								numbers[MidiFileControlChangeEvent_getChannel(event)],
								values[MidiFileControlChangeEvent_getChannel(event)]);
						}
						else
						{
							new_event = MidiFileTrack_createRpnEvent(
								track,
								MidiFileEvent_getTick(event),
								MidiFileControlChangeEvent_getChannel(event),
								numbers[MidiFileControlChangeEvent_getChannel(event)],
								values[MidiFileControlChangeEvent_getChannel(event)]);
						}

						MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
						MidiFileEvent_setPreviousEvent(new_event, event);
						MidiFileEvent_delete(event);
						next_event = MidiFileEvent_getNextEventInTrack(new_event);
					}
				}
				else if (MidiFileControlChangeEvent_getNumber(event) == 38)
				{
					MidiFileEvent_t new_event;

					values[MidiFileControlChangeEvent_getChannel(event)] = (values[MidiFileControlChangeEvent_getChannel(event)] & ~0x7F) | MidiFileControlChangeEvent_getValue(event);

					if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
					{
						new_event = MidiFileTrack_createNrpnEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							numbers[MidiFileControlChangeEvent_getChannel(event)],
							values[MidiFileControlChangeEvent_getChannel(event)]);
					}
					else
					{
						new_event = MidiFileTrack_createRpnEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							numbers[MidiFileControlChangeEvent_getChannel(event)],
							values[MidiFileControlChangeEvent_getChannel(event)]);
					}

					MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
					MidiFileEvent_setPreviousEvent(new_event, event);
					MidiFileEvent_delete(event);
					next_event = MidiFileEvent_getNextEventInTrack(new_event);
				}
				else if (MidiFileControlChangeEvent_getNumber(event) == 96)
				{
					MidiFileEvent_t new_event;

					values[MidiFileControlChangeEvent_getChannel(event)]++;

					if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
					{
						new_event = MidiFileTrack_createNrpnEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							numbers[MidiFileControlChangeEvent_getChannel(event)],
							values[MidiFileControlChangeEvent_getChannel(event)]);
					}
					else
					{
						new_event = MidiFileTrack_createRpnEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							numbers[MidiFileControlChangeEvent_getChannel(event)],
							values[MidiFileControlChangeEvent_getChannel(event)]);
					}

					MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
					MidiFileEvent_setPreviousEvent(new_event, event);
					MidiFileEvent_delete(event);
					next_event = MidiFileEvent_getNextEventInTrack(new_event);
				}
				else if (MidiFileControlChangeEvent_getNumber(event) == 97)
				{
					MidiFileEvent_t new_event;

					values[MidiFileControlChangeEvent_getChannel(event)]--;

					if (is_nrpn[MidiFileControlChangeEvent_getChannel(event)])
					{
						new_event = MidiFileTrack_createNrpnEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							numbers[MidiFileControlChangeEvent_getChannel(event)],
							values[MidiFileControlChangeEvent_getChannel(event)]);
					}
					else
					{
						new_event = MidiFileTrack_createRpnEvent(
							track,
							MidiFileEvent_getTick(event),
							MidiFileControlChangeEvent_getChannel(event),
							numbers[MidiFileControlChangeEvent_getChannel(event)],
							values[MidiFileControlChangeEvent_getChannel(event)]);
					}

					MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
					MidiFileEvent_setPreviousEvent(new_event, event);
					MidiFileEvent_delete(event);
					next_event = MidiFileEvent_getNextEventInTrack(new_event);
				}
			}
			else if ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE) && (MidiFileFineControlChangeEvent_getCoarseNumber(event) == 6))
			{
				MidiFileEvent_t new_event;

				values[MidiFileFineControlChangeEvent_getChannel(event)] = MidiFileFineControlChangeEvent_getValue(event);

				if (is_nrpn[MidiFileFineControlChangeEvent_getChannel(event)])
				{
					new_event = MidiFileTrack_createNrpnEvent(
						track,
						MidiFileEvent_getTick(event),
						MidiFileFineControlChangeEvent_getChannel(event),
						numbers[MidiFileFineControlChangeEvent_getChannel(event)],
						values[MidiFileFineControlChangeEvent_getChannel(event)]);
				}
				else
				{
					new_event = MidiFileTrack_createRpnEvent(
						track,
						MidiFileEvent_getTick(event),
						MidiFileFineControlChangeEvent_getChannel(event),
						numbers[MidiFileFineControlChangeEvent_getChannel(event)],
						values[MidiFileFineControlChangeEvent_getChannel(event)]);
				}

				MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setPreviousEvent(new_event, event);
				MidiFileEvent_delete(event);
				next_event = MidiFileEvent_getNextEventInTrack(new_event);
			}
		}
	}

	return 0;
}

int MidiFile_convertRpnAndNrpnEventsToStandardEvents(MidiFile_t midi_file)
{
	MidiFileTrack_t track;
	MidiFileEvent_t event;

	if (midi_file == NULL) return -1;

	for (track = MidiFile_getFirstTrack(midi_file); track != NULL; track = MidiFileTrack_getNextTrack(track))
	{
		for (event = MidiFileTrack_getFirstEvent(track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_RPN)
			{
				MidiFileEvent_t coarse_number_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileRpnEvent_getChannel(event),
					101,
					MidiFileRpnEvent_getCoarseNumber(event));

				MidiFileEvent_t fine_number_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileRpnEvent_getChannel(event),
					100,
					MidiFileRpnEvent_getFineNumber(event));

				MidiFileEvent_t coarse_value_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileRpnEvent_getChannel(event),
					6,
					MidiFileRpnEvent_getCoarseValue(event));

				MidiFileEvent_t fine_value_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileRpnEvent_getChannel(event),
					38,
					MidiFileRpnEvent_getFineValue(event));

				MidiFileEvent_setSelected(coarse_number_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(fine_number_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(coarse_value_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(fine_value_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setPreviousEvent(coarse_number_event, event);
				MidiFileEvent_setPreviousEvent(fine_number_event, coarse_number_event);
				MidiFileEvent_setPreviousEvent(coarse_value_event, fine_number_event);
				MidiFileEvent_setPreviousEvent(fine_value_event, coarse_value_event);
				MidiFileEvent_delete(event);
				event = fine_value_event;
			}
			else if (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_NRPN)
			{
				MidiFileEvent_t coarse_number_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileNrpnEvent_getChannel(event),
					99,
					MidiFileNrpnEvent_getCoarseNumber(event));

				MidiFileEvent_t fine_number_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileNrpnEvent_getChannel(event),
					98,
					MidiFileNrpnEvent_getFineNumber(event));

				MidiFileEvent_t coarse_value_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileNrpnEvent_getChannel(event),
					6,
					MidiFileNrpnEvent_getCoarseValue(event));

				MidiFileEvent_t fine_value_event = MidiFileTrack_createControlChangeEvent(
					MidiFileEvent_getTrack(event),
					MidiFileEvent_getTick(event),
					MidiFileNrpnEvent_getChannel(event),
					38,
					MidiFileNrpnEvent_getFineValue(event));

				MidiFileEvent_setSelected(coarse_number_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(fine_number_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(coarse_value_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setSelected(fine_value_event, MidiFileEvent_isSelected(event));
				MidiFileEvent_setPreviousEvent(coarse_number_event, event);
				MidiFileEvent_setPreviousEvent(fine_number_event, coarse_number_event);
				MidiFileEvent_setPreviousEvent(coarse_value_event, fine_number_event);
				MidiFileEvent_setPreviousEvent(fine_value_event, coarse_value_event);
				MidiFileEvent_delete(event);
				event = fine_value_event;
			}
		}
	}

	return 0;
}

float MidiFile_getBeatFromTick(MidiFile_t midi_file, long tick)
{
	switch (MidiFile_getDivisionType(midi_file))
	{
		case MIDI_FILE_DIVISION_TYPE_PPQ:
		{
			return (float)(tick) / MidiFile_getResolution(midi_file);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					tempo_event_beat += (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(24.0) / (float)(60.0) * tempo);
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_beat + (((float)(tick - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(24.0) / (float)(60.0) * tempo);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					tempo_event_beat += (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(25.0) / (float)(60.0) * tempo);
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_beat + (((float)(tick - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(25.0) / (float)(60.0) * tempo);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					tempo_event_beat += (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(29.97) / (float)(60.0) * tempo);
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_beat + (((float)(tick - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(29.97) / (float)(60.0) * tempo);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					tempo_event_beat += (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(30.0) / (float)(60.0) * tempo);
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_beat + (((float)(tick - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(30.0) / (float)(60.0) * tempo);
		}
		default:
		{
			return -1.0;
		}
	}
}

long MidiFile_getTickFromBeat(MidiFile_t midi_file, float beat)
{
	switch (MidiFile_getDivisionType(midi_file))
	{
		case MIDI_FILE_DIVISION_TYPE_PPQ:
		{
			return (long)(beat * MidiFile_getResolution(midi_file));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					float next_tempo_event_beat = tempo_event_beat + (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(24.0) / (float)(60.0) * tempo);
					if (next_tempo_event_beat >= beat) break;
					tempo_event_beat = next_tempo_event_beat;
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_tick + (long)((beat - tempo_event_beat) / tempo * 60.0 * 24.0 * MidiFile_getResolution(midi_file));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					float next_tempo_event_beat = tempo_event_beat + (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(25.0) / (float)(60.0) * tempo);
					if (next_tempo_event_beat >= beat) break;
					tempo_event_beat = next_tempo_event_beat;
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_tick + (long)((beat - tempo_event_beat) / tempo * 60.0 * 25.0 * MidiFile_getResolution(midi_file));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					float next_tempo_event_beat = tempo_event_beat + (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(29.97) / (float)(60.0) * tempo);
					if (next_tempo_event_beat >= beat) break;
					tempo_event_beat = next_tempo_event_beat;
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_tick + (long)((beat - tempo_event_beat) / tempo * 60.0 * 29.97 * MidiFile_getResolution(midi_file));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_beat = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					float next_tempo_event_beat = tempo_event_beat + (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (float)(30.0) / (float)(60.0) * tempo);
					if (next_tempo_event_beat >= beat) break;
					tempo_event_beat = next_tempo_event_beat;
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_tick + (long)((beat - tempo_event_beat) / tempo * 60.0 * 30.0 * MidiFile_getResolution(midi_file));
		}
		default:
		{
			return -1;
		}
	}
}

float MidiFile_getTimeFromTick(MidiFile_t midi_file, long tick)
{
	switch (MidiFile_getDivisionType(midi_file))
	{
		case MIDI_FILE_DIVISION_TYPE_PPQ:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_time = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					tempo_event_time += (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (tempo / 60));
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_time + (((float)(tick - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (tempo / 60));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			return (float)(tick) / (MidiFile_getResolution(midi_file) * (float)(24.0));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			return (float)(tick) / (MidiFile_getResolution(midi_file) * (float)(25.0));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			return (float)(tick) / (MidiFile_getResolution(midi_file) * (float)(29.97));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			return (float)(tick) / (MidiFile_getResolution(midi_file) * (float)(30.0));
		}
		default:
		{
			return -1;
		}
	}
}

long MidiFile_getTickFromTime(MidiFile_t midi_file, float time)
{
	switch (MidiFile_getDivisionType(midi_file))
	{
		case MIDI_FILE_DIVISION_TYPE_PPQ:
		{
			MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
			MidiFileEvent_t event;
			float tempo_event_time = 0.0;
			long tempo_event_tick = 0;
			float tempo = 120.0;

			for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
			{
				if (MidiFileEvent_isTempoEvent(event))
				{
					float next_tempo_event_time = tempo_event_time + (((float)(MidiFileEvent_getTick(event) - tempo_event_tick)) / MidiFile_getResolution(midi_file) / (tempo / 60));
					if (next_tempo_event_time >= time) break;
					tempo_event_time = next_tempo_event_time;
					tempo_event_tick = MidiFileEvent_getTick(event);
					tempo = MidiFileTempoEvent_getTempo(event);
				}
			}

			return tempo_event_tick + (long)((time - tempo_event_time) * (tempo / 60) * MidiFile_getResolution(midi_file));
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE24:
		{
			return (long)(time * MidiFile_getResolution(midi_file) * 24.0);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE25:
		{
			return (long)(time * MidiFile_getResolution(midi_file) * 25.0);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30DROP:
		{
			return (long)(time * MidiFile_getResolution(midi_file) * 29.97);
		}
		case MIDI_FILE_DIVISION_TYPE_SMPTE30:
		{
			return (long)(time * MidiFile_getResolution(midi_file) * 30.0);
		}
		default:
		{
			return -1;
		}
	}
}

float MidiFile_getMeasureFromTick(MidiFile_t midi_file, long tick)
{
	if (midi_file == NULL)
	{
		return -1.0;
	}
	else
	{
		MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
		MidiFileEvent_t event;
		float time_signature_event_beat = 0.0;
		float measure = 0.0;
		int numerator = 4;
		int denominator = 4;

		for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isTimeSignatureEvent(event))
			{
				float next_time_signature_event_beat = MidiFile_getBeatFromTick(midi_file, MidiFileEvent_getTick(event));
				measure += ((next_time_signature_event_beat - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
				time_signature_event_beat = next_time_signature_event_beat;
				numerator = MidiFileTimeSignatureEvent_getNumerator(event);
				denominator = MidiFileTimeSignatureEvent_getDenominator(event);
			}
		}

		measure += ((MidiFile_getBeatFromTick(midi_file, tick) - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
		return measure;
	}
}

long MidiFile_getTickFromMeasure(MidiFile_t midi_file, float measure)
{
	if (midi_file == NULL)
	{
		return -1;
	}
	else
	{
		MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
		MidiFileEvent_t event;
		float time_signature_event_beat = 0.0;
		float time_signature_event_measure = 0.0;
		int numerator = 4;
		int denominator = 4;

		for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isTimeSignatureEvent(event))
			{
				float next_time_signature_event_beat = MidiFile_getBeatFromTick(midi_file, MidiFileEvent_getTick(event));
				float next_time_signature_event_measure = time_signature_event_measure + ((next_time_signature_event_beat - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
				if (next_time_signature_event_measure >= measure) break;
				time_signature_event_beat = next_time_signature_event_beat;
				time_signature_event_measure = next_time_signature_event_measure;
				numerator = MidiFileTimeSignatureEvent_getNumerator(event);
				denominator = MidiFileTimeSignatureEvent_getDenominator(event);
			}
		}

		return MidiFile_getTickFromBeat(midi_file, time_signature_event_beat + ((measure - time_signature_event_measure) / ((float)(denominator) / numerator / 4)));
	}
}

int MidiFile_setMeasureBeatFromTick(MidiFile_t midi_file, long tick, MidiFileMeasureBeat_t measure_beat)
{
	if ((midi_file == NULL) || (measure_beat == NULL))
	{
		return -1;
	}
	else
	{
		MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
		MidiFileEvent_t event;
		float time_signature_event_beat = 0.0;
		float measure = 0.0;
		int numerator = 4;
		int denominator = 4;

		for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isTimeSignatureEvent(event))
			{
				float next_time_signature_event_beat = MidiFile_getBeatFromTick(midi_file, MidiFileEvent_getTick(event));
				measure += ((next_time_signature_event_beat - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
				time_signature_event_beat = next_time_signature_event_beat;
				numerator = MidiFileTimeSignatureEvent_getNumerator(event);
				denominator = MidiFileTimeSignatureEvent_getDenominator(event);
			}
		}

		measure += ((MidiFile_getBeatFromTick(midi_file, tick) - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
		MidiFileMeasureBeat_setMeasure(measure_beat, (long)(measure) + 1);
		MidiFileMeasureBeat_setBeat(measure_beat, ((measure - (float)(MidiFileMeasureBeat_getMeasure(measure_beat) - 1)) * numerator) + 1.0);
		return 0;
	}
}

long MidiFile_getTickFromMeasureBeat(MidiFile_t midi_file, MidiFileMeasureBeat_t measure_beat)
{
	if ((midi_file == NULL) || (measure_beat == NULL))
	{
		return -1;
	}
	else
	{
		MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
		MidiFileEvent_t event;
		float time_signature_event_beat = 0.0;
		float time_signature_event_measure = 0.0;
		long time_signature_event_visible_measure = 1;
		float time_signature_event_visible_beat = 1.0;
		int numerator = 4;
		int denominator = 4;

		for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isTimeSignatureEvent(event))
			{
				float next_time_signature_event_beat = MidiFile_getBeatFromTick(midi_file, MidiFileEvent_getTick(event));
				float next_time_signature_event_measure = time_signature_event_measure + ((next_time_signature_event_beat - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
				long next_time_signature_event_visible_measure = (long)(next_time_signature_event_measure) + 1;
				float next_time_signature_event_visible_beat = ((next_time_signature_event_measure - (float)(next_time_signature_event_visible_measure - 1)) * numerator) + 1.0;
				if ((next_time_signature_event_visible_measure > MidiFileMeasureBeat_getMeasure(measure_beat)) || ((next_time_signature_event_visible_measure == MidiFileMeasureBeat_getMeasure(measure_beat)) && (next_time_signature_event_visible_beat >= MidiFileMeasureBeat_getBeat(measure_beat)))) break;
				time_signature_event_beat = next_time_signature_event_beat;
				time_signature_event_measure = next_time_signature_event_measure;
				time_signature_event_visible_measure = next_time_signature_event_visible_measure;
				time_signature_event_visible_beat = next_time_signature_event_visible_beat;
				numerator = MidiFileTimeSignatureEvent_getNumerator(event);
				denominator = MidiFileTimeSignatureEvent_getDenominator(event);
			}
		}

		return MidiFile_getTickFromBeat(midi_file, time_signature_event_beat + ((((MidiFileMeasureBeat_getMeasure(measure_beat) - time_signature_event_visible_measure) * numerator) + (MidiFileMeasureBeat_getBeat(measure_beat) - time_signature_event_visible_beat)) * 4 / denominator));
	}
}

int MidiFile_setMeasureBeatTickFromTick(MidiFile_t midi_file, long tick, MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if ((midi_file == NULL) || (measure_beat_tick == NULL))
	{
		return -1;
	}
	else
	{
		MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
		MidiFileEvent_t event;
		float time_signature_event_beat = 0.0;
		float measure = 0.0;
		int numerator = 4;
		int denominator = 4;

		for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) < tick); event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isTimeSignatureEvent(event))
			{
				float next_time_signature_event_beat = MidiFile_getBeatFromTick(midi_file, MidiFileEvent_getTick(event));
				measure += ((next_time_signature_event_beat - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
				time_signature_event_beat = next_time_signature_event_beat;
				numerator = MidiFileTimeSignatureEvent_getNumerator(event);
				denominator = MidiFileTimeSignatureEvent_getDenominator(event);
			}
		}

		measure += ((MidiFile_getBeatFromTick(midi_file, tick) - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
		MidiFileMeasureBeatTick_setMeasure(measure_beat_tick, (long)(measure) + 1);
		MidiFileMeasureBeatTick_setBeat(measure_beat_tick, (long)((measure - (float)(MidiFileMeasureBeatTick_getMeasure(measure_beat_tick) - 1)) * numerator) + 1);
		MidiFileMeasureBeatTick_setTick(measure_beat_tick, tick - MidiFile_getTickFromBeat(midi_file, (float)((long)(MidiFile_getBeatFromTick(midi_file, tick)))));
		return 0;
	}
}

long MidiFile_getTickFromMeasureBeatTick(MidiFile_t midi_file, MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if ((midi_file == NULL) || (measure_beat_tick == NULL))
	{
		return -1;
	}
	else
	{
		MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
		MidiFileEvent_t event;
		float time_signature_event_beat = 0.0;
		float time_signature_event_measure = 0.0;
		long time_signature_event_visible_measure = 1;
		long time_signature_event_visible_beat = 1;
		int numerator = 4;
		int denominator = 4;

		for (event = MidiFileTrack_getFirstEvent(conductor_track); event != NULL; event = MidiFileEvent_getNextEventInTrack(event))
		{
			if (MidiFileEvent_isTimeSignatureEvent(event))
			{
				long next_time_signature_event_tick = MidiFileEvent_getTick(event);
				float next_time_signature_event_beat = MidiFile_getBeatFromTick(midi_file, next_time_signature_event_tick);
				float next_time_signature_event_measure = time_signature_event_measure + ((next_time_signature_event_beat - time_signature_event_beat) * ((float)(denominator) / numerator / 4));
				long next_time_signature_event_visible_measure = (long)(next_time_signature_event_measure) + 1;
				long next_time_signature_event_visible_beat = (long)((next_time_signature_event_measure - (float)(next_time_signature_event_visible_measure - 1)) * numerator) + 1;
				float next_time_signature_event_visible_tick = next_time_signature_event_tick - MidiFile_getTickFromBeat(midi_file, (float)((long)(next_time_signature_event_beat)));
				if ((next_time_signature_event_visible_measure > MidiFileMeasureBeatTick_getMeasure(measure_beat_tick)) || ((next_time_signature_event_visible_measure == MidiFileMeasureBeatTick_getMeasure(measure_beat_tick)) && ((next_time_signature_event_visible_beat > MidiFileMeasureBeatTick_getBeat(measure_beat_tick)) || ((next_time_signature_event_visible_measure == MidiFileMeasureBeatTick_getBeat(measure_beat_tick)) && (next_time_signature_event_visible_tick >= MidiFileMeasureBeatTick_getTick(measure_beat_tick)))))) break;
				time_signature_event_beat = next_time_signature_event_beat;
				time_signature_event_measure = next_time_signature_event_measure;
				time_signature_event_visible_measure = next_time_signature_event_visible_measure;
				time_signature_event_visible_beat = next_time_signature_event_visible_beat;
				numerator = MidiFileTimeSignatureEvent_getNumerator(event);
				denominator = MidiFileTimeSignatureEvent_getDenominator(event);
			}
		}

		return MidiFile_getTickFromBeat(midi_file, time_signature_event_beat + (((((MidiFileMeasureBeatTick_getMeasure(measure_beat_tick) - time_signature_event_visible_measure) * numerator) + (MidiFileMeasureBeatTick_getBeat(measure_beat_tick) - time_signature_event_visible_beat))) * 4 / denominator)) + MidiFileMeasureBeatTick_getTick(measure_beat_tick);
	}
}

int MidiFile_setHourMinuteSecondFromTick(MidiFile_t midi_file, long tick, MidiFileHourMinuteSecond_t hour_minute_second)
{
	if ((midi_file == NULL) || (hour_minute_second == NULL))
	{
		return -1;
	}
	else
	{
		float time = MidiFile_getTimeFromTick(midi_file, tick);
		MidiFileHourMinuteSecond_setHour(hour_minute_second, (long)(time / 3600.0));
		MidiFileHourMinuteSecond_setMinute(hour_minute_second, (long)((time - ((float)(MidiFileHourMinuteSecond_getHour(hour_minute_second)) * 3600.0)) / 60.0));
		MidiFileHourMinuteSecond_setSecond(hour_minute_second, time - ((float)(MidiFileHourMinuteSecond_getHour(hour_minute_second)) * (float)(3600.0)) - ((float)(MidiFileHourMinuteSecond_getMinute(hour_minute_second)) * (float)(60.0)));
		return 0;
	}
}

long MidiFile_getTickFromHourMinuteSecond(MidiFile_t midi_file, MidiFileHourMinuteSecond_t hour_minute_second)
{
	if ((midi_file == NULL) || (hour_minute_second == NULL))
	{
		return -1;
	}
	else
	{
		return MidiFile_getTickFromTime(midi_file, ((float)(MidiFileHourMinuteSecond_getHour(hour_minute_second)) * (float)(3600.0)) + ((float)(MidiFileHourMinuteSecond_getMinute(hour_minute_second)) * (float)(60.0)) + MidiFileHourMinuteSecond_getSecond(hour_minute_second));
	}
}

int MidiFile_setHourMinuteSecondFrameFromTick(MidiFile_t midi_file, long tick, MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if ((midi_file == NULL) || (hour_minute_second_frame == NULL))
	{
		return -1;
	}
	else
	{
		float time = MidiFile_getTimeFromTick(midi_file, tick);
		MidiFileHourMinuteSecondFrame_setHour(hour_minute_second_frame, (long)(time / 3600.0));
		MidiFileHourMinuteSecondFrame_setMinute(hour_minute_second_frame, (long)((time - ((float)(MidiFileHourMinuteSecondFrame_getHour(hour_minute_second_frame)) * 3600.0)) / 60.0));
		MidiFileHourMinuteSecondFrame_setSecond(hour_minute_second_frame, (long)(time - ((float)(MidiFileHourMinuteSecondFrame_getHour(hour_minute_second_frame)) * 3600.0) - ((float)(MidiFileHourMinuteSecondFrame_getMinute(hour_minute_second_frame)) * 60.0)));
		MidiFileHourMinuteSecondFrame_setFrame(hour_minute_second_frame, (time - ((float)(MidiFileHourMinuteSecondFrame_getHour(hour_minute_second_frame)) * (float)(3600.0)) - ((float)(MidiFileHourMinuteSecondFrame_getMinute(hour_minute_second_frame)) * (float)(60.0)) - (float)(MidiFileHourMinuteSecondFrame_getSecond(hour_minute_second_frame))) * MidiFile_getNumberOfFramesPerSecond(midi_file));
		return 0;
	}
}

long MidiFile_getTickFromHourMinuteSecondFrame(MidiFile_t midi_file, MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if ((midi_file == NULL) || (hour_minute_second_frame == NULL))
	{
		return -1;
	}
	else
	{
		return MidiFile_getTickFromTime(midi_file, ((float)(MidiFileHourMinuteSecondFrame_getHour(hour_minute_second_frame)) * (float)(3600.0)) + ((float)(MidiFileHourMinuteSecondFrame_getMinute(hour_minute_second_frame)) * (float)(60.0)) + (float)(MidiFileHourMinuteSecondFrame_getSecond(hour_minute_second_frame)) + (MidiFileHourMinuteSecondFrame_getFrame(hour_minute_second_frame) / MidiFile_getNumberOfFramesPerSecond(midi_file)));
	}
}

char *MidiFile_getMeasureBeatStringFromTick(MidiFile_t midi_file, long tick)
{
	if (midi_file == NULL) return NULL;
	MidiFile_setMeasureBeatFromTick(midi_file, tick, midi_file->measure_beat);
	return MidiFileMeasureBeat_toString(midi_file->measure_beat);
}

int MidiFile_getTickFromMeasureBeatString(MidiFile_t midi_file, char *measure_beat_string)
{
	if ((midi_file == NULL) || (measure_beat_string == NULL)) return -1;
	MidiFileMeasureBeat_parse(midi_file->measure_beat, measure_beat_string);
	return MidiFile_getTickFromMeasureBeat(midi_file, midi_file->measure_beat);
}

char *MidiFile_getMeasureBeatTickStringFromTick(MidiFile_t midi_file, long tick)
{
	if (midi_file == NULL) return NULL;
	MidiFile_setMeasureBeatTickFromTick(midi_file, tick, midi_file->measure_beat_tick);
	return MidiFileMeasureBeatTick_toString(midi_file->measure_beat_tick);
}

int MidiFile_getTickFromMeasureBeatTickString(MidiFile_t midi_file, char *measure_beat_tick_string)
{
	if ((midi_file == NULL) || (measure_beat_tick_string == NULL)) return -1;
	MidiFileMeasureBeatTick_parse(midi_file->measure_beat_tick, measure_beat_tick_string);
	return MidiFile_getTickFromMeasureBeatTick(midi_file, midi_file->measure_beat_tick);
}

char *MidiFile_getHourMinuteSecondStringFromTick(MidiFile_t midi_file, long tick)
{
	if (midi_file == NULL) return NULL;
	MidiFile_setHourMinuteSecondFromTick(midi_file, tick, midi_file->hour_minute_second);
	return MidiFileHourMinuteSecond_toString(midi_file->hour_minute_second);
}

int MidiFile_getTickFromHourMinuteSecondString(MidiFile_t midi_file, char *hour_minute_second_string)
{
	if ((midi_file == NULL) || (hour_minute_second_string == NULL)) return -1;
	MidiFileHourMinuteSecond_parse(midi_file->hour_minute_second, hour_minute_second_string);
	return MidiFile_getTickFromHourMinuteSecond(midi_file, midi_file->hour_minute_second);
}

char *MidiFile_getHourMinuteSecondFrameStringFromTick(MidiFile_t midi_file, long tick)
{
	if (midi_file == NULL) return NULL;
	MidiFile_setHourMinuteSecondFrameFromTick(midi_file, tick, midi_file->hour_minute_second_frame);
	return MidiFileHourMinuteSecondFrame_toString(midi_file->hour_minute_second_frame);
}

int MidiFile_getTickFromHourMinuteSecondFrameString(MidiFile_t midi_file, char *hour_minute_second_frame_string)
{
	if ((midi_file == NULL) || (hour_minute_second_frame_string == NULL)) return -1;
	MidiFileHourMinuteSecondFrame_parse(midi_file->hour_minute_second_frame, hour_minute_second_frame_string);
	return MidiFile_getTickFromHourMinuteSecondFrame(midi_file, midi_file->hour_minute_second_frame);
}

long MidiFile_getTickFromMarker(MidiFile_t midi_file, char *marker)
{
	MidiFileEvent_t event;

	for (event = MidiFile_getFirstEvent(midi_file); event != NULL; event = MidiFileEvent_getNextEventInFile(event))
	{
		if (MidiFileEvent_isMarkerEvent(event) && (strcmp(MidiFileMarkerEvent_getText(event), marker) == 0)) return MidiFileEvent_getTick(event);
	}

	return -1;
}

long MidiFile_getTickFromTimeString(MidiFile_t midi_file, char *time_string)
{
	if ((midi_file == NULL) || (time_string == NULL)) return -1;
	if (strncmp(time_string, "tick:", 5) == 0) return atol(time_string + 5);
	if (strncmp(time_string, "beat:", 5) == 0) return MidiFile_getTickFromBeat(midi_file, atof(time_string + 5));
	if (strncmp(time_string, "mb:", 3) == 0) return MidiFile_getTickFromMeasureBeatString(midi_file, time_string + 3);
	if (strncmp(time_string, "mbt:", 4) == 0) return MidiFile_getTickFromMeasureBeatTickString(midi_file, time_string + 4);
	if (strncmp(time_string, "time:", 5) == 0) return MidiFile_getTickFromTime(midi_file, atof(time_string + 5));
	if (strncmp(time_string, "hms:", 4) == 0) MidiFile_getTickFromHourMinuteSecondString(midi_file, time_string + 4);
	if (strncmp(time_string, "hmsf:", 5) == 0) MidiFile_getTickFromHourMinuteSecondFrameString(midi_file, time_string + 5);
	if (strncmp(time_string, "marker:", 7) == 0) MidiFile_getTickFromMarker(midi_file, time_string + 7);
	return -1;
}

MidiFileEvent_t MidiFile_getFirstEventForTick(MidiFile_t midi_file, long tick)
{
	MidiFileEvent_t first_event_for_tick = NULL;
	MidiFileEvent_t event;

	for (event = MidiFile_getLastEvent(midi_file); (event != NULL) && (MidiFileEvent_getTick(event) >= tick); event = MidiFileEvent_getPreviousEventInFile(event))
	{
		if (MidiFileEvent_getTick(event) == tick) first_event_for_tick = event;
	}

	return first_event_for_tick;
}

MidiFileEvent_t MidiFile_getLastEventForTick(MidiFile_t midi_file, long tick)
{
	MidiFileEvent_t last_event_for_tick = NULL;
	MidiFileEvent_t event;

	for (event = MidiFile_getFirstEvent(midi_file); (event != NULL) && (MidiFileEvent_getTick(event) <= tick); event = MidiFileEvent_getNextEventInFile(event))
	{
		if (MidiFileEvent_getTick(event) == tick) last_event_for_tick = event;
	}

	return last_event_for_tick;
}

MidiFileEvent_t MidiFile_getLatestTempoEventForTick(MidiFile_t midi_file, long tick)
{
	MidiFileEvent_t tempo_event = NULL;
	MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
	MidiFileEvent_t event;

	for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) <= tick); event = MidiFileEvent_getNextEventInTrack(event))
	{
		if (MidiFileEvent_isTempoEvent(event)) tempo_event = event;
	}

	return tempo_event;
}

MidiFileEvent_t MidiFile_getLatestTimeSignatureEventForTick(MidiFile_t midi_file, long tick)
{
	MidiFileEvent_t time_signature_event = NULL;
	MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
	MidiFileEvent_t event;

	for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) <= tick); event = MidiFileEvent_getNextEventInTrack(event))
	{
		if (MidiFileEvent_isTimeSignatureEvent(event)) time_signature_event = event;
	}

	return time_signature_event;
}

MidiFileEvent_t MidiFile_getLatestKeySignatureEventForTick(MidiFile_t midi_file, long tick)
{
	MidiFileEvent_t key_signature_event = NULL;
	MidiFileTrack_t conductor_track = MidiFile_getFirstTrack(midi_file);
	MidiFileEvent_t event;

	for (event = MidiFileTrack_getFirstEvent(conductor_track); (event != NULL) && (MidiFileEvent_getTick(event) <= tick); event = MidiFileEvent_getNextEventInTrack(event))
	{
		if (MidiFileEvent_isKeySignatureEvent(event)) key_signature_event = event;
	}

	return key_signature_event;
}

int MidiFileTrack_delete(MidiFileTrack_t track)
{
	MidiFileTrack_t subsequent_track;
	MidiFileEvent_t event, next_event_in_track;

	if (track == NULL) return -1;

	for (subsequent_track = track->next_track; subsequent_track != NULL; subsequent_track = subsequent_track->next_track)
	{
		(subsequent_track->number)--;
	}
	
	(track->midi_file->number_of_tracks)--;

	if (track->previous_track == NULL)
	{
		track->midi_file->first_track = track->next_track;
	}
	else
	{
		track->previous_track->next_track = track->next_track;
	}

	if (track->next_track == NULL)
	{
		track->midi_file->last_track = track->previous_track;
	}
	else
	{
		track->next_track->previous_track = track->previous_track;
	}

	for (event = track->first_event; event != NULL; event = next_event_in_track)
	{
		next_event_in_track = event->next_event_in_track;
		MidiFileEvent_delete(event);
	}

	free(track);
	return 0;
}

MidiFile_t MidiFileTrack_getMidiFile(MidiFileTrack_t track)
{
	if (track == NULL) return NULL;
	return track->midi_file;
}

int MidiFileTrack_getNumber(MidiFileTrack_t track)
{
	if (track == NULL) return -1;
	return track->number;
}

long MidiFileTrack_getEndTick(MidiFileTrack_t track)
{
	if (track == NULL) return -1;
	return track->end_tick;
}

int MidiFileTrack_setEndTick(MidiFileTrack_t track, long end_tick)
{
	if ((track == NULL) || ((track->last_event != NULL) && (end_tick < track->last_event->tick))) return -1;
	track->end_tick = end_tick;
	return 0;
}

MidiFileEvent_t MidiFileTrack_getFirstEventForTick(MidiFileTrack_t track, long tick)
{
	MidiFileEvent_t first_event_for_tick = NULL;
	MidiFileEvent_t event;

	for (event = MidiFileTrack_getLastEvent(track); (event != NULL) && (MidiFileEvent_getTick(event) >= tick); event = MidiFileEvent_getPreviousEventInTrack(event))
	{
		if (MidiFileEvent_getTick(event) == tick) first_event_for_tick = event;
	}

	return first_event_for_tick;
}

MidiFileEvent_t MidiFileTrack_getLastEventForTick(MidiFileTrack_t track, long tick)
{
	MidiFileEvent_t last_event_for_tick = NULL;
	MidiFileEvent_t event;

	for (event = MidiFileTrack_getFirstEvent(track); (event != NULL) && (MidiFileEvent_getTick(event) <= tick); event = MidiFileEvent_getNextEventInTrack(event))
	{
		if (MidiFileEvent_getTick(event) == tick) last_event_for_tick = event;
	}

	return last_event_for_tick;
}

MidiFileTrack_t MidiFileTrack_createTrackBefore(MidiFileTrack_t track)
{
	MidiFileTrack_t new_track, subsequent_track;

	if (track == NULL) return NULL;

	new_track = (MidiFileTrack_t)(malloc(sizeof(struct MidiFileTrack)));
	new_track->midi_file = track->midi_file;
	new_track->number = track->number;
	new_track->end_tick = 0;
	new_track->previous_track = track->previous_track;
	new_track->next_track = track;
	track->previous_track = new_track;

	if (new_track->previous_track == NULL)
	{
		track->midi_file->first_track = new_track;
	}
	else
	{
		new_track->previous_track->next_track = new_track;
	}

	for (subsequent_track = track; subsequent_track != NULL; subsequent_track = subsequent_track->next_track)
	{
		(subsequent_track->number)++;
	}

	new_track->first_event = NULL;
	new_track->last_event = NULL;
	new_track->event_iterator_current = NULL;
	new_track->event_iterator_next = NULL;

	return new_track;
}

MidiFileTrack_t MidiFileTrack_getPreviousTrack(MidiFileTrack_t track)
{
	if (track == NULL) return NULL;
	return track->previous_track;
}

MidiFileTrack_t MidiFileTrack_getNextTrack(MidiFileTrack_t track)
{
	if (track == NULL) return NULL;
	return track->next_track;
}

MidiFileEvent_t MidiFileTrack_createNoteOffEvent(MidiFileTrack_t track, long tick, int channel, int note, int velocity)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_NOTE_OFF;
	new_event->u.note_off.channel = channel;
	new_event->u.note_off.note = note;
	new_event->u.note_off.velocity = velocity;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createNoteOnEvent(MidiFileTrack_t track, long tick, int channel, int note, int velocity)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_NOTE_ON;
	new_event->u.note_on.channel = channel;
	new_event->u.note_on.note = note;
	new_event->u.note_on.velocity = velocity;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createKeyPressureEvent(MidiFileTrack_t track, long tick, int channel, int note, int amount)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_KEY_PRESSURE;
	new_event->u.key_pressure.channel = channel;
	new_event->u.key_pressure.note = note;
	new_event->u.key_pressure.amount = amount;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createControlChangeEvent(MidiFileTrack_t track, long tick, int channel, int number, int value)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE;
	new_event->u.control_change.channel = channel;
	new_event->u.control_change.number = number;
	new_event->u.control_change.value = value;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createProgramChangeEvent(MidiFileTrack_t track, long tick, int channel, int number)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE;
	new_event->u.program_change.channel = channel;
	new_event->u.program_change.number = number;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createChannelPressureEvent(MidiFileTrack_t track, long tick, int channel, int amount)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE;
	new_event->u.channel_pressure.channel = channel;
	new_event->u.channel_pressure.amount = amount;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createPitchWheelEvent(MidiFileTrack_t track, long tick, int channel, int value)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_PITCH_WHEEL;
	new_event->u.pitch_wheel.channel = channel;
	new_event->u.pitch_wheel.value = value;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createSysexEvent(MidiFileTrack_t track, long tick, int data_length, unsigned char *data_buffer)
{
	MidiFileEvent_t new_event;

	if ((track == NULL) || (data_length < 1) || (data_buffer == NULL)) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_SYSEX;
	new_event->u.sysex.data_length = data_length;
	new_event->u.sysex.data_buffer = malloc(data_length);
	memcpy(new_event->u.sysex.data_buffer, data_buffer, data_length);
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createMetaEvent(MidiFileTrack_t track, long tick, int number, int data_length, unsigned char *data_buffer)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_META;
	new_event->u.meta.number = number;
	new_event->u.meta.data_length = data_length;
	new_event->u.meta.data_buffer = malloc(data_length + 1);
	memcpy(new_event->u.meta.data_buffer, data_buffer, data_length);
	new_event->u.meta.data_buffer[data_length] = '\0';
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createNoteEvent(MidiFileTrack_t track, long tick, long duration_ticks, int channel, int note, int velocity, int end_velocity)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_NOTE;
	new_event->u.note.duration_ticks = duration_ticks;
	new_event->u.note.channel = channel;
	new_event->u.note.note = note;
	new_event->u.note.velocity = velocity;
	new_event->u.note.end_velocity = end_velocity;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createFineControlChangeEvent(MidiFileTrack_t track, long tick, int channel, int coarse_number, int value)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE;
	new_event->u.fine_control_change.channel = channel;
	new_event->u.fine_control_change.coarse_number = coarse_number;
	new_event->u.fine_control_change.value = value;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createRpnEvent(MidiFileTrack_t track, long tick, int channel, int number, int value)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_RPN;
	new_event->u.rpn.channel = channel;
	new_event->u.rpn.number = number;
	new_event->u.rpn.value = value;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createNrpnEvent(MidiFileTrack_t track, long tick, int channel, int number, int value)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	new_event->type = MIDI_FILE_EVENT_TYPE_NRPN;
	new_event->u.nrpn.channel = channel;
	new_event->u.nrpn.number = number;
	new_event->u.nrpn.value = value;
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_createNoteStartAndEndEvents(MidiFileTrack_t track, long start_tick, long end_tick, int channel, int note, int start_velocity, int end_velocity)
{
	MidiFileEvent_t start_event = MidiFileTrack_createNoteOnEvent(track, start_tick, channel, note, start_velocity);
	MidiFileTrack_createNoteOffEvent(track, end_tick, channel, note, end_velocity);
	return start_event;
}

MidiFileEvent_t MidiFileTrack_createTextEvent(MidiFileTrack_t track, long tick, char *text)
{
	return MidiFileTrack_createMetaEvent(track, tick, 0x1, strlen(text), (unsigned char *)(text));
}

MidiFileEvent_t MidiFileTrack_createLyricEvent(MidiFileTrack_t track, long tick, char *text)
{
	return MidiFileTrack_createMetaEvent(track, tick, 0x5, strlen(text), (unsigned char *)(text));
}

MidiFileEvent_t MidiFileTrack_createMarkerEvent(MidiFileTrack_t track, long tick, char *text)
{
	return MidiFileTrack_createMetaEvent(track, tick, 0x6, strlen(text), (unsigned char *)(text));
}

MidiFileEvent_t MidiFileTrack_createPortEvent(MidiFileTrack_t track, long tick, char *name)
{
	return MidiFileTrack_createMetaEvent(track, tick, 0x9, strlen(name), (unsigned char *)(name));
}

MidiFileEvent_t MidiFileTrack_createTempoEvent(MidiFileTrack_t track, long tick, float tempo)
{
	long midi_tempo = (long)(60000000 / tempo);
	unsigned char buffer[3];
	buffer[0] = (midi_tempo >> 16) & 0xFF;
	buffer[1] = (midi_tempo >> 8) & 0xFF;
	buffer[2] = midi_tempo & 0xFF;
	return MidiFileTrack_createMetaEvent(track, tick, 0x51, 3, buffer);
}

MidiFileEvent_t MidiFileTrack_createTimeSignatureEvent(MidiFileTrack_t track, long tick, int numerator, int denominator)
{
	unsigned char buffer[4];
	buffer[0] = numerator & 0xFF;

	switch (denominator)
	{
		case 1:
		{
			buffer[1] = 0;
			break;
		}
		case 2:
		{
			buffer[1] = 1;
			break;
		}
		case 4:
		{
			buffer[1] = 2;
			break;
		}
		case 8:
		{
			buffer[1] = 3;
			break;
		}
		case 16:
		{
			buffer[1] = 4;
			break;
		}
		case 32:
		{
			buffer[1] = 5;
			break;
		}
		case 64:
		{
			buffer[1] = 6;
			break;
		}
		case 128:
		{
			buffer[1] = 7;
			break;
		}
		default:
		{
			return NULL;
		}
	}

	buffer[2] = 96 / denominator;
	buffer[3] = 8;
	return MidiFileTrack_createMetaEvent(track, tick, 0x58, 4, buffer);
}

MidiFileEvent_t MidiFileTrack_createKeySignatureEvent(MidiFileTrack_t track, long tick, int number, int minor)
{
	union
	{
		unsigned char unsigned_buffer[2];
		char signed_buffer[2];
	}
	u;

	u.signed_buffer[0] = number;
	u.signed_buffer[1] = minor ? 1 : 0;
	return MidiFileTrack_createMetaEvent(track, tick, 0x59, 2, u.unsigned_buffer);
}

MidiFileEvent_t MidiFileTrack_createVoiceEvent(MidiFileTrack_t track, long tick, unsigned long data)
{
	MidiFileEvent_t new_event;

	if (track == NULL) return NULL;

	new_event = (MidiFileEvent_t)(malloc(sizeof(struct MidiFileEvent)));
	new_event->track = track;
	new_event->tick = tick;
	MidiFileVoiceEvent_setData(new_event, data);
	new_event->should_be_visited = 0;
	new_event->is_selected = 0;
	add_event(new_event);

	return new_event;
}

MidiFileEvent_t MidiFileTrack_copyEvent(MidiFileTrack_t track, MidiFileEvent_t event)
{
	MidiFileEvent_t new_event = NULL;

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			new_event = MidiFileTrack_createNoteOffEvent(track, MidiFileEvent_getTick(event), MidiFileNoteOffEvent_getChannel(event), MidiFileNoteOffEvent_getNote(event), MidiFileNoteOffEvent_getVelocity(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			new_event = MidiFileTrack_createNoteOnEvent(track, MidiFileEvent_getTick(event), MidiFileNoteOnEvent_getChannel(event), MidiFileNoteOnEvent_getNote(event), MidiFileNoteOnEvent_getVelocity(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			new_event = MidiFileTrack_createKeyPressureEvent(track, MidiFileEvent_getTick(event), MidiFileKeyPressureEvent_getChannel(event), MidiFileKeyPressureEvent_getNote(event), MidiFileKeyPressureEvent_getAmount(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE:
		{
			new_event = MidiFileTrack_createControlChangeEvent(track, MidiFileEvent_getTick(event), MidiFileControlChangeEvent_getChannel(event), MidiFileControlChangeEvent_getNumber(event), MidiFileControlChangeEvent_getValue(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_PITCH_WHEEL:
		{
			new_event = MidiFileTrack_createPitchWheelEvent(track, MidiFileEvent_getTick(event), MidiFilePitchWheelEvent_getChannel(event), MidiFilePitchWheelEvent_getValue(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE:
		{
			new_event = MidiFileTrack_createProgramChangeEvent(track, MidiFileEvent_getTick(event), MidiFileProgramChangeEvent_getChannel(event), MidiFileProgramChangeEvent_getNumber(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			new_event = MidiFileTrack_createChannelPressureEvent(track, MidiFileEvent_getTick(event), MidiFileChannelPressureEvent_getChannel(event), MidiFileChannelPressureEvent_getAmount(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_SYSEX:
		{
			new_event = MidiFileTrack_createSysexEvent(track, MidiFileEvent_getTick(event), MidiFileSysexEvent_getDataLength(event), MidiFileSysexEvent_getData(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_META:
		{
			new_event = MidiFileTrack_createMetaEvent(track, MidiFileEvent_getTick(event), MidiFileMetaEvent_getNumber(event), MidiFileMetaEvent_getDataLength(event), MidiFileMetaEvent_getData(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_NOTE:
		{
			new_event = MidiFileTrack_createNoteEvent(track, MidiFileEvent_getTick(event), MidiFileNoteEvent_getDurationTicks(event), MidiFileNoteEvent_getChannel(event), MidiFileNoteEvent_getNote(event), MidiFileNoteEvent_getVelocity(event), MidiFileNoteEvent_getEndVelocity(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE:
		{
			new_event = MidiFileTrack_createFineControlChangeEvent(track, MidiFileEvent_getTick(event), MidiFileFineControlChangeEvent_getChannel(event), MidiFileFineControlChangeEvent_getCoarseNumber(event), MidiFileFineControlChangeEvent_getValue(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_RPN:
		{
			new_event = MidiFileTrack_createRpnEvent(track, MidiFileEvent_getTick(event), MidiFileRpnEvent_getChannel(event), MidiFileRpnEvent_getNumber(event), MidiFileRpnEvent_getValue(event));
			break;
		}
		case MIDI_FILE_EVENT_TYPE_NRPN:
		{
			new_event = MidiFileTrack_createNrpnEvent(track, MidiFileEvent_getTick(event), MidiFileNrpnEvent_getChannel(event), MidiFileNrpnEvent_getNumber(event), MidiFileNrpnEvent_getValue(event));
			break;
		}
		default:
		{
			break;
		}
	}

	MidiFileEvent_setSelected(new_event, MidiFileEvent_isSelected(event));
	return new_event;
}

MidiFileEvent_t MidiFileTrack_getFirstEvent(MidiFileTrack_t track)
{
	if (track == NULL) return NULL;
	return track->first_event;
}

MidiFileEvent_t MidiFileTrack_getLastEvent(MidiFileTrack_t track)
{
	if (track == NULL) return NULL;
	return track->last_event;
}

MidiFileEvent_t MidiFileTrack_iterateEvents(MidiFileTrack_t track)
{
	if (track == NULL) return NULL;

	if (track->event_iterator_current == NULL)
	{
		for (track->event_iterator_current = MidiFileTrack_getFirstEvent(track); track->event_iterator_current != NULL; track->event_iterator_current = MidiFileEvent_getNextEventInTrack(track->event_iterator_current)) track->event_iterator_current->should_be_visited = 1;
		track->event_iterator_current = MidiFileTrack_getFirstEvent(track);
	}
	else
	{
		track->event_iterator_current = track->event_iterator_next;
	}

	while ((track->event_iterator_current != NULL) && !(track->event_iterator_current->should_be_visited))
	{
		track->event_iterator_current = MidiFileEvent_getNextEventInTrack(track->event_iterator_current);
	}

	if (track->event_iterator_current == NULL)
	{
		track->event_iterator_next = NULL;
	}
	else
	{
		track->event_iterator_current->should_be_visited = 0;
		track->event_iterator_next = MidiFileEvent_getNextEventInTrack(track->event_iterator_current);
	}

	return track->event_iterator_current;
}

int MidiFileTrack_visitEvents(MidiFileTrack_t track, MidiFileEventVisitorCallback_t visitor_callback, void *user_data)
{
	MidiFileEvent_t event;

	if ((track == NULL) || (visitor_callback == NULL)) return -1;

	for (event = MidiFileTrack_iterateEvents(track); event != NULL; event = MidiFileTrack_iterateEvents(track))
	{
		(*visitor_callback)(event, user_data);
	}

	return 0;
}

int MidiFileEvent_delete(MidiFileEvent_t event)
{
	if (event == NULL) return -1;
	if (event->track != NULL) remove_event(event);

	switch (event->type)
	{
		case MIDI_FILE_EVENT_TYPE_SYSEX:
		{
			free(event->u.sysex.data_buffer);
			break;
		}
		case MIDI_FILE_EVENT_TYPE_META:
		{
			free(event->u.meta.data_buffer);
			break;
		}
		default:
		{
			break;
		}
	}

	free(event);
	return 0;
}

int MidiFileEvent_detach(MidiFileEvent_t event)
{
	if (event == NULL) return -1;

	if (event->track != NULL)
	{
		remove_event(event);
		event->track = NULL;
		event->previous_event_in_track = NULL;
		event->next_event_in_track = NULL;
		event->previous_event_in_file = NULL;
		event->next_event_in_file = NULL;
	}

	return 0;
}

MidiFileTrack_t MidiFileEvent_getTrack(MidiFileEvent_t event)
{
	if (event == NULL) return NULL;
	return event->track;
}

int MidiFileEvent_setTrack(MidiFileEvent_t event, MidiFileTrack_t track)
{
	if ((event == NULL) || (track == NULL)) return -1;

	if (event->track != track)
	{
		if (event->track != NULL) remove_event(event);
		event->track = track;
		add_event(event);
	}

	return 0;
}

MidiFileEvent_t MidiFileEvent_getPreviousEvent(MidiFileEvent_t event)
{
	return MidiFileEvent_getPreviousEventInTrack(event);
}

MidiFileEvent_t MidiFileEvent_getNextEvent(MidiFileEvent_t event)
{
	return MidiFileEvent_getNextEventInTrack(event);
}

int MidiFileEvent_setPreviousEvent(MidiFileEvent_t event, MidiFileEvent_t previous_event)
{
	if ((event == NULL) || (event == previous_event)) return -1;
	remove_event(event);

	if (previous_event != NULL)
	{
		event->track = previous_event->track;
		event->tick = previous_event->tick;
	}

	add_event_after(event, previous_event);
	return 0;
}

int MidiFileEvent_setNextEvent(MidiFileEvent_t event, MidiFileEvent_t next_event)
{
	if ((event == NULL) || (event == next_event)) return -1;
	remove_event(event);

	if (next_event != NULL)
	{
		event->track = next_event->track;
		event->tick = next_event->tick;
	}

	add_event_before(event, next_event);
	return 0;
}

MidiFileEvent_t MidiFileEvent_getPreviousEventInTrack(MidiFileEvent_t event)
{
	if (event == NULL) return NULL;
	return event->previous_event_in_track;
}

MidiFileEvent_t MidiFileEvent_getNextEventInTrack(MidiFileEvent_t event)
{
	if (event == NULL) return NULL;
	return event->next_event_in_track;
}

MidiFileEvent_t MidiFileEvent_getPreviousEventInFile(MidiFileEvent_t event)
{
	if (event == NULL) return NULL;
	return event->previous_event_in_file;
}

MidiFileEvent_t MidiFileEvent_getNextEventInFile(MidiFileEvent_t event)
{
	if (event == NULL) return NULL;
	return event->next_event_in_file;
}

long MidiFileEvent_getTick(MidiFileEvent_t event)
{
	if (event == NULL) return -1;
	return event->tick;
}

int MidiFileEvent_setTick(MidiFileEvent_t event, long tick)
{
	if (event == NULL) return -1;
	if (event->track != NULL) remove_event(event);
	event->tick = tick;
	if (event->track != NULL) add_event(event);
	return 0;
}

MidiFileEventType_t MidiFileEvent_getType(MidiFileEvent_t event)
{
	if (event == NULL) return MIDI_FILE_EVENT_TYPE_INVALID;
	return event->type;
}

int MidiFileEvent_isSelected(MidiFileEvent_t event)
{
	if (event == NULL) return 0;
	return event->is_selected;
}

int MidiFileEvent_setSelected(MidiFileEvent_t event, int is_selected)
{
	if (event == NULL) return -1;
	event->is_selected = is_selected;
	return 0;
}

int MidiFileEvent_isNoteStartEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_NOTE_ON) && (MidiFileNoteOnEvent_getVelocity(event) > 0));
}

int MidiFileEvent_isNoteEndEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_NOTE_OFF) || ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_NOTE_ON) && (MidiFileNoteOnEvent_getVelocity(event) == 0)));
}

int MidiFileEvent_isPressureEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_KEY_PRESSURE) || (MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE));
}

int MidiFileEvent_isTextEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x1));
}

int MidiFileEvent_isLyricEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x5));
}

int MidiFileEvent_isMarkerEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x6));
}

int MidiFileEvent_isPortEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x9));
}

int MidiFileEvent_isTempoEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x51));
}

int MidiFileEvent_isTimeSignatureEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x58));
}

int MidiFileEvent_isKeySignatureEvent(MidiFileEvent_t event)
{
	return ((MidiFileEvent_getType(event) == MIDI_FILE_EVENT_TYPE_META) && (MidiFileMetaEvent_getNumber(event) == 0x59));
}

int MidiFileEvent_isVoiceEvent(MidiFileEvent_t event)
{
	switch (event->type)
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		case MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE:
		case MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE:
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		case MIDI_FILE_EVENT_TYPE_PITCH_WHEEL:
		{
			return 1;
		}
		default:
		{
			return 0;
		}
	}
}

int MidiFileNoteOffEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_OFF)) return -1;
	return event->u.note_off.channel;
}

int MidiFileNoteOffEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_OFF)) return -1;
	event->u.note_off.channel = channel;
	return 0;
}

int MidiFileNoteOffEvent_getNote(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_OFF)) return -1;
	return event->u.note_off.note;
}

int MidiFileNoteOffEvent_setNote(MidiFileEvent_t event, int note)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_OFF)) return -1;
	event->u.note_off.note = note;
	return 0;
}

int MidiFileNoteOffEvent_getVelocity(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_OFF)) return -1;
	return event->u.note_off.velocity;
}

int MidiFileNoteOffEvent_setVelocity(MidiFileEvent_t event, int velocity)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_OFF)) return -1;
	event->u.note_off.velocity = velocity;
	return 0;
}

int MidiFileNoteOnEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_ON)) return -1;
	return event->u.note_on.channel;
}

int MidiFileNoteOnEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_ON)) return -1;
	event->u.note_on.channel = channel;
	return 0;
}

int MidiFileNoteOnEvent_getNote(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_ON)) return -1;
	return event->u.note_on.note;
}

int MidiFileNoteOnEvent_setNote(MidiFileEvent_t event, int note)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_ON)) return -1;
	event->u.note_on.note = note;
	return 0;
}

int MidiFileNoteOnEvent_getVelocity(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_ON)) return -1;
	return event->u.note_on.velocity;
}

int MidiFileNoteOnEvent_setVelocity(MidiFileEvent_t event, int velocity)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE_ON)) return -1;
	event->u.note_on.velocity = velocity;
	return 0;
}

int MidiFileKeyPressureEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_KEY_PRESSURE)) return -1;
	return event->u.key_pressure.channel;
}

int MidiFileKeyPressureEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_KEY_PRESSURE)) return -1;
	event->u.key_pressure.channel = channel;
	return 0;
}

int MidiFileKeyPressureEvent_getNote(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_KEY_PRESSURE)) return -1;
	return event->u.key_pressure.note;
}

int MidiFileKeyPressureEvent_setNote(MidiFileEvent_t event, int note)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_KEY_PRESSURE)) return -1;
	event->u.key_pressure.note = note;
	return 0;
}

int MidiFileKeyPressureEvent_getAmount(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_KEY_PRESSURE)) return -1;
	return event->u.key_pressure.amount;
}

int MidiFileKeyPressureEvent_setAmount(MidiFileEvent_t event, int amount)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_KEY_PRESSURE)) return -1;
	event->u.key_pressure.amount = amount;
	return 0;
}

int MidiFileControlChangeEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)) return -1;
	return event->u.control_change.channel;
}

int MidiFileControlChangeEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)) return -1;
	event->u.control_change.channel = channel;
	return 0;
}

int MidiFileControlChangeEvent_getNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)) return -1;
	return event->u.control_change.number;
}

int MidiFileControlChangeEvent_setNumber(MidiFileEvent_t event, int number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)) return -1;
	event->u.control_change.number = number;
	return 0;
}

int MidiFileControlChangeEvent_getValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)) return -1;
	return event->u.control_change.value;
}

int MidiFileControlChangeEvent_setValue(MidiFileEvent_t event, int value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE)) return -1;
	event->u.control_change.value = value;
	return 0;
}

int MidiFileProgramChangeEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE)) return -1;
	return event->u.program_change.channel;
}

int MidiFileProgramChangeEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE)) return -1;
	event->u.program_change.channel = channel;
	return 0;
}

int MidiFileProgramChangeEvent_getNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE)) return -1;
	return event->u.program_change.number;
}

int MidiFileProgramChangeEvent_setNumber(MidiFileEvent_t event, int number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE)) return -1;
	event->u.program_change.number = number;
	return 0;
}

int MidiFileChannelPressureEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE)) return -1;
	return event->u.channel_pressure.channel;
}

int MidiFileChannelPressureEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE)) return -1;
	event->u.channel_pressure.channel = channel;
	return 0;
}

int MidiFileChannelPressureEvent_getAmount(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE)) return -1;
	return event->u.channel_pressure.amount;
}

int MidiFileChannelPressureEvent_setAmount(MidiFileEvent_t event, int amount)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE)) return -1;
	event->u.channel_pressure.amount = amount;
	return 0;
}

int MidiFilePitchWheelEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PITCH_WHEEL)) return -1;
	return event->u.pitch_wheel.channel;
}

int MidiFilePitchWheelEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PITCH_WHEEL)) return -1;
	event->u.pitch_wheel.channel = channel;
	return 0;
}

int MidiFilePitchWheelEvent_getValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PITCH_WHEEL)) return -1;
	return event->u.pitch_wheel.value;
}

int MidiFilePitchWheelEvent_setValue(MidiFileEvent_t event, int value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_PITCH_WHEEL)) return -1;
	event->u.pitch_wheel.value = value;
	return 0;
}

int MidiFileSysexEvent_getDataLength(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_SYSEX)) return -1;
	return event->u.sysex.data_length;
}

unsigned char *MidiFileSysexEvent_getData(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_SYSEX)) return NULL;
	return event->u.sysex.data_buffer;
}

int MidiFileSysexEvent_setData(MidiFileEvent_t event, int data_length, unsigned char *data_buffer)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_SYSEX) || (data_length < 1) || (data_buffer == NULL)) return -1;
	free(event->u.sysex.data_buffer);
	event->u.sysex.data_length = data_length;
	event->u.sysex.data_buffer = malloc(data_length);
	memcpy(event->u.sysex.data_buffer, data_buffer, data_length);
	return 0;
}

int MidiFileMetaEvent_getNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_META)) return -1;
	return event->u.meta.number;
}

int MidiFileMetaEvent_setNumber(MidiFileEvent_t event, int number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_META)) return -1;
	event->u.meta.number = number;
	return 0;
}

int MidiFileMetaEvent_getDataLength(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_META)) return -1;
	return event->u.meta.data_length;
}

unsigned char *MidiFileMetaEvent_getData(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_META)) return NULL;
	return event->u.meta.data_buffer;
}

int MidiFileMetaEvent_setData(MidiFileEvent_t event, int data_length, unsigned char *data_buffer)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_META) || (data_buffer == NULL)) return -1;
	free(event->u.meta.data_buffer);
	event->u.meta.data_length = data_length;
	event->u.meta.data_buffer = malloc(data_length + 1);
	memcpy(event->u.meta.data_buffer, data_buffer, data_length);
	event->u.meta.data_buffer[data_length] = '\0';
	return 0;
}

long MidiFileNoteEvent_getDurationTicks(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	return event->u.note.duration_ticks;
}

int MidiFileNoteEvent_setDurationTicks(MidiFileEvent_t event, long duration_ticks)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	event->u.note.duration_ticks = duration_ticks;
	return 0;
}

int MidiFileNoteEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	return event->u.note.channel;
}

int MidiFileNoteEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	event->u.note.channel = channel;
	return 0;
}

int MidiFileNoteEvent_getNote(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	return event->u.note.note;
}

int MidiFileNoteEvent_setNote(MidiFileEvent_t event, int note)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	event->u.note.note = note;
	return 0;
}

int MidiFileNoteEvent_getVelocity(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	return event->u.note.velocity;
}

int MidiFileNoteEvent_setVelocity(MidiFileEvent_t event, int velocity)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	event->u.note.velocity = velocity;
	return 0;
}

int MidiFileNoteEvent_getEndVelocity(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	return event->u.note.end_velocity;
}

int MidiFileNoteEvent_setEndVelocity(MidiFileEvent_t event, int end_velocity)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NOTE)) return -1;
	event->u.note.end_velocity = end_velocity;
	return 0;
}

int MidiFileFineControlChangeEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	return event->u.fine_control_change.channel;
}

int MidiFileFineControlChangeEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	event->u.fine_control_change.channel = channel;
	return 0;
}

int MidiFileFineControlChangeEvent_getCoarseNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	return event->u.fine_control_change.coarse_number;
}

int MidiFileFineControlChangeEvent_setCoarseNumber(MidiFileEvent_t event, int coarse_number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	event->u.fine_control_change.coarse_number = coarse_number;
	return 0;
}

int MidiFileFineControlChangeEvent_getFineNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	return event->u.fine_control_change.coarse_number + 32;
}

int MidiFileFineControlChangeEvent_setFineNumber(MidiFileEvent_t event, int fine_number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	event->u.fine_control_change.coarse_number = fine_number - 32;
	return 0;
}

int MidiFileFineControlChangeEvent_getValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	return event->u.fine_control_change.value;
}

int MidiFileFineControlChangeEvent_setValue(MidiFileEvent_t event, int value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	event->u.fine_control_change.value = value;
	return 0;
}

int MidiFileFineControlChangeEvent_getCoarseValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	return ((event->u.fine_control_change.value >> 7) & 0x7F);
}

int MidiFileFineControlChangeEvent_setCoarseValue(MidiFileEvent_t event, int coarse_value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	event->u.fine_control_change.value = (coarse_value << 7) | (event->u.fine_control_change.value & 0x7F);
	return 0;
}

int MidiFileFineControlChangeEvent_getFineValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	return (event->u.fine_control_change.value & 0x7F);
}

int MidiFileFineControlChangeEvent_setFineValue(MidiFileEvent_t event, int fine_value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_FINE_CONTROL_CHANGE)) return -1;
	event->u.fine_control_change.value = (event->u.fine_control_change.value & ~0x7F) | (fine_value & 0x7F);
	return 0;
}

int MidiFileRpnEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return event->u.rpn.channel;
}

int MidiFileRpnEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.channel = channel;
	return 0;
}

int MidiFileRpnEvent_getNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return event->u.rpn.number;
}

int MidiFileRpnEvent_setNumber(MidiFileEvent_t event, int number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.number = number;
	return 0;
}

int MidiFileRpnEvent_getCoarseNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return ((event->u.rpn.number >> 7) & 0x7F);
}

int MidiFileRpnEvent_setCoarseNumber(MidiFileEvent_t event, int coarse_number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.number = (coarse_number << 7) | (event->u.rpn.number & 0x7F);
	return 0;
}

int MidiFileRpnEvent_getFineNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return (event->u.rpn.number & 0x7F);
}

int MidiFileRpnEvent_setFineNumber(MidiFileEvent_t event, int fine_number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.number = (event->u.rpn.number & ~0x7F) | (fine_number & 0x7F);
	return 0;
}

int MidiFileRpnEvent_getValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return event->u.rpn.value;
}

int MidiFileRpnEvent_setValue(MidiFileEvent_t event, int value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.value = value;
	return 0;
}

int MidiFileRpnEvent_getCoarseValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return ((event->u.rpn.value >> 7) & 0x7F);
}

int MidiFileRpnEvent_setCoarseValue(MidiFileEvent_t event, int coarse_value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.value = (coarse_value << 7) | (event->u.rpn.value & 0x7F);
	return 0;
}

int MidiFileRpnEvent_getFineValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	return (event->u.rpn.value & 0x7F);
}

int MidiFileRpnEvent_setFineValue(MidiFileEvent_t event, int fine_value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_RPN)) return -1;
	event->u.rpn.value = (event->u.rpn.value & ~0x7F) | (fine_value & 0x7F);
	return 0;
}

int MidiFileNrpnEvent_getChannel(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return event->u.nrpn.channel;
}

int MidiFileNrpnEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.channel = channel;
	return 0;
}

int MidiFileNrpnEvent_getNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return event->u.nrpn.number;
}

int MidiFileNrpnEvent_setNumber(MidiFileEvent_t event, int number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.number = number;
	return 0;
}

int MidiFileNrpnEvent_getCoarseNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return ((event->u.nrpn.number >> 7) & 0x7F);
}

int MidiFileNrpnEvent_setCoarseNumber(MidiFileEvent_t event, int coarse_number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.number = (coarse_number << 7) | (event->u.nrpn.number & 0x7F);
	return 0;
}

int MidiFileNrpnEvent_getFineNumber(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return (event->u.nrpn.number & 0x7F);
}

int MidiFileNrpnEvent_setFineNumber(MidiFileEvent_t event, int fine_number)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.number = (event->u.nrpn.number & ~0x7F) | (fine_number & 0x7F);
	return 0;
}

int MidiFileNrpnEvent_getValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return event->u.nrpn.value;
}

int MidiFileNrpnEvent_setValue(MidiFileEvent_t event, int value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.value = value;
	return 0;
}

int MidiFileNrpnEvent_getCoarseValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return ((event->u.nrpn.value >> 7) & 0x7F);
}

int MidiFileNrpnEvent_setCoarseValue(MidiFileEvent_t event, int coarse_value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.value = (coarse_value << 7) | (event->u.nrpn.value & 0x7F);
	return 0;
}

int MidiFileNrpnEvent_getFineValue(MidiFileEvent_t event)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	return (event->u.nrpn.value & 0x7F);
}

int MidiFileNrpnEvent_setFineValue(MidiFileEvent_t event, int fine_value)
{
	if ((event == NULL) || (event->type != MIDI_FILE_EVENT_TYPE_NRPN)) return -1;
	event->u.nrpn.value = (event->u.nrpn.value & ~0x7F) | (fine_value & 0x7F);
	return 0;
}

int MidiFileNoteStartEvent_getChannel(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isNoteStartEvent(event)) return -1;
	return MidiFileNoteOnEvent_getChannel(event);
}

int MidiFileNoteStartEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if (! MidiFileEvent_isNoteStartEvent(event)) return -1;

	{
		MidiFileEvent_t end_event = MidiFileNoteStartEvent_getNoteEndEvent(event);

		switch (MidiFileEvent_getType(end_event))
		{
			case MIDI_FILE_EVENT_TYPE_NOTE_ON:
			{
				MidiFileNoteOnEvent_setChannel(end_event, channel);
				break;
			}
			case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
			{
				MidiFileNoteOffEvent_setChannel(end_event, channel);
				break;
			}
			default:
			{
				break;
			}
		}
	}

	return MidiFileNoteOnEvent_setChannel(event, channel);
}

int MidiFileNoteStartEvent_getNote(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isNoteStartEvent(event)) return -1;
	return MidiFileNoteOnEvent_getNote(event);
}

int MidiFileNoteStartEvent_setNote(MidiFileEvent_t event, int note)
{
	if (! MidiFileEvent_isNoteStartEvent(event)) return -1;

	{
		MidiFileEvent_t end_event = MidiFileNoteStartEvent_getNoteEndEvent(event);

		switch (MidiFileEvent_getType(end_event))
		{
			case MIDI_FILE_EVENT_TYPE_NOTE_ON:
			{
				MidiFileNoteOnEvent_setNote(end_event, note);
				break;
			}
			case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
			{
				MidiFileNoteOffEvent_setNote(end_event, note);
				break;
			}
			default:
			{
				break;
			}
		}
	}

	return MidiFileNoteOnEvent_setNote(event, note);
}

int MidiFileNoteStartEvent_getVelocity(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isNoteStartEvent(event)) return -1;
	return MidiFileNoteOnEvent_getVelocity(event);
}

int MidiFileNoteStartEvent_setVelocity(MidiFileEvent_t event, int velocity)
{
	if (! MidiFileEvent_isNoteStartEvent(event)) return -1;
	return MidiFileNoteOnEvent_setVelocity(event, velocity);
}

MidiFileEvent_t MidiFileNoteStartEvent_getNoteEndEvent(MidiFileEvent_t event)
{
	MidiFileEvent_t subsequent_event;

	if (! MidiFileEvent_isNoteStartEvent(event)) return NULL;

	for (subsequent_event = MidiFileEvent_getNextEventInTrack(event); subsequent_event != NULL; subsequent_event = MidiFileEvent_getNextEventInTrack(subsequent_event))
	{
		if (MidiFileEvent_isNoteEndEvent(subsequent_event) && (MidiFileNoteEndEvent_getChannel(subsequent_event) == MidiFileNoteStartEvent_getChannel(event)) && (MidiFileNoteEndEvent_getNote(subsequent_event) == MidiFileNoteStartEvent_getNote(event)))
		{
			return subsequent_event;
		}
	}

	return NULL;
}

int MidiFileNoteEndEvent_getChannel(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isNoteEndEvent(event)) return -1;

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			return MidiFileNoteOnEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_getChannel(event);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileNoteEndEvent_setChannel(MidiFileEvent_t event, int channel)
{
	if (! MidiFileEvent_isNoteEndEvent(event)) return -1;
	MidiFileNoteOnEvent_setChannel(MidiFileNoteEndEvent_getNoteStartEvent(event), channel);

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			return MidiFileNoteOnEvent_setChannel(event, channel);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_setChannel(event, channel);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileNoteEndEvent_getNote(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isNoteEndEvent(event)) return -1;

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			return MidiFileNoteOnEvent_getNote(event);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_getNote(event);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileNoteEndEvent_setNote(MidiFileEvent_t event, int note)
{
	if (! MidiFileEvent_isNoteEndEvent(event)) return -1;
	MidiFileNoteOnEvent_setNote(MidiFileNoteEndEvent_getNoteStartEvent(event), note);

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			return MidiFileNoteOnEvent_setNote(event, note);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_setNote(event, note);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileNoteEndEvent_getVelocity(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isNoteEndEvent(event)) return -1;

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_getVelocity(event);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			return 0;
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileNoteEndEvent_setVelocity(MidiFileEvent_t event, int velocity)
{
	if (! MidiFileEvent_isNoteEndEvent(event)) return -1;

	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_setVelocity(event, velocity);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			if (velocity == 0) return 0;
			event->type = MIDI_FILE_EVENT_TYPE_NOTE_OFF;
			return MidiFileNoteOffEvent_setVelocity(event, velocity);
		}
		default:
		{
			return -1;
		}
	}
}

MidiFileEvent_t MidiFileNoteEndEvent_getNoteStartEvent(MidiFileEvent_t event)
{
	MidiFileEvent_t preceding_event;

	if (! MidiFileEvent_isNoteEndEvent(event)) return NULL;

	for (preceding_event = MidiFileEvent_getPreviousEventInTrack(event); preceding_event != NULL; preceding_event = MidiFileEvent_getPreviousEventInTrack(preceding_event))
	{
		if (MidiFileEvent_isNoteStartEvent(preceding_event) && (MidiFileNoteStartEvent_getChannel(preceding_event) == MidiFileNoteEndEvent_getChannel(event)) && (MidiFileNoteStartEvent_getNote(preceding_event) == MidiFileNoteEndEvent_getNote(event)))
		{
			return preceding_event;
		}
	}

	return NULL;
}

int MidiFilePressureEvent_getChannel(MidiFileEvent_t event)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			return MidiFileKeyPressureEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			return MidiFileChannelPressureEvent_getChannel(event);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFilePressureEvent_setChannel(MidiFileEvent_t event, int channel)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			return MidiFileKeyPressureEvent_setChannel(event, channel);
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			return MidiFileChannelPressureEvent_setChannel(event, channel);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFilePressureEvent_getNote(MidiFileEvent_t event)
{
	return MidiFileKeyPressureEvent_getNote(event);
}

int MidiFilePressureEvent_setNote(MidiFileEvent_t event, int note)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			if (note < 0)
			{
				int amount = MidiFileKeyPressureEvent_getAmount(event);
				event->type = MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE;
				return MidiFileChannelPressureEvent_setAmount(event, amount);
			}
			else
			{
				return MidiFileKeyPressureEvent_setNote(event, note);
			}
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			if (note < 0)
			{
				return 0;
			}
			else
			{
				int amount = MidiFileChannelPressureEvent_getAmount(event);
				event->type = MIDI_FILE_EVENT_TYPE_KEY_PRESSURE;
				MidiFileKeyPressureEvent_setAmount(event, amount);
				return MidiFileKeyPressureEvent_setNote(event, note);
			}
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFilePressureEvent_getAmount(MidiFileEvent_t event)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			return MidiFileKeyPressureEvent_getAmount(event);
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			return MidiFileChannelPressureEvent_getAmount(event);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFilePressureEvent_setAmount(MidiFileEvent_t event, int amount)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			return MidiFileKeyPressureEvent_setAmount(event, amount);
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			return MidiFileChannelPressureEvent_setAmount(event, amount);
		}
		default:
		{
			return -1;
		}
	}
}

char *MidiFileTextEvent_getText(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isTextEvent(event)) return NULL;
	return (char *)(MidiFileMetaEvent_getData(event));
}

int MidiFileTextEvent_setText(MidiFileEvent_t event, char *text)
{
	if (! MidiFileEvent_isTextEvent(event)) return -1;
	return MidiFileMetaEvent_setData(event, strlen(text), (unsigned char *)(text));
}

char *MidiFileLyricEvent_getText(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isLyricEvent(event)) return NULL;
	return (char *)(MidiFileMetaEvent_getData(event));
}

int MidiFileLyricEvent_setText(MidiFileEvent_t event, char *text)
{
	if (! MidiFileEvent_isLyricEvent(event)) return -1;
	return MidiFileMetaEvent_setData(event, strlen(text), (unsigned char *)(text));
}

char *MidiFileMarkerEvent_getText(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isMarkerEvent(event)) return NULL;
	return (char *)(MidiFileMetaEvent_getData(event));
}

int MidiFileMarkerEvent_setText(MidiFileEvent_t event, char *text)
{
	if (! MidiFileEvent_isMarkerEvent(event)) return -1;
	return MidiFileMetaEvent_setData(event, strlen(text), (unsigned char *)(text));
}

char *MidiFilePortEvent_getName(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isPortEvent(event)) return NULL;
	return (char *)(MidiFileMetaEvent_getData(event));
}

int MidiFilePortEvent_setName(MidiFileEvent_t event, char *name)
{
	if (! MidiFileEvent_isPortEvent(event)) return -1;
	return MidiFileMetaEvent_setData(event, strlen(name), (unsigned char *)(name));
}

float MidiFileTempoEvent_getTempo(MidiFileEvent_t event)
{
	unsigned char *buffer;
	long midi_tempo;

	if (! MidiFileEvent_isTempoEvent(event)) return 120.0;

	buffer = MidiFileMetaEvent_getData(event);
	midi_tempo = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
	return (float)(60000000.0 / midi_tempo);
}

int MidiFileTempoEvent_setTempo(MidiFileEvent_t event, float tempo)
{
	long midi_tempo;
	unsigned char buffer[3];

	if (! MidiFileEvent_isTempoEvent(event)) return -1;

	midi_tempo = (long)(60000000 / tempo);
	buffer[0] = (midi_tempo >> 16) & 0xFF;
	buffer[1] = (midi_tempo >> 8) & 0xFF;
	buffer[2] = midi_tempo & 0xFF;
	return MidiFileMetaEvent_setData(event, 3, buffer);
}

int MidiFileTimeSignatureEvent_getNumerator(MidiFileEvent_t event)
{
	unsigned char *buffer;

	if (! MidiFileEvent_isTimeSignatureEvent(event)) return 4;

	buffer = MidiFileMetaEvent_getData(event);
	return (int)(buffer[0]);
}

int MidiFileTimeSignatureEvent_getDenominator(MidiFileEvent_t event)
{
	unsigned char *buffer;

	if (! MidiFileEvent_isTimeSignatureEvent(event)) return 4;

	buffer = MidiFileMetaEvent_getData(event);

	switch (buffer[1])
	{
		case 0:
		{
			return 1;
		}
		case 1:
		{
			return 2;
		}
		case 2:
		{
			return 4;
		}
		case 3:
		{
			return 8;
		}
		case 4:
		{
			return 16;
		}
		case 5:
		{
			return 32;
		}
		case 6:
		{
			return 64;
		}
		case 7:
		{
			return 128;
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileTimeSignatureEvent_setTimeSignature(MidiFileEvent_t event, int numerator, int denominator)
{
	unsigned char buffer[4];
	buffer[0] = numerator & 0xFF;

	switch (denominator)
	{
		case 1:
		{
			buffer[1] = 0;
			break;
		}
		case 2:
		{
			buffer[1] = 1;
			break;
		}
		case 4:
		{
			buffer[1] = 2;
			break;
		}
		case 8:
		{
			buffer[1] = 3;
			break;
		}
		case 16:
		{
			buffer[1] = 4;
			break;
		}
		case 32:
		{
			buffer[1] = 5;
			break;
		}
		case 64:
		{
			buffer[1] = 6;
			break;
		}
		case 128:
		{
			buffer[1] = 7;
			break;
		}
		default:
		{
			return -1;
		}
	}

	buffer[2] = 96 / denominator;
	buffer[3] = 8;
	return MidiFileMetaEvent_setData(event, 4, buffer);
}

int MidiFileKeySignatureEvent_getNumber(MidiFileEvent_t event)
{
	union
	{
		unsigned char unsigned_value;
		char signed_value;
	}
	u;

	if (! MidiFileEvent_isKeySignatureEvent(event)) return 0;
	u.unsigned_value = MidiFileMetaEvent_getData(event)[0];
	return u.signed_value;
}

int MidiFileKeySignatureEvent_isMinor(MidiFileEvent_t event)
{
	if (! MidiFileEvent_isKeySignatureEvent(event)) return 0;
	return MidiFileMetaEvent_getData(event)[1];
}

int MidiFileKeySignatureEvent_setKeySignature(MidiFileEvent_t event, int number, int minor)
{
	union
	{
		unsigned char unsigned_buffer[2];
		char signed_buffer[2];
	}
	u;

	u.signed_buffer[0] = number;
	u.signed_buffer[1] = minor ? 1 : 0;
	return MidiFileMetaEvent_setData(event, 2, u.unsigned_buffer);
}

int MidiFileVoiceEvent_getChannel(MidiFileEvent_t event)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			return MidiFileNoteOffEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			return MidiFileNoteOnEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			return MidiFileKeyPressureEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE:
		{
			return MidiFileControlChangeEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_PITCH_WHEEL:
		{
			return MidiFilePitchWheelEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE:
		{
			return MidiFileProgramChangeEvent_getChannel(event);
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			return MidiFileChannelPressureEvent_getChannel(event);
		}
		default:
		{
			return -1;
		}
	}
}

int MidiFileVoiceEvent_getDataLength(MidiFileEvent_t event)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		case MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE:
		case MIDI_FILE_EVENT_TYPE_PITCH_WHEEL:
		{
			return 3;
		}
		case MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE:
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			return 2;
		}
		default:
		{
			return 0;
		}
	}
}

unsigned long MidiFileVoiceEvent_getData(MidiFileEvent_t event)
{
	switch (MidiFileEvent_getType(event))
	{
		case MIDI_FILE_EVENT_TYPE_NOTE_OFF:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0x80 | MidiFileNoteOffEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFileNoteOffEvent_getNote(event);
			u.data_as_bytes[2] = MidiFileNoteOffEvent_getVelocity(event);
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		case MIDI_FILE_EVENT_TYPE_NOTE_ON:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0x90 | MidiFileNoteOnEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFileNoteOnEvent_getNote(event);
			u.data_as_bytes[2] = MidiFileNoteOnEvent_getVelocity(event);
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		case MIDI_FILE_EVENT_TYPE_KEY_PRESSURE:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0xA0 | MidiFileKeyPressureEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFileKeyPressureEvent_getNote(event);
			u.data_as_bytes[2] = MidiFileKeyPressureEvent_getAmount(event);
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		case MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0xB0 | MidiFileControlChangeEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFileControlChangeEvent_getNumber(event);
			u.data_as_bytes[2] = MidiFileControlChangeEvent_getValue(event);
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		case MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0xC0 | MidiFileProgramChangeEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFileProgramChangeEvent_getNumber(event);
			u.data_as_bytes[2] = 0;
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		case MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0xD0 | MidiFileChannelPressureEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFileChannelPressureEvent_getAmount(event);
			u.data_as_bytes[2] = 0;
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		case MIDI_FILE_EVENT_TYPE_PITCH_WHEEL:
		{
			union
			{
				unsigned char data_as_bytes[4];
				unsigned long data_as_uint32;
			}
			u;

			u.data_as_bytes[0] = 0xE0 | MidiFilePitchWheelEvent_getChannel(event);
			u.data_as_bytes[1] = MidiFilePitchWheelEvent_getValue(event) & 0x0F;
			u.data_as_bytes[2] = MidiFilePitchWheelEvent_getValue(event) >> 7;
			u.data_as_bytes[3] = 0;
			return u.data_as_uint32;
		}
		default:
		{
			return 0;
		}
	}
}

int MidiFileVoiceEvent_setData(MidiFileEvent_t event, unsigned long data)
{
	union
	{
		unsigned long data_as_uint32;
		unsigned char data_as_bytes[4];
	}
	u;

	if (event == NULL) return -1;

	u.data_as_uint32 = data;

	switch (u.data_as_bytes[0] & 0xF0)
	{
		case 0x80:
		{
			event->type = MIDI_FILE_EVENT_TYPE_NOTE_OFF;
			event->u.note_off.channel = u.data_as_bytes[0] & 0x0F;
			event->u.note_off.note = u.data_as_bytes[1];
			event->u.note_off.velocity = u.data_as_bytes[2];
			return 0;
		}
		case 0x90:
		{
			event->type = MIDI_FILE_EVENT_TYPE_NOTE_ON;
			event->u.note_on.channel = u.data_as_bytes[0] & 0x0F;
			event->u.note_on.note = u.data_as_bytes[1];
			event->u.note_on.velocity = u.data_as_bytes[2];
			return 0;
		}
		case 0xA0:
		{
			event->type = MIDI_FILE_EVENT_TYPE_KEY_PRESSURE;
			event->u.key_pressure.channel = u.data_as_bytes[0] & 0x0F;
			event->u.key_pressure.note = u.data_as_bytes[1];
			event->u.key_pressure.amount = u.data_as_bytes[2];
			return 0;
		}
		case 0xB0:
		{
			event->type = MIDI_FILE_EVENT_TYPE_CONTROL_CHANGE;
			event->u.control_change.channel = u.data_as_bytes[0] & 0x0F;
			event->u.control_change.number = u.data_as_bytes[1];
			event->u.control_change.value = u.data_as_bytes[2];
			return 0;
		}
		case 0xC0:
		{
			event->type = MIDI_FILE_EVENT_TYPE_PROGRAM_CHANGE;
			event->u.program_change.channel = u.data_as_bytes[0] & 0x0F;
			event->u.program_change.number = u.data_as_bytes[1];
			return 0;
		}
		case 0xD0:
		{
			event->type = MIDI_FILE_EVENT_TYPE_CHANNEL_PRESSURE;
			event->u.channel_pressure.channel = u.data_as_bytes[0] & 0x0F;
			event->u.channel_pressure.amount = u.data_as_bytes[1];
			return 0;
		}
		case 0xE0:
		{
			event->type = MIDI_FILE_EVENT_TYPE_PITCH_WHEEL;
			event->u.pitch_wheel.channel = u.data_as_bytes[0] & 0x0F;
			event->u.pitch_wheel.value = (u.data_as_bytes[2] << 7) | u.data_as_bytes[1];
			return 0;
		}
		default:
		{
			return -1;
		}
	}
}

MidiFileMeasureBeat_t MidiFileMeasureBeat_new(void)
{
	MidiFileMeasureBeat_t measure_beat = (MidiFileMeasureBeat_t)(malloc(sizeof(struct MidiFileMeasureBeat)));
	measure_beat->measure = 1;
	measure_beat->beat = 1.0;
	measure_beat->string[0] = '\0';
	return measure_beat;
}

int MidiFileMeasureBeat_free(MidiFileMeasureBeat_t measure_beat)
{
	if (measure_beat == NULL) return -1;
	free(measure_beat);
	return 0;
}

long MidiFileMeasureBeat_getMeasure(MidiFileMeasureBeat_t measure_beat)
{
	if (measure_beat == NULL) return -1;
	return measure_beat->measure;
}

int MidiFileMeasureBeat_setMeasure(MidiFileMeasureBeat_t measure_beat, long measure)
{
	if (measure_beat == NULL) return -1;
	measure_beat->measure = measure;
	return 0;
}

float MidiFileMeasureBeat_getBeat(MidiFileMeasureBeat_t measure_beat)
{
	if (measure_beat == NULL) return -1.0;
	return measure_beat->beat;
}

int MidiFileMeasureBeat_setBeat(MidiFileMeasureBeat_t measure_beat, float beat)
{
	if (measure_beat == NULL) return -1;
	measure_beat->beat = beat;
	return 0;
}

char *MidiFileMeasureBeat_toString(MidiFileMeasureBeat_t measure_beat)
{
	if (measure_beat == NULL) return NULL;
	sprintf(measure_beat->string, "%ld:%.3f", measure_beat->measure, measure_beat->beat);
	return measure_beat->string;
}

int MidiFileMeasureBeat_parse(MidiFileMeasureBeat_t measure_beat, char *string)
{
	if ((measure_beat == NULL) || (string == NULL)) return -1;
	measure_beat->measure = 1;
	measure_beat->beat = 1.0;
	sscanf(string, "%ld:%f", &(measure_beat->measure), &(measure_beat->beat));
	return 0;
}

MidiFileMeasureBeatTick_t MidiFileMeasureBeatTick_new(void)
{
	MidiFileMeasureBeatTick_t measure_beat_tick = (MidiFileMeasureBeatTick_t)(malloc(sizeof(struct MidiFileMeasureBeatTick)));
	measure_beat_tick->measure = 1;
	measure_beat_tick->beat = 1;
	measure_beat_tick->tick = 0.0;
	measure_beat_tick->string[0] = '\0';
	return measure_beat_tick;
}

int MidiFileMeasureBeatTick_free(MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if (measure_beat_tick == NULL) return -1;
	free(measure_beat_tick);
	return 0;
}

long MidiFileMeasureBeatTick_getMeasure(MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if (measure_beat_tick == NULL) return -1;
	return measure_beat_tick->measure;
}

int MidiFileMeasureBeatTick_setMeasure(MidiFileMeasureBeatTick_t measure_beat_tick, long measure)
{
	if (measure_beat_tick == NULL) return -1;
	measure_beat_tick->measure = measure;
	return 0;
}

long MidiFileMeasureBeatTick_getBeat(MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if (measure_beat_tick == NULL) return -1;
	return measure_beat_tick->beat;
}

int MidiFileMeasureBeatTick_setBeat(MidiFileMeasureBeatTick_t measure_beat_tick, long beat)
{
	if (measure_beat_tick == NULL) return -1;
	measure_beat_tick->beat = beat;
	return 0;
}

float MidiFileMeasureBeatTick_getTick(MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if (measure_beat_tick == NULL) return -1.0;
	return measure_beat_tick->tick;
}

int MidiFileMeasureBeatTick_setTick(MidiFileMeasureBeatTick_t measure_beat_tick, float tick)
{
	if (measure_beat_tick == NULL) return -1;
	measure_beat_tick->tick = tick;
	return 0;
}

char *MidiFileMeasureBeatTick_toString(MidiFileMeasureBeatTick_t measure_beat_tick)
{
	if (measure_beat_tick == NULL) return NULL;
	sprintf(measure_beat_tick->string, "%ld:%ld:%.3f", measure_beat_tick->measure, measure_beat_tick->beat, measure_beat_tick->tick);
	return measure_beat_tick->string;
}

int MidiFileMeasureBeatTick_parse(MidiFileMeasureBeatTick_t measure_beat_tick, char *string)
{
	if ((measure_beat_tick == NULL) || (string == NULL)) return -1;
	measure_beat_tick->measure = 1;
	measure_beat_tick->beat = 1;
	measure_beat_tick->tick = 0.0;
	sscanf(string, "%ld:%ld:%f", &(measure_beat_tick->measure), &(measure_beat_tick->beat), &(measure_beat_tick->tick));
	return 0;
}

MidiFileHourMinuteSecond_t MidiFileHourMinuteSecond_new(void)
{
	MidiFileHourMinuteSecond_t hour_minute_second = (MidiFileHourMinuteSecond_t)(malloc(sizeof(struct MidiFileHourMinuteSecond)));
	hour_minute_second->hour = 0;
	hour_minute_second->minute = 0;
	hour_minute_second->second = 0.0;
	hour_minute_second->string[0] = '\0';
	return hour_minute_second;
}

int MidiFileHourMinuteSecond_free(MidiFileHourMinuteSecond_t hour_minute_second)
{
	if (hour_minute_second == NULL) return -1;
	free(hour_minute_second);
	return 0;
}

long MidiFileHourMinuteSecond_getHour(MidiFileHourMinuteSecond_t hour_minute_second)
{
	if (hour_minute_second == NULL) return -1;
	return hour_minute_second->hour;
}

int MidiFileHourMinuteSecond_setHour(MidiFileHourMinuteSecond_t hour_minute_second, long hour)
{
	if (hour_minute_second == NULL) return -1;
	hour_minute_second->hour = hour;
	return 0;
}

long MidiFileHourMinuteSecond_getMinute(MidiFileHourMinuteSecond_t hour_minute_second)
{
	if (hour_minute_second == NULL) return -1;
	return hour_minute_second->minute;
}

int MidiFileHourMinuteSecond_setMinute(MidiFileHourMinuteSecond_t hour_minute_second, long minute)
{
	if (hour_minute_second == NULL) return -1;
	hour_minute_second->minute = minute;
	return 0;
}

float MidiFileHourMinuteSecond_getSecond(MidiFileHourMinuteSecond_t hour_minute_second)
{
	if (hour_minute_second == NULL) return -1.0;
	return hour_minute_second->second;
}

int MidiFileHourMinuteSecond_setSecond(MidiFileHourMinuteSecond_t hour_minute_second, float second)
{
	if (hour_minute_second == NULL) return -1;
	hour_minute_second->second = second;
	return 0;
}

char *MidiFileHourMinuteSecond_toString(MidiFileHourMinuteSecond_t hour_minute_second)
{
	if (hour_minute_second == NULL) return NULL;
	sprintf(hour_minute_second->string, "%ld:%02ld:%06.3f", hour_minute_second->hour, hour_minute_second->minute, hour_minute_second->second);
	return hour_minute_second->string;
}

int MidiFileHourMinuteSecond_parse(MidiFileHourMinuteSecond_t hour_minute_second, char *string)
{
	if ((hour_minute_second == NULL) || (string == NULL)) return -1;
	hour_minute_second->hour = 0;
	hour_minute_second->minute = 0;
	hour_minute_second->second = 0.0;
	sscanf(string, "%ld:%ld:%f", &(hour_minute_second->hour), &(hour_minute_second->minute), &(hour_minute_second->second));
	return 0;
}

MidiFileHourMinuteSecondFrame_t MidiFileHourMinuteSecondFrame_new(void)
{
	MidiFileHourMinuteSecondFrame_t hour_minute_second_frame = (MidiFileHourMinuteSecondFrame_t)(malloc(sizeof(struct MidiFileHourMinuteSecondFrame)));
	hour_minute_second_frame->hour = 0;
	hour_minute_second_frame->minute = 0;
	hour_minute_second_frame->second = 0;
	hour_minute_second_frame->frame = 0.0;
	hour_minute_second_frame->string[0] = '\0';
	return hour_minute_second_frame;
}

int MidiFileHourMinuteSecondFrame_free(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if (hour_minute_second_frame == NULL) return -1;
	free(hour_minute_second_frame);
	return 0;
}

long MidiFileHourMinuteSecondFrame_getHour(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if (hour_minute_second_frame == NULL) return -1;
	return hour_minute_second_frame->hour;
}

int MidiFileHourMinuteSecondFrame_setHour(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, long hour)
{
	if (hour_minute_second_frame == NULL) return -1;
	hour_minute_second_frame->hour = hour;
	return 0;
}

long MidiFileHourMinuteSecondFrame_getMinute(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if (hour_minute_second_frame == NULL) return -1;
	return hour_minute_second_frame->minute;
}

int MidiFileHourMinuteSecondFrame_setMinute(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, long minute)
{
	if (hour_minute_second_frame == NULL) return -1;
	hour_minute_second_frame->minute = minute;
	return 0;
}

long MidiFileHourMinuteSecondFrame_getSecond(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if (hour_minute_second_frame == NULL) return -1;
	return hour_minute_second_frame->second;
}

int MidiFileHourMinuteSecondFrame_setSecond(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, long second)
{
	if (hour_minute_second_frame == NULL) return -1;
	hour_minute_second_frame->second = second;
	return 0;
}

float MidiFileHourMinuteSecondFrame_getFrame(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if (hour_minute_second_frame == NULL) return -1.0;
	return hour_minute_second_frame->frame;
}

int MidiFileHourMinuteSecondFrame_setFrame(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, float frame)
{
	if (hour_minute_second_frame == NULL) return -1;
	hour_minute_second_frame->frame = frame;
	return 0;
}

char *MidiFileHourMinuteSecondFrame_toString(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame)
{
	if (hour_minute_second_frame == NULL) return NULL;
	sprintf(hour_minute_second_frame->string, "%ld:%02ld:%02ld:%.3f", hour_minute_second_frame->hour, hour_minute_second_frame->minute, hour_minute_second_frame->second, hour_minute_second_frame->frame);
	return hour_minute_second_frame->string;
}

int MidiFileHourMinuteSecondFrame_parse(MidiFileHourMinuteSecondFrame_t hour_minute_second_frame, char *string)
{
	if ((hour_minute_second_frame == NULL) || (string == NULL)) return -1;
	hour_minute_second_frame->hour = 0;
	hour_minute_second_frame->minute = 0;
	hour_minute_second_frame->second = 0;
	hour_minute_second_frame->frame = 0.0;
	sscanf(string, "%ld:%ld:%ld:%f", &(hour_minute_second_frame->hour), &(hour_minute_second_frame->minute), &(hour_minute_second_frame->second), &(hour_minute_second_frame->frame));
	return 0;
}

