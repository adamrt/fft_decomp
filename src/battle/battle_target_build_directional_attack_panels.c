#include "fft/battle_ai.h"
#include "fft/main_heap.h"

/* Provisional 20-byte action copy; same layout as battle_stats_t +0x16e. */
typedef struct battle_target_unit_action_copy {
    u8 actor_id;         /* 0x00 */
    u8 skillset_id;      /* 0x01 */
    s16 ability_id;      /* 0x02 */
    u16 calculator_type; /* 0x04 */
    u16 calculator_multiplier;
    u16 used_item;        /* 0x08 */
    u8 reaction_id;       /* 0x0a */
    u8 target_id;         /* 0x0b */
    s16 target_x;         /* 0x0c */
    s16 target_elevation; /* 0x0e */
    s16 target_y;         /* 0x10 */
    u8 _pad12[2];
} battle_target_unit_action_copy_t;

void battle_target_build_directional_attack_panels(const void* source, u8 mode) {
    battle_target_unit_action_copy_t action;
    battle_stats_t* unit;
    battle_target_panel_t* panel;
    s32 target_x;
    s32 target_y;
    s32 x;
    s32 y;
    s32 i;

    main_util_copy_action_data(source, &action);
    /* Barrier: without it `mode` and `target_x` swap $s2/$s3. */
    __asm__ volatile("");
    target_x = action.target_x;
    target_y = action.target_y;
    g_battle_sort_targets_nearest_first = 1;
    unit = &g_battle_unit_stats[action.actor_id];
    x = unit->x;
    y = unit->position.bits.y;
    if (x < target_x) {
        battle_target_calculate_linear_attack_tiles(0, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(6, x, y);
            battle_target_calculate_linear_attack_tiles(4, x, y);
        }
    }
    if (target_x < x) {
        battle_target_calculate_linear_attack_tiles(2, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(6, x, y);
            battle_target_calculate_linear_attack_tiles(4, x, y);
        }
    }
    if (y < target_y) {
        battle_target_calculate_linear_attack_tiles(6, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(0, x, y);
            battle_target_calculate_linear_attack_tiles(2, x, y);
        }
    }
    if (target_y < y) {
        battle_target_calculate_linear_attack_tiles(4, x, y);
        if (mode == 3) {
            battle_target_calculate_linear_attack_tiles(2, x, y);
            battle_target_calculate_linear_attack_tiles(0, x, y);
        }
    }
    for (i = 0; i < 0x200; i++) {
        panel = &g_battle_target_panels[i];
        if (panel->mark == 0) {
            panel->remaining_range = 0;
        } else {
            panel->mark = 0;
        }
    }
}
