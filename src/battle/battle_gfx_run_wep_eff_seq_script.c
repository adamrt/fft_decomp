#include "fft/battle.h"

/* Run a weapon/effect sprite's animation script up to its next frame.
 *
 * Script bytes other than 0xff select a frame; 0xff introduces an opcode.
 * The frame's parts are expanded into the block's display record, using the
 * weapon graphic row for weapon sprites.
 *
 * Matching constraints: the size table is indexed by element (`>> 10`); the
 * shift that combine later folds into `>> 7` keeps the script loop just large
 * enough that loop.c does not hoist the constant 1. `part` and `attr` are
 * function-scope (global allocation) while the other part temporaries are
 * block-local and `mode` is single-set, which gives the target's register
 * ties and schedule. */
void battle_gfx_run_wep_eff_seq_script(
    battle_unit_misc_data_t* unit, battle_unit_sprite_block_t* sprite, s32 index, s32 advance) {
    battle_gfx_weapon_shp_t* shp;
    u8** seq;
    u8 op;
    u8* data;
    u16 anim;
    s16 pos;
    u16 frame;
    u16 graphic_v;
    battle_gfx_source_frame_t* src;
    battle_gfx_sprite_display_data_t* display;
    battle_unit_sprite_block_t* block;
    battle_gfx_source_part_t* part;
    s16 offset;
    u16 count;
    u8 slot;
    u8 animation;
    u32 parts;
    u8 flags;
    u16 i;
    u16 attr;

    anim = sprite->animation_id;
    graphic_v = g_battle_gfx_item_graphic_data[unit->used_item_or_weapon_id * 2 + 1] * 8;
    pos = sprite->script_pos;
    shp = sprite->shp;
    seq = sprite->seq;
    if (advance == 0) {
        pos -= 2;
    }
    if (anim > 0x100) {
        main_system_handle_animation_exception(9);
    }
    if (seq[anim] == (u8*)-1) {
        main_system_handle_animation_exception(9);
    }
    for (;;) {
        data = seq[anim];
        if (data[pos] == BATTLE_SEQ_OP_PREFIX) {
            pos++;
            op = seq[anim][pos++];
            switch (op) {
            case BATTLE_SEQ_OP_FLIP_HORIZONTAL:
                sprite->flags ^= 2;
                break;
            case BATTLE_SEQ_OP_FLIP_VERTICAL:
                sprite->flags ^= 4;
                break;
            case BATTLE_SEQ_OP_END_ANIMATION:
                sprite->trigger = 0;
            case BATTLE_SEQ_OP_PAUSE_ANIMATION:
                sprite->wait = 0;
                return;
            case BATTLE_SEQ_OP_INCREMENT_LOOP:
                sprite->counter_0c++;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_SET_Y_ROTATION_0:
                sprite->display->y_rotation = 0;
                break;
            case BATTLE_SEQ_OP_QUEUE_THROW_ANIMATION:
                battle_effect_set_evade_type_data_item_and_throw_stone_hardcoding(unit);
            case BATTLE_SEQ_OP_UNKNOWN_E6:
            case BATTLE_SEQ_OP_UNKNOWN_E7:
            case BATTLE_SEQ_OP_UNKNOWN_E8:
            case BATTLE_SEQ_OP_UNKNOWN_EA:
            case BATTLE_SEQ_OP_UNKNOWN_F1:
            case BATTLE_SEQ_OP_UNKNOWN_F8:
            case BATTLE_SEQ_OP_UNKNOWN_FB:
                pos += 2;
                break;
            case BATTLE_SEQ_OP_RELOAD_ANIMATION:
                pos = sprite->saved_script_pos;
                anim = sprite->saved_animation_id;
                break;
            case BATTLE_SEQ_OP_SET_LAYER_PRIORITY:
                unit->layer_priority = seq[anim][pos++];
                break;
            case BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_2:
                if (g_battle_state_animation_continue_check != 0) {
                    offset = (s8)seq[anim][pos];
                    pos = offset + (s16)(pos - 2);
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_PLAY_SOUND:
                battle_sound_play_movement_sfx(unit, seq[anim][pos++]);
                break;
            case BATTLE_SEQ_OP_SET_SLOWDOWN:
                sprite->wait_bias = (s8)seq[anim][pos++];
                break;
            case BATTLE_SEQ_OP_SET_FRAME_OFFSET:
                sprite->frame_bias = (s8)seq[anim][pos++];
                break;
            case BATTLE_SEQ_OP_OVERRIDE_ANIMATION:
                sprite->saved_animation_id = anim;
                anim = seq[anim][pos++];
                sprite->saved_script_pos = pos;
                pos = 0;
                break;
            case BATTLE_SEQ_OP_UNKNOWN_F9:
                unit->screen_offset.vx += (s8)seq[anim][pos++];
                unit->screen_offset.vy += (s8)seq[anim][pos++];
                break;
            case BATTLE_SEQ_OP_WAIT:
                offset = (s8)seq[anim][pos++];
                count = seq[anim][pos++];
                if (sprite->loop_count != 0) {
                    if (--sprite->loop_count != 0) {
                        pos = offset + (s16)(pos - 4);
                    }
                } else {
                    pos = offset + (s16)(pos - 4);
                    sprite->loop_count = count;
                }
                break;
            case BATTLE_SEQ_OP_QUEUE_SPRITE_ANIM:
                slot = seq[anim][pos++];
                animation = seq[anim][pos++];
                if (slot == 0) {
                    main_system_handle_animation_exception(0xd);
                }
                block = &unit->sprite_blocks[slot - 1];
                block->animation_id = animation;
                block->script_pos = 0;
                block->wait = 1;
                block->wait_bias = 0;
                block->frame_bias = 0;
                block->loop_count = 0;
                block->trigger = 1;
                break;
            case BATTLE_SEQ_OP_SAVE_Y_SPIN:
                /* The one-trip loop is load-bearing: flow weights register
                 * references by loop depth, and the extra weight lets `count`
                 * take a0 ahead of `offset` (target a0/a1 in 0xfc and 0xe5),
                 * as in battle_gfx_run_unit_seq_script. */
                do {
                    count = seq[anim][pos++];
                    count |= seq[anim][pos++] << 8;
                    sprite->display->y_rotation = count;
                } while (0);
                break;
            case BATTLE_SEQ_OP_UNKNOWN_F7:
            case BATTLE_SEQ_OP_MOVE_UNIT:
                pos += 3;
                break;
            case BATTLE_SEQ_OP_WEAPON_SHEATHE_CHECK_1:
                if (g_battle_state_animation_continue_check == 1) {
                    offset = (s8)seq[anim][pos];
                    pos = offset + (s16)(pos - 2);
                } else {
                    pos++;
                }
                break;
            case BATTLE_SEQ_OP_HOLD_WEAPON:
                offset = (s8)seq[anim][pos];
                pos = offset + (s16)(pos - 2);
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
            case BATTLE_SEQ_OP_POST_GENERIC_ATTACK:
            case BATTLE_SEQ_OP_CLEAR_SHADOW:
            case BATTLE_SEQ_OP_SET_SHADOW:
                break;
            default:
                main_system_handle_animation_exception(0xd);
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
                main_system_handle_animation_exception(8);
            }
            if (src == (battle_gfx_source_frame_t*)-1) {
                main_system_handle_animation_exception(8);
            }
            display = sprite->display;
            parts = display->part_count = (src->part_count_and_rotation & 7) + 1;
            display->y_rotation = g_battle_gfx_sprite_y_rotations[src->part_count_and_rotation >> 3];
            if (unit->sprite_display_flags.half & 2) {
                display->y_rotation = -display->y_rotation;
            }
            if (parts > 3) {
                parts = 3;
            }
            flags = src->flags;
            switch (sprite->graphic_type) {
            case 1:
                display->spritesheet_id = (flags & 0x60) | 9;
                display->clut = unit->vram_palette_id + index * 64 + 0x80;
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
                    size = (attr & 0x3c00) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & 0x3ff;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8 + graphic_v;
                    mode = (flags & 0x61) | ((attr & 0xc000) >> 13);
                    /* The callee takes narrow parameters; this caller passes
                     * sign-extended coordinates and full-width values. */
                    ((void (*)(battle_gfx_sprite_display_data_t*, s32, s32, s32, s32, s32, s32, s32,
                        s32))battle_gfx_store_sprite_display_data)(
                        display, i, part->x_shift, part->y_shift, u, v, (s16)(width * 8), (s16)(height * 8), mode);
                }
                break;
            case 2:
                display->spritesheet_id = (flags & 0x60) | 8;
                display->clut = unit->vram_palette_id + index * 64 + 0x80;
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
                    size = (attr & 0x3c00) >> 10;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & 0x3ff;
                    u = (tile & 0x1f) * 8;
                    v = (tile >> 5) * 8;
                    mode = (flags & 0x61) | ((attr & 0xc000) >> 13);
                    /* Same int-argument view as the call above. */
                    ((void (*)(battle_gfx_sprite_display_data_t*, s32, s32, s32, s32, s32, s32, s32,
                        s32))battle_gfx_store_sprite_display_data)(
                        display, i, part->x_shift, part->y_shift, u, v, (s16)(width * 8), (s16)(height * 8), mode);
                }
                break;
            }
            sprite->script_pos = pos;
            sprite->frame = frame;
            sprite->animation_id = anim;
            return;
        }
    }
}
