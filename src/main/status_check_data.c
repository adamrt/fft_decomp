/* Status masks and item-location flags prepared before a fresh session. */

#include "fft/item.h"
#include "fft/memory.h"
#include "fft/status.h"
#include "fft/unit.h"

/* At boot, clear the item location flags and build g_status_check_sets: rows 0 to 9
 * from each status's effect flags, and the unmountable row from a fixed status list. */
void status_check_data_init(void) {
    int status;
    u8 flags4;
    u8 flags5;
    int byte;
    u8 mask;

    mem_zero(g_item_location_flags, sizeof(g_item_location_flags));
    mem_zero(g_status_check_sets, sizeof(g_status_check_sets));
    for (status = 0; status < STATUS_COUNT; status++) {
        flags4 = g_status_effect_data[status].flags_1;
        flags5 = g_status_effect_data[status].flags_2;
        byte = status / 8;
        mask = 0x80 >> (status & 7);
        if (flags4 & STATUS_EFFECT_FLAG_1_KO)
            g_status_check_sets[STATUS_CHECK_SET_KO][byte] |= mask;
        if (flags4 & STATUS_EFFECT_FLAG_1_UNIT_LOST)
            g_status_check_sets[STATUS_CHECK_SET_UNIT_LOST][byte] |= mask;
        if (flags4 & STATUS_EFFECT_FLAG_1_CHECK_SET_2)
            g_status_check_sets[STATUS_CHECK_SET_UNREAD_2][byte] |= mask;
        if (flags4 & STATUS_EFFECT_FLAG_1_FREEZE_CT)
            g_status_check_sets[STATUS_CHECK_SET_FREEZE_CT][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_PREVENT_REACTION)
            g_status_check_sets[STATUS_CHECK_SET_PREVENT_REACTION][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_IMMORTAL_IMMUNITY)
            g_status_check_sets[STATUS_CHECK_SET_IMMORTAL_IMMUNITY][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_FORMATION_IMMUNITY)
            g_status_check_sets[STATUS_CHECK_SET_FORMATION_IMMUNITY][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_CHECK_SET_7)
            g_status_check_sets[STATUS_CHECK_SET_UNREAD_7][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_SCRIPT_FILTER_EXCLUDED)
            g_status_check_sets[STATUS_CHECK_SET_SCRIPT_FILTER_EXCLUDED][byte] |= mask;
        if (flags5 & STATUS_EFFECT_FLAG_2_MOUNT_REMOVAL)
            g_status_check_sets[STATUS_CHECK_SET_MOUNT_REMOVAL][byte] |= mask;
    }
    g_status_check_sets[STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_CRYSTAL)]
        = STATUS_BYTE_MASK(STATUS_ID_CRYSTAL) | STATUS_BYTE_MASK(STATUS_ID_DEAD);
    g_status_check_sets[STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_PETRIFY)]
        = STATUS_BYTE_MASK(STATUS_ID_PETRIFY) | STATUS_BYTE_MASK(STATUS_ID_BLOOD_SUCK)
        | STATUS_BYTE_MASK(STATUS_ID_TREASURE);
    g_status_check_sets[STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_BERSERK)]
        = STATUS_BYTE_MASK(STATUS_ID_BERSERK) | STATUS_BYTE_MASK(STATUS_ID_CHICKEN) | STATUS_BYTE_MASK(STATUS_ID_FROG);
    g_status_check_sets[STATUS_CHECK_SET_UNMOUNTABLE][STATUS_BYTE_INDEX(STATUS_ID_CHARM)]
        = STATUS_BYTE_MASK(STATUS_ID_CHARM);
}
