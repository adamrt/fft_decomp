#include "fft/job.h"
#include "psx/types.h"

/* Map a generic job id (Squire..Mime) to its 0-based index; anything else is 0.
 *
 * job_id is u16 so the compare and the assignment stay separate subtractions. */
s16 world_job_get_generic_index(u16 job_id) {
    s16 index;

    if ((u16)(job_id - JOB_ID_SQUIRE) < JOB_ID_GENERIC_COUNT) {
        index = job_id;
        index -= JOB_ID_SQUIRE;
    } else {
        index = 0;
    }
    return index;
}
