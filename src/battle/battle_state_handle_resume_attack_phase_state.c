#include "fft/battle_ai.h"
#include "fft/battle_gfx.h"
#include "fft/main_gfx.h"
#include "fft/main_runtime.h"

struct battle_misc_data_header;
extern void battle_gfx_invalidate_sp2_vram_slot(struct battle_misc_data_header*);

void battle_state_handle_resume_attack_phase_state(void) {
    s32 facing;
    battle_stats_t* battle_data;
    s32 selected_ability;
    battle_unit_misc_data_t* unit;

    battle_state_handle_free_cursor_input();
    battle_menu_draw_selection_data(main_gfx_get_otag(), g_controller_input_raw);
    selected_ability = *battle_menu_get_selected_ability_address();
    if (selected_ability >= 7 && (selected_ability < 9 || selected_ability == 0xff)) {
        g_battle_action_post_action = 1;
    }
    unit = battle_unit_get_casting_misc_data();
    if (g_battle_action_post_action != 0 && battle_action_resume_attack_phase_control() == 0
        && unit->numeric_display_active == 0) {
        if (unit->continue_attack != 0) {
            unit->continue_attack_count += 1;
            battle_action_set_damage_display_type_based_on_ability();
            return;
        }
        if (g_battle_gfx_sp2_data != 0) {
            main_heap_free(g_battle_gfx_sp2_data);
            g_battle_gfx_sp2_data = 0;
        }
        battle_gfx_invalidate_sp2_vram_slot((struct battle_misc_data_header*)unit);
        if (g_action_type == BATTLE_TURN_EVENT_UNIT_READY && g_battle_action_phase == 1) {
            facing = *(s16*)&unit->facing;
            if (facing < 0) {
                facing += 0x3ff;
            }
            facing = (u32)facing >> 10;
            battle_data = unit->battle_data;
            /* The target sets up a0/a1 at the call although the callee reads globals. */
            ((void (*)(battle_stats_t*, s32))battle_noop_8018ef2c)(battle_data, facing & 0xff);
        }
        battle_action_store_acting_unit_data(unit->battle_data);
        battle_unit_call_set_animation_based_on_status(unit);
        battle_unit_update_anim_display_for_all_targets(unit);
        battle_unit_get_source_misc_data();
        if (g_battle_action_phase != 0) {
            if (g_battle_action_phase != 2) {
                g_battle_action_phase += 1;
            }
            if (battle_state_announce_next_charged_action() != 0) {
                return;
            }
        }
        battle_action_handle_post_action_xp_jp_ability();
    }
}
