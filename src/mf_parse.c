#include <mf_parse.h>

#include <mf_construct.h>
#include <mf_util.h>

/* so many stds. just like mewgenics! */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* some of these functions probably need adjustments for big-endian systems */

/* check if the first four bytes of the file contains the 'MThd' marker */
static bool mf_check_magic(mf_midi_file *file) {
    /* 4d 54 68 64 = "MThd" */
    /* offset = 0 bytes (read 4 bytes) */
    return ((uint32_t *)(file->buffer))[0] == 0x6468544d;
}

/* check if 'header length' field in the file is equal to 6 */
static bool mf_check_header_length(mf_midi_file *file) {
    /* checking a few sources, the length of the header is always 6 bytes.. */
    /* could bswap here but it'd be faster to compare the value as-is */
    /* offset = 4 bytes (read 4 bytes) */
    return ((uint32_t *)(file->buffer))[1] == 0x06000000;
}

/* gets the format id from the file */
static uint8_t mf_get_format(mf_midi_file *file) {
    /* the format field is actually two bytes, but only 2 bits of this field
     * is actually used. just so we don't have to swap endianness, let's read
     * only the first byte */
    /* offset = 9 bytes (read 1 byte) */
    return ((uint8_t *)(file->buffer))[9];
}

/* gets the number of tracks in the file */
static uint16_t mf_get_nr_tracks(mf_midi_file *file) {
    /* offset = 9 bytes (read 2 bytes) */
    uint16_t val = *((uint16_t *)(file->buffer + 10));
    val = swap_16(val);
    return val;
}

/* gets the time division value (pulses per quarter) from the file */
static uint16_t mf_get_division(mf_midi_file *file) {
    /* offset = 12 bytes (read 2 bytes) */
    uint16_t val = ((uint16_t *)(file->buffer))[6];
    val = swap_16(val);
    return val;
}

bool mf_check_and_read_header(mf_midi_file *file) {
    if (!mf_check_magic(file)) {
        fprintf(stderr, "magic number in header doesn't match..\n");
        return false;
    }

    if (!mf_check_header_length(file)) {
        fprintf(stderr, "header length isn't 6..\n");
        return false;
    }

    file->format = mf_get_format(file);
    if (file->format > 1) {
        fprintf(stderr, "format ids > 1 not supported!\n");
        return false;
    }
    file->nr_tracks = mf_get_nr_tracks(file);
    file->time_division = mf_get_division(file);

    file->buffer_offset = 14;

    return true;
}

/* reads a variable-length integer from the file (at file.buffer_offset)
 *
 * loosely based on fluid_midi_file_read_varlen from fluidsynth */
static bool mf_read_vlq(mf_midi_file *file) {
    file->_read_vlq = 0;

    int i = 0;
    uint8_t val;

    for (;; i++) {
        if (i == 4) return false;

        val = *((uint8_t *)(file->buffer + file->buffer_offset));
        if (val & 0x80) {
            file->_read_vlq |= (uint32_t)(val & 0x7f);
            file->_read_vlq <<= 7;
            file->buffer_offset++;
        } else {
            file->_read_vlq += val;
            file->buffer_offset++;
            break;
        }
    }

    return true;
}

/* reads meta event data from the file (at file.buffer_offset) */
static bool mf_read_meta_event_data(mf_midi_file *file, mf_midi_event *event) {
    event->data_meta.type =
        *((uint8_t *)(file->buffer + file->buffer_offset));

    file->buffer_offset++;

    if (!mf_read_vlq(file)) {
        fprintf(stderr, "invalid vlq read\n");
        return false;
    }
    event->data_meta.length = file->_read_vlq;

    if (file->_read_vlq > 0) {
        uint8_t *data = malloc(event->data_meta.length);
        if (data == NULL) {
            fprintf(stderr, "meta event data allocation failure\n");
            return false;
        }
        memcpy(data, ((uint8_t *)(file->buffer + file->buffer_offset)),
               event->data_meta.length);
        event->data_meta.data = data;
    } else {
        event->data_meta.data = NULL;
    }

    file->buffer_offset += event->data_meta.length;

    return true;
}

/* reads basic event data (1 or 2 1-byte parameters) from the file
 * (at file.buffer_offset) */
static bool mf_read_basic_event_data(mf_midi_file *file, mf_midi_event *event) {
    if (event->status >= 0x80 && event->status <= 0xef) {
        event->data_basic.parm1 =
            *((uint8_t *)(file->buffer + file->buffer_offset++));
        /* program changes only have 1 data byte */
        if (!(event->status >= 0xc0 && event->status <= 0xcf))
        {
            event->data_basic.parm2 =
                *((uint8_t *)(file->buffer + file->buffer_offset++));
        }
        else event->data_basic.parm2 = 0;
    } else {
        fprintf(stderr, "%02x isn't a basic event..\n", event->status);
        return false;
    }

    return true;
}

/* reads event data from the file (populates parameters accordingly) */
static bool mf_read_event(mf_midi_file *file, mf_midi_event *event) {
    if (!mf_read_vlq(file)) {
        fprintf(stderr, "invalid vlq read\n");
        return false;
    }
    event->delta = file->_read_vlq;

    uint8_t first_byte = *((uint8_t *)(file->buffer + file->buffer_offset));
    if (first_byte >= 0x80) {
        event->status = first_byte;
        file->_running_status = event->status;
        file->buffer_offset++;
    } else {
        if (file->_running_status < 0x80) {
            fprintf(stderr, "running status buffer is not a status byte? "
                            "(%02x)\n", file->_running_status);
        }
        event->status = file->_running_status;
        /* we've landed on event data. we shouldn't increment the buffer
         * offset at this point */
    }

    if (event->status == 0xff) {
        return mf_read_meta_event_data(file, event);
    } else if (event->status >= 0x80) {
        return mf_read_basic_event_data(file, event);
    } else {
        fprintf(stderr, "we're at an unexpected point in the file!!\n");
        fprintf(stderr, "buffer_offset: %zu\n", file->buffer_offset);
        return false;
    }

    return true;
}

bool mf_read_next_track(mf_midi_file *file, mf_midi_track *track) {
    track->byte_length = 0;
    track->nr_events = 0;
    track->events = NULL;

    uint32_t magic = *((uint32_t *)(file->buffer + file->buffer_offset));
    if (magic != 0x6b72544d) {
        fprintf(stderr, "invalid track magic..\n");
        return false;
    }
    file->buffer_offset += 4;

    uint32_t byte_length = *((uint32_t *)(file->buffer + file->buffer_offset));
    byte_length = swap_32(byte_length);
    track->byte_length = byte_length;
    file->buffer_offset += 4;

    size_t last_offset = file->buffer_offset;

    track->events = mf_create_event_linked_list();
    if (track->events == NULL) {
        fprintf(stderr, "couldn't allocate list for track events\n");
        return false;
    }

    mf_midi_event_linked_list *e = track->events;
    while (1) {
        if (!mf_read_event(file, &e->cur_event)) {
            fprintf(stderr, "mf_read_event failed, see above\n");
            goto event_read_fail;
        }
        track->nr_events++;

        /* end of track */
        if (e->cur_event.status == 0xff &&
            e->cur_event.data_meta.type == 0x2f) {
            break;
        }

        if (file->buffer_offset - last_offset >= track->byte_length) {
            fprintf(stderr, "unexpectedly reached past end of track!! (o: %zu)"
                            "\n", file->buffer_offset);
            goto event_read_fail;
        }

        e->next_event = mf_create_event_linked_list();
        if (e->next_event == NULL) {
            fprintf(stderr, "next_event allocation failure\n");
            goto event_read_fail;
        }
        e = e->next_event;
    }

    return true;

event_read_fail:
    fprintf(stderr, "event read fail..\n");
    mf_free_track(track);
    return false;
}

bool mf_read_all_tracks(mf_midi_file *file) {
    if (file->tracks == NULL) {
        file->tracks = malloc(sizeof(mf_midi_track) * file->nr_tracks);
        if (file->tracks == NULL) return false;
    }

    for (uint16_t i = 0; i < file->nr_tracks; i++) {
        if (!mf_read_next_track(file, &file->tracks[i])) {
            return false;
        }
    }

    return true;
}