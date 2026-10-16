#include "psx/libgs.h"
#include "psx/libgte.h"

/* Initialize the GTE with a black far color and zero geometry offset, and clear the 2D origin
 * offsets. */
void psyq_gs_init_gte(void) {
    InitGeom();
    SetFarColor(0, 0, 0);
    SetGeomOffset(0, 0);
    GsORGOFSY = 0;
    GsORGOFSX = 0;
}
