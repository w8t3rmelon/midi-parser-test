#ifndef _MF_CONSTRUCT_H
#define _MF_CONSTRUCT_H

#include <stdio.h>
#include <stdlib.h>

#include "mf_structs.h"

/* frees a track and all the events within */
void mf_free_track(mf_midi_track *track);
/* initialises a linked list of midi events */
mf_midi_event_linked_list *mf_create_event_linked_list();
/* creates a midi file object from a memory buffer */
mf_midi_file *mf_create_from_buf(void *buf, size_t bufSize);
/* creates a midi file object from a file */
mf_midi_file *mf_create_from_file(const char *fileName);
/* frees a midi file object (if you used mf_create_from_buf, you must free
 * the buffer yourself afterwards!) */
void mf_free(mf_midi_file *file);

#endif /* MF_CONSTRUCT_H */
