#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"

/* Builds the effect-slot and secondary-effect free chains, clears their
 * sentinels and work areas, then resets effect rendering and the camera. */
void battle_effect_init_system(void) {
    s32 i;
    s32 j;

    g_battle_effect_slots[0].prev = 0;
    g_battle_effect_slots[0].next = 0;
    g_battle_effect_slot_free_head = 1;
    for (i = 1; i < 32; i++) {
        g_battle_effect_slots[i].next = i + 1;
        g_battle_effect_slots[i].prev = i - 1;
    }
    g_battle_effect_slots[i].next = 0;
    g_battle_effect_slots[i].prev = i - 1;
    g_battle_effect_active_slot_head = 0;
    g_battle_effect_projectile_matrix.t[2] = 0;
    g_battle_effect_projectile_matrix.t[1] = 0;
    g_battle_effect_projectile_matrix.t[0] = 0;
    for (i = 1; i < 16; i++) {
        g_battle_effect_secondary_data[i].own_slot_id = i;
        g_battle_effect_secondary_data[i].previous_slot_id = i + 1;
        g_battle_effect_secondary_data[i].next_slot_id = i - 1;
        g_battle_effect_secondary_data[i].function_id = 0;
        g_battle_effect_secondary_data[i].allocation = 0;
    }
    g_battle_effect_secondary_data[0].previous_slot_id = 0;
    g_battle_effect_secondary_data[16].next_slot_id = 15;
    g_battle_effect_secondary_data[16].own_slot_id = 16;
    g_battle_effect_free_slot_head = 1;
    g_battle_effect_secondary_data[0].next_slot_id = 0;
    g_battle_effect_secondary_data[0].own_slot_id = 0;
    g_battle_effect_secondary_data[0].function_id = 0;
    g_battle_effect_secondary_data[0].allocation = 0;
    g_battle_effect_secondary_data[16].previous_slot_id = 0;
    g_battle_effect_secondary_data[16].function_id = 0;
    g_battle_effect_secondary_data[16].allocation = 0;
    g_battle_effect_active_slot_tail = 0;
    g_battle_effect_trap_frame_data_ptr = g_battle_effect_trap_frame_data;
    for (i = 0; i < 17; i++) {
        g_battle_effect_secondary_data[i].active_count = 0;
        for (j = 31; j >= 0; j--) {
            g_battle_effect_secondary_data[i].slot_ids[j] = 0;
        }
    }
    battle_effect_reset_render_state();
    battle_effect_init_camera_and_screen_data();
}
