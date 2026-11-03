#include <mf_construct.h>
#include <mf_parse.h>

int main() {
    mf_midi_file *file = mf_create_from_file("test.mid");
    if (file == NULL) {
        return 1;
    }

    if (!mf_check_and_read_header(file)) {
        mf_free(file);
        return 1;
    }

    if (!mf_read_all_tracks(file)) {
        mf_free(file);
        return 1;
    }

    mf_free(file);

    return 0;
}
