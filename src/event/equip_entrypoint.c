#include "fft/battle.h"
#include "fft/equip.h"
#include "fft/main_unit.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Run one frame of the equipment screen. Returns the frame's ordering table
 * while the screen is open, then whether the unit's equipment changed once it
 * closes. */
s32 equip_entrypoint(s32 unit, void* otag) {
    s32 result;
    s32 i;
    u8 changed;
    s32 message_idle;
    battle_stats_t* stats;
    s16* saved;
    u8* equipment;

    if (g_equip_menu_screen_state == 0) {
        equip_menu_init_screen(unit);
        bcopy((u8*)g_equip_unit_data[0] + 0x54, g_equip_unit_initial_equipment, 0xA);
        g_equip_menu_screen_state = 1;
    }
    g_equip_gfx_render_otag = otag;
    equip_gfx_swap_context_and_clear_otag(0, -1);
    result = (s32)g_equip_gfx_context->otag;
    changed = 0;
    if (g_event_mode != 0) {
        message_idle = g_equip_text_message_thread_active == 0;
        changed = message_idle;
    }
    g_equip_text_restore_pending = changed;
    equip_input_update_with_message_state();
    if (g_equip_input_primary_repeat & PSX_PAD_SELECT) {
        equip_text_start_help_thread(3);
    }
    if (g_equip_text_restore_pending != 0) {
        if (g_event_mode == 0) {
            battle_text_restore_pointer_table();
        }
    }
    equip_text_set_palette_and_metrics(g_event_mode);
    g_equip_menu_screen_state_handlers[g_equip_menu_screen_state]();
    if (g_equip_sound_suppress_queued != 0) {
        g_equip_sound_queued_effect_id = 0;
    }
    if (g_equip_sound_queued_effect_id != 0) {
        g_sound_effect_id_to_play = g_equip_sound_queued_effect_id;
    }
    g_equip_sound_queued_effect_id = 0;
    if (g_equip_menu_screen_state != 0xFF) {
        return result;
    }
    i = 0;
    saved = g_equip_unit_initial_equipment;
    equipment = (u8*)g_equip_unit_data[0];
    for (; i < 5; i++) {
        if (*(u16*)(equipment + 0x54) != *saved) {
            break;
        }
        saved++;
        equipment += 2;
    }
    stats = battle_unit_get_stats_from_battle_id(unit);
    if (stats->max_hp < stats->hp) {
        stats->hp = stats->max_hp;
    }
    if (stats->max_mp < stats->mp) {
        stats->mp = stats->max_mp;
    }
    /* The target loads no argument for this one-argument callee. */
    ((void (*)(void))main_unit_refresh_stats_and_statuses)();
    return i < 5;
}
