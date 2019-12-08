#define MY_ENCODING "UTF-8"

// amount of whammy movement before sending midi
#define MIDI_PITCH_MAX       0x3FFF
#define MIDI_PITCH_CENTER    0x2000
#define MIDI_MODULATION_MAX  0x7F
#define MIDI_DATA_NULL       0xFF   

#define MIDI_NOTE_OFF       0x80
#define MIDI_NOTE_ON        0x90
#define MIDI_PROGRAM_CHANGE 0xC0
#define MIDI_PITCH_WHEEL    0xE0
#define MIDI_CONTROL_CHANGE 0xB0

#define MIDI_CC_BANK_SELECT_MSB  0x00
#define MIDI_CC_BANK_SELECT_LSB  0x20
#define MIDI_CC_MODULATION_MSB   0x01
#define MIDI_CC_MODULATION_LSB   0x21
#define MIDI_CC_BREATH_CTL_MSB   0x02
#define MIDI_CC_BREATH_CTL_LSB   0x22
#define MIDI_CC_FOOT_CTL_MSB     0x04
#define MIDI_CC_FOOT_CTL_LSB     0x24
#define MIDI_CC_VOLUME_MSB       0x07
#define MIDI_CC_VOLUME_LSB       0x27
#define MIDI_CC_BALANCE_MSB      0x08
#define MIDI_CC_BALANCE_LSB      0x28
#define MIDI_CC_EFFECT_CTL_1_MSB 0x0C
#define MIDI_CC_EFFECT_CTL_1_LSB 0x2C

#define MAX_ACTIVE_NOTES_COUNT   120
#define MAX_QUEUED_NOTES_COUNT   120
#define MAX_DELAYED_NOTES_COUNT  120
#define MAX_SUSTAIN_STRINGS_COUNT 12

// flags for chord selection / drums status
#define NONE   0x00
#define GREEN  0x01
#define RED    0x02
#define YELLOW 0x04
#define BLUE   0x08
#define ORANGE 0x10
#define ALL_COLOR_COMBINATIONS (GREEN | RED | YELLOW | BLUE | ORANGE) + 1
#define PEDAL  0x20


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
	BOTH,
	UP,
	DOWN
};

enum sustain_mode_t {
	SUSTAIN_OFF,      // the note ends when the strummer returns to mid position
	SUSTAIN_SEQUENCE, // the note ends when the same note is struck on the same midi channel again, or until a different sequence/chord is strummed
	SUSTAIN_STRING    // the note ends when a new note is triggered on the same string, or when a chord is strummed
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
	BUTTONS_ACTION_BANK_CHANGE
};

enum scaled_value_type_t {
	SCALED_CC,
	SCALED_PITCH
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


struct note_t {
	unsigned char direction;
	unsigned short int velocity;
	unsigned short int note_number;
	unsigned int delay;  // used for strumming patterns (not implemented yet)
	unsigned int midi_channel;
	unsigned char sustain_mode;
	unsigned char string;
};

struct chord_t {
	struct note_t * note;
	int size;
};

struct sequence_t {
	struct chord_t* step;
	int length;
	unsigned char keep_position;
	unsigned char position;
	int shared_counter;
	char reset_shared_counter;
};

struct cc_message_t {
	int channel;
	int parameter;
	int value;
};

struct midi_info_t {
	int channel;
	int bank_msb;
	int bank_lsb;
	int program;
};

struct scaled_message_t {
	char type;
	int min;
	int max;
	int cc;
	int cc_lsb;
	int midi_channel;
};

struct counter_t {
	int length;
	int position;
};

struct patch_t {
	struct midi_info_t midi;
	int cc_length;
	struct cc_message_t *cc;
	int whammy_length;
	struct scaled_message_t *whammy;
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

struct stick_state_t {
	unsigned char position[2];
	unsigned char average_value;
	unsigned char rotation_cw_counter;
	unsigned char rotation_ccw_counter;
	unsigned char last_sent_value;
	struct value_accumulator acc;
	enum stick_zone_t zone;
};

struct action_t {
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
	unsigned int previous_strummed_chord;
	unsigned int drums;
	uint8_t drums_buttons_previous;
	uint16_t buttons_previous;
	
	struct stick_state_t stick;
	struct turntables_state current_turntables_state;
	struct effect_dial_state_t effect_dial;

	// playing state
	int8_t transpose;
	int8_t selected_bank;
	struct delayed_note_t *delayed_notes;
	struct note_t *sustain_string;
	struct chord_t active_notes;
	struct chord_t queued_notes;
};

// a bank is a collection of chords and sequences
struct bank_t {
	char selectable;
	struct midi_info_t midi;
	struct chord_t chord[ALL_COLOR_COMBINATIONS];
	struct sequence_t sequence[ALL_COLOR_COMBINATIONS];
	int cc_length;
	struct cc_message_t *cc;
	int whammy_length;
	struct scaled_message_t *whammy;
	int number_of_counters;
	struct counter_t *counter;
};

#define MAX_BANKS_COUNT 3
struct patch_t patch;
struct bank_t *bank;
struct state_t state;

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
