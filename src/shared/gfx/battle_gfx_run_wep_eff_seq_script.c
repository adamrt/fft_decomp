#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"

/* Run a weapon or effect sprite block's sequence up to its next frame
 * instruction, or to its end, as battle_gfx_run_unit_seq_script does for the
 * unit's own sprite. The unit-only commands are errors here, the ones that
 * move a unit only skip their operands, and BATTLE_SEQ_OP_STRIKE_TARGETS and
 * the two shadow commands do nothing.
 *
 * A frame id counts from the first frame of the unit's weapon type
 * (battle_gfx_weapon_shp_t.first_frame). The frame's parts, three at most,
 * are expanded into the block's display: a weapon block draws them from the
 * weapon sheet, at the row of the used item's graphic, and an effect block
 * from the effect sheet. `index` is the block's number; it selects the CLUT
 * row.
 *
 * Matching constraints: the size table is indexed by element (`>> 10`); the
 * shift that combine later folds into `>> 7` keeps the script loop just large
 * enough that loop.c does not hoist the constant 1. `part` and `attr` are
 * function-scope (global allocation) while the other part temporaries are
 * block-local and `mode` is single-set, which gives the target's register
 * ties and schedule. */
void battle_gfx_run_wep_eff_seq_script(
    battle_unit_t* unit, battle_unit_sprite_block_t* sprite, s32 index, s32 advance) {
    battle_gfx_weapon_shp_t* shp;
    u8** seq;
    u8 op;
    u8* data;
    u16 sequence_id;
    s16 pos;
    u16 frame;
    u16 graphic_v;
    battle_gfx_source_frame_t* src;
    battle_gfx_sprite_display_data_t* display;
    battle_unit_sprite_block_t* block;
    battle_gfx_source_part_t* part;
    s16 offset;
    u16 value;
    u8 block_id;
    u8 block_sequence;
    u32 parts;
    u8 flags;
    u16 i;
    u16 attr;

    sequence_id = sprite->sequence_id;
    graphic_v = g_battle_gfx_item_graphic_data[unit->used_item_or_weapon_id].texture_row * 8;
    pos = sprite->script_pos;
    shp = sprite->shp;
    seq = sprite->seq;
    if (advance == 0) {
        pos -= 2;
    }
    if (sequence_id > 0x100) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    if (seq[sequence_id] == (u8*)-1) {
        main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_ANIMATION);
    }
    for (;;) {
        data = seq[sequence_id];
        if (data[pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq[sequence_id][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                sprite->flags ^= BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                sprite->flags ^= BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
                sprite->trigger = 0;
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                sprite->wait = 0;
                return;
            case BATTLE_SEQ_OP_RESTART:
                sprite->restart_count++;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_CLEAR_ROTATION:
                sprite->display->rotation = 0;
                break;
            case BATTLE_SEQ_OP_LAUNCH_PROJECTILE:
                effect_start_action_projectile_secondary(unit);
            case BATTLE_SEQ_OP_UNKNOWN_E6:
            case BATTLE_SEQ_OP_UNKNOWN_E7:
            case BATTLE_SEQ_OP_UNKNOWN_E8:
            case BATTLE_SEQ_OP_UNKNOWN_EA:
            case BATTLE_SEQ_OP_UNKNOWN_F1:
            case BATTLE_SEQ_OP_UNKNOWN_F8:
            case BATTLE_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case BATTLE_SEQ_OP_RETURN:
                pos = sprite->saved_script_pos;
                sequence_id = sprite->saved_sequence_id;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                value = seq[sequence_id][pos++];
                sound_play_movement_sfx(unit, value);
                break;
            case BATTLE_SEQ_OP_SET_WAIT_BIAS:
                sprite->wait_bias = (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_BIAS:
                sprite->frame_bias = (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_CALL_SEQUENCE:
                sprite->saved_sequence_id = sequence_id;
                sequence_id = seq[sequence_id][pos++];
                sprite->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_MOVE_SCREEN_OFFSET:
                unit->screen_offset.vx += (s8)seq[sequence_id][pos++];
                unit->screen_offset.vy += (s8)seq[sequence_id][pos++];
                break;
            case BATTLE_SEQ_OP_REPEAT:
                offset = (s8)seq[sequence_id][pos++];
                value = seq[sequence_id][pos++];
                if (sprite->repeat_count != 0) {
                    if (--sprite->repeat_count != 0) {
                        pos += offset - 4;
                    }
                } else {
                    pos += offset - 4;
                    sprite->repeat_count = value;
                }
                break;
            case BATTLE_SEQ_OP_START_BLOCK_SEQUENCE:
                block_id = seq[sequence_id][pos++];
                block_sequence = seq[sequence_id][pos++];
                if (block_id == 0) {
                    main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                }
                block = &unit->sprite_blocks[block_id - 1];
                block->sequence_id = block_sequence;
                block->script_pos = 0;
                block->wait = 1;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->repeat_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_SET_ROTATION:
                value = seq[sequence_id][pos++];
                value |= seq[sequence_id][pos++] << 8;
                sprite->display->rotation = value;

                break;
            case BATTLE_SEQ_OP_UNKNOWN_F7:
            case BATTLE_SEQ_OP_MOVE_UNIT:
                pos += 3;
                break;
            case BATTLE_SEQ_OP_WAIT_FOR_EFFECT_LOAD:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq[sequence_id][pos];
                    pos += offset - 2;
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_JUMP:
                offset = (s8)seq[sequence_id][pos];
                pos += offset - 2;
                break;
            case BATTLE_SEQ_OP_UNKNOWN_D7:
            case BATTLE_SEQ_OP_UNKNOWN_E3:
            case BATTLE_SEQ_OP_UNKNOWN_E4:
            case BATTLE_SEQ_OP_UNKNOWN_E9:
            case BATTLE_SEQ_OP_UNKNOWN_ED:
            case BATTLE_SEQ_OP_MOVE_UNIT_FB:
            case BATTLE_SEQ_OP_MOVE_UNIT_DU:
            case BATTLE_SEQ_OP_MOVE_UNIT_RL:
            case BATTLE_SEQ_OP_UNKNOWN_F3:
            case BATTLE_SEQ_OP_UNKNOWN_F4:
            case BATTLE_SEQ_OP_UNKNOWN_F5:
                pos++;
                break;
            case BATTLE_SEQ_OP_STRIKE_TARGETS:
            case BATTLE_SEQ_OP_HIDE_SHADOW:
            case BATTLE_SEQ_OP_SHOW_SHADOW:
                break;
            default:
                main_system_handle_error(MAIN_SYSTEM_ERROR_SEQ_OPCODE);
                break;
            }
        } else {
            frame = sprite->frame_bias + (data[pos++] + shp->first_frame[unit->equipped_weapon_type]);
            if (advance != 0) {
                sprite->wait = data[pos++] + sprite->wait_bias;
            } else {
                pos++;
            }
            src = shp->frames[frame];
            if (frame > 0x200) {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
            }
            if (src == (battle_gfx_source_frame_t*)-1) {
                main_system_handle_error(MAIN_SYSTEM_ERROR_SHP_FRAME);
            }
            display = sprite->display;
            parts = display->part_count = (src->part_count_and_rotation & 7) + 1;
            display->rotation = g_battle_gfx_sprite_y_rotations[src->part_count_and_rotation >> 3];
            if (unit->sprite_display_flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
                display->rotation = -display->rotation;
            }
            if (parts > 3) {
                parts = 3;
            }
            flags = src->flags;
            switch (sprite->graphic_type) {
            case BATTLE_SPRITE_GRAPHIC_WEAPON:
                display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_WEAPON;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8 + graphic_v;
                    mode = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                    battle_gfx_store_sprite_display_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            case BATTLE_SPRITE_GRAPHIC_EFFECT:
                display->tpage = (flags & BATTLE_SPRITE_DISPLAY_BLEND_MASK) | BATTLE_GFX_TPAGE_EFFECT;
                display->clut = unit->clut + index * 64 + 0x80; /* MAP_CLUT_ROW_ITEM_A or _B */
                for (i = 0; i < parts; i++) {
                    u32 size;
                    u32 tile;
                    s32 width;
                    s32 height;
                    s32 u;
                    s32 v;
                    u32 mode;

                    part = &src->parts[i];
                    attr = part->attributes;
                    size = (attr & BATTLE_SHP_PART_SIZE_MASK) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & BATTLE_SHP_PART_TILE_MASK;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8;
                    mode = (flags & (BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT | BATTLE_SPRITE_DISPLAY_BLEND_MASK))
                        | ((attr & BATTLE_SHP_PART_FLIP_MASK) >> 13);
                    battle_gfx_store_sprite_display_data(
                        display, i, part->x_shift, part->y_shift, u, v, width * 8, height * 8, mode);
                }
                break;
            }
            sprite->script_pos = pos;
            sprite->frame = frame;
            sprite->sequence_id = sequence_id;
            return;
        }
    }
}
