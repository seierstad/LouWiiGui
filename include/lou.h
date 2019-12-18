

#define MY_ENCODING "UTF-8"

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
	struct note_t * note;
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

struct cc_message_t {
	int channel;
	int parameter;
	int value;
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

struct patch_t {
	char name[MAX_NAME_LENGTH];
	struct midi_configuration_t midi;
	int number_of_banks;
	int cc_length;
	struct cc_message_t *cc;
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
	int cc_length;
	struct cc_message_t *cc;
	unsigned int whammy_length;
	struct scaled_message_t *whammy;
	int touchbar_length;
	struct scaled_message_t *touchbar;
	int number_of_counters;
	struct counter_t *counter;
};

#define MAX_BANKS_COUNT 3
struct patch_t patch;
struct bank_t *bank;
struct state_t state;
struct chord_t empty_chord;

jack_client_t *client;
jack_port_t *output_port;
jack_port_t *input_port;

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

struct range_t whammy_range;
struct range_t touchbar_range;
struct range_t stick_range;
