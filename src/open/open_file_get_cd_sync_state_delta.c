#include "psx/cd.h"
#include "psx/types.h"

s32 open_file_get_cd_sync_state_delta(void) {
    s32 status;

    status = CdSync(1, 0);
    if (status == 2) {
        return 1;
    }

    return -(status == 5);
}

/* The OPEN state machines add this result to their current state. */
/* CD completion advances it; a disk error moves it back one state. */
