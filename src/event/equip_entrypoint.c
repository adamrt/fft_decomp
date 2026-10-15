#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Run one frame of the equipment screen. Returns the frame's ordering table
 * while the screen is open, then whether the unit's equipment changed once it
 * closes. */
s32 equip_entrypoint(s32 unit, u32* otag) {
    s32 result;
    s32 i;
    u8 changed;
    s32 message_idle;
    unit_stats_t* stats;

    if (g_equip_menu_screen_state == EQUIP_SCREEN_STATE_INIT) {
        equip_menu_init_screen(unit);
        bcopy(g_equip_unit_data[0]->equipment, g_equip_unit_initial_equipment, 0xA);
        g_equip_menu_screen_state = EQUIP_SCREEN_STATE_COMMAND_MENU;
    }
    g_equip_gfx_current_ot = otag;
    equip_gfx_swap_context_and_clear_otag(0, -1);
    result = (s32)g_menu_gfx_context->otag;
    changed = 0;
    if (g_menu_input_lock_mode != 0) {
        message_idle = g_equip_text_message_thread_active == 0;
        changed = message_idle;
    }
    g_equip_text_restore_pending = changed;
    equip_input_update_with_message_state();
    if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
        menu_text_start_help_thread(3);
    }
    if (g_equip_text_restore_pending != 0) {
        if (g_menu_input_lock_mode == 0) {
            text_restore_section_pointers();
        }
    }
    menu_set_palette_mode(g_menu_input_lock_mode);
    g_equip_menu_screen_state_handlers[g_equip_menu_screen_state]();
    if (g_equip_sound_suppress_queued != 0) {
        g_equip_sound_queued_effect_id = MAIN_SFX_NONE;
    }
    if (g_equip_sound_queued_effect_id != MAIN_SFX_NONE) {
        g_sound_pending_sfx_id = g_equip_sound_queued_effect_id;
    }
    g_equip_sound_queued_effect_id = MAIN_SFX_NONE;
    if (g_equip_menu_screen_state != EQUIP_SCREEN_STATE_CLOSED) {
        return result;
    }
    for (i = 0; i < 5; i++) {
        if (g_equip_unit_data[0]->equipment[i] != g_equip_unit_initial_equipment[i]) {
            break;
        }
    }
    stats = battle_unit_get_stats_from_battle_id(unit);
    if (stats->max_hp < stats->hp) {
        stats->hp = stats->max_hp;
    }
    if (stats->max_mp < stats->mp) {
        stats->mp = stats->max_mp;
    }
    main_unit_refresh_stats_and_statuses(stats);
    return i < 5;
}
