#ifndef _MF_STRUCTS_H
#define _MF_STRUCTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    void            *buffer;
    size_t          buffer_size;
    size_t          buffer_offset;

    uint8_t         format;
    uint16_t        nr_tracks;
    uint16_t        time_division;

    mf_midi_track   *tracks;

    bool            _buf_is_managed;
    uint8_t         _running_status;
    uint32_t        _read_vlq;
};

#endif /* _MF_STRUCTS_H */
