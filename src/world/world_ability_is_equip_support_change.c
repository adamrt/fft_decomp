#include "fft/world.h"
#include "psx/types.h"

/* Return whether a support change requires equipment revalidation.
 *
 * The preview record at index 20 is excluded. With Max Equip at Job Change
 * enabled, equipment-type supports, Martial Arts, Two Hands and Two Swords
 * can invalidate the current loadout. */
s32 world_ability_is_equip_support_change(s16 unit_id, s16 ability_id) {
    if (unit_id == 20 || g_main_game_options.fields.max_equip_at_job_change != GAME_OPTION_ON)
        return 0;
    if ((u32)((u16)(ability_id - ABILITY_ID_SUPPORT_EQUIP_ARMOR)) < 8U)
        return 1;
    if (ability_id == ABILITY_ID_SUPPORT_MARTIAL_ARTS || ability_id == ABILITY_ID_SUPPORT_TWO_HANDS)
        return 1;
    return ability_id == ABILITY_ID_SUPPORT_TWO_SWORDS;
}
