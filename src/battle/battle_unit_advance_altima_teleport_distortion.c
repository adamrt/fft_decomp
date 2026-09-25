#include "fft/battle.h"
#include "psx/types.h"

/* Altima teleport distortion animation: advance one frame of the eight-step
 * warp-out / warp-in sequence.
 *
 * Jump table: .rodata 0x80068208, 8 words.
 *
 * The return type is s32 with no value ever returned: keeping $v0 live at the
 * epilogue is what leaves the switch range check's delay slot empty, matching
 * the target.  Nothing uses the result. */
s32 battle_unit_advance_altima_teleport_distortion(battle_unit_misc_data_t* unit) {
    unit->distortion_timer++;
    switch (unit->distortion_phase) {
    case 0:
        if (battle_effect_start_altima_teleport_departure(unit) == 0) {
            battle_effect_start_altima_teleport_white_flash(unit);
        }
        unit->distortion_phase++;
        break;
    case 1:
        /* The target passes no argument here. */
        if (((s32 (*)(void))battle_effect_init_data)() != 0) {
            return;
        }
        if (battle_camera_is_active() != 0) {
            return;
        }
        battle_effect_start_altima_teleport_white_flash(unit);
        break;
    case 2:
        if (unit->distortion_timer < 0x11) {
            return;
        }
        battle_gfx_start_misc_unit_palette_modulation(4, 4, unit->unit_id, -31, -31, -31);
        unit->distortion_timer = 0;
        unit->distortion_phase++;
        break;
    case 3:
        if (g_battle_state_animation_continue_check != 0) {
            return;
        }
        unit->map_x = unit->movement.bytes.destination_x;
        unit->map_y = unit->movement.bytes.destination_y;
        unit->map_z = unit->movement.bytes.destination_z;
        battle_unit_set_real_coords_from_map_coords(unit);
        battle_unit_set_screen_coords_from_real_coords(unit);
        if (battle_effect_start_altima_teleport_arrival(unit) == 0) {
            battle_effect_start_altima_teleport_fade_out(unit);
        }
        unit->distortion_phase++;
        break;
    case 4:
        if (((s32 (*)(void))battle_effect_init_data)() != 0) {
            return;
        }
        if (battle_camera_is_active() != 0) {
            return;
        }
        battle_effect_start_altima_teleport_fade_out(unit);
        break;
    case 5:
        if (unit->distortion_timer < 0x11) {
            return;
        }
        battle_gfx_start_misc_unit_palette_modulation(4, 4, unit->unit_id, 31, 31, 31);
        unit->distortion_phase++;
        break;
    case 6:
        if (unit->distortion_timer < 0x21) {
            return;
        }
        battle_gfx_start_misc_unit_palette_modulation(8, 2, unit->unit_id, 0, 0, 0);
        unit->distortion_timer = 0;
        unit->distortion_phase++;
        break;
    case 7:
        if (g_battle_state_animation_continue_check != 0) {
            return;
        }
        unit->distortion_animation_id = 0;
        unit->sprite_display_flags.half &= 0xfffe;
        break;
    }
}
