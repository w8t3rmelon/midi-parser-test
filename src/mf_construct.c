#include <mf_construct.h>

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

mf_midi_file *mf_create_from_buf(void *buf, size_t bufSize) {
    mf_midi_file *mf = malloc(sizeof(mf_midi_file));
    if (mf == NULL) return NULL;
    mf->buffer = buf;
    mf->buffer_size = bufSize;
    mf->buffer_offset = 0;
    mf->_buf_is_managed = false;
    mf->_running_status = 0;
    mf->_read_vlq = 0;
    mf->tracks = NULL;
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
    mf->tracks = NULL;

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
    if (file->tracks != NULL) {
        for (uint16_t i = 0; i < file->nr_tracks; i++) {
            mf_free_track(&file->tracks[i]);
        }
        free(file->tracks);
    }

    free(file);
}