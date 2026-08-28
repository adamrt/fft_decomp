#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_runtime.h"
#include "fft/effect.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"

s32 battle_effect_update_stage(void) {
    s32 result;
    s32 id;
    s32 next;

    id = (s16)g_battle_effect_allocated_record_head;
    switch (g_battle_effect_phase) {
    case 0:
        result = 0;
        break;
    case 1:
        result = 1;
        break;
    case 2:
        battle_map_freeze();
        battle_menu_suppress_height_display();
        battle_effect_init_palette(g_ability_effect_id);
        g_battle_effect_phase = 3;
        D_801BF000 = 0;
        battle_state_reset_frame_pacing_timer();
        result = 1;
        break;
    case 3:
        battle_heap_init((battle_heap_node_t*)g_battle_effect_palette_ptr,
            g_battle_heap_end_address - (u32)g_battle_effect_palette_ptr);
        battle_effect_reset_subsystems();
        battle_effect_copy_camera_angles_and_screen_coords();
        battle_state_clear_frame_pacing();
        battle_effect_set_frame_data_pointer(g_ability_effect_id);
        battle_effect_set_parameter_sets_pointer(g_ability_effect_id);
        battle_effect_set_motion_header_pointer(g_ability_effect_id);
        battle_effect_set_animation_curve_data_pointer(g_ability_effect_id);
        battle_effect_set_particle_system_data_pointer(g_ability_effect_id);
        battle_effect_set_misc_data_pointers(g_ability_effect_id);
        battle_effect_load_sound_section(g_ability_effect_id);
        battle_effect_init_flags_section_pointer(g_ability_effect_id);
        battle_effect_set_nibble_table_pointer(g_ability_effect_id);
        if (g_battle_effect_coord_data.math_skill_flag != 0) {
            if (g_battle_effect_flags_section->spawn_delay_override != 0) {
                g_battle_effect_misc_data->spawn_delay = g_battle_effect_flags_section->spawn_delay_override;
            } else {
                g_battle_effect_misc_data->spawn_delay = 100;
            }
        }
        g_battle_effect_root_record_index = battle_effect_start_script_record(g_battle_effect_motion_header, 0, 0);
        g_battle_effect_phase = 4;
        battle_map_configure_effect_data_state(0x88);
        result = 1;
        break;
    case 4:
        g_battle_effect_otag = main_gfx_get_otag();
        battle_effect_start_render_frame();
        while (id != 0) {
            next = (s16)g_effect_state_records[id].next_index;
            g_battle_effect_current_record_index = id;
            g_effect_state_records[id].emitter_index++;
            if (battle_effect_run_block(id) == 0) {
                battle_effect_free_state_record(id);
            }
            id = next;
        }
        /* This stage reloads the list head as a signed halfword. The shared
         * declaration remains unsigned because other traversals use lhu. */
        if (*(s16*)&g_battle_effect_allocated_record_head != 0) {
            result = 1;
        } else {
            if (g_battle_effect_flags_section->flags & EFFECT_FLAG_AUDIO_FADE) {
                main_sound_set_current_music_volume_timed(0x78);
            }
            if (g_battle_effect_sound_section_ptr != 0) {
                main_sound_remove_vfx_resource(g_battle_effect_sound_section_ptr);
                g_battle_effect_sound_section_ptr = 0;
            }
            battle_map_configure_effect_data_state(0x87);
            battle_map_unfreeze();
            battle_menu_enable_height_display();
            battle_state_clear_frame_pacing();
            g_battle_effect_phase = 0;
            result = 0;
        }
        break;
    case 6:
        battle_heap_init((battle_heap_node_t*)g_battle_effect_palette_ptr,
            g_battle_heap_end_address - (u32)g_battle_effect_palette_ptr);
        battle_effect_reset_subsystems();
        battle_effect_reset_frame_data_pointer();
        battle_effect_reset_parameter_sets_pointer();
        g_battle_effect_root_record_index = battle_effect_start_script_record(g_battle_effect_motion_header, 0, 0);
        g_battle_effect_phase = 4;
        return 1;
    }
    return result;
}
