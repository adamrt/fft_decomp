/* Effect file section pointers, palette/texture/sound upload, the load state machine, ability selection and the
 * playback frame. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Send an effect file's palette and texture section to VRAM. Its two
 * 256-color blocks go to MAP_CLUT_ROW_EFFECT and MAP_CLUT_ROW_EFFECT_SECOND; the
 * texture image that follows them to (0x180, 0).
 *
 * The image is preceded by its byte size as a 24-bit little-endian value and a
 * width flag: nonzero for an image 0x80 16-bit VRAM columns (256 bytes) wide, zero for
 * 0x40 16-bit columns (128 bytes). The height is the size over the row length.
 * g_effect_palette_ptr keeps the section's address. */
void effect_init_palette(s16 index) {
    RECT rect;
    effect_file_header_t* base;
    effect_palette_section_t* data;
    s32 size;
    s32 raw_size;
    s32 height;

    base = g_effect_data_ptrs[index];
    data = (effect_palette_section_t*)((u8*)base + base->palette_and_texture);
    g_effect_palette_ptr = data;
    map_load_palette_data(data->primary_clut, MAP_CLUT_ROW_EFFECT, 0, 1);
    map_load_palette_data(data->secondary_clut, MAP_CLUT_ROW_EFFECT_SECOND, 0, 1);
    raw_size = data->image_size[0] + (data->image_size[1] << 8);
    raw_size = (data->image_size[2] << 16) + raw_size;
    height = raw_size;
    size = height;
    if (data->wide_image != 0) {
        height = size / 0x100;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = height;
        LoadImage(&rect, data->pixels);
    } else {
        height = size;
        height = height / 0x80;
        rect.x = 0x180;
        rect.y = 0;
        rect.w = 0x40;
        rect.h = height;
        LoadImage(&rect, data->pixels);
    }
}

void effect_reset_frame_data_pointer(void) {
    g_effect_frame_data = g_effect_frame_data_start;
}

void effect_reset_animation_sequences_pointer(void) {
    g_effect_animation_sequences = g_effect_animation_sequences_start;
}

void effect_set_frame_data_pointer(s16 effect_id) {
    effect_file_header_t* header;
    effect_sprite_section_t* section;

    header = g_effect_data_ptrs[effect_id];
    section = (effect_sprite_section_t*)((u8*)header + header->frame_data);
    g_effect_frame_data = (u8*)section->groups;
}

void effect_set_animation_sequences_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_animation_sequences = (u8*)header + header->animation_sequences + 4;
}

void effect_set_script_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_script = (const u8*)header + header->script;
}

void effect_set_animation_curve_data_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_animation_curve_data = (const effect_animation_curve_table_t*)((u8*)header + header->animation_curve);
}

void effect_set_particle_system_data_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_geometry_table = (const effect_geometry_table_t*)((u8*)header + header->particle_system);
}

void effect_set_timeline_pointers(s16 effect_id) {
    effect_file_header_t* header;
    effect_timeline_t* timeline;

    header = g_effect_data_ptrs[effect_id];
    timeline = (effect_timeline_t*)((u8*)header + header->timeline);
    g_effect_timeline = timeline;
    g_effect_child_timeline = &timeline->child;
}

/* Replace the effect sounds: unregister the sound resource of the previous effect
 * file, then register this file's sound section and take its id as the base for
 * the effect's sound ids. A file without a sound section clears both globals. */
void effect_load_sound_section(s16 index) {
    effect_file_header_t* base;
    s32 offset;

    if (g_effect_sound_section_ptr != 0) {
        suzuki_sound_remove_resource(g_effect_sound_section_ptr);
    }

    base = g_effect_data_ptrs[index];
    offset = base->sound_section;
    if (offset != 0) {
        g_effect_sound_section_ptr = (suzuki_sound_resource_t*)((u8*)base + offset);
        suzuki_sound_append_resource(g_effect_sound_section_ptr);
        g_effect_sound_data_base = SUZUKI_SOUND_ID(g_effect_sound_section_ptr->id, 0);
    } else {
        g_effect_sound_section_ptr = 0;
        g_effect_sound_data_base = 0;
    }
}

void effect_init_flags_section_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_flags_section = (const effect_flags_section_t*)((u8*)header + header->flags_section);
}

/* A zero section offset binds the pointer to the file header. */
void effect_set_timing_curves_pointer(s16 effect_id) {
    effect_file_header_t* header;

    header = g_effect_data_ptrs[effect_id];
    g_effect_timing_curves = (const effect_timing_curves_t*)((u8*)header + header->timing_curves);
}

/* Time scale of the parent timeline's frame record->timeline.frame: one of the
 * 4-bit values the effect file's timing-curve section packs two to a byte, low
 * nibble first. */
s32 effect_get_timing_curve_value(effect_record_t* record) {
    s16 packed_index;
    s32 packed_value;
    s32 offset;

    packed_index = record->timeline.frame;
    offset = packed_index / 2;
    packed_value = g_effect_timing_curves->parent_timeline[offset];
    if (packed_index & 1) {
        packed_value >>= 4;
    }
    return packed_value & 0xf;
}

/* Time scale of the child timeline's frame record->timeline.frame: one of the
 * 4-bit values the effect file's timing-curve section packs two to a byte, low
 * nibble first. */
s32 effect_get_animation_timing_curve_value(effect_record_t* record) {
    s16 packed_index;
    s32 offset;
    s32 packed_value;

    packed_index = record->timeline.frame;
    offset = packed_index / 2;
    packed_value = g_effect_timing_curves->child_timeline[offset];
    if (packed_index & 1) {
        packed_value >>= 4;
    }
    return packed_value & 0xf;
}

/* Look up a native overlay callback by its resident catalog index. */
effect_callback_t effect_get_native_callback(s16 callback_id) {
    return g_effect_native_callbacks[callback_id];
}

void effect_load_lba_and_size(s32 effect_id, s32* lba, s32* byte_length) {
    u32 stack_padding[2];

    *lba = g_effect_disc_entries[effect_id].sector;
    *byte_length = g_effect_disc_entries[effect_id].size;
}

/* Report an effect stub and return zero. */
s32 effect_report_stub_return_zero(void) {
    main_system_report_stub(1, 0x320);
    return 0;
}

/* Report an effect stub. */
void effect_report_stub(void) {
    main_system_report_stub(1, 0x320);
}

/* Calls the empty main_noop_800449f8 with (1, 0x320) from a 0x100-byte frame it
 * does not use. No code calls it. */
void battle_noop_801a1328(void) {
    s32 unused[0x40];
    main_system_report_stub(1, 0x320);
}

/* Start loading the effect file of g_battle_ability_effect_id; an effect whose
 * file has no size is replaced by the Cure effect. Nothing in the game calls it:
 * effect_init_data does the same in its first state. */
void effect_load_data(void) {
    s32 lba;
    s32 byte_length;

    effect_load_lba_and_size(g_battle_ability_effect_id, &lba, &byte_length);
    if (byte_length == 0) {
        g_battle_ability_effect_id = EFFECT_FILE_CURE;
        effect_load_lba_and_size(EFFECT_FILE_CURE, &lba, &byte_length);
    }
    main_file_call_build_header(lba, byte_length, 0);
}

/*
 * Effect-file load state machine: state 0 starts loading the ability's effect
 * file (falling back to effect 1 when its size is zero) 0x3500 bytes into the
 * event overlay area, state 1 polls the load, state 2 waits out the countdown
 * (60 frames when a secondary effect is active), finalizes the secondary
 * effect and fades the music when the effect header flag 0x10 is set, and
 * state 3 waits for the secondary effect phase to end. Returns nonzero while
 * work remains. A load state outside 0-3 leaves `result` unset: the test then
 * reads whatever the caller left in $a0.
 */
s32 effect_init_data(void) {
    s32 result;
    s32 lba;
    s32 byte_length;
    u8* destination;

    destination = g_main_heap_companion_overlay_load_address + 0x3500;
    if (g_battle_ability_effect_id >= 0) {
        switch (g_effect_load_state) {
        case EFFECT_LOAD_START:
            if (g_effect_charge_secondary_id != 0) {
                g_effect_load_countdown = 60;
            } else {
                g_effect_load_countdown = 0;
            }
            effect_load_lba_and_size(g_battle_ability_effect_id, &lba, &byte_length);
            if (byte_length == 0) {
                g_battle_ability_effect_id = EFFECT_FILE_CURE;
                effect_load_lba_and_size(EFFECT_FILE_CURE, &lba, &byte_length);
            }
            result = main_file_call_build_header(lba, byte_length, destination);
            if (result == 0) {
                g_effect_load_state = EFFECT_LOAD_READING;
                result = 1;
            }
            break;
        case EFFECT_LOAD_READING:
            result = main_file_is_still_loading();
            if (result == 0) {
                g_effect_load_state = EFFECT_LOAD_COUNTDOWN;
                result = 1;
            }
            break;
        case EFFECT_LOAD_COUNTDOWN:
            if (g_effect_load_countdown != 0) {
                result = 1;
                break;
            }
            if (g_effect_charge_secondary_id != 0) {
                effect_finalize_secondary(g_effect_charge_secondary_id);
            }
            g_effect_load_state = EFFECT_LOAD_ENDING_SECONDARY;
            effect_init_flags_section_pointer(g_battle_ability_effect_id);
            /* An `lw` of the section's leading 32-bit value, not a byte test. */
            if (g_effect_flags_section->flags & EFFECT_FLAG_QUIET_MUSIC) {
                main_sound_set_current_music_volume(0x40, 0xF0);
            }
            result = 1;
            break;
        case EFFECT_LOAD_ENDING_SECONDARY:
            result = effect_get_secondary_phase(g_effect_charge_secondary_id) != EFFECT_SECONDARY_PHASE_ENDED;
            break;
        }
        if (g_effect_load_countdown > 0) {
            g_effect_load_countdown--;
        }
        return result != 0;
    }
    return 0;
}

/*
 * Select the effect file for an ability and start its charge secondary effect.
 *
 * `packed` carries the ability's charge animation set in its low 16 bits and
 * its element mask in its high 16 bits. `ability` is an ability id or, from
 * ABILITY_ID_COUNT up, one of the effects past the abilities. A thrown ball
 * (ABILITY_ID_THROW_BALL) takes the Fire, Ice or Bolt effect by the ball's
 * element. A negative table entry means the ability has no effect file: only
 * the two fixed secondary animations for the equipment-break and Charge
 * ability ranges are started, and nothing is retained.
 */
void effect_set_ability_animation(u32 packed, s32 ability, effect_secondary_init_t* source) {
    g_effect_load_state = EFFECT_LOAD_START;
    if (ability < ABILITY_ID_COUNT) {
        if (ability == ABILITY_ID_THROW_BALL) {
            s32 ball = effect_get_element_index(packed >> 16);
            if (ball == EFFECT_ELEMENT_INDEX_FIRE)
                g_battle_ability_effect_id = EFFECT_FILE_FIRE;
            else if (ball == EFFECT_ELEMENT_INDEX_WATER)
                g_battle_ability_effect_id = EFFECT_FILE_ICE;
            else if (ball == EFFECT_ELEMENT_INDEX_LIGHTNING)
                g_battle_ability_effect_id = EFFECT_FILE_BOLT;
        } else {
            g_battle_ability_effect_id = g_effect_ability_files[ability];
        }
    } else {
        /* Event IDs continue after the action/reaction ability rows. */
        g_battle_ability_effect_id = g_effect_ability_files[ability - ABILITY_ID_COUNT + ABILITY_ID_SUPPORT_FIRST];
    }

    if (g_battle_ability_effect_id >= 0) {
        g_battle_ability_effect_id &= EFFECT_ABILITY_FILE_MASK;
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_REQUESTED;
        if (g_effect_secondary_by_charge_animation[packed & 0xffff] != 0) {
            g_effect_charge_secondary_id
                = effect_init_secondary(g_effect_secondary_by_charge_animation[packed & 0xffff], packed >> 16, source);
        } else {
            g_effect_charge_secondary_id = 0;
        }
        g_effect_sound_call_counts[3] = 0;
        g_effect_sound_call_counts[2] = 0;
        g_effect_sound_call_counts[1] = 0;
        g_effect_sound_call_counts[0] = 0;
        g_effect_context = *source;
        g_battle_unknown_801bc0d4 = 0;
        return;
    }

    if (ability >= ABILITY_ID_BATTLE_SKILL_HEAD_BREAK && ability <= ABILITY_ID_BATTLE_SKILL_MIND_BREAK)
        effect_init_secondary(EFFECT_SECONDARY_ANIMATION_EQUIP_BREAK, 0, source);
    if ((ability >= ABILITY_ID_CHARGE_FIRST && ability < ABILITY_ID_MATH_FIRST)
        || ability == ABILITY_ID_BASIC_SKILL_ACCUMULATE)
        effect_init_secondary(EFFECT_SECONDARY_ANIMATION_RED_ORBS, 0, source);
}

/* Returns 1 when the ability has no effect file (effect id 0xffff), 0 for
 * Vertical Jump8, all Charge abilities and Accumulate. The target's range
 * starts at Vertical Jump8 rather than Charge +1. */
u32 effect_load_ability(s32 ability_id) {
    u32 result;

    if (ability_id >= ABILITY_ID_JUMP_VERTICAL_8 && ability_id < ABILITY_ID_MATH_FIRST) {
        return 0;
    }
    if (ability_id == ABILITY_ID_BASIC_SKILL_ACCUMULATE) {
        result = 0;
    } else {
        result = g_effect_ability_files[ability_id] < 0;
    }
    return result;
}

/* 1 when the ability's entry in g_effect_ability_files carries
 * EFFECT_ABILITY_FLAG_ITEM. A negative entry (an ability without an effect
 * file) is complemented before the test. */
s32 effect_is_item_ability(s32 ability_id) {
    s32 entry;
    entry = g_effect_ability_files[ability_id];
    if (entry < 0) {
        entry = ~entry;
    }
    return (entry & EFFECT_ABILITY_FLAG_ITEM) > 0;
}

/* Releases a requested effect: effect_update_stage starts it on its
 * next frame. */
void effect_play(void) {
    if (g_effect_playback_phase == EFFECT_PLAYBACK_PHASE_REQUESTED) {
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_INIT_PALETTE;
    }
}

/* Drops a requested effect that has not been released. */
void effect_cancel_playback_request(void) {
    if (g_effect_playback_phase == EFFECT_PLAYBACK_PHASE_REQUESTED) {
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
    }
}

s32 effect_get_playback_phase(void) {
    return g_effect_playback_phase;
}

/* Runs the loaded effect file one frame and returns 1 while it plays.
 *
 * Once effect_play releases a requested effect, the file's two CLUT
 * rows and its texture go to VRAM; the memory from that last section of the
 * file on then becomes the effect heap, the section pointers are bound and
 * the root record starts the script. Each later frame runs every allocated
 * record's script until it yields, freeing the records whose scripts end, and
 * the effect is over when none is left. */
s32 effect_update_stage(void) {
    s32 result;
    s32 id;
    s32 next;

    id = g_effect_allocated_record_head;
    switch (g_effect_playback_phase) {
    case EFFECT_PLAYBACK_PHASE_IDLE:
        result = 0;
        break;
    case EFFECT_PLAYBACK_PHASE_REQUESTED:
        result = 1;
        break;
    case EFFECT_PLAYBACK_PHASE_INIT_PALETTE:
        map_freeze();
        menu_suppress_height_display();
        effect_init_palette(g_battle_ability_effect_id);
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_START_SCRIPT;
        D_801BF000 = 0;
        battle_state_reset_frame_pacing_timer();
        result = 1;
        break;
    case EFFECT_PLAYBACK_PHASE_START_SCRIPT:
        battle_heap_init(
            (battle_heap_node_t*)g_effect_palette_ptr, g_battle_heap_end_address - (u32)g_effect_palette_ptr);
        effect_reset_subsystems();
        effect_copy_camera_angles_and_world_coords();
        battle_state_clear_frame_pacing();
        effect_set_frame_data_pointer(g_battle_ability_effect_id);
        effect_set_animation_sequences_pointer(g_battle_ability_effect_id);
        effect_set_script_pointer(g_battle_ability_effect_id);
        effect_set_animation_curve_data_pointer(g_battle_ability_effect_id);
        effect_set_particle_system_data_pointer(g_battle_ability_effect_id);
        effect_set_timeline_pointers(g_battle_ability_effect_id);
        effect_load_sound_section(g_battle_ability_effect_id);
        effect_init_flags_section_pointer(g_battle_ability_effect_id);
        effect_set_timing_curves_pointer(g_battle_ability_effect_id);
        if (g_effect_context.math_skill_flag != 0) {
            if (g_effect_flags_section->spawn_delay_override != 0) {
                g_effect_timeline->spawn_delay = g_effect_flags_section->spawn_delay_override;
            } else {
                g_effect_timeline->spawn_delay = 100;
            }
        }
        g_effect_root_record_index = effect_start_script_record(g_effect_script, 0, 0);
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_RUNNING;
        map_configure_effect_data_state(MAP_EFFECT_DATA_BEGIN);
        result = 1;
        break;
    case EFFECT_PLAYBACK_PHASE_RUNNING:
        g_effect_otag = main_gfx_get_otag();
        effect_start_render_frame();
        while (id != 0) {
            next = g_effect_state_records[id].next_index;
            g_effect_current_record_index = id;
            g_effect_state_records[id].frame_counter++;
            if (effect_run_block(id) == 0) {
                effect_free_state_record(id);
            }
            id = next;
        }
        if (g_effect_allocated_record_head != 0) {
            result = 1;
        } else {
            if (g_effect_flags_section->flags & EFFECT_FLAG_QUIET_MUSIC) {
                main_sound_set_current_music_volume_timed(0x78);
            }
            if (g_effect_sound_section_ptr != 0) {
                suzuki_sound_remove_resource(g_effect_sound_section_ptr);
                g_effect_sound_section_ptr = 0;
            }
            map_configure_effect_data_state(MAP_EFFECT_DATA_END);
            map_unfreeze();
            menu_enable_height_display();
            battle_state_clear_frame_pacing();
            g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
            result = 0;
        }
        break;
    case EFFECT_PLAYBACK_PHASE_RESTART_SCRIPT:
        battle_heap_init(
            (battle_heap_node_t*)g_effect_palette_ptr, g_battle_heap_end_address - (u32)g_effect_palette_ptr);
        effect_reset_subsystems();
        effect_reset_frame_data_pointer();
        effect_reset_animation_sequences_pointer();
        g_effect_root_record_index = effect_start_script_record(g_effect_script, 0, 0);
        g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_RUNNING;
        return 1;
    }
    return result;
}

/* The effect system's frame: the effect file, the secondary effects and the
 * effect camera moves. Returns 1 while an effect of either kind runs. */
s32 effect_update_cycle(void) {
    s32 stage_result;
    s32 related_result;

    stage_result = effect_update_stage();
    related_result = effect_update_secondary_effects();
    camera_update_effect_rotation_and_zoom();

    if (stage_result != 0 || related_result != 0) {
        return 1;
    }
    return 0;
}
