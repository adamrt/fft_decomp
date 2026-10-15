#include "fft/battle.h"
#include "psx/types.h"

/* Altima teleport distortion animation: advance one frame of the eight-step
 * warp-out / warp-in sequence.
 *
 * Jump table: .rodata 0x80068208, 8 32-bit addresses.
 *
 * The return type is s32 with no value ever returned: keeping $v0 live at the
 * epilogue is what leaves the switch range check's delay slot empty, matching
 * the target.  Nothing reads the result: the distortion dispatcher calls its
 * handlers as void. */
s32 battle_unit_advance_altima_teleport_distortion(battle_unit_t* unit) {
    unit->distortion_timer++;
    switch (unit->distortion_phase) {
    case 0:
        if (effect_start_altima_teleport_departure(unit) == 0) {
            effect_start_altima_teleport_white_flash(unit);
        }
        unit->distortion_phase++;
        break;
    case 1:
        /* The target passes no argument here. */
        if (effect_init_data() == 0 && camera_is_active() == 0) {
            effect_start_altima_teleport_white_flash(unit);
        }
        break;
    case 2:
        if (unit->distortion_timer >= 0x11) {
            battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_BASE, 4, unit->runtime_id, -31, -31, -31);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case 3:
        if (g_battle_state_animation_continue_check == 0) {
            unit->map_x = unit->movement.destination_x;
            unit->map_y = unit->movement.destination_y;
            unit->map_z = unit->movement.destination_z;
            battle_unit_set_real_coords_from_map_coords(unit);
            battle_unit_set_world_coords_from_real_coords(unit);
            if (effect_start_altima_teleport_arrival(unit) == 0) {
                effect_start_altima_teleport_fade_out(unit);
            }
            unit->distortion_phase++;
        }
        break;
    case 4:
        if (effect_init_data() == 0 && camera_is_active() == 0) {
            effect_start_altima_teleport_fade_out(unit);
        }
        break;
    case 5:
        if (unit->distortion_timer >= 0x11) {
            battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_BASE, 4, unit->runtime_id, 31, 31, 31);
            unit->distortion_phase++;
        }
        break;
    case 6:
        if (unit->distortion_timer >= 0x21) {
            battle_gfx_apply_unit_palette_modulation_to_units(MAP_COLOR_MODE_RESTORE, 2, unit->runtime_id, 0, 0, 0);
            unit->distortion_timer = 0;
            unit->distortion_phase++;
        }
        break;
    case 7:
        if (g_battle_state_animation_continue_check == 0) {
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
            unit->sprite_display_flags &= ~BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT;
        }
        break;
    }
}
