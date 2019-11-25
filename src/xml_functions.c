#include <stddef.h>

#include <libxml2/libxml/encoding.h>
#include <libxml2/libxml/xmlwriter.h>

#include "xml_functions.h"
#include "lou.h"

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
    xmlNode *root_element, *chord_element, *note_element, *cur = NULL;
		int chord_index, note_index, number_of_notes, note_number, velocity, delay = 0;

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
      printf ("%s\n", cur->name);
			if (xmlGetProp(cur, "midi_channel")) {
				sscanf(xmlGetProp(cur, "midi_channel"), "%d", &midi_channel);
			}
      cur = cur->children;
			while (cur->type != XML_ELEMENT_NODE) { 
				cur = cur->next;
			}
			 if (!strcmp(cur->name, "chords")) {
		    printf ("%s\n", cur->name);
				chord_element = cur->children;
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

						chord[chord_index].size = number_of_notes;
						chord[chord_index].note = malloc(chord[chord_index].size * sizeof(chord->note));

						note_element = chord_element->children;
						note_index = 0;
						while (note_element != NULL && note_index < number_of_notes) {
							if (note_element->type == XML_ELEMENT_NODE) {
								note_number = 0;
								velocity = 0;
								delay = 0;
	
								if (xmlGetProp(note_element, "note_number")) {
									sscanf(xmlGetProp(note_element, "note_number"), "%d", &note_number);
									printf("note: %d\t", note_number);
								}
								chord[chord_index].note[note_index].note_number = note_number;

								if (xmlGetProp(note_element, "velocity")) {
									sscanf(xmlGetProp(note_element, "velocity"), "%d", &velocity);
									printf("velocity: %d\t", velocity);
								}
								chord[chord_index].note[note_index].velocity = velocity;
						
								if (xmlGetProp(note_element, "delay")) {
									sscanf(xmlGetProp(note_element, "delay"), "%d", &delay);
								}
								chord[chord_index].note[note_index].delay = delay;

								printf("delay: %d\n", delay);
								note_index++;
							}
							note_element = note_element->next;
						}
					}
					chord_element = chord_element->next;
				}
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

