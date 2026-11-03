#ifndef _MF_PARSE_H
#define _MF_PARSE_H

#include <stdbool.h>

#include "mf_structs.h"

/* validates some header fields and reads them to the file struct */
bool mf_check_and_read_header(mf_midi_file *file);

/* reads a track and its events from the file */
bool mf_read_next_track(mf_midi_file *file, mf_midi_track *track);

/* reads all tracks and their events from the file */
bool mf_read_all_tracks(mf_midi_file *file);

#endif // _MF_PARSE_H
