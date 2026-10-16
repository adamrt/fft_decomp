/* SCUS_942.21 0x80020f3c..0x80020f93. */
#include "psx/libcd.h"

/* Stop streaming into the ring buffer: remove the data and ready callbacks and clear the CD index
 * and request registers, inside a critical section. */
void StUnSetRing(void) {
    EnterCriticalSection();
    CdDataCallback(0);
    CdReadyCallback(0);
    *g_psyq_cd_unset_index_register = 0;
    *g_psyq_cd_unset_request_register = 0;
    ExitCriticalSection();
}
