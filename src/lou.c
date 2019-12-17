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

#include <jack/jack.h>
#include <jack/midiport.h>

#include <bluetooth/bluetooth.h>
#include <cwiid.h>

#include <libxml2/libxml/encoding.h>
#include <libxml2/libxml/xmlwriter.h>

#include "lou.h"

#define LOUWIIGUI_CWIID_RPT_MODE (CWIID_RPT_EXT | CWIID_RPT_BTN) // | CWIID_RPT_ACC)

#define DISTANCE(a,b) sqrt(pow(a, 2) + pow(b, 2));
#define USE_MIDI_CHANNEL bank[state.selected_bank].midi.default_channel ? bank[state.selected_bank].midi.default_channel : patch.midi.default_channel

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

void sendProgramChange(struct midi_program_change_t pc, void *port_buf, jack_nframes_t time) {

    unsigned char* buffer;
    int use_midi_channel = pc.channel ? pc.channel : USE_MIDI_CHANNEL;
    if (pc.bank_msb != MIDI_DATA_NULL) {
        buffer = jack_midi_event_reserve(port_buf, time, 3);
        buffer[2] = pc.bank_msb;
        buffer[1] = MIDI_CC_BANK_SELECT_MSB;
        buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;
    }
    if (pc.bank_lsb != MIDI_DATA_NULL) {
        buffer = jack_midi_event_reserve(port_buf, time, 3);
        buffer[2] = pc.bank_lsb;
        buffer[1] = MIDI_CC_BANK_SELECT_LSB;
        buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;
    }
    if (pc.program != MIDI_DATA_NULL) {
        buffer = jack_midi_event_reserve(port_buf, time, 2);
        buffer[1] = pc.program;
        buffer[0] = MIDI_PROGRAM_CHANGE + use_midi_channel - 1;
    }

}

void sendCCMessage(struct cc_message_t cc, void *port_buf, jack_nframes_t time) {
	int use_midi_channel = (cc.channel != MIDI_DATA_NULL) ? cc.channel : USE_MIDI_CHANNEL;
    printf("sending CC\tparameter: %d\t,value: %d,\tchannel: %d\n", cc.parameter, cc.value, use_midi_channel);
	unsigned char *buffer = jack_midi_event_reserve(port_buf, time, 3);

	buffer[2] = cc.value;
	buffer[1] = cc.parameter;
	buffer[0] = MIDI_CONTROL_CHANGE + use_midi_channel - 1;
}

void sendMidiConfiguration (struct midi_configuration_t config, void* port_buf, jack_nframes_t time) {
    if (config.program_change_count > 0) {
        for (int i = 0; i < config.program_change_count; i++) {
            sendProgramChange(config.program_change[i], port_buf, time);
        }
    }
}

void selectBank (unsigned char bank_number, void *port_buf, jack_nframes_t time) {
    if (bank[bank_number].selectable && state.selected_bank != bank_number) {
        state.selected_bank = bank_number;

        if (bank[state.selected_bank].midi.program_change_count > 0) {
            sendMidiConfiguration(bank[state.selected_bank].midi, port_buf, time);
        } else {
            sendMidiConfiguration(patch.midi, port_buf, time);
        }

        if (bank[state.selected_bank].cc_length > 0) {
            printf("cc %d i bank %d\n", bank[state.selected_bank].cc_length, state.selected_bank);
        	for (int j = 0; j < bank[state.selected_bank].cc_length; j++) {
        		sendCCMessage(bank[state.selected_bank].cc[j], port_buf, time);
        	}
        }

        printf("selected bank %d: %s\n", state.selected_bank, bank[state.selected_bank].name);
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

void legato_note_on (struct note_t *new_note, struct note_t *old_note, void* port_buf, int i) {
    unsigned char* buffer;

    buffer = jack_midi_event_reserve(port_buf, i, 3);
    buffer[2] = MIDI_PEDAL_ON;
    buffer[1] = MIDI_CC_LEGATO_PEDAL;
    buffer[0] = MIDI_CONTROL_CHANGE + (*new_note).midi_channel - 1;

    buffer = jack_midi_event_reserve(port_buf, i, 3);
    buffer[2] = (*new_note).velocity;        /* velocity */
    buffer[1] = (*new_note).note_number;     /* note number */
    buffer[0] = MIDI_NOTE_ON + (*new_note).midi_channel - 1;

    if ((*old_note).velocity != 0) {
        buffer = jack_midi_event_reserve(port_buf, i, 3);
        buffer[2] = (*old_note).velocity; /* velocity */
        buffer[1] = (*old_note).note_number; /* previous note played on string */
        buffer[0] = MIDI_NOTE_OFF + (*old_note).midi_channel - 1;    /* note off */
    }
    (*old_note).velocity = 0;

    buffer = jack_midi_event_reserve(port_buf, i, 3);
    buffer[2] = MIDI_PEDAL_OFF;
    buffer[1] = MIDI_CC_LEGATO_PEDAL;
    buffer[0] = MIDI_CONTROL_CHANGE + (*new_note).midi_channel - 1;

}


void note_on (struct note_t note, void* port_buf, int i) {
    unsigned char* buffer;
    int use_midi_channel = note.midi_channel ? note.midi_channel : USE_MIDI_CHANNEL;
    struct note_t current_note;

    // copy the information neccessary to end the note as configured
    current_note.velocity = note.velocity;
    current_note.note_number = note.note_number + state.transpose;
    current_note.midi_channel = use_midi_channel;
    current_note.sustain_mode = note.sustain_mode;
    current_note.string = note.string;
    current_note.legato = note.legato;

    if (current_note.sustain_mode == SUSTAIN_STRING) {
    	if (state.sustain_string[current_note.string].velocity != 0) {
    		buffer = jack_midi_event_reserve(port_buf, i, 3);
		    buffer[2] = state.sustain_string[current_note.string].velocity;	/* velocity */
		    buffer[1] = state.sustain_string[current_note.string].note_number; /* previous note played on string */
		    buffer[0] = MIDI_NOTE_OFF + state.sustain_string[current_note.string].midi_channel - 1;    /* note off */
    	}

		buffer = jack_midi_event_reserve(port_buf, i, 3);
	    buffer[2] = current_note.velocity;        /* velocity */
	    buffer[1] = current_note.note_number;     /* note number */
	    buffer[0] = MIDI_NOTE_ON + use_midi_channel - 1;

    	state.sustain_string[current_note.string] = current_note;
    } else {
        if (current_note.string != 0) {
            if (current_note.legato == 1) {
                legato_note_on(&current_note, &(state.string[current_note.string]), port_buf, i);
            } else {
                if (state.string[current_note.string].velocity != 0) {
                    buffer = jack_midi_event_reserve(port_buf, i, 3);
                    buffer[2] = state.string[current_note.string].velocity; /* velocity */
                    buffer[1] = state.string[current_note.string].note_number; /* previous note played on string */
                    buffer[0] = MIDI_NOTE_OFF + state.string[current_note.string].midi_channel - 1;    /* note off */
                }

                buffer = jack_midi_event_reserve(port_buf, i, 3);
                buffer[2] = current_note.velocity;        /* velocity */
                buffer[1] = current_note.note_number;     /* note number */
                buffer[0] = MIDI_NOTE_ON + current_note.midi_channel - 1;
            }
            state.string[current_note.string] = current_note;
        } else {
    	    buffer = jack_midi_event_reserve(port_buf, i, 3);
    	    buffer[2] = current_note.velocity;        /* velocity */
    	    buffer[1] = current_note.note_number;    /* note number */
    	    buffer[0] = MIDI_NOTE_ON + current_note.midi_channel - 1;    /* note on */

    	    state.active_notes.note[state.active_notes.size] = current_note;
    	    state.active_notes.size++;
        }
	}
}

void strum_chord (struct chord_t chord, unsigned char direction, void* port_buf, int i) {
    int q,r = 0;
    for (q = 0; q < chord.size; q++) {
    	if (chord.note[q].direction == BOTH || chord.note[q].direction == direction) {
	        if (chord.note[q].delay == 0) {
	            note_on(chord.note[q], port_buf, i);
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

void strum_chord_stringed_notes_only (struct chord_t chord, void* port_buf, int i) {
    int q,r = 0;
    for (q = 0; q < chord.size; q++) {
        if (chord.note[q].string != 0 && state.string[chord.note[q].string].velocity != 0) {
            note_on(chord.note[q], port_buf, i);
        }
    }
}

void mute (void *port_buf, int i) {
    unsigned char* buffer;
    int use_midi_channel = USE_MIDI_CHANNEL;
    int note_midi_channel;

    jack_nframes_t use_time = (i != 0) ? ((jack_nframes_t) i) : jack_get_current_transport_frame(client);     

    int j;
    for(j=0; j < MAX_DELAYED_NOTES_COUNT; j++){
        if (state.delayed_notes[j].note.velocity != 0) {
            timer_delete(state.delayed_notes[j].timer);
        }
        state.delayed_notes[j].note.velocity = 0;
    }
    state.queued_notes.size = 0;

    for (j = 0; j < STRINGS_COUNT; j++) {
        if (state.string[j].velocity != 0) {
            buffer = jack_midi_event_reserve(port_buf, use_time, 3);
            buffer[2] = state.string[j].velocity;        /* velocity */
            buffer[1] = state.string[j].note_number;    /* note number */
            buffer[0] = MIDI_NOTE_OFF + state.string[j].midi_channel - 1;
            state.string[j].velocity = 0;
        }
    }

    int sustained_notes_count = 0;
    while(state.active_notes.size > sustained_notes_count) {
        note_midi_channel = state.active_notes.note[state.active_notes.size - 1].midi_channel;
        buffer = jack_midi_event_reserve(port_buf, use_time, 3);
        buffer[2] = state.active_notes.note[state.active_notes.size - 1].velocity;        /* velocity */
        buffer[1] = state.active_notes.note[state.active_notes.size - 1].note_number;    /* note number */
        buffer[0] = MIDI_NOTE_OFF + (note_midi_channel ? note_midi_channel : use_midi_channel) - 1;    /* note off, standard midi channel */
        state.active_notes.size--;
    }
    state.active_chord = &empty_chord;
}

void sendScaledMessages (int messages_length, struct scaled_message_t *messages, struct range_t input_limits, int input_value, void *port_buf, jack_nframes_t time) {
    unsigned char *buffer;
    int use_midi_channel = USE_MIDI_CHANNEL;
    int output_range;
    int shift;
    unsigned int output_value;
    int midi_channel;
    int input_min;
    int input_max;
    int input_range;

    for (int i = 0; i < messages_length; i++) {
        input_min = messages[i].in.min != MIDI_DATA_NULL ? messages[i].in.min : input_limits.min;
        input_max = messages[i].in.max != MIDI_DATA_NULL ? messages[i].in.max : input_limits.max;

        if (input_value >= input_min && input_value <= input_max) {

            input_range = input_max - input_min;
    		output_range = messages[i].out.max - messages[i].out.min;
            if (input_range != 0 && output_range != 0) {
    	       shift = (output_range * (input_value - input_min)) / input_range;
    	       output_value = messages[i].out.min + shift;
            } else if (output_range == 0) {
                output_value = messages[i].out.min;
            } else {
                output_value = messages[i].out.min + (output_range / 2);
            }
    	    midi_channel = messages[i].midi_channel == MIDI_DATA_NULL ? use_midi_channel : messages[i].midi_channel;


        	switch (messages[i].type) {
        		case SCALED_PITCH:
    		    	buffer = jack_midi_event_reserve(port_buf, time, 3);
    			    buffer[2] = (output_value & 0x3F80) >> 7;  // most significant bits
    			    buffer[1] = output_value & 0x007f;         // least significant bits
    			    buffer[0] = MIDI_PITCH_WHEEL + midi_channel - 1;    // pitch wheel change 
    		    	break;

    		    case SCALED_CC:
    		    	if (messages[i].cc_lsb && messages[i].cc_lsb != MIDI_DATA_NULL) {
    			    	buffer = jack_midi_event_reserve(port_buf, time, 3);
    				    buffer[2] = (output_value & 0x3F80) >> 7;  // most significant bits
    				    buffer[1] = messages[i].cc;         
    				    buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;   

    			    	buffer = jack_midi_event_reserve(port_buf, time, 3);
    				    buffer[2] = (output_value & 0x007F);  // least significant bits
    				    buffer[1] = messages[i].cc_lsb;         
    				    buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;   
    		    	} else {
    			    	buffer = jack_midi_event_reserve(port_buf, time, 3);
    				    buffer[2] = output_value;
    				    buffer[1] = messages[i].cc;         
    				    buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;
    		    	}
    		    	break;
        	}
        }
    }
}

void sendDefaultScaledMessages (int messages_length, struct scaled_message_t *messages, void *port_buf, jack_nframes_t time) {
    unsigned char *buffer;
    int use_midi_channel = USE_MIDI_CHANNEL;
    int midi_channel;

    for (int i = 0; i < messages_length; i++) {
        if (messages[i].default_value != MIDI_DATA_NULL) {
            midi_channel = messages[i].midi_channel == MIDI_DATA_NULL ? use_midi_channel : messages[i].midi_channel;

            switch (messages[i].type) {
                case SCALED_PITCH:
                    buffer = jack_midi_event_reserve(port_buf, time, 3);
                    buffer[2] = (messages[i].default_value & 0x3F80) >> 7;  // most significant bits
                    buffer[1] = messages[i].default_value & 0x007f;         // least significant bits
                    buffer[0] = MIDI_PITCH_WHEEL + midi_channel - 1;    // pitch wheel change 

                    break;

                case SCALED_CC:
                    if (messages[i].cc_lsb && messages[i].cc_lsb != MIDI_DATA_NULL) {
                        buffer = jack_midi_event_reserve(port_buf, time, 3);
                        buffer[2] = (messages[i].default_value & 0x3F80) >> 7;  // most significant bits
                        buffer[1] = messages[i].cc;         
                        buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;   

                        buffer = jack_midi_event_reserve(port_buf, time, 3);
                        buffer[2] = (messages[i].default_value & 0x007F);  // least significant bits
                        buffer[1] = messages[i].cc_lsb;         
                        buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;   
                    } else {
                        buffer = jack_midi_event_reserve(port_buf, time, 3);
                        buffer[2] = messages[i].default_value;
                        buffer[1] = messages[i].cc;         
                        buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;
                    }
                    break;
            }
        }
    }
}


void mute_string_notes (void *port_buf, int i) {
	unsigned char* buffer;
	int j;
	for (j = 0; j < MAX_SUSTAIN_STRINGS_COUNT; j++) {
		if (state.sustain_string[j].note_number) {
		    buffer = jack_midi_event_reserve(port_buf, i, 3);
		    buffer[2] = state.sustain_string[j].velocity;        /* velocity */
		    buffer[1] = state.sustain_string[j].note_number;    /* note number */
		    buffer[0] = MIDI_NOTE_OFF + state.sustain_string[j].midi_channel - 1;    /* note off */
			state.sustain_string[i].velocity = 0;
		}
	}
}

void initPatch (void *port_buf, jack_nframes_t i) {
    int j;
    printf("patch midi init\n");
    sendMidiConfiguration(patch.midi, port_buf, i);
    state.selected_bank = 0;

    if (patch.cc_length > 0) {
        for (j = 0; j < patch.cc_length; j++) {
            sendCCMessage(patch.cc[j], port_buf, i);
        }
    }

    if (bank[state.selected_bank].selectable) {
        printf("bank midi init\n");
        sendMidiConfiguration(bank[state.selected_bank].midi, port_buf, i);
        printf("bank-midiconfig ferdig\n");
        if (bank[state.selected_bank].cc_length > 0) {
            for (j = 0; j < bank[state.selected_bank].cc_length; j++) {
                sendCCMessage(bank[state.selected_bank].cc[j], port_buf, i);
            }
        }
    }
}

void freePatchMemory ();
int readPatchFromFile (const char* file);

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
    int use_midi_channel = USE_MIDI_CHANNEL;
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
        sendScaledMessages(patch.stick_target[state.stick.current_target].number_of_messages, patch.stick_target[state.stick.current_target].messages, stick_range, stick_value, port_buf, nframes);
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
    int use_midi_channel = USE_MIDI_CHANNEL;
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
            note_on(state.queued_notes.note[state.queued_notes.size - 1], port_buf, i);
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
                            strum_chord_stringed_notes_only((*state.active_chord), port_buf, i);
                        } else {
                            for (int v = 0; v < (*state.active_chord).variation_count; v++) {
                                if ((*state.active_chord).variation[v].frets == state.chord) {
                                    strum_chord((*state.active_chord).variation[v], BOTH, port_buf, i);
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
	                strum_chord(*chord, strummer_direction, port_buf, i);
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
    		            	strum_chord((*sequence).step[(bank[state.selected_bank].counter[(*sequence).shared_counter].position++) % (*sequence).length], strummer_direction, port_buf, i);
    		            	if (bank[state.selected_bank].counter[(*sequence).shared_counter].position == bank[state.selected_bank].counter[(*sequence).shared_counter].length) {
    		            		bank[state.selected_bank].counter[(*sequence).shared_counter].position = 0;
    		            	}
    		            } else {
    		            	if (state.chord != state.previous_strummed_chord) {
    		            		if (!(*sequence).keep_position) {
    		            			(*sequence).position = 0;
    		            		}
    		            	}
    		            	strum_chord((*sequence).step[(*sequence).position++], strummer_direction, port_buf, i);
    		            	if ((*sequence).position == (*sequence).length) {
    		            		(*sequence).position = 0;
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
        		sendScaledMessages(bank[state.selected_bank].whammy_length, bank[state.selected_bank].whammy, whammy_range, state.whammy, port_buf, i);
        	} else if (patch.whammy_length > 0) {
        		sendScaledMessages(patch.whammy_length, patch.whammy, whammy_range, state.whammy, port_buf, i);
        	}
            state.action.whammy = WHAMMY_ACTION_NONE;
        }


        if (state.action.touchbar != TOUCHBAR_ACTION_NONE) {
            switch (state.action.touchbar) {
                case TOUCHBAR_ACTION_RELEASE:
                    printf("touchbar released\n");
                    if (bank[state.selected_bank].touchbar_length > 0) {
                        sendDefaultScaledMessages(bank[state.selected_bank].touchbar_length, bank[state.selected_bank].touchbar, port_buf, i);
                    } else if (patch.touchbar_length > 0) {
                        sendDefaultScaledMessages(patch.touchbar_length, patch.touchbar, port_buf, i);
                    }
                    break;
                default:
                    if (bank[state.selected_bank].touchbar_length > 0) {
                        sendScaledMessages(bank[state.selected_bank].touchbar_length, bank[state.selected_bank].touchbar, touchbar_range, state.touchbar, port_buf, i);
                    } else if (patch.touchbar_length > 0) {
                        sendScaledMessages(patch.touchbar_length, patch.touchbar, touchbar_range, state.touchbar, port_buf, i);
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



/* XML FUNCTIONS: */


void writeCurrentPatchToFile(const char *file) {
    int rc, chord_index, note_index;
    xmlTextWriterPtr writer;
    xmlChar *tmp;
    xmlDocPtr doc;

    // Create a new XmlWriter for DOM, with no compression.
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
    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "midi_channel", "%d", patch.midi.default_channel);
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

	/*    
    for (chord_index = 0; chord_index < ALL_COLOR_COMBINATIONS; chord_index++) {
        if (0) {
            // Start an element named "chord" as child of patch. 
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
                // Start an element named "note" as child of chord. 
                rc = xmlTextWriterStartElement(writer, BAD_CAST "note");
                if (rc < 0) {
                    printf("writeCurrentPatchToFile: Error at xmlTextWriterStartElement\n");
                    return;
                }

                // Add an attribute with name "note_number" and value chord[i].note.note_number to note. 
                rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "note_number", "%d", chord[chord_index].note[note_index].note_number);
                if (rc < 0) {
                    printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
                    return;
                }

                // Add an attribute with name "velocity" and value chord[i].note.velocity to note. 
                rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "velocity", "%d", chord[chord_index].note[note_index].velocity);
                if (rc < 0) {
                    printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
                    return;
                }
                if (chord[chord_index].note[note_index].delay != 0) {
                    // Add an attribute with name "delay" and value chord[i].note.delay to note. 
                    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "delay", "%d", chord[chord_index].note[note_index].delay);
                    if (rc < 0) {
                        printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute\n");
                        return;
                    }
                }
                if (chord[chord_index].note[note_index].midi_channel != 0) {
                    // Add an attribute with name "midi_channel" and value chord[i].note.midi_channel to note. 
                    rc = xmlTextWriterWriteFormatAttribute(writer, BAD_CAST "midi_channel", "%d", chord[chord_index].note[note_index].midi_channel);
                    if (rc < 0) {
                        printf("writeCurrentPatchToFile: Error at xmlTextWriterWriteAttribute note.midi_channel\n");
                        return;
                    }
                }

                // Close the element named note. 
                rc = xmlTextWriterEndElement(writer);
                if (rc < 0) {
                    printf("writeCurrentPatchToFile: Error at xmlTextWriterEndElement\n");
                    return;
                }
            }
            // Close the element named chord. 
            rc = xmlTextWriterEndElement(writer);
            if (rc < 0) {
                printf("writeCurrentPatchToFile: Error at xmlTextWriterEndElement\n");
                return;
            }

        }
    }
    */

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

void freeStateMemory () {
/*
    free(state.delayed_notes);
    free(state.string);
    free(state.sustain_string);
    free(state.queued_notes.note);
    free(state.active_notes.note);
*/
}

void readNote (xmlNode *note_element, struct note_t *noteData) {
	int note_number = 0, velocity = 0, delay = 0, midi_channel = 0, direction = BOTH;
	char directionString[5] = "    \0";
	char sustainString[9] = "";
	int string = 0;

    if (xmlGetProp(note_element, "note_number")) {
        sscanf(xmlGetProp(note_element, "note_number"), "%d", &note_number);
        printf("note: %d", note_number);
    }
    (*noteData).note_number = note_number;

    if (xmlGetProp(note_element, "velocity")) {
        sscanf(xmlGetProp(note_element, "velocity"), "%d", &velocity);
        printf("\tvelocity: %d", velocity);
    }
    (*noteData).velocity = velocity;

    if (xmlGetProp(note_element, "direction")) {
        sscanf(xmlGetProp(note_element, "direction"), "%s", directionString);
        printf("\tdirection: %s", directionString);
    }
    if (strcmp(directionString, "up") == 0) {
  		(*noteData).direction = UP;
  	} else if (strcmp(directionString, "down") == 0) {
	  	(*noteData).direction = DOWN;
	} else {
		(*noteData).direction = BOTH;
	}

    if (xmlGetProp(note_element, "string")) {
        sscanf(xmlGetProp(note_element, "string"), "%d", &string);
        printf("\tstring: %d", string);
    }
    (*noteData).string = string;

    if (xmlGetProp(note_element, "legato")) {
        (*noteData).legato = 1;
        printf("\tlegato");
    } else {
        (*noteData).legato = 0;
    }

    if (xmlGetProp(note_element, "sustain")) {
        sscanf(xmlGetProp(note_element, "sustain"), "%s", sustainString);
        printf("\tsustain: %s", sustainString);
    }
    if (strcmp(sustainString, "string") == 0) {
  		(*noteData).sustain_mode = SUSTAIN_STRING;
  	} else if (strcmp(sustainString, "sequence") == 0) {
	  	(*noteData).sustain_mode = SUSTAIN_SEQUENCE;
	} else {
		(*noteData).sustain_mode = SUSTAIN_OFF;
	}


    if (xmlGetProp(note_element, "delay")) {
        sscanf(xmlGetProp(note_element, "delay"), "%d", &delay);
        printf("\tdelay: %d", delay);
    }
    (*noteData).delay = delay;

    if (xmlGetProp(note_element, "midi_channel")) {
        sscanf(xmlGetProp(note_element, "midi_channel"), "%d", &midi_channel);
        (*noteData).midi_channel = midi_channel;
        printf("\tmidi_channel: %d", midi_channel);
    } else {
        (*noteData).midi_channel = 0;
    }

    printf("\n");
}

int parseAsMidiValue (xmlNode * node, char * prop) {
    int numberValue;
    char stringValue[20];

    if (xmlGetProp(node, prop)) {
        sscanf(xmlGetProp(node, prop), "%s", stringValue);
        if (!strcmp(stringValue, "pitch-max")) {
            numberValue = MIDI_PITCH_MAX;
        } else if (!strcmp(stringValue, "pitch_mid")) {
            numberValue = MIDI_PITCH_CENTER;
        } else if (!strcmp(stringValue, "pitch_min")) {
            numberValue = MIDI_PITCH_MIN;
        } else if (!strcmp(stringValue, "cc2_max")) {
            numberValue = MIDI_CC2_MAX;
        } else if (!strcmp(stringValue, "cc2_mid")) {
            numberValue = MIDI_CC2_MID;
        } else if (!strcmp(stringValue, "cc2_min")) {
            numberValue = MIDI_CC2_MIN;
        } else if (!strcmp(stringValue, "pedal_on")) {
            numberValue = MIDI_PEDAL_ON;
        } else if (!strcmp(stringValue, "pedal_off")) {
            numberValue = MIDI_PEDAL_OFF;
        } else {
            sscanf(xmlGetProp(node, prop), "%d", &numberValue);
        }
    } else {
        numberValue = MIDI_DATA_NULL;
    }

    return numberValue;
}



int parseAsCCNumber (xmlNode * node, char * prop) {
    int numberValue;
    char stringValue[20] = "                   \0";

    if (xmlGetProp(node, prop)) {
        sscanf(xmlGetProp(node, prop), "%s", stringValue);
        if (!strcmp(stringValue, "bank_select") || !strcmp(stringValue, "bank_select_msb")) {
            numberValue = MIDI_CC_BANK_SELECT_MSB;
        } else if (!strcmp(stringValue, "bank_select_lsb")) {
            numberValue = MIDI_CC_BANK_SELECT_LSB;
        } else if (!strcmp(stringValue, "modulation") || !strcmp(stringValue, "modulation_msb")) {
            numberValue = MIDI_CC_MODULATION_MSB;
        } else if (!strcmp(stringValue, "modulation_lsb")) {
            numberValue = MIDI_CC_MODULATION_LSB;
        } else if (!strcmp(stringValue, "breath") || !strcmp(stringValue, "breath_msb")) {
            numberValue = MIDI_CC_BREATH_CTL_MSB;
        } else if (!strcmp(stringValue, "breath_lsb")) {
            numberValue = MIDI_CC_BREATH_CTL_LSB;
        } else if (!strcmp(stringValue, "foot_ctl") || !strcmp(stringValue, "foot_ctl_msb")) {
            numberValue = MIDI_CC_FOOT_CTL_MSB;
        } else if (!strcmp(stringValue, "foot_ctl_lsb")) {
            numberValue = MIDI_CC_FOOT_CTL_LSB;
        } else if (!strcmp(stringValue, "portamento_time") || !strcmp(stringValue, "portamento_time_msb")) {
            numberValue = MIDI_CC_PORTAMENTO_TIME_MSB;
        } else if (!strcmp(stringValue, "portamento_time_lsb")) {
            numberValue = MIDI_CC_PORTAMENTO_TIME_LSB;
        } else if (!strcmp(stringValue, "data_entry") || !strcmp(stringValue, "data_entry_msb")) {
            numberValue = MIDI_CC_DATA_ENTRY_MSB;
        } else if (!strcmp(stringValue, "data_entry_lsb")) {
            numberValue = MIDI_CC_DATA_ENTRY_LSB;
        } else if (!strcmp(stringValue, "volume") || !strcmp(stringValue, "volume_msb")) {
            numberValue = MIDI_CC_VOLUME_MSB;
        } else if (!strcmp(stringValue, "volume_lsb")) {
            numberValue = MIDI_CC_VOLUME_LSB;
        } else if (!strcmp(stringValue, "balance") || !strcmp(stringValue, "balance_msb")) {
            numberValue = MIDI_CC_BALANCE_MSB;
        } else if (!strcmp(stringValue, "balance_lsb")) {
            numberValue = MIDI_CC_BALANCE_LSB;
        } else if (!strcmp(stringValue, "pan") || !strcmp(stringValue, "pan_msb")) {
            numberValue = MIDI_CC_PAN_MSB;
        } else if (!strcmp(stringValue, "pan_lsb")) {
            numberValue = MIDI_CC_PAN_LSB;
        } else if (!strcmp(stringValue, "expression") || !strcmp(stringValue, "expression_msb")) {
            numberValue = MIDI_CC_EXPRESSION_MSB;
        } else if (!strcmp(stringValue, "expression_lsb")) {
            numberValue = MIDI_CC_EXPRESSION_LSB;
        } else if (!strcmp(stringValue, "effect_ctl1") || !strcmp(stringValue, "effect_ctl1_msb")) {
            numberValue = MIDI_CC_EFFECT_CTL_1_MSB;
        } else if (!strcmp(stringValue, "effect_ctl1_lsb")) {
            numberValue = MIDI_CC_EFFECT_CTL_1_LSB;
        } else if (!strcmp(stringValue, "effect_ctl2") || !strcmp(stringValue, "effect_ctl2_msb")) {
            numberValue = MIDI_CC_EFFECT_CTL_2_MSB;
        } else if (!strcmp(stringValue, "effect_ctl2_lsb")) {
            numberValue = MIDI_CC_EFFECT_CTL_2_LSB;
        } else if (!strcmp(stringValue, "general_ctl1") || !strcmp(stringValue, "general_ctl1_msb")) {
            numberValue = MIDI_CC_GENERAL_CTL_1_MSB;
        } else if (!strcmp(stringValue, "general_ctl1_lsb")) {
            numberValue = MIDI_CC_GENERAL_CTL_1_LSB;
        } else if (!strcmp(stringValue, "general_ctl2") || !strcmp(stringValue, "general_ctl2_msb")) {
            numberValue = MIDI_CC_GENERAL_CTL_2_MSB;
        } else if (!strcmp(stringValue, "general_ctl2_lsb")) {
            numberValue = MIDI_CC_GENERAL_CTL_2_LSB;
        } else if (!strcmp(stringValue, "general_ctl3") || !strcmp(stringValue, "general_ctl3_msb")) {
            numberValue = MIDI_CC_GENERAL_CTL_3_MSB;
        } else if (!strcmp(stringValue, "general_ctl3_lsb")) {
            numberValue = MIDI_CC_GENERAL_CTL_3_LSB;
        } else if (!strcmp(stringValue, "general_ctl4") || !strcmp(stringValue, "general_ctl4_msb")) {
            numberValue = MIDI_CC_GENERAL_CTL_4_MSB;
        } else if (!strcmp(stringValue, "general_ctl4_lsb")) {
            numberValue = MIDI_CC_GENERAL_CTL_4_LSB;
        } else if (!strcmp(stringValue, "sustain_pedal") || !strcmp(stringValue, "sustain")) {
            numberValue = MIDI_CC_SUSTAIN_PEDAL;
        } else if (!strcmp(stringValue, "portamento_pedal") || !strcmp(stringValue, "portamento")) {
            numberValue = MIDI_CC_PORTAMENTO_PEDAL;
        } else if (!strcmp(stringValue, "sostenuto_pedal") || !strcmp(stringValue, "sostenuto")) {
            numberValue = MIDI_CC_SOSTENUTO_PEDAL;
        } else if (!strcmp(stringValue, "soft_pedal") || !strcmp(stringValue, "soft")) {
            numberValue = MIDI_CC_SOFT_PEDAL;
        } else if (!strcmp(stringValue, "legato_pedal") || !strcmp(stringValue, "legato")) {
            numberValue = MIDI_CC_LEGATO_PEDAL;
        } else if (!strcmp(stringValue, "resonance")) {
            numberValue = MIDI_CC_RESONANCE;
        } else if (!strcmp(stringValue, "brightness") || !strcmp(stringValue, "filter_cutoff")) {
            numberValue = MIDI_CC_BRIGHTNESS;
        } else if (!strcmp(stringValue, "release") || !strcmp(stringValue, "release_time")) {
            numberValue = MIDI_CC_RELEASE_TIME;
        } else if (!strcmp(stringValue, "attack") || !strcmp(stringValue, "attack_time")) {
            numberValue = MIDI_CC_ATTACK_TIME;
        } else {
            sscanf(xmlGetProp(node, prop), "%d", &numberValue);
        }
    } else {
        numberValue = MIDI_DATA_NULL;
    }

    return numberValue;
}



int parseAsInputValue (xmlNode * node, char * prop) {
    int numberValue;
    char stringValue[20];

    if (xmlGetProp(node, prop)) {
        sscanf(xmlGetProp(node, prop), "%s", stringValue);
        if (!strcmp(stringValue, "touch_none")) {
            numberValue = TOUCHBAR_STATE_NONE;
        } else if (!strcmp(stringValue, "touch_1st")) {
            numberValue = TOUCHBAR_STATE_1ST;
        } else if (!strcmp(stringValue, "touch_1st_2nd")) {
            numberValue = TOUCHBAR_STATE_1ST_AND_2ND;
        } else if (!strcmp(stringValue, "touch_2nd")) {
            numberValue = TOUCHBAR_STATE_2ND;
        } else if (!strcmp(stringValue, "touch_2nd_3rd")) {
            numberValue = TOUCHBAR_STATE_2ND_AND_3RD;
        } else if (!strcmp(stringValue, "touch_3rd")) {
            numberValue = TOUCHBAR_STATE_3RD;
        } else if (!strcmp(stringValue, "touch_3rd_4th")) {
            numberValue = TOUCHBAR_STATE_3RD_AND_4TH;
        } else if (!strcmp(stringValue, "touch_4th")) {
            numberValue = TOUCHBAR_STATE_4TH;
        } else if (!strcmp(stringValue, "touch_4th_5th")) {
            numberValue = TOUCHBAR_STATE_4TH_AND_5TH;
        } else if (!strcmp(stringValue, "touch_5th")) {
            numberValue = TOUCHBAR_STATE_5TH;
        } else if (!strcmp(stringValue, "touchbar_min")) {
            numberValue = touchbar_range.min;
        } else if (!strcmp(stringValue, "touchbar_mid")) {
            numberValue = (touchbar_range.max - touchbar_range.min) / 2;
        } else if (!strcmp(stringValue, "touchbar_max")) {
            numberValue = touchbar_range.max;
        } else if (!strcmp(stringValue, "whammy_min")) {
            numberValue = whammy_range.min;
        } else if (!strcmp(stringValue, "whammy_mid")) {
            numberValue = (whammy_range.max - whammy_range.min) / 2;
        } else if (!strcmp(stringValue, "whammy_max")) {
            numberValue = whammy_range.max;
        } else {
            sscanf(xmlGetProp(node, prop), "%d", &numberValue);
        }
    } else {
        numberValue = MIDI_DATA_NULL;
    }

    return numberValue;
}


int getFretStatus (xmlNode* node) {
	int status = 0;
	printf("\n");
    if (xmlGetProp(node, "green")) {
        status = status | GREEN;
        printf("GREEN\t");
    }
    if (xmlGetProp(node, "red")) {
        status = status | RED;
        printf("RED\t");
    }
    if (xmlGetProp(node, "yellow")) {
        status = status | YELLOW;
        printf("YELLOW\t");
    }
    if (xmlGetProp(node, "blue")) {
        status = status | BLUE;
        printf("BLUE\t");
    }
    if (xmlGetProp(node, "orange")) {
        status = status | ORANGE;
        printf("ORANGE");
    }
    printf("\n");
    return status;
}


void readCounter(xmlNode *node, struct counter_t* counter) {
    if (xmlGetProp(node, "length")) {
        sscanf(xmlGetProp(node, "length"), "%d", &(*counter).length);
    }
    (*counter).position = 0;
}

void freeCounterMemory (struct counter_t* counter) {}

void readCounters (xmlNode *countersNode, int *number_of_counters, struct counter_t** counters) {
    int counter_index;
    xmlNode *counterNode;


    *number_of_counters = (int)xmlChildElementCount(countersNode);
    *counters = malloc(*number_of_counters * sizeof(struct counter_t));

    counterNode = countersNode->children;
    counter_index = 0;

    while (counterNode != NULL && counter_index < *number_of_counters) {
        if (counterNode->type == XML_ELEMENT_NODE && counter_index < *number_of_counters) {
            readCounter(counterNode, &(*counters)[counter_index]);
            counter_index++;
        }
        counterNode = counterNode->next;
    }
}

void freeCountersMemory (int *number_of_counters, struct counter_t** counters) {
    if (*number_of_counters > 0) {
        for (int i = *number_of_counters; i >= 0; i--) {
            freeCounterMemory(&(*counters)[i]);
        }
    }
    free(*counters);
}

void readCCMessage (xmlNode *node, struct cc_message_t *cc) {

    if (xmlGetProp(node, "midi_channel")) {
        sscanf(xmlGetProp(node, "midi_channel"), "%d", &(*cc).channel);
    } else {
        (*cc).channel = MIDI_DATA_NULL;
    }
    (*cc).parameter = parseAsCCNumber(node, "parameter");

    if (xmlGetProp(node, "value")) {
        sscanf(xmlGetProp(node, "value"), "%d", &(*cc).value);
    }
    printf("cc (read):\tv: %d\tp: %d\t c: %d\n", (*cc).parameter, (*cc).value, (*cc).channel);
}

void freeCCMessageMemory(struct cc_message_t *cc) {}

void readCC (xmlNode* node, int *number_of_messages, struct cc_message_t **cc) {
    xmlNode* message_element;
    int message_index;

    *number_of_messages = (int)xmlChildElementCount(node);
    *cc = malloc(*number_of_messages * sizeof(struct cc_message_t));

    message_element = node->children;
    message_index = 0;
    while (message_element != NULL && message_index < *number_of_messages) {
        if (message_element->type == XML_ELEMENT_NODE) {
            readCCMessage(message_element, &(*cc)[message_index]);
            message_index++;
       }
       message_element = message_element->next;
    }
}

void freeCCMemory (int *number_of_messages, struct cc_message_t **cc) {
    if (number_of_messages > 0) {
        for (int i = *number_of_messages - 1; i <= 0; i--) {
            freeCCMessageMemory(&(*cc)[i]);
        }
    }
    free(*cc);
}


void readScaledMessage (xmlNode *node, struct scaled_message_t *sm) {

    char typeString[5] = "";

    if (xmlGetProp(node, "type")) {
        sscanf(xmlGetProp(node, "type"), "%s", typeString);
    }

    if (!strcmp(typeString, "pitch")) {
        (*sm).type = SCALED_PITCH;
    } else if (!strcmp(typeString, "cc")) {
        (*sm).type = SCALED_CC;
    }

    (*sm).cc = parseAsCCNumber(node, "cc");
    (*sm).cc_lsb = parseAsCCNumber(node, "cc_lsb");

    if (xmlGetProp(node, "default")) {
        sscanf(xmlGetProp(node, "default"), "%d", &(*sm).default_value);
    } else {
        (*sm).default_value = MIDI_DATA_NULL;
    }

    if (xmlGetProp(node, "min")) {
        sscanf(xmlGetProp(node, "min"), "%d", &(*sm).out.min);
    }
    if (xmlGetProp(node, "max")) {
        sscanf(xmlGetProp(node, "max"), "%d", &(*sm).out.max);
    }

    (*sm).in.min = parseAsInputValue(node, "in_min");
    (*sm).in.max = parseAsInputValue(node, "in_max");

    if (xmlGetProp(node, "midi_channel")) {
        sscanf(xmlGetProp(node, "midi_channel"), "%d", &(*sm).midi_channel);
    } else {
        (*sm).midi_channel = MIDI_DATA_NULL;
    }
}

void freeScaledMessage (struct scaled_message_t *sm) {}

void readScaledMessages (xmlNode *messagesNode, unsigned int *number_of_messages, struct scaled_message_t** messages) {
    xmlNode *message_element;
    int message_index;
    
    *number_of_messages = (unsigned int)xmlChildElementCount(messagesNode);
    *messages = malloc(*number_of_messages * sizeof(struct scaled_message_t));

    message_element = messagesNode->children;
    message_index = 0;
    while (message_element != NULL) {
        if (message_element->type == XML_ELEMENT_NODE && message_index < *number_of_messages) {
            readScaledMessage(message_element, &(*messages)[message_index]);
            message_index++;
       }
       message_element = message_element->next;
    }
}

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

void printScaledMessageList (struct scaled_message_t *message) {
    printf("type: %d,\tmin: %d,\tmax: %d,\tcc: %d,\tcc_lsb: %d,\tin_min: %d,\tin_max: %d\n",
        message->type, message->out.min, message->out.max, message->cc, message->cc_lsb, message->in.min, message->in.max);
}

void readChords (xmlNode *chordsNode, unsigned char *count, struct chord_t **chords);
void freeChordsMemory (unsigned char *count, struct chord_t **chords);

void readChord(xmlNode* chordNode, struct chord_t *chord) {
    xmlNode *chord_content;
    int note_index = 0;
    (*chord).size = 0;

    (*chord).frets = getFretStatus(chordNode);
    (*chord).size = (unsigned int)xmlChildElementCount(chordNode);
    (*chord).variation_count = 0;
    (*chord).note = malloc((*chord).size * sizeof(struct note_t));
    /* a little unused memory (1x sizeof struct note_t) will be allocated when the chord has variations */


    chord_content = chordNode->children;
    while (chord_content != NULL) {
        if (chord_content->type == XML_ELEMENT_NODE) {
            if (!strcmp(chord_content->name, "note") && note_index < (*chord).size) {
                readNote(chord_content, &(*chord).note[note_index]);
                note_index++;
            } else if (!strcmp(chord_content->name, "variations")) {
                (*chord).size -= 1;
                readChords(chord_content, &(*chord).variation_count, &(*chord).variation);
            }
        }
        chord_content = chord_content->next;
    }
}

void freeChordMemory (struct chord_t *chord) {
    if ((*chord).variation_count > 0) {
        freeChordsMemory(&(*chord).variation_count, &(*chord).variation);
    }
    free((*chord).note);
}

void readChords (xmlNode *chordsNode, unsigned char *count, struct chord_t **chords) {
    xmlNode *chord_element;
    int chord_index = 0;

    chord_element = chordsNode->children;

    *count = (unsigned int)xmlChildElementCount(chordsNode);
    *chords = malloc(*count * sizeof(struct chord_t));

    while (chord_element != NULL && chord_index < *count) {
        if (chord_element->type == XML_ELEMENT_NODE) {
            readChord(chord_element, &(*chords)[chord_index]);
            chord_index++;
        }
        chord_element = chord_element->next;
    }
}

void freeChordsMemory (unsigned char *count, struct chord_t **chords) {
    if (*count > 0) {
        for (int i = *count - 1; i >= 0; i--) {
            freeChordMemory(&(*chords)[i]);
        }
    }
    free(*chords);
}

struct sequence_t readSequence (xmlNode *sequenceNode, struct sequence_t *sequence) {
    xmlNode* step_element;
    int number_of_steps, step_index = 0;
    (*sequence).length = 0;


    (*sequence).frets = getFretStatus(sequenceNode);
    (*sequence).length = (int)xmlChildElementCount(sequenceNode);

    if (xmlGetProp(sequenceNode, "shared_counter")) {
        sscanf(xmlGetProp(sequenceNode, "shared_counter"), "%d", &(*sequence).shared_counter);
    } else {
        (*sequence).shared_counter = MIDI_DATA_NULL;
    }

    if (xmlGetProp(sequenceNode, "reset_shared_counter")) {
        (*sequence).reset_shared_counter = 1;
    } else {
        (*sequence).reset_shared_counter = 0;
    }

    (*sequence).keep_position = 0;
    if (xmlGetProp(sequenceNode, "keep_position")) {
        (*sequence).keep_position = 1;
    }
    (*sequence).step = malloc((*sequence).length * sizeof(struct chord_t));
    (*sequence).position = 0;

    step_element = sequenceNode->children;
    while (step_element != NULL && step_index < (*sequence).length) {
        if (step_element->type == XML_ELEMENT_NODE) {
            printf("step %d:\n", step_index);
            readChord(step_element, &(*sequence).step[step_index]);
            step_index++;
        }
        step_element = step_element->next;
    }
}

void freeSequenceMemory (struct sequence_t *sequence) {
    for (int i = (*sequence).length - 1; i >= 0; i--) {
        freeChordMemory(&(*sequence).step[i]);
    }
    free((*sequence).step);
}

void readSequences (xmlNode *node, unsigned char *count, struct sequence_t** sequences) {
    xmlNode *sequence_element = node->children;
    int sequence_index = 0;

    *count = (int)xmlChildElementCount(node);
    *sequences = malloc(*count * sizeof(struct sequence_t));


    while (sequence_element != NULL) {
        if (sequence_element->type == XML_ELEMENT_NODE) {
            readSequence(sequence_element, &(*sequences)[sequence_index]);
            sequence_index++;
        }
        sequence_element = sequence_element->next;
    }
}

void freeSequencesMemory (int count, struct sequence_t **sequences) {
    for (int i = count - 1; i >= 0; i--) {
        freeSequenceMemory(&(*sequences)[i]);
    }
    free(*sequences);
}

void readProgramChange (xmlNode *node, struct midi_program_change_t *pc) {
    if (xmlGetProp(node, "channel")) {
        sscanf(xmlGetProp(node, "channel"), "%d", &(*pc).channel);
    } else {
        (*pc).channel = 0;
    }
    if (xmlGetProp(node, "bank_msb")) {
        sscanf(xmlGetProp(node, "bank_msb"), "%d", &(*pc).bank_msb);
    } else {
        (*pc).bank_msb = MIDI_DATA_NULL;
    }
    if (xmlGetProp(node, "bank_lsb")) {
        sscanf(xmlGetProp(node, "bank_lsb"), "%d", &(*pc).bank_lsb);
    } else {
        (*pc).bank_lsb = MIDI_DATA_NULL;
    }
    if (xmlGetProp(node, "program")) {
        sscanf(xmlGetProp(node, "program"), "%d", &(*pc).program);
    }
}

void freeProgramChangeMemory (struct midi_program_change_t *pc) {}

void initializeEmptyMidiConfiguration (struct midi_configuration_t *midiConfiguration) {
    (*midiConfiguration).default_channel = 0;
    (*midiConfiguration).program_change_count = 0;
}

void readMidiConfiguration (xmlNode *node, struct midi_configuration_t *midiConfiguration) {
    int pc_index = 0;
    xmlNode *pcNode = node->children;

    if (xmlGetProp(node, "default_channel")) {
        sscanf(xmlGetProp(node, "default_channel"), "%d", &(*midiConfiguration).default_channel);
    } else {
        (*midiConfiguration).default_channel = 0;
    }

    (*midiConfiguration).program_change_count = (int)xmlChildElementCount(node);
    (*midiConfiguration).program_change = malloc((*midiConfiguration).program_change_count * sizeof(struct midi_program_change_t));

    printf("reading midi config\n");
    while (pcNode != NULL) {
        if (pcNode->type == XML_ELEMENT_NODE) {
            readProgramChange(pcNode, &((*midiConfiguration).program_change[pc_index]));
            pc_index++;
        }
        pcNode = pcNode->next;
    }
}

void freeMidiMemory (struct midi_configuration_t *midiConfiguration) {
    if ((*midiConfiguration).program_change_count > 0) {
        for (int i = (*midiConfiguration).program_change_count - 1; i >= 0; i--) {
            freeProgramChangeMemory(&((*midiConfiguration).program_change[i]));
        }
    }
    free((*midiConfiguration).program_change);
}

void readName (xmlNode* node, char name[MAX_NAME_LENGTH]) {
    if (xmlGetProp(node, "name")) {
        sscanf(xmlGetProp(node, "name"), "%[^\t\n]", &(*name));
    }
}

void readBank (xmlNode* node, struct bank_t* bank) {
    (*bank).whammy_length = 0;
    (*bank).touchbar_length = 0;
    (*bank).cc_length = 0;
    (*bank).selectable = 0;
    (*bank).chord_count = 0;
    (*bank).sequence_count = 0;
    (*bank).number_of_counters = 0;
    memset((*bank).name, '\0', sizeof((*bank).name));
    initializeEmptyMidiConfiguration(&(*bank).midi);


    xmlNode* bank_content;
    bank_content = node->children;
    readName(node, (*bank).name);
    printf("%s:\n", bank->name);

    while (bank_content != NULL) {
        if (bank_content->type == XML_ELEMENT_NODE) {
            if (!strcmp(bank_content->name, "midi_configuration")) {
                readMidiConfiguration(bank_content, &bank->midi);
            }
            if (!strcmp(bank_content->name, "sequence_counters")) {
                readCounters(bank_content, &bank->number_of_counters, &bank->counter);
            }
            if (!strcmp(bank_content->name, "cc")) {
                readCC(bank_content, &bank->cc_length, &bank->cc);
            }
            if (!strcmp(bank_content->name, "whammy")) {
                readScaledMessages(bank_content, &bank->whammy_length, &bank->whammy);
            }
            if (!strcmp(bank_content->name, "touchbar")) {
                readScaledMessages(bank_content, &bank->touchbar_length, &bank->touchbar);
                for (int km = 0; km < bank->touchbar_length; km++) {
                    printScaledMessageList(&bank->touchbar[km]);
                }
            }
            if (!strcmp(bank_content->name, "chords")) {
                readChords(bank_content, &(bank->chord_count), &(bank->chord));
            }
            if (!strcmp(bank_content->name, "sequences")) {
                readSequences(bank_content, &(bank->sequence_count), &(bank->sequence));
            }
        }
        bank_content = bank_content->next;
    }
    bank->selectable = 1;
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
        if ((*bank).cc_length > 0) {
            freeCCMemory(&(*bank).cc_length, &(*bank).cc);
        }
        if ((*bank).number_of_counters > 0) {
            freeCountersMemory(&(*bank).number_of_counters, &(*bank).counter);
        }
        freeMidiMemory(&(*bank).midi);
    }
}

void readBanks (xmlNode *node, int *count, struct bank_t **bank) {
    printf("%s:\n", node->name);
    xmlNode *bank_element = node->children;
    int bank_index = 0;

    *count = (unsigned int)xmlChildElementCount(node);
    *bank = malloc(*count * sizeof(struct bank_t));


    while (bank_element != NULL) {
        if (bank_element->type == XML_ELEMENT_NODE) {
            printf("bank %d ", bank_index);
            readBank(bank_element, &(*bank)[bank_index]);
            bank_index += 1;
        }
        bank_element = bank_element->next;
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

void initializeStickTargetStates (unsigned int count, struct stick_target_state_t **stick_target_states) {
    if (count > 0) {
        *stick_target_states = malloc(count * sizeof(struct stick_target_state_t));
    }
    for (int i = 0; i < count; i++) {
        (*stick_target_states)->last_sent_value = 0;
    }
}

void readStickTargets (xmlNode *node, int *count, struct stick_target_t **target) {
    xmlNode *target_element = node->children;
    int target_index = 0;

    *count = (unsigned int)xmlChildElementCount(node);
    *target = malloc(*count * sizeof(struct stick_target_t));


    while (target_element != NULL) {
        if (target_element->type == XML_ELEMENT_NODE) {
            printf("stick target %d ", target_index);
            readScaledMessages(target_element, &(*target)[target_index].number_of_messages, &(*target)[target_index].messages);
            target_index += 1;
        }
        target_element = target_element->next;
    }
}

void freeStickTargetsMemory (int *count, struct stick_target_t **target) {
    for (int i = *count - 1; i >= 0; i--) {
        freeScaledMessages(&(*target)[i].number_of_messages, &(*target)[i].messages);
    }
    free(*target);    
}

int readPatchFromFile (const char *file) {

    initializeEmptyMidiConfiguration(&patch.midi);
    patch.cc_length = 0;
    patch.whammy_length = 0;
    patch.touchbar_length = 0;
    patch.number_of_banks = 0;
    patch.number_of_stick_targets = 0;
    memset(patch.name, '\0', sizeof(patch.name));

    xmlDoc *doc = NULL;
    xmlNode *root_element, *cur = NULL;
    int bank_index;

    /*
     * this initialize the library and check potential ABI mismatches
     * between the version it was compiled for and the actual shared
     * library used.
     */
    LIBXML_TEST_VERSION

    /*parse the file and get the DOM */
    doc = xmlReadFile(file, NULL, 0);

    if (doc == NULL) {
        fprintf(stderr, "error: could not parse file %s\n", file);
        return 1;
    }


    /*Get the root element node */
    root_element = xmlDocGetRootElement(doc);
    cur = root_element;

    if (!strcmp(cur->name, "patch")) {
        readName(cur, patch.name);
        printf ("patch name: %s\n", patch.name);
        cur = cur->children;

        while (cur != NULL) {
        	if (cur->type == XML_ELEMENT_NODE) {
                if (!strcmp(cur->name, "midi_configuration")) {
                    readMidiConfiguration(cur, &patch.midi);
                }
                if (!strcmp(cur->name, "stick")) {
                    readStickTargets(cur, &patch.number_of_stick_targets, &patch.stick_target);
                    initializeStickTargetStates(patch.number_of_stick_targets, &state.stick.target);
                }
                if (!strcmp(cur->name, "cc")) {
                	readCC(cur, &patch.cc_length, &patch.cc);
                }

                if (!strcmp(cur->name, "whammy")) {
                	readScaledMessages(cur, &patch.whammy_length, &patch.whammy);
                }

                if (!strcmp(cur->name, "touchbar")) {
                    readScaledMessages(cur, &patch.touchbar_length, &patch.touchbar);
                }

		        if (!strcmp(cur->name, "banks")) {
                    readBanks(cur, &patch.number_of_banks, &bank);
		        }
		    }
		    cur = cur->next;
		}
    }

    /*free the document */
    xmlFreeDoc(doc);

    /*
     *Free the global variables that may
     *have been allocated by the parser.
     */
    xmlCleanupParser();
    state.action.system = SYSTEM_ACTION_PATCH_INIT;
    return 0;
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
    if (patch.cc_length > 0) {
        freeCCMemory(&patch.cc_length, &patch.cc);
    }
    freeMidiMemory(&patch.midi);
}


/* END XML FUNCTIONS */



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
    struct cwiid_state state;    /* wiimote state */
    bdaddr_t bdaddr;    /* bluetooth device address */
    unsigned char mesg = 0;
    unsigned char rpt_mode = 0;

//experiment with delayed notes goes here 
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


    /* Connect to address given on command-line, if present */
    //if (argc > 2) {
    //    str2ba(argv[2], &bdaddr);
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
//    input_port = jack_port_register (client, "Jack MIDI in", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    nframes = jack_get_buffer_size(client);


    if (jack_activate(client)) {
        fprintf (stderr, "cannot activate client");
        return 1;
    }

    const char **ports;
    if ((ports = jack_get_ports (client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput)) == NULL) {
        fprintf(stderr, "Cannot find any physical playback ports\n");
    } else {
        if (jack_connect (client, jack_port_name(output_port), ports[0])) {
            fprintf (stderr, "cannot connect output ports\n");
        }
    }
    free(ports);

    if (system("stty -echo")) {
        fprintf(stderr, "Unable to disable local echo\n");
    }
    signal(SIGINT, siginthandler);

    while (1) {
        sleep(1);
    }
    return 0;
}

