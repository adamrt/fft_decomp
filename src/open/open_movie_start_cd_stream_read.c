#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Set the read location and start the streaming read (CdRead2 mode 0x1a0),
 * repeating both until the read is accepted. */
void open_movie_start_cd_stream_read(const CdlLOC* location) {
    for (;;) {
        while (CdControl(CdlSetloc, (u8*)location, 0) == 0) { }
        if (CdRead2(0x1a0) != 0) {
            break;
        }
    }
}
