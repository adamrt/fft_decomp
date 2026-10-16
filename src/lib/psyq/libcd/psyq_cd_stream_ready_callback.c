#include "psx/libcd.h"
/* Ready callback CdRead2 installs for streaming reads: pass each data-ready interrupt to
 * StCdInterrupt. */
void psyq_cd_stream_ready_callback(u8 unused_event, u8* unused_result) {
    StCdInterrupt();
}
