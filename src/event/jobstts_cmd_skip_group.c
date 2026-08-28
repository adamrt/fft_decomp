#include "psx/types.h"

const u8* jobstts_cmd_skip_group(const u8* data) {
    s32 count;

    if (data[0] == 1) {
        count = data[4] + 1;
    } else if (data[0] == 2) {
        count = data[4] + data[5] + 1;
    } else {
        count = 1;
    }

    while (--count != -1) {
        data += data[1];
    }
    return data;
}
