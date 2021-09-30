/* lou.h - LouWiiGui, a Wii Guitar MIDI controller */

#ifndef _LOU_H
#define _LOU_H

#include <signal.h>
#include <cwiid.h>

#define MY_ENCODING "UTF-8"


#define MAX_ACTIVE_NOTES_COUNT   120
#define MAX_QUEUED_NOTES_COUNT   120
#define MAX_DELAYED_NOTES_COUNT  120
#define MAX_SUSTAIN_STRINGS_COUNT 12
#define STRINGS_COUNT 12
#define MAX_NAME_LENGTH 33
// 32 letters (+1 for \0 string termination)

// flags for chord selection / drums status
#define NONE   0x00
#define GREEN  0x01
#define RED    0x02
#define YELLOW 0x04
#define BLUE   0x08
#define ORANGE 0x10
#define ALL_COLOR_COMBINATIONS (GREEN | RED | YELLOW | BLUE | ORANGE) + 1
#define PEDAL  0x20
#define NO_CHORD 0xFF

#define WHAMMY_STATE_UNKNOWN 0xFF
#define TOUCHBAR_UNTOUCHED 0x0F
#define TOUCHBAR_SLIDE_MARGIN 2

#define CWIID_GUITAR_STICK_MID ((CWIID_GUITAR_STICK_MAX ) / 2)

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


enum midi_message_type_t {
	MIDI_MESSAGE_TYPE_UNKNOWN,
	MIDI_MESSAGE_TYPE_NOTE_ON,
	MIDI_MESSAGE_TYPE_NOTE_OFF,
	MIDI_MESSAGE_TYPE_PITCH_SHIFT,
	MIDI_MESSAGE_TYPE_CC,
	MIDI_MESSAGE_TYPE_RTC
};

enum rtc_message_type_t {
	RTC_MSG_TYPE_UNKNOWN,
	RTC_MSG_TYPE_TIMING,
	RTC_MSG_TYPE_START,
	RTC_MSG_TYPE_STOP,
	RTC_MSG_TYPE_CONTINUE
};

enum strummer_state_t {
	STRUMMER_STATE_MID,
	STRUMMER_STATE_DOWN,
	STRUMMER_STATE_UP,
	STRUMMER_STATE_SUSTAINED,
	STRUMMER_STATE_UNKNOWN
};

enum strummer_action_t {
	STRUMMER_ACTION_NONE,
	STRUMMER_ACTION_MID_UP,
	STRUMMER_ACTION_UP_MID,
	STRUMMER_ACTION_MID_DOWN,
	STRUMMER_ACTION_DOWN_MID
};

enum strummer_direction_t {
	UP = 1,
	DOWN = 2,
	BOTH =3
};

enum sustain_mode_t {
	SUSTAIN_OFF      = 0, // the note ends when the strummer returns to mid position
	SUSTAIN_SEQUENCE = 1, // the note ends when the same note is struck on the same midi channel again, or until a different sequence/chord is strummed
	SUSTAIN_STRING   = 2  // the note ends when a new note is triggered on the same string, or when a chord is strummed
};

enum whammy_action_t {
	WHAMMY_ACTION_NONE,
	WHAMMY_ACTION_UP,
	WHAMMY_ACTION_DOWN
};

enum touchbar_state_t {
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
};

enum touchbar_action_t {
	TOUCHBAR_ACTION_NONE,
	TOUCHBAR_ACTION_TAP,
	TOUCHBAR_ACTION_RELEASE,
	TOUCHBAR_ACTION_HAMMERON,
	TOUCHBAR_ACTION_PULLOFF,
	TOUCHBAR_ACTION_SLIDE_UP,
	TOUCHBAR_ACTION_SLIDE_DOWN
};

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
};

enum stick_action_t {
	STICK_ACTION_NONE,
	STICK_ACTION_ROTATE_CLOCKWISE,
	STICK_ACTION_ROTATE_COUNTER_CLOCKWISE
};

enum turntables_mode_t {
	TURNTABLE_MODE_PLAY,	// when euphoria button is pressed, G is rewind, R is play, B is fast forward, spin is jog wheel
	TURNTABLE_MODE_SET_CUE, // when plus buton is pressed, GRB sets cues 1, 2 and 3
	TURNTABLE_MODE_REMOVE_CUE, // when minus button is pressed, GRB removes cues 1, 2 and 3 
	TURNTABLE_MODE_TRIGGER_CUE // when no button is pressed, GRB plays from cues 1, 2 and 3
};

enum crossfader_action_t {
	CROSSFADER_ACTION_NONE,
	CROSSFADER_ACTION_FADE_LEFT,
	CROSSFADER_ACTION_FADE_RIGHT
};

enum effect_dial_action_t {
	EFFECT_DIAL_ACTION_NONE,
	EFFECT_DIAL_ACTION_INITIALIZE,
	EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE,
	EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE
};

enum buttons_action_t {
	BUTTONS_ACTION_NONE,
	BUTTONS_ACTION_BANK_CHANGE,
	BUTTONS_ACTION_NEXT_PATCH,
	BUTTONS_ACTION_PREVIOUS_PATCH,
	BUTTONS_ACTION_NEXT_STICK_TARGET,
	BUTTONS_ACTION_PREVIOUS_STICK_TARGET
};

enum scaled_value_type_t {
	SCALED_CC,
	SCALED_PITCH,
	SCALED_NOTE_ON,
	SCALED_ALL_NOTES_OFF
};


struct effect_dial_state_t {
	int8_t change;
	uint8_t value;
	uint8_t max_value;
	uint8_t  min_value;
	uint8_t  initial_value;
};


enum system_action_t {
	SYSTEM_ACTION_NONE,
	SYSTEM_ACTION_PATCH_INIT,
	SYSTEM_ACTION_BANK_INIT
};

enum neck_action_t {
	NECK_ACTION_NONE,
	NECK_ACTION_CHANGE
};

struct note_t {
	unsigned char direction : 2;
	unsigned char legato : 1;
	unsigned char sustain_mode : 2;
	unsigned char velocity;
	unsigned char note_number;
	unsigned char midi_channel;
	unsigned char string;
	unsigned int delay;  // used for strumming patterns (not implemented yet)
};



struct chord_t {
	uint8_t frets;
	int size;
	struct note_t* note;
	int touchbar_length;
	struct scaled_message_t *touchbar;
	unsigned char variation_count;
	struct chord_t* variation;
};

struct sequence_t {
	uint32_t frets;
	unsigned char length;
	struct chord_t* step;
	unsigned char position;
	unsigned char keep_position;
	unsigned int reset_to;
	int shared_counter;
	char reset_shared_counter;
};

struct clock_message_t {
	enum rtc_message_type_t command;
};

struct cc_message_t {
	int channel;
	int parameter;
	int value;
};

struct pitch_bend_message_t {
	int channel;
	int value;
};

enum note_message_type {
	NOTE_ON_MESSAGE,
	NOTE_OFF_MESSAGE
};

struct note_message_t {
	enum note_message_type type;
	int channel;
	int note;
	int velocity;
};

struct program_change_message_t {
	int channel;
	int bank; // 1-16384
	int program; // 1-128
};

struct channel_aftertouch_message_t {
	int channel;
	int pressure;
};

struct key_pressure_message_t {
	int channel;
	int note;
	int pressure;
};

struct song_select_message_t {
	int song;
};

struct song_position_pointer_message_t {
	int position;
};

union static_message_t {
	struct cc_message_t cc;
	struct clock_message_t clock;
	struct note_message_t note;
	struct program_change_message_t program;
	struct pitch_bend_message_t pitch;
	struct channel_aftertouch_message_t channel_pressure;
	struct key_pressure_message_t key_pressure;
	struct song_position_pointer_message_t position;
	struct song_select_message_t song;
};

struct midi_program_change_t {
	int channel;
	int bank_msb;
	int bank_lsb;
	int program;
};

struct midi_configuration_t {
	int default_channel;
	int program_change_count;
	struct midi_program_change_t *program_change;
};

struct range_t {
	int min;
	int max;
};

struct scaled_message_t {
	enum scaled_value_type_t type;
	struct range_t out;
	struct range_t in;
	int cc;
	int cc_lsb;
	int default_value;
	int midi_channel;
};

struct counter_t {
	int length;
	int position;
	unsigned int reset_to;
};


struct stick_target_t {
	int number_of_messages;
	struct scaled_message_t *messages;
};

struct midi_static_messages_t {
	int length;
	unsigned char *type;
	union static_message_t *message;
};

struct patch_t {
	char name[MAX_NAME_LENGTH];
	struct midi_configuration_t midi;
	int number_of_banks;
	struct midi_static_messages_t *midi_static_messages;
	int whammy_length;
	struct scaled_message_t *whammy;
	int touchbar_length;
	struct scaled_message_t *touchbar;
	int number_of_stick_targets;
	struct stick_target_t *stick_target;
};


// initial attempt to implement delay:
#define DELAYED_NOTE_TRIGGERED 13
struct delayed_note_t {
	struct note_t note;
	struct itimerspec time;
	struct sigevent sevent;
	timer_t timer;
};

struct value_accumulator {
	unsigned int count;
	unsigned int value;
};

struct stick_target_state_t {
	int last_sent_value;
};

struct stick_state_t {
	unsigned char position[2];
	unsigned char average_value;
	unsigned char rotation_cw_counter;
	unsigned char rotation_ccw_counter;
	struct value_accumulator acc;
	enum stick_zone_t zone;
	unsigned char current_target;
	struct stick_target_state_t* target;
};

struct action_t {
	enum neck_action_t neck;
	enum system_action_t system;
	enum crossfader_action_t crossfader;
	enum touchbar_action_t touchbar;
	enum strummer_action_t strummer;
	unsigned char buttons_data;
	unsigned char drums;
	enum stick_action_t stick;
	enum whammy_action_t whammy;
	enum buttons_action_t buttons;
	enum effect_dial_action_t effect_dial;
};


struct state_t {
	struct action_t action;
	// current fret button + drum trigger states
	enum touchbar_state_t touchbar;
	unsigned char strummer;
	unsigned char whammy;
	unsigned int chord;
	unsigned char current_patch;
	unsigned int previous_strummed_chord;
	unsigned int drums;
	uint8_t drums_buttons_previous;
	uint16_t buttons_previous;
	struct chord_t* active_chord;
	
	struct stick_state_t stick;
	struct turntables_state current_turntables_state;
	struct effect_dial_state_t effect_dial;

	// playing state
	int8_t transpose;
	int8_t selected_bank;
	struct delayed_note_t *delayed_notes;
	struct note_t *sustain_string;
	struct note_t *string;
	struct chord_t active_notes;
	struct chord_t queued_notes;
	unsigned char system_pause;

	int *argc;
	char **argv;
};

// a bank is a collection of chords and sequences
struct bank_t {
	char name[MAX_NAME_LENGTH];
	char selectable;
	struct midi_configuration_t midi;
	struct chord_t *chord;
	unsigned char chord_count;
	struct sequence_t *sequence;
	unsigned char sequence_count;
	struct midi_static_messages_t *midi_static_messages;
	struct cc_message_t *cc;
	unsigned int whammy_length;
	struct scaled_message_t *whammy;
	int touchbar_length;
	struct scaled_message_t *touchbar;
	int number_of_counters;
	struct counter_t *counter;
};

#define MAX_BANKS_COUNT 3


struct range_t whammy_range;
struct range_t touchbar_range;
struct range_t stick_range;



 #endif /* _LOU_H */
