#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/main_runtime.h"

/* s16 view of battle_gfx_store_sprite_display_data: the target sign-extends
 * u, v, width and height, which its u16 prototype would andi 0xffff. */
#define STORE_PART                                                                                                     \
    ((void (*)(battle_gfx_sprite_display_data_t*, s32, s16, s16, s16, s16, s16, s16,                                   \
        s32))battle_gfx_store_sprite_display_data)

/* Build a unit's sprite-part records for one SHP frame, including trap sheets
 * (id >= 0x9b) and Altima's split parts. */
void battle_gfx_load_trap_and_unit_frame_parts(
    battle_unit_misc_data_t* unit, battle_unit_anim_state_t* sprite, u16 frame, u16 animation) {
    s32* frame_table;
    s32 frame_data;
    battle_gfx_sprite_display_data_t* display;
    battle_gfx_source_part_t* part;
    u32 y_offset;
    u32 count;
    u16 i;
    u8 flags;
    u16 attr;
    u16 size;
    s32 u;
    u32 tile_index;
    u32 part_flags;
    u32 row;
    u32 mask;

    y_offset = (u16)unit->special_graphic_y_offset;
    battle_gfx_select_unit_shp_frame(unit, sprite, frame, animation, &frame_table, &frame_data);
    if (frame_data == -1) {
        main_system_handle_animation_exception(8);
    }
    display = sprite->display;
    count = display->part_count = (((battle_gfx_source_frame_t*)frame_data)->part_count_and_rotation & 7) + 1;
    display->y_rotation
        = g_battle_gfx_sprite_y_rotations[((battle_gfx_source_frame_t*)frame_data)->part_count_and_rotation >> 3];
    if (count > 8) {
        count = 8;
        main_system_handle_animation_exception(10);
    }
    flags = ((battle_gfx_source_frame_t*)frame_data)->flags;
    if (unit->spritesheet_id >= 0x9b) {
        display->spritesheet_id = (flags & 0x60) | 0xb;
        display->clut = unit->vram_palette_id;
        for (i = 0; i < count; i++) {
            part = &((battle_gfx_source_frame_t*)frame_data)->parts[i];
            attr = part->attributes;
            size = (attr & 0x3c00) >> 10;
            {
                u32 tile;
                s32 part_u;
                s16 part_v;
                s32 width;
                s32 height;
                width = g_battle_gfx_part_sizes[size].width;
                height = g_battle_gfx_part_sizes[size].height;
                tile = attr & 0x3ff;
                part_u = (tile & 0x1f) * 8;
                part_v = y_offset + (tile >> 5) * 8;
                STORE_PART(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8, height * 8,
                    (flags & 0x61) | ((attr & 0xc000) >> 13));
            }
        }
    } else if (frame < (u32)sprite->shp[0]) {
        display->spritesheet_id = unit->vram_spritesheet_id | (flags & 0x60);
        display->clut = unit->vram_palette_id;
        i = 0;
        if (count != 0) {
            mask = flags & 0x61;
            /* Emits nothing; keeps the target's separate `andi 0xff` of the
             * hoisted mask, which combine would fold into the `andi 0x61`. */
            __asm__("" : "=r"(mask) : "0"(mask));
            do {
                part = &((battle_gfx_source_frame_t*)frame_data)->parts[i];
                attr = part->attributes;
                size = (attr & 0x3c00) >> 10;
                if (size == 0xe) {
                    if (unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_SECOND_FORM
                        || unit->spritesheet_id == BATTLE_SPRITESHEET_ID_ALTIMA_FIRST_FORM) {
                        u32 tile;
                        s32 part_u;
                        s16 part_v;
                        s32 width;
                        s32 height;
                        tile = attr & 0x3ff;
                        part_u = (tile & 0x1f) * 8;
                        part_v = y_offset + (tile >> 5) * 8;
                        width = g_battle_gfx_part_sizes[size].width;
                        height = g_battle_gfx_part_sizes[size].height;
                        STORE_PART(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8, height * 8,
                            (u8)mask | ((attr & 0xc000) >> 13));
                    } else {
                        tile_index = attr & 0x3ff;
                        u = (tile_index & 0x1f) * 8;
                        row = tile_index >> 5;
                        part_flags = (u8)mask | ((attr & 0xc000) >> 13);
                        STORE_PART(display, i, part->x_shift, (s8)part->y_shift + 0x18, u, row * 8 + 0x18, 0x30, 0x18,
                            part_flags);
                        STORE_PART(
                            display, i + 1, part->x_shift, part->y_shift, u, row * 8, 0x30, 0x18, part_flags | 0x80);
                        display->part_count++;
                        return;
                    }
                } else {
                    u32 tile;
                    s32 part_u;
                    s16 part_v;
                    s32 width;
                    s32 height;
                    width = g_battle_gfx_part_sizes[size].width;
                    height = g_battle_gfx_part_sizes[size].height;
                    tile = attr & 0x3ff;
                    part_u = (tile & 0x1f) * 8;
                    part_v = y_offset + (tile >> 5) * 8;
                    STORE_PART(display, i, part->x_shift, part->y_shift, part_u, part_v, width * 8, height * 8,
                        (u8)mask | ((attr & 0xc000) >> 13));
                }
            } while (++i < count);
        }
    } else if (frame < 0xd2) {
        if ((g_battle_gfx_vram_slots[0].owner & 0x1f) == unit->unit_id) {
            battle_gfx_load_unit_frame_parts(unit, display, (battle_gfx_source_frame_t*)frame_data, 4);
        } else if ((g_battle_gfx_vram_slots[1].owner & 0x1f) == unit->unit_id) {
            battle_gfx_load_unit_frame_parts(unit, display, (battle_gfx_source_frame_t*)frame_data, 5);
        }
    } else if (animation < 600) {
        battle_gfx_load_unit_frame_parts(unit, display, (battle_gfx_source_frame_t*)frame_data, 4);
    } else {
        battle_gfx_load_unit_frame_parts(unit, display, (battle_gfx_source_frame_t*)frame_data, 5);
    }
}
