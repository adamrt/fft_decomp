#include "psx/cd.h"
#include "psx/types.h"

void open_movie_start_cd_stream_read(const void* location) {
    for (;;) {
        while (CdControl(2, (u8*)location, 0) == 0) { }
        if (CdRead2(0x1a0) != 0) {
            break;
        }
    }
}
