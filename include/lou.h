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

#define MAX_ACTIVE_NOTES_COUNT  120
#define MAX_QUEUED_NOTES_COUNT  120
#define MAX_DELAYED_NOTES_COUNT 120

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

enum turntables_mode_t {
	TURNTABLE_MODE_PLAY,	// when euphoria button is pressed, G is rewind, R is play, B is fast forward, spin is jog wheel
	TURNTABLE_MODE_SET_CUE, // when plus buton is pressed, GRB sets cues 1, 2 and 3
	TURNTABLE_MODE_REMOVE_CUE, // when minus button is pressed, GRB removes cues 1, 2 and 3 
	TURNTABLE_MODE_TRIGGER_CUE // when no button is pressed, GRB plays from cues 1, 2 and 3
} turntables_mode;

enum crossfader_action_t {
	CROSSFADER_ACTION_NONE,
	CROSSFADER_ACTION_FADE_LEFT,
	CROSSFADER_ACTION_FADE_RIGHT
} crossfader_action;

enum effect_dial_action_t {
	EFFECT_DIAL_ACTION_NONE,
	EFFECT_DIAL_ACTION_INITIALIZE,
	EFFECT_DIAL_ACTION_ROTATE_CLOCKWISE,
	EFFECT_DIAL_ACTION_ROTATE_COUNTER_CLOCKWISE
} effect_dial_action;

enum buttons_action_t {
	BUTTONS_ACTION_NONE,
	BUTTONS_ACTION_BANK_CHANGE
} buttons_actions;


struct effect_dial_state_t {
	int8_t change;
	uint8_t value;
	uint8_t max_value;
	uint8_t  min_value;
	uint8_t  initial_value;
} effect_dial_state;


enum system_action_t {
	SYSTEM_ACTION_NONE,
	SYSTEM_ACTION_PATCH_INIT,
	SYSTEM_ACTION_BANK_INIT
} system_actions;

unsigned char system_action;
unsigned char touchbar_state;
unsigned char touchbar_action;
unsigned char strummer_state;
unsigned char strummer_action;
unsigned char whammy_state;
unsigned char whammy_action;
unsigned char buttons_action;
unsigned char buttons_action_data;
unsigned char stick_state[2];
unsigned char stick_zone_average_value;
unsigned char stick_zone_rotation_clockwise_counter;
unsigned char stick_zone_rotation_counter_clockwise_counter;
unsigned char last_sent_volume_value;



struct stick_zone_value_accumulator {
	unsigned int count;
	unsigned int value;
};
struct stick_zone_value_accumulator stick_zone_acc;

struct note_t {
	unsigned short int velocity;
	unsigned short int note_number;
	unsigned int delay;  // used for strumming patterns (not implemented yet)
	unsigned int midi_channel;
};

struct chord_t {
	struct note_t * note;
	int size;
};

// for all combinations of fret buttons: which notes to trigger
struct chord_t chord[ALL_COLOR_COMBINATIONS];  


// a bank is a collection of chords and sequences
struct bank_t {
	char selectable;
	int midi_channel;
	int midi_bank_msb;
	int midi_bank_lsb;
	int midi_program;
	struct chord_t chord[ALL_COLOR_COMBINATIONS];
};

#define MAX_BANKS_COUNT 3
struct bank_t *bank;


// current fret button + drum trigger states
unsigned int chord_state = 0;
unsigned int drums_action = 0;
unsigned int drums_state = 0;
uint8_t drums_buttons_previous = 0;
uint16_t buttons_previous = 0;

struct turntables_state current_turntables_state;

struct chord_t active_notes;
struct chord_t queued_notes;

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

int midi_channel;
int midi_bank_msb;
int midi_bank_lsb;
int midi_program;
int bank_midi_channel;
int8_t transpose;
int8_t selected_bank;

// initial attempt to implement delay:
#define DELAYED_NOTE_TRIGGERED 13
struct delayed_note_t {
	struct note_t note;
	struct itimerspec time;
	struct sigevent sevent;
	timer_t timer;
};

struct delayed_note_t *delayed_notes;
