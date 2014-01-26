/*
 *  Copyright (C) 2002-2013  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <fluidsynth.h>
#include <math.h>

#define FLUID_FREE(_p)               free(_p)
#define FLUID_OK   (0) 
#define FLUID_NEW(_t)                (_t*)malloc(sizeof(_t))

/* Missing fluidsynth API follow: */
extern "C" {
  typedef struct _fluid_midi_parser_t fluid_midi_parser_t;
  typedef struct _fluid_midi_event_t fluid_midi_event_t;

  fluid_midi_parser_t * new_fluid_midi_parser(void);
  fluid_midi_event_t * fluid_midi_parser_parse(fluid_midi_parser_t *parser, unsigned char c);

  void fluid_log_config(void);
}

static fluid_log_function_t fluid_log_function[LAST_LOG_LEVEL];
static int fluid_log_initialized = 0;
#if 0
void fluid_log_config(void) {
  if (fluid_log_initialized == 0) {

    fluid_log_initialized = 1;

    if (fluid_log_function[FLUID_PANIC] == NULL) {
      fluid_set_log_function(FLUID_PANIC, fluid_default_log_function, NULL);
    }

    if (fluid_log_function[FLUID_ERR] == NULL) {
      fluid_set_log_function(FLUID_ERR, fluid_default_log_function, NULL);
    }

    if (fluid_log_function[FLUID_WARN] == NULL) {
      fluid_set_log_function(FLUID_WARN, fluid_default_log_function, NULL);
    }

    if (fluid_log_function[FLUID_INFO] == NULL) {
      fluid_set_log_function(FLUID_INFO, fluid_default_log_function, NULL);
    }

    if (fluid_log_function[FLUID_DBG] == NULL) {
      fluid_set_log_function(FLUID_DBG, fluid_default_log_function, NULL);
    }
  }
}
#endif
struct _fluid_midi_event_t {
  fluid_midi_event_t* next; /* Link to next event */
  void *paramptr;           /* Pointer parameter (for SYSEX data), size is stored to param1, param2 indicates if pointer should be freed (dynamic if TRUE) */
  unsigned int dtime;       /* Delay (ticks) between this and previous event. midi tracks. */
  unsigned int param1;      /* First parameter */
  unsigned int param2;      /* Second parameter */
  unsigned char type;       /* MIDI event type */
  unsigned char channel;    /* MIDI channel */
};

struct _fluid_midi_parser_t {
  unsigned char status;           /* Identifies the type of event, that is currently received ('Noteon', 'Pitch Bend' etc). */
  unsigned char channel;          /* The channel of the event that is received (in case of a channel event) */
  unsigned int nr_bytes;          /* How many bytes have been read for the current event? */
  unsigned int nr_bytes_total;    /* How many bytes does the current event type include? */
  unsigned char data[1024]; /* The parameters or SYSEX data */
  fluid_midi_event_t event;        /* The event, that is returned to the MIDI driver. */
};
#if 0
fluid_midi_parser_t* new_fluid_midi_parser() {
      fluid_midi_parser_t* parser;
      parser = FLUID_NEW(fluid_midi_parser_t);
      if (parser == NULL) {
            //FLUID_LOG(FLUID_ERR, "Out of memory");
            return NULL;
      }
      parser->status = 0;
      return parser;
}
#endif
enum fluid_midi_event_type {
  /* channel messages */
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  KEY_PRESSURE = 0xa0,
  CONTROL_CHANGE = 0xb0,
  PROGRAM_CHANGE = 0xc0,
  CHANNEL_PRESSURE = 0xd0,
  PITCH_BEND = 0xe0,
  /* system exclusive */
  MIDI_SYSEX = 0xf0,
  /* system common - never in midi files */
  MIDI_TIME_CODE = 0xf1,
  MIDI_SONG_POSITION = 0xf2,
  MIDI_SONG_SELECT = 0xf3,
  MIDI_TUNE_REQUEST = 0xf6,
  MIDI_EOX = 0xf7,
  /* system real-time - never in midi files */
  MIDI_SYNC = 0xf8,
  MIDI_TICK = 0xf9,
  MIDI_START = 0xfa,
  MIDI_CONTINUE = 0xfb,
  MIDI_STOP = 0xfc,
  MIDI_ACTIVE_SENSING = 0xfe,
  MIDI_SYSTEM_RESET = 0xff,
  /* meta event - for midi files only */
  MIDI_META_EVENT = 0xff
};

static int fluid_midi_event_length(unsigned char event) {
      switch (event & 0xF0) {
            case NOTE_OFF: 
            case NOTE_ON:
            case KEY_PRESSURE:
            case CONTROL_CHANGE:
            case PITCH_BEND: 
                  return 3;
            case PROGRAM_CHANGE:
            case CHANNEL_PRESSURE: 
                  return 2;
      }
      switch (event) {
            case MIDI_TIME_CODE:
            case MIDI_SONG_SELECT:
            case 0xF4:
            case 0xF5:
                  return 2;
            case MIDI_TUNE_REQUEST:
                  return 1;
            case MIDI_SONG_POSITION:
                  return 3;
      }
      return 1;
}
#if 0
fluid_midi_event_t * fluid_midi_parser_parse(fluid_midi_parser_t* parser, unsigned char c) {
  fluid_midi_event_t *event;

  /* Real-time messages (0xF8-0xFF) can occur anywhere, even in the middle
   * of another message. */
  if (c >= 0xF8)
  {
    if (c == MIDI_SYSTEM_RESET)
    {
      parser->event.type = c;
      parser->status = 0;       /* clear the status */
      return &parser->event;
    }

    return NULL;
  }

  /* Status byte? - If previous message not yet complete, it is discarded (re-sync). */
  if (c & 0x80)
  {
    /* Any status byte terminates SYSEX messages (not just 0xF7) */
    if (parser->status == MIDI_SYSEX && parser->nr_bytes > 0)
    {
      event = &parser->event;
      fluid_midi_event_set_sysex (event, parser->data, parser->nr_bytes, FALSE);
    }
    else event = NULL;

    if (c < 0xF0)       /* Voice category message? */
    {
      parser->channel = c & 0x0F;
      parser->status = c & 0xF0;

      /* The event consumes x bytes of data... (subtract 1 for the status byte) */
      parser->nr_bytes_total = fluid_midi_event_length (parser->status) - 1;

      parser->nr_bytes = 0;     /* 0  bytes read so far */
    }
    else if (c == MIDI_SYSEX)
    {
      parser->status = MIDI_SYSEX;
      parser->nr_bytes = 0;
    }
    else parser->status = 0;    /* Discard other system messages (0xF1-0xF7) */

    return event;       /* Return SYSEX event or NULL */
  }


  /* Data/parameter byte */


  /* Discard data bytes for events we don't care about */
  if (parser->status == 0)
    return NULL;

  /* Max data size exceeded? (SYSEX messages only really) */
  if (parser->nr_bytes == 1024)
  {
    parser->status = 0;         /* Discard the rest of the message */
    return NULL;
  }

  /* Store next byte */
  parser->data[parser->nr_bytes++] = c;

  /* Do we still need more data to get this event complete? */
  if (parser->nr_bytes < parser->nr_bytes_total)
    return NULL;

  /* Event is complete, return it.
   * Running status byte MIDI feature is also handled here. */
  parser->event.type = parser->status;
  parser->event.channel = parser->channel;
  parser->nr_bytes = 0;         /* Reset data size, in case there are additional running status messages */

  switch (parser->status)
  {
    case NOTE_OFF:
    case NOTE_ON:
    case KEY_PRESSURE:
    case CONTROL_CHANGE:
    case PROGRAM_CHANGE:
    case CHANNEL_PRESSURE:
      parser->event.param1 = parser->data[0]; /* For example key number */
      parser->event.param2 = parser->data[1]; /* For example velocity */
      break;
    case PITCH_BEND:
      /* Pitch-bend is transmitted with 14-bit precision. */
      parser->event.param1 = (parser->data[1] << 7) | parser->data[0];
      break;
    default: /* Unlikely */
      return NULL;
  }

  return &parser->event;
}
#endif
int delete_fluid_midi_parser(fluid_midi_parser_t* parser) {
      FLUID_FREE(parser);
      return FLUID_OK;
}

/* Protect against multiple inclusions */
#ifndef MIXER_BUFSIZE
#include "mixer.h"
#endif

static MixerChannel *synthchan;

static fluid_synth_t *synth_soft;
static fluid_midi_parser_t *synth_parser;

static int synthsamplerate;


static void synth_log(int level, char *message, void *data) {
	switch (level) {
	case FLUID_PANIC:
	case FLUID_ERR:
		LOG(LOG_ALL,LOG_ERROR)(message);
		break;

	case FLUID_WARN:
		LOG(LOG_ALL,LOG_WARN)(message);
		break;

	default:
		LOG(LOG_ALL,LOG_NORMAL)(message);
		break;
	}
}

static void synth_CallBack(Bitu len) {
	fluid_synth_write_s16(synth_soft, len, MixTemp, 0, 2, MixTemp, 1, 2);
	synthchan->AddSamples_s16(len,(Bit16s *)MixTemp);
}

class MidiHandler_synth: public MidiHandler {
private:
	fluid_settings_t *settings;
	fluid_midi_router_t *router;
	int sfont_id;
	bool isOpen;

public:
	MidiHandler_synth() : isOpen(false),MidiHandler() {};
	const char * GetName(void) { return "synth"; };
	bool Open(const char *conf) {

		/* Sound font file required */
		if (!conf || (conf[0] == '\0')) {
			LOG_MSG("SYNTH: Specify .SF2 sound font file with config=");
			return false;
		}

		fluid_log_config();
		fluid_set_log_function(FLUID_PANIC, synth_log, NULL);
		fluid_set_log_function(FLUID_ERR, synth_log, NULL);
		fluid_set_log_function(FLUID_WARN, synth_log, NULL);
		fluid_set_log_function(FLUID_INFO, synth_log, NULL);
		fluid_set_log_function(FLUID_DBG, synth_log, NULL);

		/* Create the settings. */
		settings = new_fluid_settings();
		if (settings == NULL) {
			LOG_MSG("SYNTH: Error allocating MIDI soft synth settings");
			return false;
		}

		fluid_settings_setstr(settings, "audio.sample-format", "16bits");

		if (synthsamplerate == 0) {
			synthsamplerate = 48000;
		}

		fluid_settings_setnum(settings,
			"synth.sample-rate", (double)synthsamplerate);
		
		fluid_settings_setnum(settings,
         		"synth.gain", 0.6);
		
		fluid_settings_setstr(settings,
         		"synth.reverb.active", "yes");

		fluid_settings_setstr(settings,
         		"synth.chorus.active", "yes");

		fluid_settings_setnum(settings,
         		"audio.periods", 2);

		fluid_settings_setnum(settings,
         		"audio.period-size", 256);

		fluid_settings_setnum(settings,
        		"player.reset-synth", 0);

		fluid_settings_setnum(settings,
        		"synth.min-note-length", 0);

		fluid_settings_setstr(settings,
        		"player.timing-source", "system");

		fluid_settings_setnum(settings,
        		"synth.cpu-cores", 1);

		//fluid_settings_setnum(settings,
        	//	"synth.overflow.age", -10000);

		//fluid_settings_setnum(settings,
        	//	"synth.overflow.percussion", -10000);

		//fluid_settings_setnum(settings,
        	//	"synth.overflow.released", -10000);

		//fluid_settings_setnum(settings,
        	//	"synth.overflow.sustained", -10000);

		//fluid_settings_setnum(settings,
        	//	"synth.overflow.volume", -10000);

    		// gm ignores CC0 and CC32 msgs
    		// gs CC0 becomes the channel bank, CC32 is ignored; default
    		// xg CC32 becomes the channel bank, CC0 is ignored
    		// mma bank = CC0*128+CC32 
		fluid_settings_setstr(settings,
         		"synth.midi-bank-select", "gs");

		/* Create the synthesizer. */
		synth_soft = new_fluid_synth(settings);
		if (synth_soft == NULL) {
			LOG_MSG("SYNTH: Error initialising MIDI soft synth");
			delete_fluid_settings(settings);
			return false;
		}

		/* Load a SoundFont */
		sfont_id = fluid_synth_sfload(synth_soft, conf, 0);
		if (sfont_id == -1) {
			LOG_MSG("SYNTH: Failed to load MIDI sound font file \"%s\"",
			   conf);
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		/* Allocate one event to store the input data */
		synth_parser = new_fluid_midi_parser();
		if (synth_parser == NULL) {
			LOG_MSG("SYNTH: Failed to allocate MIDI parser");
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		router = new_fluid_midi_router(settings,
					       fluid_synth_handle_midi_event,
					       (void*)synth_soft);
		if (router == NULL) {
			LOG_MSG("SYNTH: Failed to initialise MIDI router");
			delete_fluid_midi_parser(synth_parser);
			delete_fluid_synth(synth_soft);
			delete_fluid_settings(settings);
			return false;
		}

		synthchan=MIXER_AddChannel(synth_CallBack, synthsamplerate,
					   "SYNTH");
		synthchan->Enable(false);
		isOpen = true;
		return true;
	};
	void Close(void) {
		if (!isOpen) return;
		delete_fluid_midi_router(router);
		delete_fluid_midi_parser(synth_parser);
		delete_fluid_synth(synth_soft);
		delete_fluid_settings(settings);
		isOpen=false;
	};
	void PlayMsg(Bit8u *msg) {
		fluid_midi_event_t *evt;
		Bitu len;
		int i;

		len=MIDI_evt_len[*msg];
		synthchan->Enable(true);

		/* let the parser convert the data into events */
		for (i = 0; i < len; i++) {
			evt = fluid_midi_parser_parse(synth_parser, msg[i]);
			if (evt != NULL) {
			  /* send the event to the next link in the chain */
			  fluid_midi_router_handle_midi_event(router, evt);
			}
		}
	};
	void PlaySysex(Bit8u *sysex, Bitu len) {
		fluid_midi_event_t *evt;
		int i;

		/* let the parser convert the data into events */
		for (i = 0; i < len; i++) {
			evt = fluid_midi_parser_parse(synth_parser, sysex[i]);
			if (evt != NULL) {
			  /* send the event to the next link in the chain */
			  fluid_midi_router_handle_midi_event(router, evt);
			}
		}
	};
};

MidiHandler_synth Midi_synth;
