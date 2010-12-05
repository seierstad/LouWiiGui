/*
    Copyright (C) 2004 Ian Esten
    Copyright (C) 2010 Erik E. Seierstad
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include <jack/jack.h>
#include <jack/midiport.h>


#include <bluetooth/bluetooth.h>
#include <cwiid.h>

#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>

#define MY_ENCODING "UTF-8"



// amount of whammy movement before sending midi
#define MIDI_PITCH_MAX       0x3FFF
#define MIDI_PITCH_CENTER    0x2000
#define MIDI_MODULATION_MAX  0x7F

#define MAX_ACTIVE_NOTES_COUNT 120


// flags for chord selection
#define NONE   0x00
#define GREEN  0x01
#define RED    0x02
#define YELLOW 0x04
#define BLUE   0x08
#define ORANGE 0x10
#define ALL_COLOR_COMBINATIONS (GREEN | RED | YELLOW | BLUE | ORANGE) + 1

#define WHAMMY_STATE_UNKNOWN 0xFF
#define TOUCHBAR_UNTOUCHED 0x0F
#define TOUCHBAR_SLIDE_MARGIN 2

#define CWIID_GUITAR_STICK_MID ((CWIID_GUITAR_STICK_MAX ) / 2)
#define MAX(a,b) ((a)<(b)?(b):(a))

#define STICK_QUARTER_1(x, y) (x <  CWIID_GUITAR_STICK_MID && y <=  CWIID_GUITAR_STICK_MID)
#define STICK_QUARTER_2(x, y) (x >= CWIID_GUITAR_STICK_MID && y <  CWIID_GUITAR_STICK_MID)
#define STICK_QUARTER_3(x, y) (x > CWIID_GUITAR_STICK_MID && y >= CWIID_GUITAR_STICK_MID)
#define STICK_QUARTER_4(x, y) (x <=  CWIID_GUITAR_STICK_MID && y > CWIID_GUITAR_STICK_MID)

#define STICK_ZONE_1(x, y) (STICK_QUARTER_1(x, y) && x < y)
#define STICK_ZONE_2(x, y) (STICK_QUARTER_1(x, y) && x >= y)
#define STICK_ZONE_3(x, y) (STICK_QUARTER_2(x, y) && y < (CWIID_GUITAR_STICK_MAX - x))
#define STICK_ZONE_4(x, y) (STICK_QUARTER_2(x, y) && y >= (CWIID_GUITAR_STICK_MAX - x))
#define STICK_ZONE_5(x, y) (STICK_QUARTER_3(x, y) && x > y)
#define STICK_ZONE_6(x, y) (STICK_QUARTER_3(x, y) && x <= y)
#define STICK_ZONE_7(x, y) (STICK_QUARTER_4(x, y) && y > (CWIID_GUITAR_STICK_MAX - x))
#define STICK_ZONE_8(x, y) (STICK_QUARTER_4(x, y) && y <= (CWIID_GUITAR_STICK_MAX - x))
#define STICK_ROTATION_ZONE_THRESHOLD 5

enum {
	STRUMMER_STATE_MID,
	STRUMMER_STATE_DOWN,
	STRUMMER_STATE_UP,
	STRUMMER_STATE_SUSTAINED,
	STRUMMER_STATE_UNKNOWN
} strummer_states;

enum {
	STRUMMER_ACTION_NONE,
	STRUMMER_ACTION_MID_UP,
	STRUMMER_ACTION_UP_MID,
	STRUMMER_ACTION_MID_DOWN,
	STRUMMER_ACTION_DOWN_MID
} strummer_actions;

enum {
	WHAMMY_ACTION_NONE,
	WHAMMY_ACTION_UP,
	WHAMMY_ACTION_DOWN
} whammy_actions;

enum {
	TOUCHBAR_STATE_NONE,
	TOUCHBAR_STATE_1ST,
	TOUCHBAR_STATE_1ST_AND_2ND,
	TOUCHBAR_STATE_2ND,
	TOUCHBAR_STATE_2ND_AND_3RD,
	TOUCHBAR_STATE_3RD,
	TOUCHBAR_STATE_3RD_AND_4TH,
	TOUCHBAR_STATE_4TH,
	TOUCHBAR_STATE_4TH_AND_5TH,
	TOUCHBAR_STATE_5TH
} touchbar_states;

enum {
	TOUCHBAR_ACTION_NONE,
	TOUCHBAR_ACTION_TAP,
	TOUCHBAR_ACTION_RELEASE,
	TOUCHBAR_ACTION_HAMMERON,
	TOUCHBAR_ACTION_PULLOFF,
	TOUCHBAR_ACTION_SLIDE_UP,
	TOUCHBAR_ACTION_SLIDE_DOWN
} touchbar_actions;

enum stick_zone_t {
	STICK_ZONE_1ST,
	STICK_ZONE_2ND,
	STICK_ZONE_3RD,
	STICK_ZONE_4TH,
	STICK_ZONE_5TH,
	STICK_ZONE_6TH,
	STICK_ZONE_7TH,
	STICK_ZONE_8TH,
	STICK_ZONE_CENTER,
	STICK_ZONE_UNKNOWN
} stick_zone;

enum stick_action_t {
	STICK_ACTION_NONE,
	STICK_ACTION_ROTATE_CLOCKWISE,
	STICK_ACTION_ROTATE_COUNTER_CLOCKWISE
} stick_action;

unsigned char touchbar_state;
unsigned char touchbar_action;
unsigned char strummer_state;
unsigned char strummer_action;
unsigned char whammy_state;
unsigned char whammy_action;
unsigned char stick_state[2];
unsigned char stick_zone_average_value;
unsigned char stick_zone_rotation_clockwise_counter;
unsigned char stick_zone_rotation_counter_clockwise_counter;

struct stick_zone_value_accumulator {
	unsigned int count;
	unsigned int value;
};
struct stick_zone_value_accumulator stick_zone_acc;

struct note_t {
	unsigned short int velocity;
	unsigned short int note_number;
	unsigned int delay;  // used for strumming patterns (not implemented yet)
};

struct chord_t {
	struct note_t * note;
	int size;
};

// for all combinations of fret buttons: which notes to trigger
struct chord_t chord[ALL_COLOR_COMBINATIONS];  

// current fret button state
unsigned int chord_state = 0;

struct chord_t active_notes;

jack_client_t *client;
jack_port_t *output_port;

unsigned char* note_frqs;
jack_nframes_t* note_starts;
jack_nframes_t* note_lengths;
jack_nframes_t num_notes;
jack_nframes_t loop_nsamp;
jack_nframes_t loop_index;

timer_t countdown_id;
struct itimerspec margin;
struct itimerspec time_left;

cwiid_wiimote_t *wiimote;	/* wiimote handle */
cwiid_mesg_callback_t cwiid_callback;

void usage() {
	fprintf(stderr, "neio [btaddr]\n");
}

void cwiid_callback(cwiid_wiimote_t *wiimote, int mesg_count,
                    union cwiid_mesg mesg[], struct timespec *timestamp){
	int i, j;
	int valid_source;
	for (i=0; i < mesg_count; i++) {
		// set strummer_state and strummer_action
		if (((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_DOWN) == CWIID_GUITAR_BTN_DOWN) && strummer_state != STRUMMER_STATE_DOWN) {
			strummer_state = STRUMMER_STATE_DOWN;
			strummer_action = STRUMMER_ACTION_MID_DOWN;
			timer_settime(countdown_id, 0, &margin, NULL);
			printf("strum down\n");
		} else if (!((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_DOWN) == CWIID_GUITAR_BTN_DOWN) && strummer_state == STRUMMER_STATE_DOWN) {
			strummer_state = STRUMMER_STATE_MID;
			strummer_action = STRUMMER_ACTION_DOWN_MID;
			timer_gettime(countdown_id, &time_left);
			if (time_left.it_value.tv_sec > 0 || time_left.it_value.tv_nsec > 0) {
				strummer_state = STRUMMER_STATE_SUSTAINED;
			}
			printf("strum neutral\n");
		} else if (((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_UP) == CWIID_GUITAR_BTN_UP) && strummer_state != STRUMMER_STATE_UP) {
			strummer_state = STRUMMER_STATE_UP;
			strummer_action = STRUMMER_ACTION_MID_UP;
			printf("strum up\n");
		} else if (!((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_UP) == CWIID_GUITAR_BTN_UP) && strummer_state == STRUMMER_STATE_UP) {
			strummer_state = STRUMMER_STATE_MID;
			strummer_action = STRUMMER_ACTION_UP_MID;
			printf("strum neutral\n");
		} else {
			strummer_action = STRUMMER_ACTION_NONE;
		}

		// set chord state
		chord_state = 0;
		if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_GREEN) == CWIID_GUITAR_BTN_GREEN) {
			chord_state |= GREEN;
		}
		if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_RED) == CWIID_GUITAR_BTN_RED) {
			chord_state |= RED;
		}
		if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_YELLOW) == CWIID_GUITAR_BTN_YELLOW) {
			chord_state |= YELLOW;
		}
		if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_BLUE) == CWIID_GUITAR_BTN_BLUE) {
			chord_state |= BLUE;
		}
		if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_ORANGE) == CWIID_GUITAR_BTN_ORANGE) {
			chord_state |= ORANGE;
		}

		//set whammy state and action
		if (mesg[i].guitar_mesg.whammy != whammy_state) {
			if (mesg[i].guitar_mesg.whammy > whammy_state) {
				whammy_action = WHAMMY_ACTION_DOWN;
			} else {
				whammy_action = WHAMMY_ACTION_UP;
			}

			whammy_state = mesg[i].guitar_mesg.whammy;
		}
		//set touch bar state and action
		unsigned char new_touchbar_state = mesg[i].guitar_mesg.touch_bar;
		if (new_touchbar_state != touchbar_state) {
			if (touchbar_state == TOUCHBAR_UNTOUCHED) {
				touchbar_action = TOUCHBAR_ACTION_TAP;
			} 
			else if (new_touchbar_state == TOUCHBAR_UNTOUCHED) {
				touchbar_action = TOUCHBAR_ACTION_RELEASE;
			}
			else if (new_touchbar_state < touchbar_state) {
				if (new_touchbar_state >= touchbar_state - TOUCHBAR_SLIDE_MARGIN) {
					touchbar_action = TOUCHBAR_ACTION_SLIDE_DOWN;
				} else {
					touchbar_action = TOUCHBAR_ACTION_PULLOFF;
				}
			} 
			else {
				if (new_touchbar_state <= touchbar_state + TOUCHBAR_SLIDE_MARGIN) {
					touchbar_action = TOUCHBAR_ACTION_SLIDE_UP;
				} else {
					touchbar_action = TOUCHBAR_ACTION_HAMMERON;
				}
			}
			touchbar_state = new_touchbar_state;
		}

		unsigned char new_stick_state[2];
		new_stick_state[CWIID_X] = mesg[i].guitar_mesg.stick[CWIID_X];
		new_stick_state[CWIID_Y] = mesg[i].guitar_mesg.stick[CWIID_Y];
		if (new_stick_state[CWIID_X] != stick_state[CWIID_X] || new_stick_state[CWIID_X] != stick_state[CWIID_X]) {
			int x_diff = abs(new_stick_state[CWIID_X] - CWIID_GUITAR_STICK_MID);
			int y_diff = abs(new_stick_state[CWIID_Y] - CWIID_GUITAR_STICK_MID);
			int distance_from_center = MAX(x_diff, y_diff);

			enum stick_zone_t new_stick_zone;
			if (distance_from_center < 3) {
				new_stick_zone = STICK_ZONE_CENTER;
			} else if (STICK_ZONE_1(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_1ST;
			} else if (STICK_ZONE_2(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_2ND;
			} else if (STICK_ZONE_3(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_3RD;
			} else if (STICK_ZONE_4(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_4TH;
			} else if (STICK_ZONE_5(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_5TH;
			} else if (STICK_ZONE_6(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_6TH;
			} else if (STICK_ZONE_7(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_7TH;
			} else if (STICK_ZONE_8(new_stick_state[CWIID_X], new_stick_state[CWIID_Y])) {
				new_stick_zone = STICK_ZONE_8TH;
			} 

			stick_state[CWIID_X] = new_stick_state[CWIID_X];
			stick_state[CWIID_Y] = new_stick_state[CWIID_Y];

			if (new_stick_zone != stick_zone) {
				if (new_stick_zone == stick_zone + 1 || (new_stick_zone == STICK_ZONE_1ST && stick_zone == STICK_ZONE_8TH)) {
					stick_zone_rotation_clockwise_counter = 0;
					stick_zone_rotation_counter_clockwise_counter++;
				} else if (new_stick_zone == stick_zone - 1 || (new_stick_zone == STICK_ZONE_8TH && stick_zone == STICK_ZONE_1ST)) {
					stick_zone_rotation_clockwise_counter++;
					stick_zone_rotation_counter_clockwise_counter = 0;
				} else {
					stick_zone_rotation_clockwise_counter = 0;
					stick_zone_rotation_counter_clockwise_counter = 0;
				}

				if (stick_zone_rotation_clockwise_counter >= STICK_ROTATION_ZONE_THRESHOLD) {
					stick_action = STICK_ACTION_ROTATE_CLOCKWISE;
				} else if (stick_zone_rotation_counter_clockwise_counter >= STICK_ROTATION_ZONE_THRESHOLD) {
					stick_action = STICK_ACTION_ROTATE_COUNTER_CLOCKWISE;
				}
				stick_zone_average_value = (stick_zone_acc.count == 0) ? 0 : (stick_zone_acc.value / stick_zone_acc.count);
				stick_zone = new_stick_zone;
				stick_zone_acc.count = 0;
				stick_zone_acc.value = 0;
			}
			stick_zone_acc.count++;
			stick_zone_acc.value += distance_from_center;
			
		}
		
		

	}
}

cwiid_err_t err;
void err(cwiid_wiimote_t *wiimote, const char *s, va_list ap) {
	if (wiimote) printf("%d:", cwiid_get_id(wiimote));
	else printf("-1:");
	vprintf(s, ap);
	printf("\n");
}

int process(jack_nframes_t nframes, void *arg) {
	int i,j;
	void* port_buf = jack_port_get_buffer(output_port, nframes);
	unsigned char* buffer;
	jack_midi_clear_buffer(port_buf);
	/*memset(buffer, 0, nframes*sizeof(jack_default_audio_sample_t));*/


	for(i=0; i<nframes; i++)
	{
		if (strummer_action != STRUMMER_ACTION_NONE) {

			if (strummer_action == STRUMMER_ACTION_MID_DOWN || strummer_action == STRUMMER_ACTION_MID_UP) {
				int q;
				for (q = 0; q < chord[chord_state].size; q++) {
					buffer = jack_midi_event_reserve(port_buf, i, 3);
					buffer[2] = chord[chord_state].note[q].velocity;		/* velocity */
					buffer[1] = chord[chord_state].note[q].note_number;	/* note number */
					buffer[0] = 0x90;	/* note on */
					
					active_notes.note[active_notes.size] = chord[chord_state].note[q];
					active_notes.size++;
				}
				strummer_action = STRUMMER_ACTION_NONE;
			}
			else 
			{
				if (strummer_state != STRUMMER_STATE_SUSTAINED) {
					int q;
					while(active_notes.size > 0) {
						buffer = jack_midi_event_reserve(port_buf, i, 3);
						buffer[2] = active_notes.note[active_notes.size - 1].velocity;		/* velocity */
						buffer[1] = active_notes.note[active_notes.size - 1].note_number;	/* note number */
						buffer[0] = 0x80;	/* note off */

						active_notes.size--;
					}
					strummer_action = STRUMMER_ACTION_NONE;
				}
			}
		}
		if (whammy_action != WHAMMY_ACTION_NONE) {
			buffer = jack_midi_event_reserve(port_buf, i, 3);
			unsigned int pitch_shift = (MIDI_PITCH_CENTER * whammy_state) / CWIID_GUITAR_WHAMMY_MAX ;
			unsigned int pitch_value = MIDI_PITCH_CENTER - pitch_shift;
			buffer[2] = (pitch_value & 0x3F80) >> 7;  // most significant bits
			buffer[1] = pitch_value & 0x007f;         // least significant bits
			buffer[0] = 0xE0;	// pitch wheel change 
			printf("whammy! %x, %x, %x, desimalt: %d\n", buffer[0], buffer[2], buffer[1], pitch_value);
			whammy_action = WHAMMY_ACTION_NONE;
		}
		if (touchbar_action != TOUCHBAR_ACTION_NONE) {
			buffer = jack_midi_event_reserve(port_buf, i, 3);

			// scale input to output values (5th represents maximum possible input value)
			unsigned int modulation = (MIDI_MODULATION_MAX * touchbar_state) / CWIID_GUITAR_TOUCHBAR_5TH ; 
			printf("touchbar action! %d, %x, sent %x\n", touchbar_action, touchbar_state, modulation);
			buffer[2] = modulation;
			buffer[1] = 0x1;        // modulation
			buffer[0] = 0xB0;	// control change 
			touchbar_action = TOUCHBAR_ACTION_NONE;
		}
		if (stick_action != STICK_ACTION_NONE) {
			printf("stick_action!\n");
			stick_action = STICK_ACTION_NONE;
		}

	}
	return 0;
}

void set_rpt_mode(cwiid_wiimote_t *wiimote, uint8_t rpt_mode) {
	if (cwiid_set_rpt_mode(wiimote, rpt_mode)) {
		fprintf(stderr, "Error setting report mode\n");
	}
}

void writeCurrentPatchToFile(const char *file) {
    int rc, chord_index, note_index;
    xmlTextWriterPtr writer;
    xmlChar *tmp;
    xmlDocPtr doc;

    /* Create a new XmlWriter for DOM, with no compression. */
    writer = xmlNewTextWriterDoc(&doc, 0);
    if (writer == NULL) {
        printf("writeCurrentPatchToFile: Error creating the xml writer\n");
        return;
    }

    /* Start the document with the xml default for the version,
     * encoding UTF-8 and the default for the standalone
     * declaration. */
    rc = xmlTextWriterStartDocument(writer, NULL, MY_ENCODING, NULL);
    if (rc < 0) {
        printf("writeCurrentPatchToFile: Error at xmlTextWriterStartDocument\n");
        return;
    }

    /* Start an element named "patch". Since thist is the first
     * element, this will be the root element of the document. */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "patch");
    if (rc < 0) {
        printf("writeCurrentPatchToFile: Error at xmlTextWriterStartElement\n");
        return;
    }

    /* Start an element named "chords". */
    rc = xmlTextWriterStartElement(writer, BAD_CAST "chords");
    if (rc < 0) {
        printf("writeCurrentPatchToFile: Error at xmlTextWriterStartElement\n");
        return;
    }

	
	for (chord_index = 0; chord_index < ALL_COLOR_COMBINATIONS; chord_index++) {
		if (chord[chord_index].size > 0) {
			/* Start an element named "chord" as child of patch. */
			rc = xmlTextWriterStartElement(writer, BAD_CAST "chord");
			if (rc < 0) {
				printf("writeCurrentPatchToFile: Error at xmlTextWriterStartElement\n");
				return;
			}

			if (chord_index & GREEN) {

				rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "green", BAD_CAST "true");
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}
			}
			if (chord_index & RED) {
				rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "red", BAD_CAST "true");
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}
			}
			if (chord_index & YELLOW) {
				rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "yellow", BAD_CAST "true");
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}
			}
			if (chord_index & BLUE) {
				rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "blue", BAD_CAST "true");
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}
			}
			if (chord_index & ORANGE) {
				rc = xmlTextWriterWriteAttribute(writer, BAD_CAST "orange", BAD_CAST "true");
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}
			}

			rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "number_of_notes", "%d", chord[chord_index].size);
			if (rc < 0) {
				printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
				return;
			}

			for (note_index = 0; note_index < chord[chord_index].size; note_index++) {
				/* Start an element named "note" as child of chord. */
				rc = xmlTextWriterStartElement(writer, BAD_CAST "note");
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterStartElement\n");
					return;
				}

				/* Add an attribute with name "note_number" and value chord[i].note.note_number to note. */
				rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "note_number", "%d", chord[chord_index].note[note_index].note_number);
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}

				/* Add an attribute with name "velocity" and value chord[i].note.velocity to note. */
				rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "velocity", "%d", chord[chord_index].note[note_index].velocity);
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}

				/* Add an attribute with name "delay" and value chord[i].note.delay to note. */
				rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "delay", "%d", chord[chord_index].note[note_index].delay);
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
					return;
				}


				/* Close the element named note. */
				rc = xmlTextWriterEndElement(writer);
				if (rc < 0) {
					printf("writeCurrentPatchToFile: Error at xmlTextWriterEndElement\n");
					return;
				}
			}
			/* Close the element named chord. */
			rc = xmlTextWriterEndElement(writer);
			if (rc < 0) {
				printf("writeCurrentPatchToFile: Error at xmlTextWriterEndElement\n");
				return;
			}

		}
	}

    /* Here we could close the elements ORDER and EXAMPLE using the
     * function xmlTextWriterEndElement, but since we do not want to
     * write any other elements, we simply call xmlTextWriterEndDocument,
     * which will do all the work. */
    rc = xmlTextWriterEndDocument(writer);
    if (rc < 0) {
        printf("writeCurrentPatchToFile: Error at xmlTextWriterEndDocument\n");
        return;
    }

    xmlFreeTextWriter(writer);

    xmlSaveFileEnc(file, doc, MY_ENCODING);

    xmlFreeDoc(doc);
}

void freePatchMemory() {
	int chord_index;
	for (chord_index = 0; chord_index < ALL_COLOR_COMBINATIONS; chord_index++) {
		free(chord[chord_index].note);
	}
}

void readPatchFromFile (const char *file) {

    xmlDoc *doc = NULL;
    xmlNode *root_element, *chord_element, *note_element, *cur = NULL;
		int chord_index, note_index, number_of_notes, note_number, velocity, delay = 0;

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    /*parse the file and get the DOM */
    doc = xmlReadFile(file, NULL, 0);

    if (doc == NULL) {
        printf("error: could not parse file %s\n", file);
    }

    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);
		cur = root_element;

		if (!strcmp(cur->name, "patch")) {
      printf ("%s\n", cur->name);
      cur = cur->children;
			 if (!strcmp(cur->name, "chords")) {
		    printf ("%s\n", cur->name);
				chord_element = cur->children;
				while (chord_element != NULL) {
					chord_index = 0;
					number_of_notes = 0;
					if (xmlGetProp(chord_element, "green")) {
						chord_index = chord_index | GREEN;
					}
					if (xmlGetProp(chord_element, "red")) {
						chord_index = chord_index | RED;
					}
					if (xmlGetProp(chord_element, "yellow")) {
						chord_index = chord_index | YELLOW;
					}
					if (xmlGetProp(chord_element, "blue")) {
						chord_index = chord_index | BLUE;
					}
					if (xmlGetProp(chord_element, "orange")) {
						chord_index = chord_index | ORANGE;
					}
					if (xmlGetProp(chord_element, "number_of_notes")) {
						sscanf(xmlGetProp(chord_element, "number_of_notes"), "%d", &number_of_notes);
					}
					printf("neio: %d, %d noter\n", chord_index, number_of_notes);

					chord[chord_index].size = number_of_notes;
					chord[chord_index].note = malloc(chord[chord_index].size * sizeof(chord->note));

					note_element = chord_element->children;
					for (note_index = 0; note_element != NULL; note_index++) {
						note_number = 0;
						velocity = 0;
						delay = 0;

						if (xmlGetProp(note_element, "note_number")) {
							sscanf(xmlGetProp(note_element, "note_number"), "%d", &note_number);
							printf("note: %d\t", note_number);
						}
						chord[chord_index].note[note_index].note_number = note_number;

						if (xmlGetProp(note_element, "velocity")) {
							sscanf(xmlGetProp(note_element, "velocity"), "%d", &velocity);
							printf("velocity: %d\t", velocity);
						}
						chord[chord_index].note[note_index].velocity = velocity;
						
						if (xmlGetProp(note_element, "delay")) {
							sscanf(xmlGetProp(note_element, "delay"), "%d", &delay);
						}
						chord[chord_index].note[note_index].delay = delay;

						printf("delay: %d\n", delay);
						note_element = note_element->next;
					}

					chord_element = chord_element->next;
				}
			}
    }

    /*free the document */
    xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
}


void init_program() {
	chord[GREEN].size = 1;
	chord[GREEN].note = malloc(1 * sizeof(chord->note));
	chord[GREEN].note[0].note_number = 52;
	chord[GREEN].note[0].velocity = 110;

	chord[GREEN | RED].size = 3;
	chord[GREEN | RED].note = malloc(chord[GREEN | RED].size * sizeof(chord->note));
	chord[GREEN | RED].note[0].note_number = 52;
	chord[GREEN | RED].note[0].velocity = 100;
	chord[GREEN | RED].note[1].note_number = 59;
	chord[GREEN | RED].note[1].velocity = 110;
	chord[GREEN | RED].note[2].note_number = 64;
	chord[GREEN | RED].note[2].velocity = 110;

	chord[GREEN | YELLOW].size = 3;
	chord[GREEN | YELLOW].note = malloc(chord[GREEN | YELLOW].size * sizeof(chord->note));
	chord[GREEN | YELLOW].note[0].note_number = 52;
	chord[GREEN | YELLOW].note[0].velocity = 100;
	chord[GREEN | YELLOW].note[1].note_number = 61;
	chord[GREEN | YELLOW].note[1].velocity = 110;
	chord[GREEN | YELLOW].note[2].note_number = 64;
	chord[GREEN | YELLOW].note[2].velocity = 110;


	chord[GREEN | BLUE].size = 3;
	chord[GREEN | BLUE].note = malloc(chord[GREEN | BLUE].size * sizeof(chord->note));
	chord[GREEN | BLUE].note[0].note_number = 52;
	chord[GREEN | BLUE].note[0].velocity = 100;
	chord[GREEN | BLUE].note[1].note_number = 62;
	chord[GREEN | BLUE].note[1].velocity = 110;
	chord[GREEN | BLUE].note[2].note_number = 64;
	chord[GREEN | BLUE].note[2].velocity = 110;

	chord[GREEN | RED | YELLOW].size = 4;
	chord[GREEN | RED | YELLOW].note = malloc(chord[GREEN | RED | YELLOW].size * sizeof(chord->note));
	chord[GREEN | RED | YELLOW].note[0].note_number = 52;
	chord[GREEN | RED | YELLOW].note[0].velocity = 100;
	chord[GREEN | RED | YELLOW].note[1].note_number = 59;
	chord[GREEN | RED | YELLOW].note[1].velocity = 110;
	chord[GREEN | RED | YELLOW].note[2].note_number = 64;
	chord[GREEN | RED | YELLOW].note[2].velocity = 110;
	chord[GREEN | RED | YELLOW].note[3].note_number = 67;
	chord[GREEN | RED | YELLOW].note[3].velocity = 110;

	chord[GREEN | RED | BLUE].size = 4;
	chord[GREEN | RED | BLUE].note = malloc(chord[GREEN | RED | BLUE].size * sizeof(chord->note));
	chord[GREEN | RED | BLUE].note[0].note_number = 52;
	chord[GREEN | RED | BLUE].note[0].velocity = 100;
	chord[GREEN | RED | BLUE].note[1].note_number = 59;
	chord[GREEN | RED | BLUE].note[1].velocity = 110;
	chord[GREEN | RED | BLUE].note[2].note_number = 64;
	chord[GREEN | RED | BLUE].note[2].velocity = 110;
	chord[GREEN | RED | BLUE].note[3].note_number = 68;
	chord[GREEN | RED | BLUE].note[3].velocity = 110;

	chord[RED].size = 1;
	chord[RED].note = malloc(1 * sizeof(chord->note));
	chord[RED].note[0].note_number = 57;
	chord[RED].note[0].velocity = 110;

	chord[RED | YELLOW].size = 3;
	chord[RED | YELLOW].note = malloc(chord[RED | YELLOW].size * sizeof(chord->note));
	chord[RED | YELLOW].note[0].note_number = 57;
	chord[RED | YELLOW].note[0].velocity = 100;
	chord[RED | YELLOW].note[1].note_number = 64;
	chord[RED | YELLOW].note[1].velocity = 110;
	chord[RED | YELLOW].note[2].note_number = 69;
	chord[RED | YELLOW].note[2].velocity = 110;

	chord[RED | BLUE].size = 3;
	chord[RED | BLUE].note = malloc(chord[RED | BLUE].size * sizeof(chord->note));
	chord[RED | BLUE].note[0].note_number = 57;
	chord[RED | BLUE].note[0].velocity = 100;
	chord[RED | BLUE].note[1].note_number = 66;
	chord[RED | BLUE].note[1].velocity = 110;
	chord[RED | BLUE].note[2].note_number = 69;
	chord[RED | BLUE].note[2].velocity = 110;

	chord[RED | ORANGE].size = 3;
	chord[RED | ORANGE].note = malloc(chord[RED | ORANGE].size * sizeof(chord->note));
	chord[RED | ORANGE].note[0].note_number = 57;
	chord[RED | ORANGE].note[0].velocity = 100;
	chord[RED | ORANGE].note[1].note_number = 67;
	chord[RED | ORANGE].note[1].velocity = 110;
	chord[RED | ORANGE].note[2].note_number = 69;
	chord[RED | ORANGE].note[2].velocity = 110;

	chord[RED | YELLOW | BLUE].size = 4;
	chord[RED | YELLOW | BLUE].note = malloc(chord[RED | YELLOW | BLUE].size * sizeof(chord->note));
	chord[RED | YELLOW | BLUE].note[0].note_number = 57;
	chord[RED | YELLOW | BLUE].note[0].velocity = 100;
	chord[RED | YELLOW | BLUE].note[1].note_number = 64;
	chord[RED | YELLOW | BLUE].note[1].velocity = 110;
	chord[RED | YELLOW | BLUE].note[2].note_number = 69;
	chord[RED | YELLOW | BLUE].note[2].velocity = 110;
	chord[RED | YELLOW | BLUE].note[3].note_number = 72;
	chord[RED | YELLOW | BLUE].note[3].velocity = 110;

	chord[RED | YELLOW | ORANGE].size = 4;
	chord[RED | YELLOW | ORANGE].note = malloc(chord[RED | YELLOW | ORANGE].size * sizeof(chord->note));
	chord[RED | YELLOW | ORANGE].note[0].note_number = 57;
	chord[RED | YELLOW | ORANGE].note[0].velocity = 100;
	chord[RED | YELLOW | ORANGE].note[1].note_number = 64;
	chord[RED | YELLOW | ORANGE].note[1].velocity = 110;
	chord[RED | YELLOW | ORANGE].note[2].note_number = 69;
	chord[RED | YELLOW | ORANGE].note[2].velocity = 110;
	chord[RED | YELLOW | ORANGE].note[3].note_number = 73;
	chord[RED | YELLOW | ORANGE].note[3].velocity = 110;

	chord[YELLOW].size = 1;
	chord[YELLOW].note = malloc(chord[YELLOW].size * sizeof(chord->note));
	chord[YELLOW].note[0].note_number = 59;
	chord[YELLOW].note[0].velocity = 110;

	chord[YELLOW | BLUE].size = 3;
	chord[YELLOW | BLUE].note = malloc(chord[YELLOW | BLUE].size * sizeof(chord->note));
	chord[YELLOW | BLUE].note[0].note_number = 59;
	chord[YELLOW | BLUE].note[0].velocity = 100;
	chord[YELLOW | BLUE].note[1].note_number = 66;
	chord[YELLOW | BLUE].note[1].velocity = 110;
	chord[YELLOW | BLUE].note[2].note_number = 71;
	chord[YELLOW | BLUE].note[2].velocity = 110;

	chord[YELLOW | ORANGE].size = 3;
	chord[YELLOW | ORANGE].note = malloc(chord[YELLOW | ORANGE].size * sizeof(chord->note));
	chord[YELLOW | ORANGE].note[0].note_number = 59;
	chord[YELLOW | ORANGE].note[0].velocity = 100;
	chord[YELLOW | ORANGE].note[1].note_number = 68;
	chord[YELLOW | ORANGE].note[1].velocity = 110;
	chord[YELLOW | ORANGE].note[2].note_number = 71;
	chord[YELLOW | ORANGE].note[2].velocity = 110;

	chord[BLUE].size = 1;
	chord[BLUE].note = malloc(1 * sizeof(chord->note));
	chord[BLUE].note[0].note_number = 55;
	chord[BLUE].note[0].velocity = 110;

	chord[ORANGE].size = 1;
	chord[ORANGE].note = malloc(1 * sizeof(chord->note));
	chord[ORANGE].note[0].note_number = 56;
	chord[ORANGE].note[0].velocity = 110;

	chord[BLUE | ORANGE].size = 3;
	chord[BLUE | ORANGE].note = malloc(chord[BLUE | ORANGE].size * sizeof(chord->note));
	chord[BLUE | ORANGE].note[0].note_number = 62;
	chord[BLUE | ORANGE].note[0].velocity = 100;
	chord[BLUE | ORANGE].note[1].note_number = 69;
	chord[BLUE | ORANGE].note[1].velocity = 110;
	chord[BLUE | ORANGE].note[2].note_number = 74;
	chord[BLUE | ORANGE].note[2].velocity = 110;

	chord[NONE].size = 3;
	chord[NONE].note = malloc(chord[NONE].size * sizeof(chord->note));
	chord[NONE].note[0].note_number = 55;
	chord[NONE].note[0].velocity = 100;
	chord[NONE].note[1].note_number = 62;
	chord[NONE].note[1].velocity = 110;
	chord[NONE].note[2].note_number = 67;
	chord[NONE].note[2].velocity = 110;
}

struct sigevent sigev;

void init() {
	margin.it_value.tv_sec = 0;
	margin.it_value.tv_nsec = 50000000;
	margin.it_interval.tv_sec = 0;
	margin.it_interval.tv_nsec = 0;
	sigev.sigev_notify = SIGEV_NONE;
	timer_create(CLOCK_MONOTONIC, &sigev, &countdown_id);
	timer_settime(countdown_id, 0, &margin, NULL);
	timer_gettime(countdown_id, &time_left);

	stick_state[CWIID_X] = 0;
	stick_state[CWIID_Y] = 0;
	stick_zone_acc.count = 0;
	stick_zone_acc.value = 0;
	stick_zone_average_value = 0;
	stick_zone_rotation_clockwise_counter = 0;
	stick_zone_rotation_counter_clockwise_counter = 0;


	active_notes.size = 0;
	active_notes.note = malloc(MAX_ACTIVE_NOTES_COUNT * sizeof(chord->note));

	strummer_state = STRUMMER_STATE_UNKNOWN;
	strummer_action = STRUMMER_ACTION_NONE;

	whammy_action = WHAMMY_ACTION_NONE;
	whammy_state = WHAMMY_STATE_UNKNOWN;

	stick_action = STICK_ACTION_NONE;
	stick_zone = STICK_ZONE_UNKNOWN;
}

void siginthandler(int param) {
    printf("User pressed Ctrl+C\n");
	if (client) {
		jack_deactivate(client);
		if (output_port) {
			jack_port_unregister(client, output_port);
		}
		jack_client_close(client);
	}
	if (wiimote) {
		cwiid_disable(wiimote, CWIID_FLAG_MESG_IFC);
		cwiid_close(wiimote);
	}

    system("stty echo");
    exit(1);
}


int main(int argc, char *argv[]) {
	init();
	
	struct cwiid_state state;	/* wiimote state */
	bdaddr_t bdaddr;	/* bluetooth device address */
	unsigned char mesg = 0;
	unsigned char rpt_mode = 0;

//	init_program();
	freePatchMemory();
	if (argc > 1) {
		readPatchFromFile(argv[1]);
	} else {
		readPatchFromFile("testIn.xml");
	}
//	writeCurrentPatchToFile("testOut.xml");
	cwiid_set_err(err);


	/* Connect to address given on command-line, if present */
	if (argc > 2) {
		str2ba(argv[2], &bdaddr);
	}
	else {
		bdaddr = *BDADDR_ANY;
	}

	/* Connect to the wiimote */
	printf("Put Wiimote in discoverable mode now (press 1+2)...\n");
	if (!(wiimote = cwiid_open(&bdaddr, 0))) {
		fprintf(stderr, "Unable to connect to wiimote\n");
		return -1;
	}
	if (cwiid_set_mesg_callback(wiimote, cwiid_callback)) {
		fprintf(stderr, "Unable to set message callback\n");
	}
	set_rpt_mode(wiimote, CWIID_RPT_EXT);
	cwiid_enable(wiimote, CWIID_FLAG_MESG_IFC);


	if (cwiid_get_state(wiimote, &state)) {
		fprintf(stderr, "Error getting state\n");
	}
	if (cwiid_request_status(wiimote)) {
		fprintf(stderr, "Error requesting status message\n");
	}


	jack_nframes_t nframes;
	if((client = jack_client_open ("LouWiiGui", JackNullOption, NULL)) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}
	jack_set_process_callback (client, process, 0);
	output_port = jack_port_register (client, "Jack MIDI out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	nframes = jack_get_buffer_size(client);


	if (jack_activate(client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

    system("stty -echo");
    signal(SIGINT, siginthandler);

	while (1) {
		sleep(1);
	}
	return 0;
}

