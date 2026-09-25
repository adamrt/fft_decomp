#include "fft/event_require.h"
#include "psx/types.h"

#define MESSAGE_ID(index)   (*(s32*)(g_require_work + 0x72c0 + ((index) << 2)))
#define BATTLE_INDEX(index) (*(s32*)(g_require_work + 0x7388 + ((index) << 2)))

s32 require_party_find_low_brave_high_faith_departures(void) {
    s32 departure_count = 0;
    s32 battle_index = 0;
    u8* sprite_groups = (u8*)g_require_gfx_formation_sprite_groups;
    s32* message_args = g_require_party_affected_unit_message_args;

    g_require_party_affected_unit_count = 0;
    do {
        battle_stats_t* unit = battle_unit_get_stats_from_battle_id(battle_index);
        s32 found_index;
        battle_stats_t* found_unit = battle_find_unit_data_pointer_for_entd_unit_id(unit->unit_id, &found_index);
        u32 sprite;

        if (found_index >= 0 && battle_index == found_index
            && found_unit->formation_index != BATTLE_FORMATION_INDEX_NONE && found_unit->unit_id != 0
            && found_unit->formation_index < PARTY_GUEST_SLOT_FIRST
            && ((sprite = found_unit->character_identity) == CHARACTER_IDENTITY_ENTD_NONE
                || sprite > CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
            && found_unit->unit_id >= 4) {
            s32 brave = found_unit->original_brave;
            s32 faith = found_unit->original_faith;

            if (brave < 6) {
                s32 formation_index = 0;

                if (sprite < 0x80) {
                    s32 current_sprite = sprite;
                    /* Pin: unpinned GCC reassigns the search loop registers. */
                    register u8* row_start __asm__("$5") = sprite_groups;
                    for (;;) {
                        s32 column = 0;
                        u8* row = row_start;
                        for (; column < 3; column++) {
                            if (current_sprite == *row)
                                break;
                            row++;
                        }
                        {
                            /* Pin: keeps the 3 in the loop; unpinned GCC hoists it out of the search. */
                            register s32 three __asm__("$2") = 3;
                            if (column != three)
                                break;
                        }
                        formation_index++;
                        if (formation_index >= 0x49)
                            break;
                        row_start += 3;
                    }
                    if (formation_index == 0x49)
                        formation_index = 0;
                    if (formation_index == 0x47)
                        formation_index = 0x48;
                    {
                        s32 count = g_require_party_affected_unit_count;
                        s32 offset = count << 2;
                        s32 message_id;
                        message_args[count] = current_sprite;
                        message_id = formation_index + 0x1018;
                        MESSAGE_ID(count) = message_id;
                        BATTLE_INDEX(count) = battle_index;
                        g_require_party_affected_unit_count = count + 1;
                    }
                    require_party_update_bio_variable_for_unit_class(found_unit->character_identity, 0xd);
                } else {
                    s32 count = g_require_party_affected_unit_count;
                    s32 offset = count << 2;
                    u32 generic_sprite;
                    message_args[count] = found_unit->unit_id;
                    generic_sprite = found_unit->character_identity;
                    g_require_party_affected_unit_count = count + 1;
                    BATTLE_INDEX(count) = battle_index;
                    MESSAGE_ID(count) = ((generic_sprite - 0x80) << 2) + (battle_index & 3) + 0x1000;
                }
                /* The target passes three arguments the callee does not take. */
                ((void (*)(s32, s32, s32))require_noop_801c43e0)(
                    battle_index, found_unit->unit_id, found_unit->character_identity);
                departure_count++;
            } else if (faith >= 0x5f) {
                s32 formation_index = 0;

                if (sprite < 0x80) {
                    s32 current_sprite = sprite;
                    u8* row_start = sprite_groups;
                    for (;;) {
                        s32 column = 0;
                        u8* row = row_start;
                        for (; column < 3; column++) {
                            if (current_sprite == *row)
                                break;
                            row++;
                        }
                        {
                            /* Pin: keeps the 3 in the loop; unpinned GCC hoists it out of the search. */
                            register s32 three __asm__("$2") = 3;
                            if (column != three)
                                break;
                        }
                        formation_index++;
                        if (formation_index >= 0x49)
                            break;
                        row_start += 3;
                    }
                    if (formation_index == 0x49)
                        formation_index = 0;
                    if (formation_index == 0x47)
                        formation_index = 0x48;
                    {
                        s32 count = g_require_party_affected_unit_count;
                        s32 message_id;
                        s32 unit_id = found_unit->unit_id;
                        message_args[count] = unit_id;
                        message_id = formation_index + 0x1062;
                        MESSAGE_ID(count) = message_id;
                        BATTLE_INDEX(count) = battle_index;
                        g_require_party_affected_unit_count = count + 1;
                    }
                    require_party_update_bio_variable_for_unit_class(found_unit->character_identity, 0xe);
                } else {
                    s32 count = g_require_party_affected_unit_count;
                    u32 generic_sprite;
                    message_args[count] = found_unit->unit_id;
                    generic_sprite = found_unit->character_identity;
                    g_require_party_affected_unit_count = count + 1;
                    BATTLE_INDEX(count) = battle_index;
                    MESSAGE_ID(count) = ((generic_sprite - 0x80) << 2) + (battle_index & 3) + 0x100c;
                }
                departure_count++;
            }
        }
        battle_index++;
    } while (battle_index < BATTLE_UNIT_SLOT_COUNT);
    return departure_count;
}
#undef MESSAGE_ID
#undef BATTLE_INDEX
