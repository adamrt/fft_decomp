#include "fft/open.h"
#include "psx/libcd.h"
#include "psx/types.h"

/* Poll the pending CD command: 1 when it has completed, -1 on a disc error and 0
 * while it runs. open_script_update_xa_audio adds the result to its step, so
 * completion advances the sequence and an error repeats the previous step. */
s32 open_file_get_cd_sync_state_delta(void) {
    s32 status;

    status = CdSync(1, 0);
    if (status == 2) {
        return 1;
    }

    return -(status == 5);
}
