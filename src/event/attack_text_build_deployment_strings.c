#include "fft/attack.h"
#include "fft/battle_text.h"
#include "fft/data.h"
#include "fft/main_file.h"
#include "fft/main_heap.h"
#include "fft/main_unit.h"
#include "fft/thread.h"

/* Appends one 0xfe/0xff-terminated string. Overflowing the arena jumps
 * straight to the cleanup and skips the help-text copy. */
#define APPEND_TEXT(text_id)                                                                                           \
    src = battle_text_init_entry(text_id);                                                                             \
    *dst++ = *src++;                                                                                                   \
    while ((dst[-1] & 0xfe) != 0xfe) {                                                                                 \
        if (dst >= g_attack_scenario_table + 0x4000) {                                                                 \
            goto end;                                                                                                  \
        }                                                                                                              \
        *dst++ = *src++;                                                                                               \
    }

/* An id of 0 or with all eleven low bits set stores an empty string. */
#define APPEND_TEXT_ID(id, bank)                                                                                       \
    if ((id) == 0 || ((id) & 0x7ff) == 0x7ff) {                                                                        \
        *dst++ = 0xfe;                                                                                                 \
    } else {                                                                                                           \
        APPEND_TEXT((id) + (bank));                                                                                    \
    }

/*
 * Builds the deployment screen's per-roster-unit text strings.
 *
 * Loads the text file, then for each of the 16 roster slots loads the unit
 * into the editor and appends its 11 strings (five item names, five
 * ability or skillset names, the job name); an empty or unavailable slot
 * gets 11 empty strings. Finally publishes the two text pointers and copies
 * the 0x1400-byte help text.
 */
void attack_text_build_deployment_strings(void) {
    u8* dst;
    void* file;
    s32 i;
    s32 j;
    u8* src;
    s32 k;
    party_data_t* unit;

    dst = g_attack_scenario_table;
    file = main_heap_alloc(0x18000);
    g_battle_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    battle_thread_call_on_main_stack(0x1a3a, 0x16800, file);
    battle_text_save_pointer_table();
    attack_text_init_battle_pointers(file);
    for (i = 0; i < 0x10; i++) {
        unit = main_party_get_data_pointer(i);
        if (unit->party_id == 0xff || i >= 0x10 || unit->proposition_status != 0) {
            for (k = 0; k < 11; k++) {
                *dst++ = 0xfe;
            }
            continue;
        }
        attack_load_party_unit_into_editor(1, i);
        for (j = 0; j < 5; j++) {
            APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j], 0x6800);
        }
        if (((status_panel_slot_state_t*)g_attack_unit_editor_state)->generic_monster != 0) {
            for (j = 0; j < 5; j++) {
                APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j + 5], 0x7800);
            }
        } else {
            for (j = 0; j < 2; j++) {
                APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j + 5], 0x9800);
            }
            for (j = 2; j < 5; j++) {
                APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j + 5], 0x7800);
            }
        }
        APPEND_TEXT(g_attack_editor_job_id + 0x6000);
    }
    g_attack_deploy_unit_text_section = g_attack_scenario_table;
    g_attack_deploy_help_text_section = g_attack_event_condition_blocks;
    src = battle_text_init_entry(0x5800);
    dst = g_attack_event_condition_blocks;
    for (i = 0; i < 0x1400; i++) {
        *dst++ = *src++;
    }
end:
    battle_text_restore_pointer_table();
    main_heap_call_free(file);
}
