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
#include <stddef.h>

#include <jack/jack.h>
#include <jack/midiport.h>

#include <bluetooth/bluetooth.h>
#include <cwiid.h>

#include <libxml2/libxml/encoding.h>
#include <libxml2/libxml/xmlwriter.h>

#include "lou.h"

#define LOUWIIGUI_CWIID_RPT_MODE (CWIID_RPT_EXT | CWIID_RPT_BTN) // | CWIID_RPT_ACC)

#define USE_MIDI_CHANNEL bank[selected_bank].midi_channel ? bank[selected_bank].midi_channel : midi_channel

void neio(int sig, siginfo_t *si, void *uc) {
	struct delayed_note_t * dn;
	dn = (struct delayed_note_t *) si->si_value.sival_ptr;
	queued_notes.note[queued_notes.size].velocity = dn->note.velocity;
	queued_notes.note[queued_notes.size].note_number = dn->note.note_number;
	queued_notes.size++;
	dn->note.velocity = 0;
}
// end attempt



void usage() {
	fprintf(stderr, "louwiigui [filename] [btaddr]\n");
}
/* used for debugging
#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0)
*/
void cwiid_callback(cwiid_wiimote_t *wiimote, int mesg_count,
                    union cwiid_mesg mesg[], struct timespec *timestamp){
	int i, j;
	int valid_source;
	uint8_t drums_buttons_change;
	uint16_t buttons_change;
	uint16_t buttons;

	for (i=0; i < mesg_count; i++) {
		switch (mesg[i].type) {
			case CWIID_MESG_BTN:
				printf("button\n");
				buttons = ((uint16_t)mesg[i].btn_mesg.buttons);
				buttons_change = buttons ^ buttons_previous;
				buttons_previous = buttons;
				if ((buttons & CWIID_BTN_MINUS & buttons_change) == CWIID_BTN_MINUS) {
					transpose += 1;
				}
				if ((buttons & CWIID_BTN_PLUS & buttons_change) == CWIID_BTN_PLUS) {
					transpose -= 1;
				}
				if ((buttons & CWIID_BTN_HOME & buttons_change) == CWIID_BTN_HOME) {
					transpose = 0;
				}
				if ((buttons & CWIID_BTN_1) && (buttons & CWIID_BTN_2) && bank[2].selectable && selected_bank != 2) {
					selected_bank = 2;
					printf("selected bank: %d\n", selected_bank);
				}
				if ((buttons & CWIID_BTN_1) && !(buttons & CWIID_BTN_2) && !(buttons_change & CWIID_BTN_2) && bank[0].selectable && selected_bank != 0) {
					selected_bank = 0;
					printf("selected bank: %d\n", selected_bank);
				}
				if ((buttons & CWIID_BTN_2) && !(buttons & CWIID_BTN_1) && !(buttons_change & CWIID_BTN_1) && bank[1].selectable && selected_bank != 1) {
					selected_bank = 1;
					printf("selected bank: %d\n", selected_bank);
				}


				printf("buttons: %d\ntranspose: %d\n", buttons, transpose);
				break;
			case CWIID_MESG_ACC:
				printf("acc: x %d\ty %d\tz %d", mesg[i].acc_mesg.acc[0], mesg[i].acc_mesg.acc[1], mesg[i].acc_mesg.acc[2]);
				break;
			case CWIID_MESG_DRUMS:
				drums_buttons_change = ((uint8_t)mesg[i].drums_mesg.buttons) ^ drums_buttons_previous;
				drums_buttons_previous = (uint8_t)mesg[i].drums_mesg.buttons;

				if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_PEDAL & drums_buttons_change) == CWIID_DRUMS_PEDAL 
		//			&& (drums_buttons_change & CWIID_DRUMS_PEDAL) == CWIID_DRUMS_PEDAL
					) {
					drums_action |= PEDAL;
					printf("pedal\n");
				}
				if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_RED & drums_buttons_change) == CWIID_DRUMS_RED 
		//			&& (drums_buttons_change & CWIID_DRUMS_RED) == CWIID_DRUMS_RED
					) {
					drums_action |= RED;
					printf("red\n");
				}
				if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_YELLOW & drums_buttons_change) == CWIID_DRUMS_YELLOW 
		//			&& (drums_buttons_change & CWIID_DRUMS_YELLOW) == CWIID_DRUMS_YELLOW
					) {
					drums_action |= YELLOW;
					printf("yellow\n");
				}
				if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_BLUE & drums_buttons_change) == CWIID_DRUMS_BLUE 
		//		    && (drums_buttons_change & CWIID_DRUMS_BLUE) == CWIID_DRUMS_BLUE
					) {
					drums_action |= BLUE;
					printf("blue\n");
				}
				if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_ORANGE & drums_buttons_change) == CWIID_DRUMS_ORANGE 
		//			&& (drums_buttons_change & CWIID_DRUMS_ORANGE) == CWIID_DRUMS_ORANGE
					) {
					drums_action |= ORANGE;
					printf("orange\n");
				}
				if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_GREEN & drums_buttons_change) == CWIID_DRUMS_GREEN 
		//			&& (drums_buttons_change & CWIID_DRUMS_GREEN) == CWIID_DRUMS_GREEN
					) {
					drums_action |= GREEN;
					printf("green\n");
				}


				break;
			case CWIID_MESG_GUITAR:
				printf("\n\ngitarbeskjed\n\n");
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

				// set stick state and action
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
				break;
			case CWIID_MESG_TURNTABLES:
				printf("platespillerbeskjed\n");
				printf("stick x: %x,\ty: %x\n", mesg[i].turntables_mesg.stick[CWIID_X], mesg[i].turntables_mesg.stick[CWIID_Y]);
				printf("left: %d,\tright: %d\n", mesg[i].turntables_mesg.left_turntable, mesg[i].turntables_mesg.right_turntable);
				printf("X-fader: %d, effect: %d\n", mesg[i].turntables_mesg.crossfader, mesg[i].turntables_mesg.effect_dial);
				if (current_turntables_state.crossfader != mesg[i].turntables_mesg.crossfader) {
					if (current_turntables_state.crossfader < mesg[i].turntables_mesg.crossfader) {
						crossfader_action = CROSSFADER_ACTION_FADE_LEFT;
					} else {
						crossfader_action = CROSSFADER_ACTION_FADE_RIGHT;
					}
					current_turntables_state.crossfader = mesg[i].turntables_mesg.crossfader;
				}
				if (effect_dial_action == EFFECT_DIAL_ACTION_INITIALIZE) {
					current_turntables_state.effect_dial = mesg[i].turntables_mesg.effect_dial;
				}
				if (current_turntables_state.effect_dial != mesg[i].turntables_mesg.effect_dial) {
				
					if (mesg[i].turntables_mesg.effect_dial > current_turntables_state.effect_dial) {
						if (mesg[i].turntables_mesg.effect_dial > current_turntables_state.effect_dial + (CWIID_TURNTABLES_EFFECT_DIAL_MAX / 2)) {
							// effect dial has passed 0 (equals 32 modulo 32...)
							effect_dial_state.change = mesg[i].turntables_mesg.effect_dial - current_turntables_state.effect_dial - CWIID_TURNTABLES_EFFECT_DIAL_MAX - 1;
							effect_dial_action = EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE;
						} else {
							effect_dial_state.change = mesg[i].turntables_mesg.effect_dial - current_turntables_state.effect_dial;
							effect_dial_action = EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE;
						}
					} else {
						if (mesg[i].turntables_mesg.effect_dial < current_turntables_state.effect_dial - (CWIID_TURNTABLES_EFFECT_DIAL_MAX / 2)) {
							// effect dial has passed max value (equals 0 modulo max)
							effect_dial_state.change = mesg[i].turntables_mesg.effect_dial + (CWIID_TURNTABLES_EFFECT_DIAL_MAX + 1 - current_turntables_state.effect_dial);
							effect_dial_action = EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE;
						} else {
							effect_dial_state.change = mesg[i].turntables_mesg.effect_dial - current_turntables_state.effect_dial;
							effect_dial_action = EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE;
						}
					}
					current_turntables_state.effect_dial = mesg[i].turntables_mesg.effect_dial;
				}
				break;
			default:
				printf("\nUNKNOWN MESSAGE TYPE\n\n");
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

void note_on(struct note_t note, void* port_buf, int i) {
	unsigned char* buffer;
	int use_midi_channel = note.midi_channel ? note.midi_channel : USE_MIDI_CHANNEL;

	struct note_t current_note;
	current_note.velocity = note.velocity;
	current_note.note_number = note.note_number + transpose;
	current_note.midi_channel = use_midi_channel;
	
	printf("use: %d, bank: %d, note: %d, patch: %d\n", use_midi_channel, bank[selected_bank].midi_channel, note.midi_channel, midi_channel);
	buffer = jack_midi_event_reserve(port_buf, i, 3);
	buffer[2] = note.velocity;		/* velocity */
	buffer[1] = note.note_number + transpose;	/* note number */
	buffer[0] = MIDI_NOTE_ON + use_midi_channel - 1;	/* note on */
	active_notes.note[active_notes.size] = current_note;
	active_notes.size++;
}

void strum_chord(struct chord_t chord, void* port_buf, int i) {
	int q,r = 0;
	for (q = 0; q < chord.size; q++) {
		if (chord.note[q].delay == 0) {
			note_on(chord.note[q], port_buf, i);
		} 
		else {
			if (chord.note[q].velocity > 0) {
				while (delayed_notes[r].note.velocity != 0) {
					r++;
				}
				delayed_notes[r].note = chord.note[q];
				delayed_notes[r].sevent.sigev_signo = SIGUSR1;
				delayed_notes[r].sevent.sigev_notify = SIGEV_SIGNAL;
				delayed_notes[r].sevent.sigev_value.sival_ptr = &(delayed_notes[r]);

				timer_create(CLOCK_MONOTONIC, &(delayed_notes[r].sevent), &(delayed_notes[r].timer));

				delayed_notes[r].note.velocity = chord.note[q].velocity;
				delayed_notes[r].note.note_number = chord.note[q].note_number;
				delayed_notes[r].time.it_value.tv_sec = chord.note[q].delay / 1000;
				delayed_notes[r].time.it_value.tv_nsec = (chord.note[q].delay * 1000000) % 1000000000;
				delayed_notes[r].time.it_interval.tv_sec = 0;
				delayed_notes[r].time.it_interval.tv_nsec = 0;

				timer_settime(delayed_notes[r].timer, 0, &(delayed_notes[r].time), NULL);
			}
		}
	}
}

void mute(void *port_buf, int i) {
	unsigned char* buffer;
	int use_midi_channel = USE_MIDI_CHANNEL;
	int note_midi_channel;

	int j;
	for(j=0; j < MAX_DELAYED_NOTES_COUNT; j++){
		if (delayed_notes[j].note.velocity != 0) {
			timer_delete(delayed_notes[j].timer);
		}
		delayed_notes[j].note.velocity = 0;
	}
	queued_notes.size = 0;
	while(active_notes.size > 0) {
		note_midi_channel = active_notes.note[active_notes.size - 1].midi_channel;
		buffer = jack_midi_event_reserve(port_buf, i, 3);
		buffer[2] = active_notes.note[active_notes.size - 1].velocity;		/* velocity */
		buffer[1] = active_notes.note[active_notes.size - 1].note_number;	/* note number */
		buffer[0] = MIDI_NOTE_OFF + (note_midi_channel ? note_midi_channel : use_midi_channel) - 1;	/* note off, standard midi channel */
		active_notes.size--;
	}
}

int process(jack_nframes_t nframes, void *arg) {
	int i,j;
	void* port_buf = jack_port_get_buffer(output_port, nframes);
	unsigned char* buffer;
	jack_midi_clear_buffer(port_buf);
	int use_midi_channel = USE_MIDI_CHANNEL;

	for(i = 0; i < nframes; i++) {
		while(queued_notes.size > 0) {
			note_on(queued_notes.note[queued_notes.size - 1], port_buf, i);
			queued_notes.size--;
		}
		if (strummer_action != STRUMMER_ACTION_NONE) {
			if (strummer_action == STRUMMER_ACTION_MID_DOWN || strummer_action == STRUMMER_ACTION_MID_UP) {
				strum_chord(bank[selected_bank].chord[chord_state], port_buf, i);
				strummer_action = STRUMMER_ACTION_NONE;
			}
			else {
				if (strummer_state != STRUMMER_STATE_SUSTAINED) {
					mute(port_buf, i);
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
			buffer[0] = MIDI_PITCH_WHEEL + use_midi_channel - 1;	// pitch wheel change 
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
			buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;	// control change 
			touchbar_action = TOUCHBAR_ACTION_NONE;
		}
		if (stick_action != STICK_ACTION_NONE) {
			printf("stick_action!\n");
			unsigned int volume = last_sent_volume_value;
			if (stick_action == STICK_ACTION_ROTATE_COUNTER_CLOCKWISE) {
				volume += stick_zone_average_value / 2 ;
				if (volume > 127) {
					volume = 127; 
				}
			}
			else if (stick_action == STICK_ACTION_ROTATE_CLOCKWISE) {
				if (volume >= stick_zone_average_value / 2) {
					volume -= stick_zone_average_value / 2;
				} else {
					volume = 0;
				}
			}
			if (volume != last_sent_volume_value) {
				printf("volume: %d\n", volume);
				buffer = jack_midi_event_reserve(port_buf, i, 3);
				buffer[2] = volume;
				buffer[1] = 0x7;        // volume
				buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;	// control change
				last_sent_volume_value = volume;
			}
			stick_action = STICK_ACTION_NONE;
		}
		while (drums_action != 0) {
			uint8_t send_note_off = 0;
			buffer = jack_midi_event_reserve(port_buf, i, 3);
			buffer[2] = 0x7F;		/* velocity */
			if ((drums_action & RED) == RED) {
/*				if ((drums_state & RED) == RED) {
					send_note_off = 1;
				}
				drums_state ^= RED;
*/
				buffer[1] = 38;	/* note number */
				drums_action &= ~RED;
			} else if ((drums_action & YELLOW) == YELLOW) {
/*				if ((drums_state & YELLOW) == YELLOW) {
					send_note_off = 1;
				}
				drums_state ^= YELLOW;
*/
				buffer[1] = 42;	/* note number */
				drums_action &= ~YELLOW;
			} else if ((drums_action & BLUE) == BLUE) {
/*				if ((drums_state & BLUE) == BLUE) {
					send_note_off = 1;
				}
				drums_state ^= BLUE;
*/
				buffer[1] = 48;	/* note number */
				drums_action &= ~BLUE;
			} else if ((drums_action & ORANGE) == ORANGE) {
/*				if ((drums_state & ORANGE) == ORANGE) {
					send_note_off = 1;
				}
				drums_state ^= ORANGE;
*/
				buffer[1] = 51;	/* note number */
				drums_action &= ~ORANGE;
			} else if ((drums_action & GREEN) == GREEN) {
/*				if ((drums_state & GREEN) == GREEN) {
					send_note_off = 1;
				}
				drums_state ^= GREEN;
*/
				buffer[1] = 45;	/* note number */
				drums_action &= ~GREEN;
			} else if ((drums_action & PEDAL) == PEDAL) {
/*				if ((drums_state & PEDAL) == PEDAL) {
					send_note_off = 1;
				}
				drums_state ^= PEDAL;
*/
				buffer[1] = 36;	/* note number */
				drums_action &= ~PEDAL;
			}
			
			if (send_note_off) {
				buffer[0] = MIDI_NOTE_OFF + 9;	// note off
			} else {
				buffer[0] = MIDI_NOTE_ON + 9;	// note on
			}
		}
		
		if (crossfader_action != CROSSFADER_ACTION_NONE) {
			buffer = jack_midi_event_reserve(port_buf, i, 3);
			uint8_t crossfader_midi_value = current_turntables_state.crossfader * 127 / CWIID_TURNTABLES_CROSSFADER_MAX;
			buffer[2] = crossfader_midi_value;
			buffer[1] = MIDI_BALANCE_MSB;
			buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;	// control change
			crossfader_action = CROSSFADER_ACTION_NONE;
		}
		if (effect_dial_action != EFFECT_DIAL_ACTION_NONE) {
			buffer = jack_midi_event_reserve(port_buf, i, 3);
			buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;	// control change
			buffer[1] = MIDI_EFFECT_CTL_1_MSB;
			int16_t signed_value;
			
			switch (effect_dial_action) {
			case EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE:
				effect_dial_state.value += effect_dial_state.change;
				if (effect_dial_state.value > effect_dial_state.max_value) {
					effect_dial_state.value = effect_dial_state.max_value;
				}
				break;
			case EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE:
				signed_value = effect_dial_state.value + effect_dial_state.change;
				effect_dial_state.value += effect_dial_state.change;
				if (signed_value < effect_dial_state.min_value) {
					effect_dial_state.value = effect_dial_state.min_value;
				}
				break;
			case EFFECT_DIAL_ACTION_INITIALIZE:
				printf("initialize\n");
				current_turntables_state.effect_dial = effect_dial_state.initial_value;
				break;
			}
			buffer[2] = effect_dial_state.value;
			effect_dial_action = EFFECT_DIAL_ACTION_NONE;
		}

	}
	return 0;
}

void set_rpt_mode(cwiid_wiimote_t *wiimote, uint16_t rpt_mode) {
	if (cwiid_set_rpt_mode(wiimote, rpt_mode)) {
		fprintf(stderr, "Error setting report mode\n");
	}
}








/* XML FUNCTIONS: */


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

		/* Add an attribute with name "velocity" and value chord[i].note.velocity to note. */
		rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "midi_channel", "%d", midi_channel);
		if (rc < 0) {
			printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
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
				if (chord[chord_index].note[note_index].delay != 0) {
					/* Add an attribute with name "delay" and value chord[i].note.delay to note. */
					rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "delay", "%d", chord[chord_index].note[note_index].delay);
					if (rc < 0) {
						printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
						return;
					}
				}
				if (chord[chord_index].note[note_index].midi_channel != 0) {
					/* Add an attribute with name "midi_channel" and value chord[i].note.midi_channel to note. */
					rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "midi_channel", "%d", chord[chord_index].note[note_index].midi_channel);
					if (rc < 0) {
						printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute note.midi_channel\n");
						return;
					}
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
    xmlNode *root_element, *bank_element, *bank_content, *chord_element, *note_element, *cur = NULL;
		int bank_index, chord_index, note_index, number_of_notes, note_number, velocity, delay = 0, bank_midi_channel, note_midi_channel;

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
  		printf ("patch name: %s\n", xmlGetProp(cur, "name"));
		if (xmlGetProp(cur, "midi_channel")) {
			sscanf(xmlGetProp(cur, "midi_channel"), "%d", &midi_channel);
		}
		printf("midi channel: %d\n", midi_channel);
  		cur = cur->children;

		while (cur->type != XML_ELEMENT_NODE) { 
			cur = cur->next;
		}
		if (!strcmp(cur->name, "banks")) {
			printf("%s\n", cur->name);
			bank_element = cur->children;
			bank_index = 0;

			while (bank_element != NULL) {
				if (bank_element->type == XML_ELEMENT_NODE) {
					printf("bank %d %s\n", bank_index, xmlGetProp(bank_element, "name"));
					if (xmlGetProp(bank_element, "midi_channel")) {
						sscanf(xmlGetProp(bank_element, "midi_channel"), "%d", &(bank[bank_index].midi_channel));
					}
					bank_content = bank_element->children;

					while (bank_content != NULL) {
						if (bank_content->type == XML_ELEMENT_NODE) {
							printf("%s\n", bank_content->name);

							if (!strcmp(bank_content->name, "chords")) {
								chord_element = bank_content->children;

								while (chord_element != NULL) {
									if (chord_element->type == XML_ELEMENT_NODE) {
										chord_index = 0;
										number_of_notes = 0;

										if (xmlGetProp(chord_element, "green")) {
											chord_index = chord_index | GREEN;
											printf("GREEN\t");
										}
										if (xmlGetProp(chord_element, "red")) {
											chord_index = chord_index | RED;
											printf("RED\t");
										}
										if (xmlGetProp(chord_element, "yellow")) {
											chord_index = chord_index | YELLOW;
											printf("YELLOW\t");
										}
										if (xmlGetProp(chord_element, "blue")) {
											chord_index = chord_index | BLUE;
											printf("BLUE\t");
										}
										if (xmlGetProp(chord_element, "orange")) {
											chord_index = chord_index | ORANGE;
											printf("ORANGE");
										}
										if (xmlGetProp(chord_element, "number_of_notes")) {
											sscanf(xmlGetProp(chord_element, "number_of_notes"), "%d", &number_of_notes);
										}
										printf("\n");

										bank[bank_index].chord[chord_index].size = number_of_notes;
										bank[bank_index].chord[chord_index].note = malloc(bank[bank_index].chord[chord_index].size * sizeof(struct note_t));

										note_element = chord_element->children;
										note_index = 0;
										while (note_element != NULL && note_index < number_of_notes) {
											if (note_element->type == XML_ELEMENT_NODE) {
												note_number = 0;
												velocity = 0;
												delay = 0;
												note_midi_channel = 0;
					

												if (xmlGetProp(note_element, "note_number")) {
													sscanf(xmlGetProp(note_element, "note_number"), "%d", &note_number);
													printf("note: %d\t", note_number);
												}
												bank[bank_index].chord[chord_index].note[note_index].note_number = note_number;

												if (xmlGetProp(note_element, "velocity")) {
													sscanf(xmlGetProp(note_element, "velocity"), "%d", &velocity);
													printf("velocity: %d\t", velocity);
												}
												bank[bank_index].chord[chord_index].note[note_index].velocity = velocity;
										
												if (xmlGetProp(note_element, "delay")) {
													sscanf(xmlGetProp(note_element, "delay"), "%d", &delay);
												}
												bank[bank_index].chord[chord_index].note[note_index].delay = delay;
												printf("delay: %d", delay);

												if (xmlGetProp(note_element, "midi_channel")) {
													sscanf(xmlGetProp(note_element, "midi_channel"), "%d", &note_midi_channel);
													bank[bank_index].chord[chord_index].note[note_index].midi_channel = note_midi_channel;
													printf("\tmidi_channel: %d\n", note_midi_channel);
												} else {
													bank[bank_index].chord[chord_index].note[note_index].midi_channel = 0;
													printf("\n");
												}


												note_index++;
											}
											note_element = note_element->next;
										}
									}
									chord_element = chord_element->next;
								}

							}
						}
						bank_content = bank_content->next;
					}
					bank[bank_index].selectable = 1;
					bank_index += 1;
				}


				bank_element = bank_element->next;
			}
			printf("\n");
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




/* END XML FUNCTIONS */









struct sigevent sigev;


void init() {
	midi_channel = 0;
	bank_midi_channel = 0;
	midi_program = 3;
	transpose = 0;
	last_sent_volume_value = 127;

	margin.it_value.tv_sec = 0;
	margin.it_value.tv_nsec = 50000000;
	margin.it_interval.tv_sec = 0;
	margin.it_interval.tv_nsec = 0;
	sigev.sigev_notify = SIGEV_NONE;
	timer_create(CLOCK_MONOTONIC, &sigev, &countdown_id);
	timer_settime(countdown_id, 0, &margin, NULL);
	timer_gettime(countdown_id, &time_left);

	drums_action = 0;
	stick_state[CWIID_X] = 0;
	stick_state[CWIID_Y] = 0;
	stick_zone_acc.count = 0;
	stick_zone_acc.value = 0;
	stick_zone_average_value = 0;
	stick_zone_rotation_clockwise_counter = 0;
	stick_zone_rotation_counter_clockwise_counter = 0;

	int i;
	bank = malloc(MAX_BANKS_COUNT * sizeof(struct bank_t));
	for(i = 0; i < MAX_BANKS_COUNT; i++) {
		bank[i].midi_channel = 0;
		bank[i].selectable = 0;
	}	

	delayed_notes = malloc(MAX_DELAYED_NOTES_COUNT * sizeof(struct delayed_note_t));
	for(i = 0; i < MAX_DELAYED_NOTES_COUNT; i++){
		delayed_notes[i].note.velocity = 0;
	}

	queued_notes.size = 0;
	queued_notes.note = malloc(MAX_QUEUED_NOTES_COUNT * sizeof(chord->note));

	active_notes.size = 0;
	active_notes.note = malloc(MAX_ACTIVE_NOTES_COUNT * sizeof(chord->note));

	strummer_state = STRUMMER_STATE_UNKNOWN;
	strummer_action = STRUMMER_ACTION_NONE;

	whammy_action = WHAMMY_ACTION_NONE;
	whammy_state = WHAMMY_STATE_UNKNOWN;

	stick_action = STICK_ACTION_NONE;
	stick_zone = STICK_ZONE_UNKNOWN;
	
	drums_buttons_previous = 0;
	
	effect_dial_action = EFFECT_DIAL_ACTION_INITIALIZE;
	effect_dial_state.max_value = 127;
	effect_dial_state.min_value = 0;
	effect_dial_state.initial_value = 63;
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

//experiment with delayed notes goes here 
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = neio;
	sigaction(SIGUSR1, &sa, NULL);

// end experiment


	freePatchMemory();
	if (argc > 1) {
		readPatchFromFile(argv[1]);
	} else {
		readPatchFromFile("testIn.xml");
	}
	writeCurrentPatchToFile("testOut.xml");
	cwiid_set_err(err);


	/* Connect to address given on command-line, if present */
	//if (argc > 2) {
	//	str2ba(argv[2], &bdaddr);
	//}
	//else {
		bdaddr = *BDADDR_ANY;
	//}

	/* Connect to the wiimote */
	printf("Put Wiimote in discoverable mode now (press 1+2)...\n");
	if (!(wiimote = cwiid_open(&bdaddr, 0))) {
		fprintf(stderr, "Unable to connect to wiimote\n");
		return -1;
	}
	if (cwiid_set_mesg_callback(wiimote, cwiid_callback)) {
		fprintf(stderr, "Unable to set message callback\n");
	}
	set_rpt_mode(wiimote, LOUWIIGUI_CWIID_RPT_MODE);
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
	output_port = jack_port_register (client, "JackMIDIout", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
//	input_port = jack_port_register (client, "Jack MIDI in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	nframes = jack_get_buffer_size(client);


	if (jack_activate(client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	const char **ports;
  if ((ports = jack_get_ports (client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput)) == NULL) {
    fprintf(stderr, "Cannot find any physical playback ports\n");
  }
	else {
		if (jack_connect (client, jack_port_name(output_port), ports[0])) {
		  fprintf (stderr, "cannot connect output ports\n");
		}
	}
  free(ports);

	printf("kaci");


	system("stty -echo");
	signal(SIGINT, siginthandler);

	while (1) {
		sleep(1);
	}
	return 0;
}

