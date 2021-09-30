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
#include <string.h>
#include <math.h>

//*
#include <jack/jack.h>
#include <jack/midiport.h>
//*/

#include <bluetooth/bluetooth.h>
#include <cwiid.h>

#include "lou.h"
#include "midi.h"
#include "xml_file_io.h"

#define LOUWIIGUI_CWIID_RPT_MODE (CWIID_RPT_EXT | CWIID_RPT_BTN) // | CWIID_RPT_ACC)

#define DISTANCE(a,b) sqrt(pow(a, 2) + pow(b, 2));
#define DEFAULT_MIDI_CHANNEL bank[state.selected_bank].midi.default_channel ? bank[state.selected_bank].midi.default_channel : patch.midi.default_channel



struct patch_t patch;
struct bank_t *bank;
struct state_t state;
struct chord_t empty_chord;

unsigned char* note_frqs;
jack_nframes_t* note_starts;
jack_nframes_t* note_lengths;
jack_nframes_t num_notes;
jack_nframes_t loop_nsamp;
jack_nframes_t loop_index;

timer_t countdown_id;
struct itimerspec margin;
struct itimerspec time_left;

cwiid_wiimote_t *wiimote;   /* wiimote handle */
cwiid_mesg_callback_t cwiid_callback;

jack_client_t *client;
jack_port_t *output_port;
jack_port_t *input_port;


void neio(int sig, siginfo_t *si, void *uc) {
    struct delayed_note_t * dn;
    dn = (struct delayed_note_t *) si->si_value.sival_ptr;
    state.queued_notes.note[state.queued_notes.size].velocity = dn->note.velocity;
    state.queued_notes.note[state.queued_notes.size].note_number = dn->note.note_number;
    state.queued_notes.note[state.queued_notes.size].midi_channel = dn->note.midi_channel;
    state.queued_notes.note[state.queued_notes.size].sustain_mode = dn->note.sustain_mode;
    state.queued_notes.note[state.queued_notes.size].string = dn->note.string;
    state.queued_notes.size++;
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


void selectBank (unsigned char bank_number, void *port_buf, jack_nframes_t time) {
    if (bank[bank_number].selectable && state.selected_bank != bank_number) {
        state.selected_bank = bank_number;

        if (bank[state.selected_bank].midi.program_change_count > 0) {
            sendMidiConfiguration(bank[state.selected_bank].midi, DEFAULT_MIDI_CHANNEL, port_buf, time);
        } else {
            sendMidiConfiguration(patch.midi, DEFAULT_MIDI_CHANNEL, port_buf, time);
        }

        if (bank[state.selected_bank].midi_static_messages->length > 0) {
            sendStaticMessages(bank[state.selected_bank].midi_static_messages, DEFAULT_MIDI_CHANNEL, port_buf, time);
            /*
            printf("cc %d i bank %d\n", bank[state.selected_bank].cc_length, state.selected_bank);
        	for (int j = 0; j < bank[state.selected_bank].cc_length; j++) {
        		sendCCMessage(bank[state.selected_bank].cc[j], port_buf, time);
        	}
            */
        }

        printf("selected bank %d: %s\n", state.selected_bank, bank[state.selected_bank].name);
    }
}

void strum_chord (struct chord_t chord, unsigned char direction, unsigned char default_channel, void* port_buf, int i) {
    int q,r = 0;
    for (q = 0; q < chord.size; q++) {
        if (chord.note[q].direction == BOTH || chord.note[q].direction == direction) {
            if (chord.note[q].delay == 0) {
                note_on(chord.note[q], state.transpose, default_channel, port_buf, i);
            } else if (chord.note[q].velocity > 0) {
                while (state.delayed_notes[r].note.velocity != 0) {
                    r++;
                }
                state.delayed_notes[r].note = chord.note[q];
                state.delayed_notes[r].sevent.sigev_signo = SIGUSR1;
                state.delayed_notes[r].sevent.sigev_notify = SIGEV_SIGNAL;
                state.delayed_notes[r].sevent.sigev_value.sival_ptr = &(state.delayed_notes[r]);

                timer_create(CLOCK_MONOTONIC, &(state.delayed_notes[r].sevent), &(state.delayed_notes[r].timer));

                state.delayed_notes[r].note.velocity = chord.note[q].velocity;
                state.delayed_notes[r].note.note_number = chord.note[q].note_number;
                state.delayed_notes[r].note.midi_channel = chord.note[q].midi_channel;
                state.delayed_notes[r].note.sustain_mode = chord.note[q].sustain_mode;
                state.delayed_notes[r].note.string = chord.note[q].string;
                state.delayed_notes[r].time.it_value.tv_sec = chord.note[q].delay / 1000;
                state.delayed_notes[r].time.it_value.tv_nsec = (chord.note[q].delay * 1000000) % 1000000000;
                state.delayed_notes[r].time.it_interval.tv_sec = 0;
                state.delayed_notes[r].time.it_interval.tv_nsec = 0;

                timer_settime(state.delayed_notes[r].timer, 0, &(state.delayed_notes[r].time), NULL);
            }
        }
    }
}

void strum_chord_stringed_notes_only (struct chord_t chord, unsigned char default_channel, void* port_buf, int i) {
    int q,r = 0;
    for (q = 0; q < chord.size; q++) {
        if (chord.note[q].string != 0 && state.string[chord.note[q].string].velocity != 0) {
            note_on(chord.note[q], state.transpose, default_channel, port_buf, i);
        }
    }
}

void cwiid_callback(cwiid_wiimote_t *wiimote, int mesg_count,
                    union cwiid_mesg mesg[], struct timespec *timestamp){
    int i, j;
    int valid_source;
    uint8_t drums_buttons_change;
    uint16_t buttons_change;
    uint16_t buttons;
    uint8_t chord;

    for (i = 0; i < mesg_count; i++) {
        switch (mesg[i].type) {
            case CWIID_MESG_BTN:
                buttons = ((uint16_t)mesg[i].btn_mesg.buttons);
                buttons_change = buttons ^ state.buttons_previous;
                state.buttons_previous = buttons;
                if ((buttons & CWIID_BTN_MINUS & buttons_change) == CWIID_BTN_MINUS) {
                    state.transpose += 1;
                }
                if ((buttons & CWIID_BTN_PLUS & buttons_change) == CWIID_BTN_PLUS) {
                    state.transpose -= 1;
                }
                if ((buttons & CWIID_BTN_HOME & buttons_change) == CWIID_BTN_HOME) {
                    state.transpose = 0;
                }
                if ((buttons & CWIID_BTN_1) && (buttons & CWIID_BTN_2) && bank[2].selectable && state.selected_bank != 2) {
                    state.action.buttons = BUTTONS_ACTION_BANK_CHANGE;
                    state.action.buttons_data = 2;
                }
                if ((buttons & CWIID_BTN_1) && !(buttons & CWIID_BTN_2) && !(buttons_change & CWIID_BTN_2) && bank[0].selectable && state.selected_bank != 0) {
                    state.action.buttons = BUTTONS_ACTION_BANK_CHANGE;
                    state.action.buttons_data = 0;
                }
                if ((buttons & CWIID_BTN_2) && !(buttons & CWIID_BTN_1) && !(buttons_change & CWIID_BTN_1) && bank[1].selectable && state.selected_bank != 1) {
                    state.action.buttons = BUTTONS_ACTION_BANK_CHANGE;
                    state.action.buttons_data = 1;
                }
                if (buttons == CWIID_BTN_A + CWIID_BTN_RIGHT) {
                    state.action.buttons = BUTTONS_ACTION_NEXT_PATCH;
                } else if (buttons == CWIID_BTN_A + CWIID_BTN_LEFT) {
                    state.action.buttons = BUTTONS_ACTION_PREVIOUS_PATCH;
                }
                if (buttons == CWIID_BTN_RIGHT) {
                    state.action.buttons = BUTTONS_ACTION_NEXT_STICK_TARGET;
                }
                if (buttons == CWIID_BTN_LEFT) {
                    state.action.buttons = BUTTONS_ACTION_PREVIOUS_STICK_TARGET;
                }


                printf("buttons: %d\ttranspose: %d\n", buttons, state.transpose);
                break;
            case CWIID_MESG_ACC:
                printf("acc: x %d\ty %d\tz %d", mesg[i].acc_mesg.acc[0], mesg[i].acc_mesg.acc[1], mesg[i].acc_mesg.acc[2]);
                break;
            case CWIID_MESG_DRUMS:
                drums_buttons_change = ((uint8_t)mesg[i].drums_mesg.buttons) ^ state.drums_buttons_previous;
                state.drums_buttons_previous = (uint8_t)mesg[i].drums_mesg.buttons;

                if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_PEDAL & drums_buttons_change) == CWIID_DRUMS_PEDAL 
        //            && (drums_buttons_change & CWIID_DRUMS_PEDAL) == CWIID_DRUMS_PEDAL
                    ) {
                    state.action.drums |= PEDAL;
                    printf("pedal\n");
                }
                if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_RED & drums_buttons_change) == CWIID_DRUMS_RED 
        //            && (drums_buttons_change & CWIID_DRUMS_RED) == CWIID_DRUMS_RED
                    ) {
                    state.action.drums |= RED;
                    printf("red\n");
                }
                if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_YELLOW & drums_buttons_change) == CWIID_DRUMS_YELLOW 
        //            && (drums_buttons_change & CWIID_DRUMS_YELLOW) == CWIID_DRUMS_YELLOW
                    ) {
                    state.action.drums |= YELLOW;
                    printf("yellow\n");
                }
                if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_BLUE & drums_buttons_change) == CWIID_DRUMS_BLUE 
        //            && (drums_buttons_change & CWIID_DRUMS_BLUE) == CWIID_DRUMS_BLUE
                    ) {
                    state.action.drums |= BLUE;
                    printf("blue\n");
                }
                if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_ORANGE & drums_buttons_change) == CWIID_DRUMS_ORANGE 
        //            && (drums_buttons_change & CWIID_DRUMS_ORANGE) == CWIID_DRUMS_ORANGE
                    ) {
                    state.action.drums |= ORANGE;
                    printf("orange\n");
                }
                if ((mesg[i].drums_mesg.buttons & CWIID_DRUMS_GREEN & drums_buttons_change) == CWIID_DRUMS_GREEN 
        //            && (drums_buttons_change & CWIID_DRUMS_GREEN) == CWIID_DRUMS_GREEN
                    ) {
                    state.action.drums |= GREEN;
                    printf("green\n");
                }


                break;
            case CWIID_MESG_GUITAR:
                printf("guitar message: ");
                // set state.strummer and state.action.strummer
                if (((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_DOWN) == CWIID_GUITAR_BTN_DOWN) && state.strummer != STRUMMER_STATE_DOWN) {
                    state.strummer = STRUMMER_STATE_DOWN;
                    state.action.strummer = STRUMMER_ACTION_MID_DOWN;
                    timer_settime(countdown_id, 0, &margin, NULL);
                    printf("strum down");
                } else if (!((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_DOWN) == CWIID_GUITAR_BTN_DOWN) && state.strummer == STRUMMER_STATE_DOWN) {
                    state.strummer = STRUMMER_STATE_MID;
                    state.action.strummer = STRUMMER_ACTION_DOWN_MID;
                    timer_gettime(countdown_id, &time_left);
                    if (time_left.it_value.tv_sec > 0 || time_left.it_value.tv_nsec > 0) {
                        state.strummer = STRUMMER_STATE_SUSTAINED;
                    }
                    printf("strum neutral");
                } else if (((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_UP) == CWIID_GUITAR_BTN_UP) && state.strummer != STRUMMER_STATE_UP) {
                    state.strummer = STRUMMER_STATE_UP;
                    state.action.strummer = STRUMMER_ACTION_MID_UP;
                    printf("strum up");
                } else if (!((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_UP) == CWIID_GUITAR_BTN_UP) && state.strummer == STRUMMER_STATE_UP) {
                    state.strummer = STRUMMER_STATE_MID;
                    state.action.strummer = STRUMMER_ACTION_UP_MID;
                    printf("strum neutral");
                } else {
                    state.action.strummer = STRUMMER_ACTION_NONE;
                }

                // set chord state
                chord = 0;
                if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_GREEN) == CWIID_GUITAR_BTN_GREEN) {
                    chord |= GREEN;
                }
                if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_RED) == CWIID_GUITAR_BTN_RED) {
                    chord |= RED;
                }
                if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_YELLOW) == CWIID_GUITAR_BTN_YELLOW) {
                    chord |= YELLOW;
                }
                if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_BLUE) == CWIID_GUITAR_BTN_BLUE) {
                    chord |= BLUE;
                }
                if ((mesg[i].guitar_mesg.buttons & CWIID_GUITAR_BTN_ORANGE) == CWIID_GUITAR_BTN_ORANGE) {
                    chord |= ORANGE;
                }
                if (chord != state.chord) {
                    state.action.neck = NECK_ACTION_CHANGE;
                    state.chord = chord;
                }

                //set whammy state and action
                if (mesg[i].guitar_mesg.whammy != state.whammy) {
                    if (mesg[i].guitar_mesg.whammy > state.whammy) {
                        state.action.whammy = WHAMMY_ACTION_DOWN;
                    } else {
                        state.action.whammy = WHAMMY_ACTION_UP;
                    }

                    state.whammy = mesg[i].guitar_mesg.whammy;
                }
                //set touch bar state and action
                unsigned char new_touchbar_state = mesg[i].guitar_mesg.touch_bar;
                if (new_touchbar_state != state.touchbar) {
                    if (state.touchbar == TOUCHBAR_UNTOUCHED) { // initial touchbar state
                        state.action.touchbar = TOUCHBAR_ACTION_TAP;
                    } 
                    else if (new_touchbar_state == TOUCHBAR_STATE_NONE) {
                        state.action.touchbar = TOUCHBAR_ACTION_RELEASE;
                    }
                    else if (new_touchbar_state < state.touchbar) {
                        if (new_touchbar_state >= state.touchbar - TOUCHBAR_SLIDE_MARGIN) {
                            state.action.touchbar = TOUCHBAR_ACTION_SLIDE_DOWN;
                        } else {
                            state.action.touchbar = TOUCHBAR_ACTION_PULLOFF;
                        }
                    } 
                    else {
                        if (new_touchbar_state <= state.touchbar + TOUCHBAR_SLIDE_MARGIN) {
                            state.action.touchbar = TOUCHBAR_ACTION_SLIDE_UP;
                        } else {
                            state.action.touchbar = TOUCHBAR_ACTION_HAMMERON;
                        }
                    }
                    state.touchbar = new_touchbar_state;
                }

                // set stick state and action
                unsigned char new_stick_state[2];
                new_stick_state[CWIID_X] = mesg[i].guitar_mesg.stick[CWIID_X];
                new_stick_state[CWIID_Y] = mesg[i].guitar_mesg.stick[CWIID_Y];
                if (new_stick_state[CWIID_X] != state.stick.position[CWIID_X] || new_stick_state[CWIID_X] != state.stick.position[CWIID_X]) {
                    int x_diff = abs(new_stick_state[CWIID_X] - CWIID_GUITAR_STICK_MID);
                    int y_diff = abs(new_stick_state[CWIID_Y] - CWIID_GUITAR_STICK_MID);
                    int distance_from_center = DISTANCE(x_diff, y_diff);

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

                    state.stick.position[CWIID_X] = new_stick_state[CWIID_X];
                    state.stick.position[CWIID_Y] = new_stick_state[CWIID_Y];

                    if (new_stick_zone != state.stick.zone) {
                        if (new_stick_zone == state.stick.zone + 1 || (new_stick_zone == STICK_ZONE_1ST && state.stick.zone == STICK_ZONE_8TH)) {
                            state.stick.rotation_cw_counter = 0;
                            state.stick.rotation_ccw_counter++;
                        } else if (new_stick_zone == state.stick.zone - 1 || (new_stick_zone == STICK_ZONE_8TH && state.stick.zone == STICK_ZONE_1ST)) {
                            state.stick.rotation_cw_counter++;
                            state.stick.rotation_ccw_counter = 0;
                        } else {
                            state.stick.rotation_cw_counter = 0;
                            state.stick.rotation_ccw_counter = 0;
                        }

                        if (state.stick.rotation_cw_counter >= STICK_ROTATION_ZONE_THRESHOLD) {
                            state.action.stick = STICK_ACTION_ROTATE_CLOCKWISE;
                        } else if (state.stick.rotation_ccw_counter >= STICK_ROTATION_ZONE_THRESHOLD) {
                            state.action.stick = STICK_ACTION_ROTATE_COUNTER_CLOCKWISE;
                        }
                        state.stick.average_value = (state.stick.acc.count == 0) ? 0 : (state.stick.acc.value / state.stick.acc.count);
                        state.stick.zone = new_stick_zone;
                        state.stick.acc.count = 0;
                        state.stick.acc.value = 0;
                    }
                    state.stick.acc.count++;
                    state.stick.acc.value += distance_from_center;
            
                }
                printf("\n");
                break;
            case CWIID_MESG_TURNTABLES:
                printf("decks message:\n");
                printf("stick x: %x,\ty: %x\n", mesg[i].turntables_mesg.stick[CWIID_X], mesg[i].turntables_mesg.stick[CWIID_Y]);
                printf("left: %d,\tright: %d\n", mesg[i].turntables_mesg.left_turntable, mesg[i].turntables_mesg.right_turntable);
                printf("X-fader: %d, effect: %d\n", mesg[i].turntables_mesg.crossfader, mesg[i].turntables_mesg.effect_dial);
                if (state.current_turntables_state.crossfader != mesg[i].turntables_mesg.crossfader) {
                    if (state.current_turntables_state.crossfader < mesg[i].turntables_mesg.crossfader) {
                        state.action.crossfader = CROSSFADER_ACTION_FADE_LEFT;
                    } else {
                        state.action.crossfader = CROSSFADER_ACTION_FADE_RIGHT;
                    }
                    state.current_turntables_state.crossfader = mesg[i].turntables_mesg.crossfader;
                }
                if (state.action.effect_dial == EFFECT_DIAL_ACTION_INITIALIZE) {
                    state.current_turntables_state.effect_dial = mesg[i].turntables_mesg.effect_dial;
                }
                if (state.current_turntables_state.effect_dial != mesg[i].turntables_mesg.effect_dial) {
                
                    if (mesg[i].turntables_mesg.effect_dial > state.current_turntables_state.effect_dial) {
                        if (mesg[i].turntables_mesg.effect_dial > state.current_turntables_state.effect_dial + (CWIID_TURNTABLES_EFFECT_DIAL_MAX / 2)) {
                            // effect dial has passed 0 (equals 32 modulo 32...)
                            state.effect_dial.change = mesg[i].turntables_mesg.effect_dial - state.current_turntables_state.effect_dial - CWIID_TURNTABLES_EFFECT_DIAL_MAX - 1;
                            state.action.effect_dial = EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE;
                        } else {
                            state.effect_dial.change = mesg[i].turntables_mesg.effect_dial - state.current_turntables_state.effect_dial;
                            state.action.effect_dial = EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE;
                        }
                    } else {
                        if (mesg[i].turntables_mesg.effect_dial < state.current_turntables_state.effect_dial - (CWIID_TURNTABLES_EFFECT_DIAL_MAX / 2)) {
                            // effect dial has passed max value (equals 0 modulo max)
                            state.effect_dial.change = mesg[i].turntables_mesg.effect_dial + (CWIID_TURNTABLES_EFFECT_DIAL_MAX + 1 - state.current_turntables_state.effect_dial);
                            state.action.effect_dial = EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE;
                        } else {
                            state.effect_dial.change = mesg[i].turntables_mesg.effect_dial - state.current_turntables_state.effect_dial;
                            state.action.effect_dial = EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE;
                        }
                    }
                    state.current_turntables_state.effect_dial = mesg[i].turntables_mesg.effect_dial;
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

void mute (void *port_buf, int i) {
    unsigned char* buffer;
    int use_midi_channel = DEFAULT_MIDI_CHANNEL;
    int note_midi_channel;

    int j;
    for(j = 0; j < MAX_DELAYED_NOTES_COUNT; j++){
        if (state.delayed_notes[j].note.velocity != 0) {
            timer_delete(state.delayed_notes[j].timer);
        }
        state.delayed_notes[j].note.velocity = 0;
    }
    state.queued_notes.size = 0;

    for (j = 0; j < STRINGS_COUNT; j++) {
        if (state.string[j].velocity != 0) {
            note_off(state.string[j], port_buf, i);
            state.string[j].velocity = 0;
        }
    }


    int sustained_notes_count = 0;
    while(state.active_notes.size > sustained_notes_count) {
        note_off(state.active_notes.note[state.active_notes.size - 1], port_buf, i);
        state.active_notes.size--;
    }

    state.active_chord = &empty_chord;
}





void mute_string_notes (void *port_buf, int i) {
	unsigned char* buffer;
	int j;
	for (j = 0; j < MAX_SUSTAIN_STRINGS_COUNT; j++) {
		if (state.sustain_string[j].note_number) {
            note_off(state.sustain_string[j], port_buf, i);
			state.sustain_string[i].velocity = 0;
		}
	}
}

void initPatch (void *port_buf, jack_nframes_t i) {
    int j;
    sendMidiConfiguration(patch.midi, DEFAULT_MIDI_CHANNEL, port_buf, i);
    state.selected_bank = 0;

    sendStaticMessages(patch.midi_static_messages, DEFAULT_MIDI_CHANNEL, port_buf, i);

    if (bank[state.selected_bank].selectable) {
        sendMidiConfiguration(bank[state.selected_bank].midi, DEFAULT_MIDI_CHANNEL, port_buf, i);
        //sendStaticMessages(bank[state.selected_bank].midi_static_messages, DEFAULT_MIDI_CHANNEL, port_buf, i);
    }
}

void freePatchMemory ();

void nextPatch (void *port_buf, jack_nframes_t i) {
    state.system_pause = 1;
    if (state.current_patch < *state.argc - 1) {
        freePatchMemory();
        state.current_patch++;
        if (!readPatchFromFile(state.argv[state.current_patch])) {
            initPatch(port_buf, i);
        } else {
            freePatchMemory();
            state.current_patch--;
            readPatchFromFile(state.argv[state.current_patch]);
            initPatch(port_buf, i);
        }
    }
    state.system_pause = 0;
}

void previousPatch (void *port_buf, jack_nframes_t i) {
    state.system_pause = 1;
    if (state.current_patch > 1) {
        freePatchMemory();
        state.current_patch--;
        if (!readPatchFromFile(state.argv[state.current_patch])) {
            initPatch(port_buf, i);
        } else {
            freePatchMemory();
            state.current_patch++;
            readPatchFromFile(state.argv[state.current_patch]);
            initPatch(port_buf, i);
        }
    }
    state.system_pause = 0;
}


void processStickAction (enum stick_action_t *action, void *port_buf, jack_nframes_t nframes) {
    unsigned char *buffer;
    int use_midi_channel = DEFAULT_MIDI_CHANNEL;
    unsigned int stick_value = state.stick.target[state.stick.current_target].last_sent_value;

    if (*action == STICK_ACTION_ROTATE_COUNTER_CLOCKWISE) {
        stick_value += state.stick.average_value / 2 ;
        if (stick_value > stick_range.max) {
            stick_value = stick_range.max; 
        }
    } else if (*action == STICK_ACTION_ROTATE_CLOCKWISE) {
        if (stick_value >= state.stick.average_value / 2) {
            stick_value -= state.stick.average_value / 2;
        } else {
            stick_value = 0;
        }
    }

    if (stick_value != state.stick.target[state.stick.current_target].last_sent_value) {
        sendScaledMessages(patch.stick_target[state.stick.current_target].number_of_messages, patch.stick_target[state.stick.current_target].messages, stick_range, stick_value, DEFAULT_MIDI_CHANNEL, port_buf, nframes);
        state.stick.target[state.stick.current_target].last_sent_value = stick_value;
    }
    *action = STICK_ACTION_NONE;
    printf("\n");

}

void setPreviousStickTarget () {
    if (state.stick.current_target > 0) {
        state.stick.current_target--;
    }
}

void setNextStickTarget () {
    if (state.stick.current_target < patch.number_of_stick_targets - 1) {
        state.stick.current_target++;
    }
}

int process(jack_nframes_t nframes, void *arg) {
    int i,j;
    void* port_buf = jack_port_get_buffer(output_port, nframes);
    unsigned char* buffer;
    jack_midi_clear_buffer(port_buf);
    int use_midi_channel = DEFAULT_MIDI_CHANNEL;
    unsigned char strummer_direction;
    struct chord_t *chord;
    char chord_found = 0;
    struct sequence_t *sequence;
    char sequence_found = 0;

    if (state.system_pause != 0) {
        printf("System paused\n");
        return 1;
    }

    for (i = 0; i < nframes; i++) {
        while(state.queued_notes.size > 0) {
            note_on(state.queued_notes.note[state.queued_notes.size - 1], state.transpose, DEFAULT_MIDI_CHANNEL, port_buf, i);
            state.queued_notes.size--;
        }


        if (state.action.system != SYSTEM_ACTION_NONE) {

            switch (state.action.system) {
                case SYSTEM_ACTION_PATCH_INIT:
                    initPatch(port_buf, i);
                    break;
            }

            state.action.system = SYSTEM_ACTION_NONE;
        }


        if (state.action.neck != NECK_ACTION_NONE) {
            switch (state.action.neck) {
                case NECK_ACTION_CHANGE:

                    if (state.active_chord != &empty_chord && (*state.active_chord).variation_count != 0) {
                        if ((*state.active_chord).frets == state.chord) {
                            strum_chord_stringed_notes_only((*state.active_chord), DEFAULT_MIDI_CHANNEL, port_buf, i);
                        } else {
                            for (int v = 0; v < (*state.active_chord).variation_count; v++) {
                                if ((*state.active_chord).variation[v].frets == state.chord) {
                                    strum_chord((*state.active_chord).variation[v], BOTH, DEFAULT_MIDI_CHANNEL, port_buf, i);
                                    break;
                                }
                            }
                        }
                    }
                    break;
            }
            state.action.neck = NECK_ACTION_NONE;
        }


        if (state.action.strummer != STRUMMER_ACTION_NONE) {
            if (state.action.strummer == STRUMMER_ACTION_MID_DOWN || state.action.strummer == STRUMMER_ACTION_MID_UP) {
            	strummer_direction = (state.action.strummer == STRUMMER_ACTION_MID_DOWN) ? DOWN : UP;

                for (int i = 0; i < bank[state.selected_bank].chord_count; i++) {
                    if (bank[state.selected_bank].chord[i].frets == state.chord) {
                        chord = &(bank[state.selected_bank].chord[i]);
                        chord_found = 1;
                        break;
                    }
                }

            	if(chord_found) {
            		if (state.chord != state.previous_strummed_chord) {
            			mute_string_notes(port_buf, i);
            		}
	                strum_chord(*chord, strummer_direction, DEFAULT_MIDI_CHANNEL, port_buf, i);
                    state.active_chord = chord;
	            } else {
                    for (int i = 0; i < bank[state.selected_bank].sequence_count; i++) {
                        if (bank[state.selected_bank].sequence[i].frets == state.chord) {
                            sequence = &(bank[state.selected_bank].sequence[i]);
                            sequence_found = 1;
                            break;
                        }
                    }

                    if (sequence_found) {
    	            	if ((*sequence).shared_counter != MIDI_DATA_NULL) {
    		            	if (state.chord != state.previous_strummed_chord) {
    		            		if ((*sequence).reset_shared_counter) {
    		            			bank[state.selected_bank].counter[(*sequence).shared_counter].position = 0;
    		            		}
    		            	} 
    		            	strum_chord((*sequence).step[(bank[state.selected_bank].counter[(*sequence).shared_counter].position++) % (*sequence).length], strummer_direction, DEFAULT_MIDI_CHANNEL, port_buf, i);
    		            	if (bank[state.selected_bank].counter[(*sequence).shared_counter].position == bank[state.selected_bank].counter[(*sequence).shared_counter].length) {
    		            		bank[state.selected_bank].counter[(*sequence).shared_counter].position = bank[state.selected_bank].counter[(*sequence).shared_counter].reset_to;
    		            	}
    		            } else {
    		            	if (state.chord != state.previous_strummed_chord) {
    		            		if (!(*sequence).keep_position) {
    		            			(*sequence).position = 0;
    		            		}
    		            	}
    		            	strum_chord((*sequence).step[(*sequence).position++], strummer_direction, DEFAULT_MIDI_CHANNEL, port_buf, i);
    		            	if ((*sequence).position == (*sequence).length) {
    		            		(*sequence).position = (*sequence).reset_to;
    		            	}
    		            }
                    }
	            }

                state.action.strummer = STRUMMER_ACTION_NONE;
				state.previous_strummed_chord = state.chord;
            } else {
                if (state.strummer != STRUMMER_STATE_SUSTAINED) {
                    mute(port_buf, i);
                    state.action.strummer = STRUMMER_ACTION_NONE;
                }
            }
        }


        if (state.action.whammy != WHAMMY_ACTION_NONE) {
        	if (bank[state.selected_bank].whammy_length > 0) {
        		sendScaledMessages(bank[state.selected_bank].whammy_length, bank[state.selected_bank].whammy, whammy_range, state.whammy, DEFAULT_MIDI_CHANNEL, port_buf, i);
        	} else if (patch.whammy_length > 0) {
        		sendScaledMessages(patch.whammy_length, patch.whammy, whammy_range, state.whammy, DEFAULT_MIDI_CHANNEL, port_buf, i);
        	}
            state.action.whammy = WHAMMY_ACTION_NONE;
        }


        if (state.action.touchbar != TOUCHBAR_ACTION_NONE) {
            switch (state.action.touchbar) {
                case TOUCHBAR_ACTION_RELEASE:
                    printf("touchbar released\n");
                    if (bank[state.selected_bank].touchbar_length > 0) {
                        sendDefaultScaledMessages(bank[state.selected_bank].touchbar_length, bank[state.selected_bank].touchbar, DEFAULT_MIDI_CHANNEL, port_buf, i);
                    } else if (patch.touchbar_length > 0) {
                        sendDefaultScaledMessages(patch.touchbar_length, patch.touchbar, DEFAULT_MIDI_CHANNEL, port_buf, i);
                    }
                    break;
                default:
                    if (bank[state.selected_bank].touchbar_length > 0) {
                        sendScaledMessages(bank[state.selected_bank].touchbar_length, bank[state.selected_bank].touchbar, touchbar_range, state.touchbar, DEFAULT_MIDI_CHANNEL, port_buf, i);
                    } else if (patch.touchbar_length > 0) {
                        sendScaledMessages(patch.touchbar_length, patch.touchbar, touchbar_range, state.touchbar, DEFAULT_MIDI_CHANNEL, port_buf, i);
                    }
                    break;
            }
            state.action.touchbar = TOUCHBAR_ACTION_NONE;
        }


        if (state.action.stick != STICK_ACTION_NONE) {
            processStickAction(&state.action.stick, port_buf, i);
        }

        while (state.action.drums != 0) {
            uint8_t send_note_off = 0;
            buffer = jack_midi_event_reserve(port_buf, i, 3);
            buffer[2] = 0x7F;        /* velocity */
            if ((state.action.drums & RED) == RED) {
/*                if ((state.drums & RED) == RED) {
                    send_note_off = 1;
                }
                state.drums ^= RED;
*/
                buffer[1] = 38;    /* note number */
                state.action.drums &= ~RED;
            } else if ((state.action.drums & YELLOW) == YELLOW) {
/*                if ((state.drums & YELLOW) == YELLOW) {
                    send_note_off = 1;
                }
                state.drums ^= YELLOW;
*/
                buffer[1] = 42;    /* note number */
                state.action.drums &= ~YELLOW;
            } else if ((state.action.drums & BLUE) == BLUE) {
/*                if ((state.drums & BLUE) == BLUE) {
                    send_note_off = 1;
                }
                state.drums ^= BLUE;
*/
                buffer[1] = 48;    /* note number */
                state.action.drums &= ~BLUE;
            } else if ((state.action.drums & ORANGE) == ORANGE) {
/*                if ((state.drums & ORANGE) == ORANGE) {
                    send_note_off = 1;
                }
                state.drums ^= ORANGE;
*/
                buffer[1] = 51;    /* note number */
                state.action.drums &= ~ORANGE;
            } else if ((state.action.drums & GREEN) == GREEN) {
/*                if ((state.drums & GREEN) == GREEN) {
                    send_note_off = 1;
                }
                state.drums ^= GREEN;
*/
                buffer[1] = 45;    /* note number */
                state.action.drums &= ~GREEN;
            } else if ((state.action.drums & PEDAL) == PEDAL) {
/*                if ((state.drums & PEDAL) == PEDAL) {
                    send_note_off = 1;
                }
                state.drums ^= PEDAL;
*/
                buffer[1] = 36;    /* note number */
                state.action.drums &= ~PEDAL;
            }
            
            if (send_note_off) {
                buffer[0] = MIDI_NOTE_OFF + 9;    // note off
            } else {
                buffer[0] = MIDI_NOTE_ON + 9;    // note on
            }
        }
        
        if (state.action.crossfader != CROSSFADER_ACTION_NONE) {
            buffer = jack_midi_event_reserve(port_buf, i, 3);
            uint8_t crossfader_midi_value = state.current_turntables_state.crossfader * 127 / CWIID_TURNTABLES_CROSSFADER_MAX;
            buffer[2] = crossfader_midi_value;
            buffer[1] = MIDI_CC_BALANCE_MSB;
            buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;    // control change
            state.action.crossfader = CROSSFADER_ACTION_NONE;
        }
        if (state.action.effect_dial != EFFECT_DIAL_ACTION_NONE) {
            buffer = jack_midi_event_reserve(port_buf, i, 3);
            buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;    // control change
            buffer[1] = MIDI_CC_EFFECT_CTL_1_MSB;
            int16_t signed_value;
            
            switch (state.action.effect_dial) {
            case EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE:
                state.effect_dial.value += state.effect_dial.change;
                if (state.effect_dial.value > state.effect_dial.max_value) {
                    state.effect_dial.value = state.effect_dial.max_value;
                }
                break;
            case EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE:
                signed_value = state.effect_dial.value + state.effect_dial.change;
                state.effect_dial.value += state.effect_dial.change;
                if (signed_value < state.effect_dial.min_value) {
                    state.effect_dial.value = state.effect_dial.min_value;
                }
                break;
            case EFFECT_DIAL_ACTION_INITIALIZE:
                printf("initialize\n");
                state.current_turntables_state.effect_dial = state.effect_dial.initial_value;
                break;
            }
            buffer[2] = state.effect_dial.value;
            state.action.effect_dial = EFFECT_DIAL_ACTION_NONE;
        }


        if (state.action.buttons != BUTTONS_ACTION_NONE) {
            switch (state.action.buttons) {
                case BUTTONS_ACTION_BANK_CHANGE:
                    selectBank(state.action.buttons_data, port_buf, i);
                    break;
                case BUTTONS_ACTION_NEXT_PATCH:
                    nextPatch(port_buf, i);
                    break;
                case BUTTONS_ACTION_PREVIOUS_PATCH:
                    previousPatch(port_buf, i);
                    break;
                case BUTTONS_ACTION_NEXT_STICK_TARGET:
                    setNextStickTarget();
                    break;
                case BUTTONS_ACTION_PREVIOUS_STICK_TARGET:
                    setPreviousStickTarget();
                    break;
            }
            state.action.buttons = BUTTONS_ACTION_NONE;
            state.action.buttons_data = 0;
        }

    }
    return 0;
}

void set_rpt_mode(cwiid_wiimote_t *wiimote, uint16_t rpt_mode) {
    if (cwiid_set_rpt_mode(wiimote, rpt_mode)) {
        fprintf(stderr, "Error setting report mode\n");
    }
}




void freeStateMemory () {
/*
    free(state.delayed_notes);
    free(state.string);
    free(state.sustain_string);
    free(state.queued_notes.note);
    free(state.active_notes.note);
*/
}


void freeCounterMemory (struct counter_t* counter) {}


void freeCountersMemory (int *number_of_counters, struct counter_t** counters) {
    if (*number_of_counters > 0) {
        for (int i = *number_of_counters; i >= 0; i--) {
            freeCounterMemory(&(*counters)[i]);
        }
    }
    free(*counters);
}

void freeCCMessageMemory(struct cc_message_t *cc) {}

void freeRtcMessageMemory(struct clock_message_t *msg) {}

void freeMidiStaticMessagesMemory (struct midi_static_messages_t *messages) {
    int i;
    for (i = 0; i < (*messages).length; i += 1) {
         switch ((*messages).type[i]) {
            case MIDI_MESSAGE_TYPE_CC:
                freeCCMessageMemory(&(*messages).message[i].cc);
                break;
            case MIDI_MESSAGE_TYPE_RTC:
                freeRtcMessageMemory(&(*messages).message[i].clock);
                break;
         }
    }
    (*messages).length = 0;
    free((*messages).message);
    free((*messages).type);
}

void freeCCMemory (int *number_of_messages, struct cc_message_t **cc) {
    if (number_of_messages > 0) {
        for (int i = *number_of_messages - 1; i <= 0; i--) {
            freeCCMessageMemory(&(*cc)[i]);

        }
    }
    free(*cc);
}

void freeScaledMessage (struct scaled_message_t *sm) {}

void freeScaledMessages (unsigned int *number_of_messages, struct scaled_message_t** messages) {
    if (*number_of_messages > 0) {
        for (int i = *number_of_messages - 1; i >= 0; i--) {
            freeScaledMessage(&(*messages)[i]);
        }
    }
    free(*messages);
}

void freeTouchbarMemory (unsigned int *number_of_messages, struct scaled_message_t** messages) {
    freeScaledMessages(number_of_messages, messages);
}

void freeWhammyMemory (unsigned int *number_of_messages, struct scaled_message_t** messages) {
    freeScaledMessages(number_of_messages, messages);
}

void freeChordsMemory (unsigned char *count, struct chord_t **chords);

void freeChordMemory (struct chord_t *chord) {
    if ((*chord).variation_count > 0) {
        freeChordsMemory(&(*chord).variation_count, &(*chord).variation);
    }
    free((*chord).note);
}

void freeChordsMemory (unsigned char *count, struct chord_t **chords) {
    if (*count > 0) {
        for (int i = *count - 1; i >= 0; i--) {
            freeChordMemory(&(*chords)[i]);
        }
    }
    free(*chords);
}



void freeSequenceMemory (struct sequence_t *sequence) {
    for (int i = (*sequence).length - 1; i >= 0; i--) {
        freeChordMemory(&(*sequence).step[i]);
    }
    free((*sequence).step);
}



void freeSequencesMemory (int count, struct sequence_t **sequences) {
    for (int i = count - 1; i >= 0; i--) {
        freeSequenceMemory(&(*sequences)[i]);
    }
    free(*sequences);
}



void freeProgramChangeMemory (struct midi_program_change_t *pc) {}

void freeMidiMemory (struct midi_configuration_t *midiConfiguration) {
    if ((*midiConfiguration).program_change_count > 0) {
        for (int i = (*midiConfiguration).program_change_count - 1; i >= 0; i--) {
            freeProgramChangeMemory(&((*midiConfiguration).program_change[i]));
        }
    }
    free((*midiConfiguration).program_change);
}



void freeBankMemory (struct bank_t *bank) {
    if ((*bank).selectable) {
        if ((*bank).sequence_count > 0) {
            freeSequencesMemory(bank->sequence_count, &(bank->sequence));
        }
        if ((*bank).chord_count > 0) {
            freeChordsMemory(&(*bank).chord_count, &(*bank).chord);
        }
        if ((*bank).touchbar_length > 0) {
            freeTouchbarMemory(&(*bank).touchbar_length, &(*bank).touchbar);
        }
        if ((*bank).whammy_length > 0) {
            freeWhammyMemory(&(*bank).whammy_length, &(*bank).whammy);
        }
        if ((*bank).number_of_counters > 0) {
            freeCountersMemory(&(*bank).number_of_counters, &(*bank).counter);
        }
        freeMidiStaticMessagesMemory((*bank).midi_static_messages);
        free((*bank).midi_static_messages);
        freeMidiMemory(&(*bank).midi);
    }
}



void freeBanksMemory (int *count, struct bank_t **bank) {
    if (*count > 0) {
        for (int i = *count; i > 0; i--) {
            freeBankMemory(&(*bank)[i]);
        }
    }
    free(*bank);
}

void freeStickTargetsMemory (int *count, struct stick_target_t **target) {
    for (int i = *count - 1; i >= 0; i--) {
        freeScaledMessages(&(*target)[i].number_of_messages, &(*target)[i].messages);
    }
    free(*target);    
}


void freePatchMemory () {
    if (patch.number_of_banks > 0) {
        freeBanksMemory(&patch.number_of_banks, &bank);
    }
    if (patch.touchbar_length > 0) {
        freeTouchbarMemory(&patch.touchbar_length, &patch.touchbar);
    }

    if (patch.whammy_length > 0) {
        freeWhammyMemory(&patch.whammy_length, &patch.whammy);
    }
    freeMidiStaticMessagesMemory(patch.midi_static_messages);
    free(patch.midi_static_messages);
    freeMidiMemory(&patch.midi);
}


int openMidiPorts () {
    jack_nframes_t nframes;
    const char **ports;

    if((client = jack_client_open("LouWiiGui", JackNullOption, NULL)) == 0) {
        fprintf (stderr, "jack server not running?\n");
        return 1;
    }
    jack_set_process_callback(client, process, 0);
    output_port = jack_port_register(client, "Jack MIDI out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
    //    input_port = jack_port_register(client, "Jack MIDI in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    nframes = jack_get_buffer_size(client);


    if (jack_activate(client)) {
        fprintf (stderr, "cannot activate client");
        return 1;
    }

    printf("LouWiiGui says: Jack buffer size == %d\n", nframes);

    if ((ports = jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput)) == NULL) {
        fprintf(stderr, "Cannot find any physical playback ports\n");
    } else {
        if (jack_connect(client, jack_port_name(output_port), ports[0])) {
            fprintf (stderr, "cannot connect output ports\n");
        }
    }
    free(ports);

    return 0;
}

struct sigevent sigev;

void init () {
    int i;
    empty_chord.size = 0;
    empty_chord.touchbar_length = 0;
    empty_chord.variation_count = 0;

    state.system_pause = 0;
    state.active_chord = &empty_chord;
    state.action.buttons = BUTTONS_ACTION_NONE;
    state.action.buttons_data = 0;
    state.action.drums = 0;
    state.action.effect_dial = EFFECT_DIAL_ACTION_INITIALIZE;
    state.action.stick = STICK_ACTION_NONE;
    state.action.strummer = STRUMMER_ACTION_NONE;
    state.action.system = SYSTEM_ACTION_NONE;
    state.action.whammy = WHAMMY_ACTION_NONE;
    state.active_notes.size = 0;
    state.buttons_previous = 0;
    state.chord = 0;
    state.current_patch = 1;
    state.drums = 0;
    state.drums_buttons_previous = 0;
    state.effect_dial.max_value = 127;
    state.effect_dial.min_value = 0;
    state.effect_dial.initial_value = 63;
    state.previous_strummed_chord = 0xFFFF;
    state.queued_notes.size = 0;
    state.stick.acc.count = 0;
    state.stick.acc.value = 0;
    state.stick.average_value = 0;
    state.stick.current_target = 0;
    state.stick.position[CWIID_X] = 0;
    state.stick.position[CWIID_Y] = 0;
    state.stick.rotation_ccw_counter = 0;
    state.stick.rotation_cw_counter = 0;
    state.stick.zone = STICK_ZONE_UNKNOWN;
    state.strummer = STRUMMER_STATE_UNKNOWN;
    state.transpose = 0;
    state.whammy = WHAMMY_STATE_UNKNOWN;
    state.active_notes.note = malloc(MAX_ACTIVE_NOTES_COUNT * sizeof(struct note_t));
    state.queued_notes.note = malloc(MAX_QUEUED_NOTES_COUNT * sizeof(struct note_t));
    state.sustain_string = malloc(MAX_SUSTAIN_STRINGS_COUNT * sizeof(struct note_t));
    state.string = malloc(STRINGS_COUNT * sizeof(struct note_t));
    state.delayed_notes = malloc(MAX_DELAYED_NOTES_COUNT * sizeof(struct delayed_note_t));
    for(i = 0; i < MAX_DELAYED_NOTES_COUNT; i++){
        state.delayed_notes[i].note.velocity = 0;
    }
    for(i = 0; i < STRINGS_COUNT; i++){
        state.string[i].velocity = 0;
    }


    bank = malloc(MAX_BANKS_COUNT * sizeof(struct bank_t));

    whammy_range.min = 0;
    whammy_range.max = CWIID_GUITAR_WHAMMY_MAX;
    touchbar_range.min = CWIID_GUITAR_TOUCHBAR_VALUE_1ST;
    touchbar_range.max = CWIID_GUITAR_TOUCHBAR_VALUE_5TH;
    stick_range.min = 0;
    stick_range.max = 256;

    margin.it_value.tv_sec = 0;
    margin.it_value.tv_nsec = 50000000;
    margin.it_interval.tv_sec = 0;
    margin.it_interval.tv_nsec = 0;
    sigev.sigev_notify = SIGEV_NONE;
    timer_create(CLOCK_MONOTONIC, &sigev, &countdown_id);
    timer_settime(countdown_id, 0, &margin, NULL);
    timer_gettime(countdown_id, &time_left);   
}


void siginthandler (int param) {
    printf("User pressed Ctrl+C\n");
    state.system_pause = 1;
    mute(output_port, 0);
    closeMidiPorts();

    if (wiimote) {
        cwiid_disable(wiimote, CWIID_FLAG_MESG_IFC);
        cwiid_close(wiimote);
    }
    freePatchMemory();
    freeStateMemory();

    if (system("stty echo")) {
        fprintf(stderr, "Unable to re-enable local echo\n");
    }
    exit(1);
}

int main (int argc, char *argv[]) {
    init();
    state.argc = &argc;
    state.argv = argv;
    struct cwiid_state state;       /* wiimote state */
    bdaddr_t bdaddr = *BDADDR_ANY;  /* bluetooth device address */
    unsigned char mesg = 0;
    unsigned char rpt_mode = 0;

    // experiment with delayed notes goes here 
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = neio;
    sigaction(SIGUSR1, &sa, NULL);

    // end experiment


    printf("Started with %d arguments:\n", argc - 1);
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            printf("%d: %s\n", i, argv[i]);
        }
        readPatchFromFile(argv[1]);
    } else {
        printf("reading default patch file: testIn.xml\n");
        readPatchFromFile("testIn.xml");
    }
    writeCurrentPatchToFile("testOut.xml");
    cwiid_set_err(err);

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


    if (openMidiPorts() == 1) {
        return 1;
    }

    if (system("stty -echo")) {
        fprintf(stderr, "Unable to disable local echo\n");
    }
    signal(SIGINT, siginthandler);

    while (1) {
        sleep(1);
    }
    return 0;
}

