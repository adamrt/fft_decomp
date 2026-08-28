#include "psx/types.h"

/* Zero the HP/MP bonus halfwords of an item stat summary and the stat block of its detail record. */
void equip_item_clear_stat_sums(s16* summary, s16* detail) {
    summary[7] = 0;
    summary[10] = 0;
    detail[0] = 0;
    detail[1] = 0;
    detail[2] = 0;
    detail[3] = 0;
    detail[4] = 0;
    detail[6] = 0;
    detail[5] = 0;
    detail[9] = 0;
    detail[10] = 0;
    detail[11] = 0;
    detail[12] = 0;
    detail[14] = 0;
    detail[15] = 0;
    detail[16] = 0;
    detail[17] = 0;
}
