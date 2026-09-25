#include "fft/event_require.h"
#include "psx/types.h"

s32 require_party_find_join_candidates(void) {
    s32 prompt_count = 0;
    s32 party_index = 0;
    s32 absent = 0xff;
    s32 found_index;
    s32 i;

    g_require_party_affected_unit_count = 0;
    do {
        party_data_t* party_unit = main_party_get_data_pointer(party_index);
        i = 0;

        if (party_unit->party_id != absent) {
            battle_stats_t* unit;

            do {
                unit = battle_unit_get_stats_from_battle_id(i);
                if (unit->formation_index == party_index && unit->existence != absent && unit->unit_id != 0)
                    break;
                i++;
            } while (i < BATTLE_UNIT_SLOT_COUNT);

            if (i != BATTLE_UNIT_ID_NONE) {
                unit = battle_unit_get_stats_from_battle_id(i);
                unit = battle_find_unit_data_pointer_for_entd_unit_id(unit->unit_id, &found_index);
                if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED || unit->existence == absent
                    || found_index == -3) {
                    if (unit->unit_id == 0 || unit->unit_id >= 4) {
                        if (!(unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_TREASURE)))
                            require_party_store_equipment_or_sell_overflow(i);
                        main_party_remove_unit(party_index);
                        require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xf);
                    }
                } else if (unit->character_identity != 0 && unit->character_identity < 4) {
                    main_party_save_unit(unit, 0);
                } else if (!(unit->unit_flags & UNIT_FLAG_SAVE_FORMATION)
                    && unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
                    main_party_remove_unit(unit->formation_index);
                    unit->formation_index = absent;
                } else {
                    main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
        party_index++;
    } while (party_index < PARTY_ROSTER_SLOT_COUNT);

    party_index = 0;
    absent = (s32)g_require_gfx_formation_sprite_groups;
    do {
        battle_stats_t* unit;
        battle_stats_t* battle_unit;

        battle_unit = battle_unit_get_stats_from_battle_id(party_index);
        unit = battle_find_unit_data_pointer_for_entd_unit_id(battle_unit->unit_id, &found_index);
        if (found_index >= 0 && party_index == found_index && unit->formation_index == BATTLE_FORMATION_INDEX_NONE
            && unit->unit_id != 0) {
            u32 base_class = unit->character_identity;
            if ((base_class == 0 || base_class >= 4) && unit->unit_id >= 4
                && ((unit->unit_flags & UNIT_FLAG_JOIN_AFTER_EVENT)
                    || (unit->status_sets.current[1] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_INVITE)))) {
                if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
                    s32 sprite;
                    u8* group;

                    i = 0;
                    sprite = base_class;
                    group = (u8*)absent;

                    for (;;) {
                        s32 column = 0;
                        u8* entry = group;
                        for (; column < 3; column++) {
                            if (sprite == *entry)
                                break;
                            entry++;
                        }
                        {
                            /* Pin: keeps the 3 in the loop; unpinned GCC hoists it out of the search. */
                            register s32 three __asm__("$2") = 3;
                            /* Skips the exhausted-search `li` below; a break lands on it. */
                            if (column != three)
                                goto reset_formation_row;
                        }
                        {
                            /* Emits the target's dead `li v0,0x49` in the bne delay slot above. */
                            s32 rows = 0x49;
                            __asm__ __volatile__("" : "=r"(rows) : "0"(rows));
                        }
                        i++;
                        if (i >= 0x49)
                            break;
                        group += 3;
                    }
                    {
                        /* Emits the target's dead `li v0,0x49` on the exhausted-search exit. */
                        s32 rows = 0x49;
                        __asm__ __volatile__("" : "=r"(rows) : "0"(rows));
                    }
                reset_formation_row:
                    i = 0;
                    /* Keeps the target's otherwise dead `move s1,zero`. */
                    __asm__ __volatile__("" : "=r"(i) : "0"(i));

                    require_party_sell_equipment_over_limit(party_index);
                    main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                } else {
                    s32 count;
                    s32 script_index = EVENT_SCRIPT_VAR_RANDOM_VALUE;
                    s32 script_variant;
                    unit->unit_flags &= ~(UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
                    g_require_party_affected_unit_message_args[g_require_party_affected_unit_count] = unit->unit_id;
                    script_variant = battle_script_get_variable(script_index) & 7;
                    prompt_count++;
                    count = g_require_party_affected_unit_count;
                    g_require_party_affected_unit_message_ids[count]
                        = ((unit->character_identity - 0x80) << 3) + script_variant + 0x1800;
                    g_require_party_affected_battle_unit_indices[count] = party_index;
                    g_require_party_affected_unit_count = count + 1;
                }
            }
        }
        party_index++;
    } while (party_index < BATTLE_UNIT_SLOT_COUNT);
    return prompt_count;
}
