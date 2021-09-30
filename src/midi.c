/*  */

#include <jack/jack.h>
#include <jack/midiport.h>

#include "midi.h"
#include "lou.h"

extern jack_client_t *client;
extern jack_port_t *output_port;
extern jack_port_t *input_port;
extern struct state_t state;
extern int (*process)(jack_nframes_t, void*);




void sendProgramChange (struct midi_program_change_t pc, unsigned char default_midi_channel, void *port_buf, jack_nframes_t time) {
    unsigned char* buffer;
    int midi_channel = pc.channel ? pc.channel : default_midi_channel;

    if (pc.bank_msb != MIDI_DATA_NULL) {
        buffer = jack_midi_event_reserve(port_buf, time, 3);
        buffer[2] = pc.bank_msb;
        buffer[1] = MIDI_CC_BANK_SELECT_MSB;
        buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;
    }
    if (pc.bank_lsb != MIDI_DATA_NULL) {
        buffer = jack_midi_event_reserve(port_buf, time, 3);
        buffer[2] = pc.bank_lsb;
        buffer[1] = MIDI_CC_BANK_SELECT_LSB;
        buffer[0] = MIDI_CONTROL_CHANGE + midi_channel - 1;
    }
    if (pc.program != MIDI_DATA_NULL) {
        buffer = jack_midi_event_reserve(port_buf, time, 2);
        buffer[1] = pc.program;
        buffer[0] = MIDI_PROGRAM_CHANGE + midi_channel - 1;
    }

}

void sendRtcMessage (struct clock_message_t message, void* port_buf, int i) {
    unsigned char* buffer = jack_midi_event_reserve(port_buf, i, 1);

    switch (message.command) {
        case RTC_MSG_TYPE_TIMING:
            buffer[0] = MIDI_RTC_TIMING;
            break;
        case RTC_MSG_TYPE_START:
            buffer[0] = MIDI_RTC_START;
            break;
        case RTC_MSG_TYPE_STOP:
            buffer[0] = MIDI_RTC_STOP;
            break;
        case RTC_MSG_TYPE_CONTINUE:
            buffer[0] = MIDI_RTC_CONTINUE;
            break;
    }
}



void sendCCMessage (struct cc_message_t cc, unsigned char default_channel, void *port_buf, jack_nframes_t time) {
    int channel = (cc.channel != MIDI_DATA_NULL) ? cc.channel : default_channel;
    printf("sending CC\tparameter: %d\t,value: %d,\tchannel: %d\n", cc.parameter, cc.value, channel);
    unsigned char *buffer = jack_midi_event_reserve(port_buf, time, 3);

    buffer[2] = cc.value;
    buffer[1] = cc.parameter;
    buffer[0] = MIDI_CONTROL_CHANGE + channel - 1;
}


void sendStaticMessages (struct midi_static_messages_t *messages, unsigned char default_channel, void *port_buf, jack_nframes_t time) {
    int i;
    if (messages->length > 0) {
        for (i = 0; i < messages->length; i += 1) {
            switch (messages->type[i]) {
                case MIDI_MESSAGE_TYPE_CC:
                    sendCCMessage(messages->message[i].cc, default_channel, port_buf, time);
                    break;
                case MIDI_MESSAGE_TYPE_RTC:
                    printf("sending rtc :) \n");
                    sendRtcMessage(messages->message[i].clock, port_buf, time);
                    break;

            }
        }
    }
}

void sendScaledMessages (int messages_length, struct scaled_message_t *messages, struct range_t input_limits, int input_value, unsigned char default_channel, void *port_buf, jack_nframes_t time) {
    unsigned char *buffer;
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
            midi_channel = messages[i].midi_channel == MIDI_DATA_NULL ? default_channel : messages[i].midi_channel;


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

void sendDefaultScaledMessages (int messages_length, struct scaled_message_t *messages, unsigned char default_channel, void *port_buf, jack_nframes_t time) {
    unsigned char *buffer;
    int midi_channel;

    for (int i = 0; i < messages_length; i++) {
        if (messages[i].default_value != MIDI_DATA_NULL) {
            midi_channel = messages[i].midi_channel == MIDI_DATA_NULL ? default_channel : messages[i].midi_channel;

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


void sendMidiConfiguration (struct midi_configuration_t config, unsigned char midi_channel, void* port_buf, jack_nframes_t time) {
    if (config.program_change_count > 0) {
        for (int i = 0; i < config.program_change_count; i++) {
            sendProgramChange(config.program_change[i], midi_channel, port_buf, time);
        }
    }
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


void note_on (struct note_t note, int transpose, unsigned char default_channel, void* port_buf, int i) {
    unsigned char* buffer;
    int use_midi_channel = note.midi_channel ? note.midi_channel : default_channel;
    struct note_t current_note;

    // copy the information neccessary to end the note as configured
    current_note.velocity = note.velocity;
    current_note.note_number = note.note_number + transpose;
    current_note.midi_channel = use_midi_channel;
    current_note.sustain_mode = note.sustain_mode;
    current_note.string = note.string;
    current_note.legato = note.legato;

    if (current_note.sustain_mode == SUSTAIN_STRING) {
        if (state.sustain_string[current_note.string].velocity != 0) {
            buffer = jack_midi_event_reserve(port_buf, i, 3);
            buffer[2] = state.sustain_string[current_note.string].velocity; /* velocity */
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

void note_off (struct note_t note, void* port_buf, int i) {
    unsigned char* buffer;
    jack_nframes_t use_time = (i != 0) ? ((jack_nframes_t) i) : jack_get_current_transport_frame(client);
    buffer = jack_midi_event_reserve(port_buf, use_time, 3);
    buffer[2] = note.velocity;        /* velocity */
    buffer[1] = note.note_number;    /* note number */
    buffer[0] = MIDI_NOTE_OFF + note.midi_channel - 1;
}




void closeMidiPorts () {

    if (client) {
        jack_deactivate(client);
        if (output_port) {
            jack_port_unregister(client, output_port);
        }
        jack_client_close(client);
    }
}
