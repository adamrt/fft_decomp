#include "fft/main.h"

job_data_t* main_job_get_data_pointer(s32 job_id) {
    if (job_id < JOB_ID_COUNT) {
        return &g_job_data[job_id];
    }
    return 0;
}
