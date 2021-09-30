/* midi_constants.h - LouWiiGui, a Wii Guitar MIDI controller */


#ifndef _MIDI_H
#define _MIDI_H

#include <jack/jack.h>
#include <jack/midiport.h>

#include "lou.h"

// amount of whammy movement before sending midi
#define MIDI_PITCH_MAX       0x3FFF
#define MIDI_PITCH_CENTER    0x2000
#define MIDI_PITCH_MIN       0x0000
#define MIDI_MODULATION_MAX  0x7F
#define MIDI_CC2_MAX         0x3FFF
#define MIDI_CC2_MID         0x2000
#define MIDI_CC2_MIN         0x0000
#define MIDI_PEDAL_ON		 0x7F
#define MIDI_PEDAL_OFF		 0x00

#define MIDI_DATA_NULL       0xFFFF

#define MIDI_NOTE_OFF       0x80
#define MIDI_NOTE_ON        0x90
#define MIDI_PROGRAM_CHANGE 0xC0
#define MIDI_PITCH_WHEEL    0xE0
#define MIDI_CONTROL_CHANGE 0xB0

#define MIDI_RTC_TIMING   0xF8
#define MIDI_RTC_START    0xFA
#define MIDI_RTC_CONTINUE 0xFB
#define MIDI_RTC_STOP     0xFC

#define MIDI_CC_BANK_SELECT_MSB     0x00
#define MIDI_CC_BANK_SELECT_LSB     0x20
#define MIDI_CC_MODULATION_MSB      0x01
#define MIDI_CC_MODULATION_LSB      0x21
#define MIDI_CC_BREATH_CTL_MSB      0x02
#define MIDI_CC_BREATH_CTL_LSB      0x22
#define MIDI_CC_FOOT_CTL_MSB        0x04
#define MIDI_CC_FOOT_CTL_LSB        0x24
#define MIDI_CC_PORTAMENTO_TIME_MSB 0x05
#define MIDI_CC_PORTAMENTO_TIME_LSB 0x25
#define MIDI_CC_DATA_ENTRY_MSB      0x06
#define MIDI_CC_DATA_ENTRY_LSB      0x26
#define MIDI_CC_VOLUME_MSB          0x07
#define MIDI_CC_VOLUME_LSB          0x27
#define MIDI_CC_BALANCE_MSB         0x08
#define MIDI_CC_BALANCE_LSB         0x28
#define MIDI_CC_PAN_MSB             0x0A
#define MIDI_CC_PAN_LSB             0x2A
#define MIDI_CC_EXPRESSION_MSB      0x0B
#define MIDI_CC_EXPRESSION_LSB      0x2B
#define MIDI_CC_EFFECT_CTL_1_MSB    0x0C
#define MIDI_CC_EFFECT_CTL_1_LSB    0x2C
#define MIDI_CC_EFFECT_CTL_2_MSB    0x0D
#define MIDI_CC_EFFECT_CTL_2_LSB    0x2D
#define MIDI_CC_GENERAL_CTL_1_MSB   0x10
#define MIDI_CC_GENERAL_CTL_1_LSB   0x30
#define MIDI_CC_GENERAL_CTL_2_MSB   0x11
#define MIDI_CC_GENERAL_CTL_2_LSB   0x31
#define MIDI_CC_GENERAL_CTL_3_MSB   0x12
#define MIDI_CC_GENERAL_CTL_3_LSB   0x32
#define MIDI_CC_GENERAL_CTL_4_MSB   0x13
#define MIDI_CC_GENERAL_CTL_4_LSB   0x33
#define MIDI_CC_SUSTAIN_PEDAL       0x40
#define MIDI_CC_PORTAMENTO_PEDAL    0x41
#define MIDI_CC_SOSTENUTO_PEDAL     0x42
#define MIDI_CC_SOFT_PEDAL          0x43
#define MIDI_CC_LEGATO_PEDAL        0x44
#define MIDI_CC_SOUND_VARIATION     0x46
#define MIDI_CC_RESONANCE           0x47
#define MIDI_CC_RELEASE_TIME        0x48
#define MIDI_CC_ATTACK_TIME         0x49
#define MIDI_CC_BRIGHTNESS          0x4A

void sendProgramChange (struct midi_program_change_t pc, unsigned char default_midi_channel, void *port_buf, jack_nframes_t time);
void sendRtcMessage (struct clock_message_t message, void* port_buf, int i);
void sendCCMessage (struct cc_message_t cc, unsigned char default_channel, void *port_buf, jack_nframes_t time);
void sendStaticMessages (struct midi_static_messages_t *messages, unsigned char default_channel, void *port_buf, jack_nframes_t time);
void sendScaledMessages (int messages_length, struct scaled_message_t *messages, struct range_t input_limits, int input_value, unsigned char default_channel, void *port_buf, jack_nframes_t time);
void sendDefaultScaledMessages (int messages_length, struct scaled_message_t *messages, unsigned char default_channel, void *port_buf, jack_nframes_t time);
void sendMidiConfiguration (struct midi_configuration_t config, unsigned char midi_channel, void* port_buf, jack_nframes_t time);
void note_on (struct note_t note, int transpose, unsigned char default_channel, void* port_buf, int i);
void note_off (struct note_t note, void* port_buf, int i);
void strum_chord_stringed_notes_only (struct chord_t chord, unsigned char default_channel, void* port_buf, int i);
void closeMidiPorts ();


#endif
