#include <stdio.h>

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct _mf_midi_event_data_meta mf_midi_event_data_meta;
struct _mf_midi_event_data_meta {
    uint8_t     type;
    uint32_t    length;
    uint8_t     *data;
};

typedef struct _mf_midi_event_data_basic mf_midi_event_data_basic;
struct _mf_midi_event_data_basic {
    uint8_t     parm1;
    uint8_t     parm2;
};

typedef struct _mf_midi_event mf_midi_event;
struct _mf_midi_event {
    uint32_t    delta;
    uint8_t     status;

    mf_midi_event_data_basic    data_basic;
    mf_midi_event_data_meta     data_meta;
};

typedef struct _mf_midi_event_linked_list mf_midi_event_linked_list;
struct _mf_midi_event_linked_list {
    mf_midi_event             cur_event;
    mf_midi_event_linked_list *next_event;
};

typedef struct _mf_midi_track mf_midi_track;
struct _mf_midi_track {
    uint32_t                    byte_length;
    uint64_t                    nr_events;
    mf_midi_event_linked_list   *events;
};

typedef struct _mf_midi_file mf_midi_file;
struct _mf_midi_file {
    void        *buffer;
    size_t      buffer_size;
    size_t      buffer_offset;

    bool        _buf_is_managed;
    uint8_t     _running_status;
    uint32_t    _read_vlq;
};

#pragma region creation / freeing
mf_midi_file *mf_create_from_buf(void *buf, size_t bufSize) {
    mf_midi_file *mf = malloc(sizeof(mf_midi_file));
    if (mf == NULL) return NULL;
    mf->buffer = buf;
    mf->buffer_size = bufSize;
    mf->buffer_offset = 0;
    mf->_buf_is_managed = false;
    mf->_running_status = 0;
    mf->_read_vlq = 0;
    return mf;
}

mf_midi_file *mf_create_from_file(const char *fileName) {
    mf_midi_file *mf = malloc(sizeof(mf_midi_file));
    if (mf == NULL) return NULL;
    FILE *file = fopen(fileName, "rb");
    if (file == NULL) {
        free(mf);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    int fileSize = ftell(file);

    mf->buffer = malloc(fileSize);
    if (mf->buffer == NULL) {
        free(mf);
        fclose(file);
        return NULL;
    }

    mf->buffer_offset = 0;
    mf->_read_vlq = 0;
    mf->_running_status = 0;
    mf->_buf_is_managed = true;
    mf->buffer_size = fileSize;

    fseek(file, 0, SEEK_SET);
    if (fread(mf->buffer, fileSize, 1, file) != 1) {
        free(mf->buffer);
        free(mf);
        fclose(file);
        return NULL;
    }

    fclose(file);

    return mf;
}

void mf_free(mf_midi_file *file) {
    if (file == NULL) return;
    if (file->_buf_is_managed) free(file->buffer);

    free(file);
}
#pragma endregion

#pragma region utility functions
uint16_t swap_16(uint16_t i) {
    uint16_t val = i;
    val = ((val & 0xff00) >> 8) | ((val & 0x00ff) << 8);
    return val;
}

uint32_t swap_32(uint32_t i) {
    uint32_t val = i;

    val = ((val & 0xff) << 24) | ((val & 0xff00) << 8) |
          ((val & 0xff0000) >> 8) | ((val & 0xff000000) >> 24);

    return val;
}

uint64_t swap_64(uint64_t i) {
    uint64_t val = i;

    val = ((val & 0x00000000000000ffULL) << 56) |
          ((val & 0xff00000000000000ULL) >> 56) |

          ((val & 0x000000000000ff00ULL) << 40) |
          ((val & 0x00ff000000000000ULL) >> 40) |

          ((val & 0x0000000000ff0000ULL) << 24) |
          ((val & 0x0000ff0000000000ULL) >> 24) |

          ((val & 0x00000000ff000000ULL) <<  8) |
          ((val & 0x000000ff00000000ULL) >> 8);

    return val;
}
#pragma endregion

/* some of these functions probably need adjustments for big-endian systems */

bool mf_check_magic(mf_midi_file *file) {
    /* 4d 54 68 64 = "MThd" */
    /* offset = 0 bytes (read 4 bytes) */
    return ((uint32_t *)(file->buffer))[0] == 0x6468544d;
}

bool mf_check_header_length(mf_midi_file *file) {
    /* checking a few sources, the length of the header is always 6 bytes.. */
    /* could bswap here but it'd be faster to compare the value as-is */
    /* offset = 4 bytes (read 4 bytes) */
    return ((uint32_t *)(file->buffer))[1] == 0x06000000;
}

uint8_t mf_get_format(mf_midi_file *file) {
    /* the format field is actually two bytes, but only 2 bits of this field
     * is actually used. just so we don't have to swap endianness, let's read
     * only the first byte */
    /* offset = 9 bytes (read 1 byte) */
    return ((uint8_t *)(file->buffer))[9];
}

uint16_t mf_get_tracks(mf_midi_file *file) {
    /* offset = 9 bytes (read 2 bytes) */
    uint16_t val = *((uint16_t *)(file->buffer + 10));
    val = swap_16(val);
    return val;
}

uint16_t mf_get_division(mf_midi_file *file) {
    /* offset = 12 bytes (read 2 bytes) */
    uint16_t val = ((uint16_t *)(file->buffer))[6];
    val = swap_16(val);
    return val;
}

void mf_header_read_finish(mf_midi_file *file) {
    file->buffer_offset = 14;
}

/* loosely based on fluid_midi_file_read_varlen from fluidsynth */
bool mf_read_vlq(mf_midi_file *file) {
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

bool mf_read_meta_event_data(mf_midi_file *file, mf_midi_event *event) {
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

bool mf_read_basic_event_data(mf_midi_file *file, mf_midi_event *event) {
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

bool mf_read_event(mf_midi_file *file, mf_midi_event *event) {
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

mf_midi_event_linked_list *mf_create_event_linked_list() {
    mf_midi_event_linked_list *list = malloc(sizeof(mf_midi_event_linked_list));
    if (list == NULL) return NULL;
    list->cur_event.delta = 0;
    list->cur_event.status = 0;
    list->cur_event.data_basic.parm1 = 0;
    list->cur_event.data_basic.parm2 = 0;
    list->cur_event.data_meta.type = 0;
    list->cur_event.data_meta.length = 0;
    list->cur_event.data_meta.data = NULL;
    list->next_event = NULL;

    return list;
}

void mf_free_track(mf_midi_track *track) {
    mf_midi_event_linked_list *tmp = NULL;
    mf_midi_event_linked_list *cur = track->events;

    while (cur != NULL) {
        tmp = cur;
        cur = cur->next_event;
        if (tmp->cur_event.status == 0xff &&
            tmp->cur_event.data_meta.data != NULL)
            free(tmp->cur_event.data_meta.data);
        free(tmp);
    }
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

int main() {
    mf_midi_file *file = mf_create_from_file("test.mid");
    if (file == NULL) {
        return 1;
    }

    //printf("%d\n", mf_check_magic(file));
    //printf("%d\n", mf_check_header_length(file));
    //printf("%d\n", mf_get_format(file));
    //printf("%d\n", mf_get_tracks(file));
    //printf("%d\n", mf_get_division(file));
    mf_header_read_finish(file);

    mf_midi_track test_track;

    for (int i = 0; i < mf_get_tracks(file); i++) {
        printf("-----TRACK%02d-----\n", i);
        if (!mf_read_next_track(file, &test_track)) {
            mf_free(file);
            return 1;
        }

        printf("%d | %zu events\n", test_track.byte_length,
               test_track.nr_events);
        mf_free_track(&test_track);
    }

    mf_free(file);

    return 0;
}
