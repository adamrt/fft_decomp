/* Effect palettes and script commands. Its start is a boundary the binary proves; its end is a cut chosen by topic. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/gte_inline.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

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
        g_effect_unknown_801bf000 = 0;
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

void effect_init_state_records(void) {
    s32 i;

    g_effect_state_records[0].next_index = 0;
    g_effect_available_record_head = 1;
    for (i = 1; i < EFFECT_STATE_RECORD_CAPACITY; i++) {
        g_effect_state_records[i].record_index = i;
        g_effect_state_records[i].next_index = i + 1;
        g_effect_state_records[i].last_native_callback = 0;
    }
    g_effect_state_records[i].next_index = 0;
    g_effect_state_records[i].record_index = i;
    g_effect_allocated_record_head = 0;
    g_effect_current_record_index = 0;
    g_effect_playback_phase = EFFECT_PLAYBACK_PHASE_IDLE;
}

/* Allocate one effect record and link it into the in-use list. */
s16 effect_alloc_state_record(void) {
    u16* available;
    s16* allocated;
    s16 index;
    u16 next_available;
    u16 next_allocated;

    available = &g_effect_available_record_head;
    index = *available;
    next_available = g_effect_state_records[index].next_index;
    allocated = &g_effect_allocated_record_head;
    next_allocated = *allocated;
    *available = next_available;
    g_effect_state_records[index].next_index = next_allocated;
    *allocated = index;
    g_effect_state_records[index].flags |= EFFECT_RECORD_FLAG_ALLOCATED;
    return index;
}

/* Tear down effect state record record_index and return it to the free list.
 *
 * Frees every work record on its list, releases the four handler work slots,
 * clears the spawning record's reference to it, then unlinks it from the
 * allocated list; the inverse of effect_alloc_state_record. The
 * separate `particle` copy keeps the call's work argument loaded at the loop
 * edges, as in the target, and the list-head pointers keep the target's
 * materialized global address. */
void effect_free_state_record(s16 record_index) {
    effect_work_record_t* work;
    effect_work_record_t* particle;
    s16 head;
    s16 i;
    s16 parent;
    s16 cur;

    particle = work = g_effect_state_records[record_index].work_head;
    head = g_effect_allocated_record_head;
    while (work != 0) {
        effect_free_work_record(record_index, particle);
        particle = work = work->next;
    }
    for (i = 0; i < EFFECT_RECORD_CALLBACK_SLOT_COUNT; i++) {
        if (g_effect_state_records[record_index].work_slots[i] != 0) {
            battle_heap_free_block(g_effect_state_records[record_index].work_slots[i]);
            g_effect_state_records[record_index].work_slots[i] = 0;
        }
        g_effect_state_records[record_index].callbacks[i] = 0;
    }
    parent = g_effect_state_records[record_index].parent_index;
    if (parent != 0 && (g_effect_state_records[parent].flags & EFFECT_RECORD_FLAG_ALLOCATED)) {
        for (i = 0; i < EFFECT_SCRIPT_CHILD_SLOT_COUNT; i++) {
            if (g_effect_state_records[parent].child_indices[i] == record_index) {
                g_effect_state_records[parent].child_indices[i] = 0;
            }
        }
    }
    if (head == record_index) {
        u16* available;

        g_effect_allocated_record_head = g_effect_state_records[record_index].next_index;
        available = &g_effect_available_record_head;
        g_effect_state_records[record_index].next_index = *available;
        *available = head;
    } else {
        u16* available_after_unlink;

        while ((cur = g_effect_state_records[head].next_index) != record_index) {
            head = cur;
        }
        g_effect_state_records[head].next_index = g_effect_state_records[cur].next_index;
        available_after_unlink = &g_effect_available_record_head;
        g_effect_state_records[cur].next_index = *available_after_unlink;
        *available_after_unlink = cur;
    }
    g_effect_state_records[record_index].flags &= ~EFFECT_RECORD_FLAG_ALLOCATED;
}

/* Clears a record's child-timeline state and sets the effect target it plays
 * on. */
void effect_reset_child_timeline(effect_record_t* record, s16 target_index) {
    s32 index;

    for (index = 0; index < EFFECT_CHILD_TRACK_RESET_SLOT_COUNT; index++) {
        record->timeline.state.child.step.reset_slots[index] = 0;
        record->timeline.state.child.remaining.reset_slots[index] = 0;
        record->timeline.state.child.keyframe.reset_slots[index] = 0;
    }
    record->timeline.frame = 0;
    record->timeline.target_index = target_index;
}

/* Clear all 26 16-bit values in each bank, including the extra reset slots. */
void effect_reset_parent_timeline(effect_record_t* record) {
    s32 index;

    for (index = 0; index < EFFECT_PARENT_TRACK_RESET_SLOT_COUNT; index++) {
        record->timeline.state.parent.step.reset_slots[index] = 0;
        record->timeline.state.parent.remaining.reset_slots[index] = 0;
        record->timeline.state.parent.keyframe.reset_slots[index] = 0;
    }
    record->timeline.state.parent.spawn_countdown = 0;
    record->timeline.frame = 0;
    record->timeline.state.parent.spawned_count = 0;
}

/* Allocate an effect state record for a script and reset its per-handler state.
 * Returns the record index, or 0 when none is available. */
s16 effect_start_script_record(const u8* script, s16 start_pc, s16 parent_index) {
    s16 index;
    s16 i;
    effect_record_t* record;

    index = effect_alloc_state_record();
    if (index == 0) {
        return 0;
    }
    record = &g_effect_state_records[index];
    record->parent_index = parent_index;
    record->script = script;
    record->pc = start_pc;
    for (i = 0; i < EFFECT_SCRIPT_COUNTER_COUNT; i++) {
        record->counters[i] = 0;
        record->child_indices[i] = 0;
    }
    for (i = 0; i < EFFECT_RECORD_CALLBACK_SLOT_COUNT; i++) {
        record->callbacks[i] = 0;
        record->callback_phase[i] = EFFECT_CALLBACK_PHASE_IDLE;
        record->work_slots[i] = 0;
    }
    record->flags = EFFECT_RECORD_FLAG_ALLOCATED;
    record->work_head = 0;
    record->work_count = 0;
    record->frame_counter = -1;
    effect_reset_child_timeline(record, 0);
    effect_reset_parent_timeline(record);
    return index;
}

effect_script_result_e effect_code_script_2e_invalid(effect_record_t* unused_record) {
    main_system_report_stub(1, 0x22);
    /* Retail leaves the dispatch result undefined after an invalid opcode. */
}

/* Opcode 00: jump to the script offset in the second 16-bit value and end the
 * record's run for this frame. */
effect_script_result_e effect_code_script_00_jump_and_yield(effect_record_t* record) {
    record->pc = effect_get_current_script_instruction(record)->immediate.value;
    return EFFECT_SCRIPT_RESULT_YIELD;
}

/* Opcode 01: jump to the script offset in the second 16-bit value. */
effect_script_result_e effect_code_script_01_jump(effect_record_t* record) {
    record->pc = effect_get_current_script_instruction(record)->immediate.value;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

effect_script_result_e effect_code_script_02_spawn_child(effect_record_t* record) {
    effect_script_parameters_t parameters;
    s16 slot;
    s16 child;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    slot = parameters.child.slot;
    child = effect_start_script_record(record->script, instruction->immediate.value, g_effect_current_record_index);
    if (slot < EFFECT_SCRIPT_CHILD_SLOT_COUNT) {
        record->child_indices[slot] = child;
    }
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 03: free the child record in the child slot named by bits 4-5 of
 * the instruction's second byte. */
effect_script_result_e effect_code_script_03_free_child(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    s16 child = record->child_indices[parameters.free_child.slot];
    if (child != 0)
        effect_free_state_record(child);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 04: end the script; effect_run_block's caller then frees the
 * record. */
effect_script_result_e effect_code_script_04_end(effect_record_t* unused_record) {
    return EFFECT_SCRIPT_RESULT_ABORT;
}

/* Opcode 05: store the high nibble of the instruction's second byte.
 * Retail clears this global before the script runs but never reads it. */
effect_script_result_e effect_code_script_05_store_high_nibble(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    effect_script_parameters_t parameters = instruction->header.parameters;
    g_effect_unknown_801bf000 = parameters.opcode_05.high_nibble;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 06: put the native callback whose catalog id is the second 16-bit value in the
 * callback slot named by the top six bits of the instruction's second byte.
 * A particle keyframe whose action names the slot binds it to the record. */
effect_script_result_e effect_code_script_06_register_native_callback(effect_record_t* record) {
    const effect_script_instruction_t* command = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = command->header.parameters;
    u32 slot = parameters.callback.slot;
    g_effect_callback_slots[slot] = effect_get_native_callback(command->immediate.value);
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 07: call the native callback whose catalog id is the second 16-bit value with
 * the record index and the top six bits of the instruction's second byte as
 * its callback slot, and remember it in the record. */
effect_script_result_e effect_code_script_07_call_native_callback(effect_record_t* record) {
    const effect_script_instruction_t* command = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = command->header.parameters;
    u32 slot = parameters.callback.slot;
    effect_callback_t handler = effect_get_native_callback(command->immediate.value);
    record->last_native_callback = handler;
    handler(record->record_index, slot, 0, 0);
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 08: move the camera to the position in the three 16-bit values that
 * follow (x, y, z in world units). */
effect_script_result_e effect_code_script_08_set_camera_position(effect_record_t* record) {
    VECTOR coords;

    setVector(&coords, effect_get_current_script_instruction(record)->vector.x * ONE,
        effect_get_current_script_instruction(record)->vector.y * ONE,
        effect_get_current_script_instruction(record)->vector.z * ONE);
    camera_set_current_real_coords(&coords);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 09: save the camera position (opcode 0a restores it). */
effect_script_result_e effect_code_script_09_save_camera_position(effect_record_t* record) {
    camera_save_current_real_coords();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

effect_script_result_e effect_code_script_0a_restore_camera_position(effect_record_t* record) {
    camera_restore_current_real_coords();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0b: set the camera rotation from the three 16-bit values that follow. */
effect_script_result_e effect_code_script_0b_set_camera_rotation(effect_record_t* record) {
    SVECTOR rotation;

    setVector(&rotation, effect_get_current_script_instruction(record)->vector.x,
        effect_get_current_script_instruction(record)->vector.y,
        effect_get_current_script_instruction(record)->vector.z);
    camera_set_rotation(&rotation);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0c: save the camera rotation (opcode 0d restores it). */
effect_script_result_e effect_code_script_0c_save_camera_rotation(effect_record_t* record) {
    camera_save_rotation();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0d: restore the camera rotation opcode 0c saved. */
effect_script_result_e effect_code_script_0d_restore_camera_rotation(effect_record_t* record) {
    camera_restore_rotation();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0e: set the camera zoom from the three 16-bit values that follow (ONE is
 * no zoom). */
effect_script_result_e effect_code_script_0e_set_camera_zoom(effect_record_t* record) {
    VECTOR zoom;

    setVector(&zoom, effect_get_current_script_instruction(record)->vector.x,
        effect_get_current_script_instruction(record)->vector.y,
        effect_get_current_script_instruction(record)->vector.z);
    camera_set_zoom(&zoom);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0f: save the camera zoom (opcode 24 restores it). */
effect_script_result_e effect_code_script_0f_save_camera_zoom(effect_record_t* record) {
    camera_save_zoom();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 10: set the counter selected by the top two bits of the
 * instruction's second byte to the second 16-bit value. */
effect_script_result_e effect_code_script_10_set_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    record->counters[parameters.counter.slot] = instruction->immediate.value;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 11: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte equals the second 16-bit value;
 * otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_11_jump_if_counter_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] == instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

effect_script_result_e effect_code_script_12_jump_if_counter_at_least(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] >= instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 13: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte exceeds the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_13_jump_if_counter_greater(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] > instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 14: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte is at most the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_14_jump_if_counter_at_most(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] <= instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 15: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte is below the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_15_jump_if_counter_less(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] < instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

effect_script_result_e effect_code_script_16_jump_if_work_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count == instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 17: jump to the offset in the third 16-bit value when the record has
 * more particles than the second 16-bit value says; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_code_script_17_jump_if_work_count_greater(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (instruction->branch.comparison < record->work_count) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 18: jump to the offset in the third 16-bit value when the record has
 * fewer particles than the second 16-bit value says; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_code_script_18_jump_if_work_count_less(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count < instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 19: jump to the offset in the third 16-bit value when the number of
 * occupied child slots equals the second 16-bit value; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_code_script_19_jump_if_child_count_equal(effect_record_t* record) {
    s16 i;
    s16 count;
    s16 target;

    target = effect_get_current_script_instruction(record)->branch.comparison;
    count = 0;
    for (i = 0; i < EFFECT_SCRIPT_CHILD_SLOT_COUNT; i++) {
        if (record->child_indices[i] != 0) {
            count++;
        }
    }
    if (count == target) {
        record->pc = effect_get_current_script_instruction(record)->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

effect_script_result_e effect_code_script_1a_jump_if_child_active(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    if (record->child_indices[parameters.child.slot] != 0) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1b: jump to the offset in the second 16-bit value when the child slot
 * named by the second byte's high nibble is empty; otherwise step over the
 * 4-byte instruction. */
effect_script_result_e effect_code_script_1b_jump_if_child_inactive(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    if (record->child_indices[parameters.child.slot] == 0) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1c: jump to the offset in the third 16-bit value when the counter
 * selected by the second byte's top two bits differs from the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_1c_jump_if_counter_not_equal(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] != instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1d: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the child timeline's duration; otherwise step over
 * the 4-byte instruction. */
effect_script_result_e effect_code_script_1d_jump_after_child_timeline_duration(effect_record_t* record) {
    if (record->timeline.frame >= g_effect_child_timeline->duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1e: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the end of the parent timeline (phase 1, a spawn
 * delay for each target after the first, the last child's duration and phase
 * 2); otherwise step over the 4-byte instruction. */
effect_script_result_e effect_code_script_1e_jump_after_total_duration(effect_record_t* record) {
    effect_timeline_t* timing;

    timing = g_effect_timeline;
    if (record->timeline.frame >= timing->phase1_duration + timing->phase2_duration
            + (g_effect_context.target_count - 1) * timing->spawn_delay + timing->child.duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1f: jump when the target count equals the header's six-bit selector.
 * Retail scripts use zero to end an effect with no targets. */
effect_script_result_e effect_code_script_1f_jump_if_target_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (g_effect_context.target_count == parameters.targets.count) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 20: add 1 to the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_code_script_20_increment_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] += 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 21: subtract 1 from the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_code_script_21_decrement_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] -= 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 22: add the second 16-bit value to the counter selected by the top two
 * bits of the instruction's second byte. */
effect_script_result_e effect_code_script_22_add_immediate_to_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;
    u16 amount;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    amount = instruction->immediate.value;
    record->counters[parameters.counter.slot] += amount;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 23: subtract the second 16-bit value from the counter selected by the
 * top two bits of the instruction's second byte. */
effect_script_result_e effect_code_script_23_subtract_immediate_from_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;
    u16 amount;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    amount = instruction->immediate.value;
    record->counters[parameters.counter.slot] -= amount;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 24: restore the camera zoom opcode 0f saved. */
effect_script_result_e effect_code_script_24_restore_camera_zoom(effect_record_t* record) {
    camera_restore_zoom();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Effect-script opcode 0x25: update every particle on the record's work list.
 *
 * Loads the camera matrix into the GTE and integrates the list's motion. Each
 * particle then ends its life once it is within a non-zero homing threshold
 * of its anchor on all three axes, runs its kind handler, spawns its mid-life
 * child and advances its curve frame. When its countdown (or, for lifetime
 * -1, its animation) ends it spawns its death child and is freed.
 *
 * The loop's locals are declared in its body: the block note ahead of the
 * exit test stops GCC from rolling the test to the loop end, which keeps the
 * target's top-tested loop. The death child is tested with an if/else chain
 * whose arms are identical (cross-jumping merges them): `mode == 1 || mode ==
 * 2` is folded into an unsigned range test that the target does not contain.
 */
effect_script_result_e effect_code_script_25_update_all_particles(effect_record_t* record) {
    effect_work_record_t* work;

    work = record->work_head;
    gte_SetRotMatrix(&g_camera_matrix);
    gte_SetTransMatrix(&g_camera_matrix);
    effect_integrate_particle_motion(record->work_head);

    for (;;) {
        effect_work_record_t* node;
        s32 threshold;
        s32 delta;

        if (work == 0) {
            break;
        }
        node = work;
        threshold
            = (node->flags & EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_MASK) >> EFFECT_EMITTER_FLAG_ARRIVAL_DISTANCE_SHIFT;
        work = node->next;
        if (threshold != 0) {
            delta = (node->motion.position.x >> 12) - node->target.x;
            if (delta < threshold && -threshold < delta) {
                delta = (node->motion.position.y >> 12) - node->target.y;
                if (delta < threshold && -threshold < delta) {
                    delta = (node->motion.position.z >> 12) - node->target.z;
                    if (delta < threshold && -threshold < delta) {
                        node->lifetime = EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN;
                    }
                }
            }
        }

        g_effect_particle_render_handlers[(u16)node->kind_flags >> 12](record, node);

        if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) == EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_1
            || (node->flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) == EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MODE_2) {
            effect_spawn_emitter_particles(
                record->record_index, record->frame_counter, node->child_emitter_mid_life, node);
        }

        node->curve_frame++;
        if (node->curve_frame == EFFECT_ANIMATION_CURVE_FRAMES) {
            node->curve_frame = 0;
        }

        if (node->lifetime == EFFECT_PARTICLE_LIFETIME_ANIMATION_DRIVEN) {
            if (node->data->frame_timer != 0) {
                continue;
            }
        } else if (--node->lifetime != 0) {
            continue;
        }

        if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) == EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_1) {
            effect_spawn_emitter_particles(
                record->record_index, record->frame_counter, node->child_emitter_on_death, node);
        } else if ((node->flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK)
            == EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MODE_2) {
            effect_spawn_emitter_particles(
                record->record_index, record->frame_counter, node->child_emitter_on_death, node);
        }
        effect_free_work_record(record->record_index, node);
    }

    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 26: spawn this frame's particles of the emitter named by bits 10-13
 * of the 16-bit instruction header, at the record's frame counter. */
effect_script_result_e effect_code_script_26_spawn_emitter_particles(effect_record_t* record) {
    /* A 16-bit header cache preserves the address operands; a direct field read changes them. */
    union {
        u16 packed;
        struct {
            u16 : 10;
            u16 emitter_index : 4;
            u16 : 2;
        } fields;
    } header;

    header.packed = effect_get_current_script_header(record);

    effect_spawn_emitter_particles(record->record_index, record->frame_counter, header.fields.emitter_index, 0);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 27: take the gravity vector and the inertia threshold from the
 * effect file's particle-system section. */
effect_script_result_e effect_code_script_27_init_particle_physics(effect_record_t* record) {
    effect_set_gravity(&g_effect_geometry_table->gravity);
    effect_set_inertia_threshold(g_effect_geometry_table->inertia_threshold);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/*
 * Dispatch one keyframe's action flags to a single effect target: bits 4-6
 * and 8-15 trigger unit reactions (bit 3 selects the caster entry), bits 0-2
 * register a callback slot.
 */
void effect_run_keyframe_actions(u16 flags, u8 target, effect_record_t* state) {
    s32 runtime_id;
    u32 animation;
    u16 slot;

    if (flags & EFFECT_KEYFRAME_ACTION_FLAG_CASTER_ONLY) {
        runtime_id = -1;
        if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id;
        }
    } else {
        runtime_id = -1;
        if (g_effect_context.targets[target].target_type == EFFECT_TARGET_TYPE_UNIT) {
            runtime_id = g_effect_context.targets[target].data.unit.runtime_id;
        }
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_POST_ACTION_DISPLAY) && runtime_id != -1) {
        battle_gfx_prepare_post_action_display_by_runtime_id(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_UPDATE_DISPLAY) && runtime_id != -1) {
        battle_unit_update_display_by_runtime_id(runtime_id);
    }
    if ((flags & EFFECT_KEYFRAME_ACTION_FLAG_TARGET_ANIMATION) && runtime_id != -1) {
        battle_unit_set_target_animation_by_runtime_id(runtime_id);
    }
    animation = (flags & EFFECT_KEYFRAME_ACTION_ANIMATION_MASK) >> EFFECT_KEYFRAME_ACTION_ANIMATION_SHIFT;
    if (animation != 0 && runtime_id != -1) {
        battle_unit_set_specific_animation_by_runtime_id(runtime_id, animation);
    }
    if (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) {
        slot = (flags & EFFECT_KEYFRAME_ACTION_CALLBACK_MASK) - 1;
        state->callback_phase[slot] = EFFECT_CALLBACK_PHASE_INIT;
        state->callbacks[slot] = g_effect_callback_slots[slot];
    }
}
