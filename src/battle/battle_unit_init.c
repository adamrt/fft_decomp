#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Claims and initializes unit runtime record for one queued unit: spritesheet VRAM
 * slot and palette, map position, displays and the three weapon/body-part
 * sprite records, then links it at the head of the runtime-unit list.
 *
 * `id` is copied before the claim call, which keeps the unextended copy in
 * fp. The unit block pointer is assigned after the call to
 * battle_unit_set_move_and_world_coords: because it lives across calls,
 * sched1 still hoists it to the join, just after that call's argument copy.
 * Separate animation and sprite-block pointers change register allocation.
 * The contradictory weapon range test (<0x86, <0x3c, >=0x4a, !=0x5d) is
 * literally what the target branches encode (the call is dead code). */
battle_unit_t* battle_unit_init(s32 map_x, s32 map_y, s32 map_level, s16 facing, s32 spritesheet_id, s16 palette,
    s16 requested_runtime_id, unit_stats_t* stats, u32 flags, const battle_gfx_unit_sprite_file_t* data) {
    battle_unit_t* unit;
    union {
        battle_unit_anim_state_t* animation;
        battle_unit_sprite_block_t* block;
    } cursor;
    map_tile_t* tile;
    s32 runtime_id;
    s32 slot;
    u16 id;
    s32 i;
    battle_unit_t** head;
    u8 tile_x;
    u8 tile_y;
    u8 tile_z;

    id = spritesheet_id;
    unit = battle_unit_claim_slot(requested_runtime_id);
    if (unit == 0) {
        return 0;
    }
    runtime_id = unit->runtime_id;
    unit->spritesheet_id = id;
    slot = unit->spritesheet_vram_slot = battle_gfx_claim_spritesheet_slot(spritesheet_id);
    if (slot == 0xffff) {
        g_battle_unit_slot_states[unit->runtime_id].in_use = 0;
        return 0;
    }
    if (id >= BATTLE_SPRITESHEET_ID_JOB_FIRST && id < BATTLE_SPRITESHEET_ID_CHOCOBO && stats != 0) {
        palette = stats->sprite_palette;
    }
    if (palette >= 6) {
        palette = 0;
    }
    unit->stored_palette = palette;
    unit->statuses_to_remove_5_6 = 0;
    unit->statuses_to_remove_1_4 = 0;
    unit->statuses_to_add_5_6 = 0;
    unit->statuses_to_add_1_4 = 0;
    unit->status_flags_5_6 = 0;
    unit->status_flags_1_4 = 0;
    battle_gfx_load_spritesheet_into_vram_slot(data, slot, runtime_id, unit, palette);
    tile = map_get_tile_data_pointer(map_x, map_y, map_level);
    battle_gfx_check_tile_status_palette_mod(unit, tile->flags.shading, 1);
    map_load_palette_data(&g_battle_gfx_shadow_palette, MAP_CLUT_ROW_UNIT_SHADOWS, runtime_id, 0);
    unit->clut = GetClut(runtime_id << 4, MAP_CLUT_ROWS_Y + MAP_CLUT_ROW_UNITS);
    unit->tpage = slot + BATTLE_GFX_TPAGE_SPRITESHEET_SLOT_0;
    if (flags == 0) {
        unit->draw_enabled = 1;
        unit->animation.trigger = 1;
    } else {
        unit->draw_enabled = 0;
        unit->animation.trigger = 0;
    }
    unit->standing_on_battle_id_plus_one = 0;
    unit->stats = stats;
    unit->requested_animation = 0;
    unit->facing = facing * 1024;
    unit->camera_facing_sixteenth = unit->camera_facing_quadrant = unit->attack_facing = -1;
    unit->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
    unit->horizontal_flip_flag = 0;
    unit->depth_height_offset = 0;
    unit->float_bob_offset = 0;
    unit->float_bob_phase = 0;
    unit->step_phase = MOVE_STEP_PHASE_NONE;
    unit->path.count = 0;
    unit->sprite_display_flags = 0;
    unit->layer_priority = 0;
    unit->turn_action_flags = 0;
    unit->special_graphic_y_offset = 0;
    unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
    unit->distortion_phase = 0;
    tile_x = map_x;
    unit->map_x = tile_x;
    unit->movement.suppress_facing_update = 0;
    tile_y = map_y;
    unit->map_y = tile_y;
    unit->movement.suppress_palette_update = 0;
    tile_z = map_level;
    unit->map_z = tile_z;
    unit->movement.alternate_short_hop = 0;
    unit->movement.floating = 0;
    unit->movement.force_animation_update = 0;
    unit->movement.destination_x = tile_x;
    unit->movement.destination_y = tile_y;
    unit->movement.destination_z = tile_z;
    battle_unit_set_move_and_world_coords(unit);
    cursor.animation = &unit->animation;
    unit->velocity.vz = 0;
    unit->velocity.vy = 0;
    unit->velocity.vx = 0;
    unit->walk_speed = 0x2000;
    unit->seq_offset.vz = 0;
    unit->seq_offset.vy = 0;
    unit->seq_offset.vx = 0;
    unit->screen_offset.vz = 0;
    unit->screen_offset.vy = 0;
    unit->screen_offset.vx = 0;
    unit->event_offset.vz = 0;
    unit->event_offset.vy = 0;
    unit->event_offset.vx = 0;
    unit->fade_out_phase = BATTLE_UNIT_FADE_OUT_NONE;
    unit->shadow_graphic_trigger = 1;
    unit->shadow_dirty = 1;
    unit->shadow_counter = 0;
    unit->numeric_display.active = 0;
    unit->numeric_display.selector = 0;
    unit->numeric_display.progress = 0;
    unit->numeric_display.displays[0] = battle_gfx_init_numeric_sprite_display(runtime_id, 0);
    unit->numeric_display.displays[1] = battle_gfx_init_numeric_sprite_display(runtime_id, 1);
    unit->numeric_display.displays[2] = battle_gfx_init_numeric_sprite_display(runtime_id, 2);
    unit->numeric_display.displays[0]->scale_x = unit->numeric_display.displays[0]->scale_y
        = unit->numeric_display.displays[1]->scale_x = unit->numeric_display.displays[1]->scale_y
        = unit->numeric_display.displays[2]->scale_x = unit->numeric_display.displays[2]->scale_y = ONE;
    unit->numeric_display.displays[0]->tpage = unit->numeric_display.displays[1]->tpage
        = unit->numeric_display.displays[2]->tpage = BATTLE_GFX_TPAGE_FRAME;
    unit->numeric_display.displays[0]->clut = unit->numeric_display.displays[1]->clut
        = unit->numeric_display.displays[2]->clut
        = unit->clut + 0x100; /* the unit's CLUT of MAP_CLUT_ROW_ACTION_RESULTS */
    unit->numeric_display.displays[0]->part_count = unit->numeric_display.displays[1]->part_count
        = unit->numeric_display.displays[2]->part_count = 1;
    unit->status_bubble_active = 0;
    unit->status_bubble_id = 0;
    unit->status_bubble_timer = 0;
    unit->status_bubble_display = battle_gfx_init_status_bubble_sprite_display(runtime_id);
    unit->item_display_active = 0;
    unit->item_display = battle_gfx_init_item_sprite_display(runtime_id);
    unit->position_copies_active = 0;
    unit->animation.display = battle_gfx_init_unit_sprite_display(runtime_id);
    cursor.animation->graphic_type = BATTLE_SPRITE_GRAPHIC_UNIT;
    unit->animation.shp = unit->animation.saved_shp = battle_gfx_get_unit_shp_by_spritesheet_id(id);
    unit->animation.seq = unit->animation.saved_seq = battle_gfx_get_unit_seq_by_spritesheet_id(id);
    cursor.animation->wait = 1;
    cursor.animation->restart_count = 0;
    cursor.animation->script_pos = 0;
    cursor.animation->sequence_id = BATTLE_UNIT_ANIMATION_IDLE << 1;
    cursor.animation->frame = 0;
    cursor.animation->flags = 0;
    cursor.animation->wait_bias = 0;
    cursor.animation->frame_bias = 0;
    cursor.animation->repeat_count = 0;
    unit->animation.display->scale_x = ONE;
    unit->animation.display->scale_y = ONE;
    unit->animation.display->rotation = 0;
    battle_unit_set_animation_based_on_status(unit);
    battle_gfx_run_unit_seq_script(unit, cursor.animation, 1);
    for (i = 0; i < 3; i++) {
        cursor.block = &unit->sprite_blocks[i];
        cursor.block->display = battle_gfx_init_weapon_sprite_display(runtime_id, i);
        if (i == 0) {
            cursor.block->graphic_type = BATTLE_SPRITE_GRAPHIC_WEAPON;
            cursor.block->shp = battle_gfx_get_weapon_shp_by_spritesheet_id(id);
            cursor.block->seq = battle_gfx_get_weapon_seq_by_spritesheet_id(id);
        } else {
            cursor.block->graphic_type = BATTLE_SPRITE_GRAPHIC_EFFECT;
            cursor.block->shp = battle_gfx_get_effect_shp_data(id);
            cursor.block->seq = battle_gfx_get_effect_seq_data(id);
        }
        cursor.block->trigger = 0;
        cursor.block->wait = 1;
        cursor.block->script_pos = 0;
        cursor.block->sequence_id = 0;
        cursor.block->frame = 0;
        cursor.block->flags = 0;
        cursor.block->wait_bias = 0;
        cursor.block->frame_bias = 0;
        cursor.block->repeat_count = 0;
        cursor.block->display->scale_x = ONE;
        cursor.block->display->scale_y = ONE;
        cursor.block->display->rotation = 0;
        if ((s16)id < BATTLE_SPRITESHEET_ID_CHOCOBO && (s16)id < BATTLE_SPRITESHEET_ID_VELIUS
            && (s16)id >= BATTLE_SPRITESHEET_ID_BOY && (s16)id != BATTLE_SPRITESHEET_ID_DAMI) {
            battle_gfx_run_wep_eff_seq_script(unit, &unit->sprite_blocks[i], i, 1);
        }
    }
    head = &g_battle_unit_list_head;
    unit->previous = *head;
    *head = unit;
    battle_gfx_set_thrown_item_graphic_palette(unit->stats->equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON], unit);
    status_init_special_flag_enabling(unit->stats->battle_id);
    unit->movement.suppress_sfx_and_landing_effects = 1;
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_unit_set_animation_based_on_status(unit);
    unit->movement.suppress_sfx_and_landing_effects = 0;
    return unit;
}
