#include <libxml2/libxml/encoding.h>
#include <libxml2/libxml/xmlwriter.h>

#include "lou.h"
#include "midi.h"

extern struct patch_t patch;
extern struct bank_t *bank;
extern struct state_t state;

/* XML FUNCTIONS: */

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


enum rtc_message_type_t parseAsRtcCommand (xmlNode *node, char *prop) {
    char stringValue[20] = "                   \0";
    if (xmlGetProp(node, prop)) {
        sscanf(xmlGetProp(node, prop), "%s", stringValue);
        if (!strcmp(stringValue, "timing") || !strcmp(stringValue, "tick")) {
            return RTC_MSG_TYPE_TIMING;
        }
        if (!strcmp(stringValue, "start")) {
            return RTC_MSG_TYPE_START;
        }
        if (!strcmp(stringValue, "stop")) {
            return RTC_MSG_TYPE_STOP;
        }
        if (!strcmp(stringValue, "continue")) {
            return RTC_MSG_TYPE_CONTINUE;
        }
    }

    return RTC_MSG_TYPE_UNKNOWN;
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
    if (xmlGetProp(node, "reset_to")) {
        sscanf(xmlGetProp(node, "reset_to"), "%d", &(*counter).reset_to);
    } else {
        (*counter).reset_to = 0;
    }
    (*counter).position = 0;
}

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
    printf("cc (read):\tv: %d\tp: %d\t c: %d-----------------\n", (*cc).parameter, (*cc).value, (*cc).channel);
}

void readRtcMessage (xmlNode *node, struct clock_message_t *message) {
    int value;

    (*message).command = parseAsRtcCommand(node, "command");

    printf("rtc (read):\ttype: %d ---------------------------\n", (*message).command);
}

void readMidiStatic (xmlNode* node, struct midi_static_messages_t *messages) {
    xmlNode* message_element;
    int message_index;
    char typeString[5] = "";

    (*messages).length = (int)xmlChildElementCount(node);
    (*messages).message = malloc((*messages).length * sizeof(union static_message_t));
    (*messages).type = malloc((*messages).length * sizeof(unsigned char));

    message_element = node->children;
    message_index = 0;
    while (message_element != NULL && message_index < (*messages).length) {
        if (message_element->type == XML_ELEMENT_NODE) {
            if (xmlGetProp(message_element, "type")) {
                sscanf(xmlGetProp(message_element, "type"), "%s", typeString);
            }
            if (!strcmp(typeString, "cc")) {
                readCCMessage(message_element, &(*messages).message[message_index].cc);
                (*messages).type[message_index] = MIDI_MESSAGE_TYPE_CC;
            } else if (!strcmp(typeString, "clock") || !strcmp(typeString, "rtc")) {
                readRtcMessage(message_element, &(*messages).message[message_index].clock);
                (*messages).type[message_index] = MIDI_MESSAGE_TYPE_RTC;
                printf("------------------ LESTE ÉN!\n\n");
            }

            message_index++;
       }
       message_element = message_element->next;
    }
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

void readChords (xmlNode *chordsNode, unsigned char *count, struct chord_t **chords);

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

void readName (xmlNode* node, char name[MAX_NAME_LENGTH]) {
    if (xmlGetProp(node, "name")) {
        sscanf(xmlGetProp(node, "name"), "%[^\t\n]", &(*name));
    }
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
    if (xmlGetProp(sequenceNode, "reset_to")) {
        sscanf(xmlGetProp(sequenceNode, "reset_to"), "%u", &(*sequence).reset_to);
    } else {
        (*sequence).reset_to = 0;
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


void initializeEmptyMidiConfiguration (struct midi_configuration_t *midiConfiguration) {
    (*midiConfiguration).default_channel = 0;
    (*midiConfiguration).program_change_count = 0;
}

void printScaledMessageList (struct scaled_message_t *message) {
    printf("type: %d,\tmin: %d,\tmax: %d,\tcc: %d,\tcc_lsb: %d,\tin_min: %d,\tin_max: %d\n",
        message->type, message->out.min, message->out.max, message->cc, message->cc_lsb, message->in.min, message->in.max);
}

void readBank (xmlNode* node, struct bank_t* bank) {
    (*bank).whammy_length = 0;
    (*bank).touchbar_length = 0;
    (*bank).selectable = 0;
    (*bank).chord_count = 0;
    (*bank).sequence_count = 0;
    (*bank).number_of_counters = 0;
    (*bank).midi_static_messages = malloc(sizeof(struct midi_static_messages_t));

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
            if (!strcmp(bank_content->name, "midi_static")) {
                readMidiStatic(bank_content, bank->midi_static_messages);
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

void initializeStickTargetStates (unsigned int count, struct stick_target_state_t **stick_target_states) {
    if (count > 0) {
        *stick_target_states = malloc(count * sizeof(struct stick_target_state_t));
    }
    for (int i = 0; i < count; i++) {
        (*stick_target_states)->last_sent_value = 0;
    }
}

int readPatchFromFile (const char *file) {

    initializeEmptyMidiConfiguration(&patch.midi);
    patch.midi_static_messages = malloc(sizeof(struct midi_static_messages_t));
    patch.midi_static_messages->length = 0;
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
                if (!strcmp(cur->name, "midi_static")) {
                    readMidiStatic(cur, patch.midi_static_messages);
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
