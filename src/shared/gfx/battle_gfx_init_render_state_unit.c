#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* One-time setup of the sprite renderer: the numeric display matrix's third row
 * and column, a depth of 0x200 for the four GTE input vertices the part quads are
 * built from, the primitive ring index, and the primitive pool, which starts
 * right after the sixteen unit unit records. */
void battle_gfx_init_render_state(void) {
    g_battle_gfx_numeric_display_matrix.m[2][2] = ONE;
    g_battle_gte_rtps_vxy0.vz = 0x200;
    g_battle_gte_rtpt_vxy2.vz = 0x200;
    g_battle_gte_rtpt_vxy1.vz = 0x200;
    g_battle_gte_rtpt_vxy0.vz = 0x200;
    g_battle_gfx_numeric_display_matrix.m[2][1] = 0;
    g_battle_gfx_numeric_display_matrix.m[2][0] = 0;
    g_battle_gfx_numeric_display_matrix.m[1][2] = 0;
    g_battle_gfx_numeric_display_matrix.m[0][2] = 0;
    g_battle_gfx_numeric_display_matrix.t[2] = 0;
    g_battle_gfx_counter = 0;
    g_battle_gfx_previous_counter = 0;
    D_800b6290 = 0;
    D_800b628c = 0;
    /* The in-use 32-bit value just past the last g_battle_units record. */
    g_battle_gfx_sprite_prim_pool = (POLY_FT4*)&g_battle_unit_slot_states[BATTLE_UNIT_RUNTIME_SLOT_COUNT].in_use;
}

/* Reset a unit's own sprite display: neutral color, no flags on its eight
 * parts and none of them in use. */
battle_gfx_sprite_display_data_t* battle_gfx_init_unit_sprite_display(s32 runtime_id) {
    s32 body_part;
    battle_gfx_sprite_display_data_t* display;

    display = &g_battle_units[runtime_id].unit_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (body_part = 7; body_part >= 0; body_part--) {
        display->parts[body_part].flags = 0;
    }
    display->part_count = 0;
    return display;
}

/* Reset the display of one of a unit's three sprite blocks: neutral color,
 * no flags on its three parts and none of them in use. */
battle_gfx_sprite_display_data_t* battle_gfx_init_weapon_sprite_display(s32 unit_index, s32 display_type) {
    s32 weapon_part;
    battle_gfx_sprite_display_data_t* display;

    display = &g_battle_units[unit_index].weapon_sprite_displays[display_type].header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (weapon_part = 2; weapon_part >= 0; weapon_part--) {
        display->parts[weapon_part].flags = 0;
    }
    display->part_count = 0;
    return display;
}

/* Resets one of a unit's three numeric sprite displays to neutral color and no
 * parts. A display_section outside 0..2 leaves `display` unset; the only caller
 * passes 0, 1 and 2. */
battle_gfx_sprite_display_data_t* battle_gfx_init_numeric_sprite_display(s32 unit_index, s32 display_section) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    switch (display_section) {
    case 0:
        display = &g_battle_units[unit_index].numeric_sprite_displays[0].header;
        break;
    case 1:
        display = &g_battle_units[unit_index].numeric_sprite_displays[1].header;
        break;
    case 2:
        display = &g_battle_units[unit_index].numeric_sprite_displays[2].header;
        break;
    }

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    for (i = 0; i < 1; i++)
        display->parts[i].flags = 0;
    display->part_count = 0;
    return display;
}

/* Resets a unit's status bubble display to one 14 x 12 part at (0, 0xb0) of the
 * FRAME.BIN page with CLUT 7 of the sprite row, neutral color and scale ONE. */
battle_gfx_sprite_display_data_t* battle_gfx_init_status_bubble_sprite_display(s32 unit_index) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = &g_battle_units[unit_index].status_bubble_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = BATTLE_GFX_TPAGE_FRAME;
    display->clut = MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7);
    display->rotation = 0;
    display->part_count = 1;
    for (i = 0; i < 1; i++) {
        display->parts[i].y_shift = 0;
        display->parts[i].x_shift = 0;
        display->parts[i].width = 0xe;
        display->parts[i].height = 0xc;
        display->parts[i].u = 0;
        display->parts[i].v = 0xb0;
        display->parts[i].flags = 0;
    }
    return display;
}

/* Resets a unit's item sprite display to one 16 x 16 part of the ITEM.BIN page
 * with CLUT 7 of the sprite row, neutral color and scale ONE. */
battle_gfx_sprite_display_data_t* battle_gfx_init_item_sprite_display(s32 runtime_id) {
    battle_gfx_sprite_display_data_t* display;
    s32 i;

    display = &g_battle_units[runtime_id].item_sprite_display.header;
    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = BATTLE_GFX_TPAGE_ITEM;
    display->clut = MAP_CLUT(MAP_CLUT_ROW_SPRITES, 7);
    display->rotation = 0;
    display->part_count = 1;
    for (i = 0; i < 1; i++) {
        display->parts[i].y_shift = 0;
        display->parts[i].x_shift = 0;
        display->parts[i].width = 0x10;
        display->parts[i].height = 0x10;
        display->parts[i].flags = 0;
    }
    return display;
}

/* Empty. battle_unit_remove passes the removed unit's sprite display, a member of its
 * g_battle_units record that battle_gfx_init_unit_sprite_display handed out. */
void battle_gfx_release_unit_sprite_display_stub(battle_gfx_sprite_display_data_t* unused_display) {
}

/* Starts a frame of sprite drawing: the GTE offset is reset to (0, 0) and the
 * primitive ring index reached last frame is recorded. */
void battle_gfx_init_render_frame(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_battle_gfx_previous_counter = g_battle_gfx_counter;
}

/* Takes the next of the 256 primitives of g_battle_gfx_sprite_prim_pool, used as
 * a ring, and returns its index. */
s16 battle_gfx_increment_counter(void) {
    g_battle_gfx_counter = (g_battle_gfx_counter + 1) % 0x100;
    return g_battle_gfx_counter;
}

/* Queue one POLY_FT4 per sprite part in [start, end) of a sprite display.
 *
 * The display's rotation, the caller's angle and scale, and position build
 * the GTE matrix. `mode` supplies the whole-sprite flags and blend selector:
 * a flip bit mirrors each part's place about the sprite's origin, and its
 * picture unless the part's own flip bit is set too. Either one's
 * semi-transparent bit applies, and the blend bits of `mode` are added to
 * the display's texture page. A part flagged BATTLE_SPRITE_DISPLAY_NEARER is
 * queued ahead of `ot` by the camera pitch and zoom while the pitch is below
 * a quarter turn.
 *
 * The unused VECTOR reproduces the target's 16-byte locals area. The nested
 * test with its duplicated `addPrim(ot, prim)` arm is the original shape: jump
 * optimization merges the arms, but the references counted before that merge
 * give `ot` the allocation priority that places it in $s5 ahead of `display`. */
void battle_gfx_construct_polygon_data_for_units(battle_gfx_sprite_display_data_t* display, s32 end, s32 start,
    SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot) {
    VECTOR unused;
    battle_gfx_sprite_part_display_data_t* part;
    POLY_FT4* prim;
    s32 i;
    s16 flags;
    s16 index;
    s16 x;
    s16 y;
    s16 cosine;
    s16 sine;

    cosine = rcos(angle + display->rotation);
    g_battle_gfx_numeric_display_matrix.m[0][0] = ((cosine * scale->vx) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][1] = ((cosine * scale->vy) >> 12) * display->scale_y >> 12;
    sine = rsin(angle + display->rotation);
    g_battle_gfx_numeric_display_matrix.m[0][1] = ((-sine * scale->vy) >> 12) * display->scale_y >> 12;
    g_battle_gfx_numeric_display_matrix.m[1][0] = ((sine * scale->vx) >> 12) * display->scale_x >> 12;
    g_battle_gfx_numeric_display_matrix.t[0] = position->vx;
    g_battle_gfx_numeric_display_matrix.t[1] = position->vy;
    SetRotMatrix(&g_battle_gfx_numeric_display_matrix);
    SetTransMatrix(&g_battle_gfx_numeric_display_matrix);
    for (i = start; i < end; i++) {
        part = &display->parts[i];
        flags = part->flags;
        index = battle_gfx_increment_counter();
        flags ^= mode;
        prim = g_battle_gfx_sprite_prim_pool + index;
        setlen(prim, 9);
        prim->code = 0x2c;
        if (mode & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            x = -part->x_shift - part->width;
        } else {
            x = part->x_shift;
        }
        if (mode & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) {
            y = -part->y_shift - part->height;
        } else {
            y = part->y_shift;
        }
        if (flags & BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x;
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x + part->width;
        } else {
            g_battle_gte_rtpt_vxy0.vx = g_battle_gte_rtpt_vxy2.vx = x;
            g_battle_gte_rtpt_vxy1.vx = g_battle_gte_rtps_vxy0.vx = x + part->width;
        }
        if (flags & BATTLE_SPRITE_DISPLAY_VERTICAL_FLIP) {
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y;
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y + part->height;
        } else {
            g_battle_gte_rtpt_vxy0.vy = g_battle_gte_rtpt_vxy1.vy = y;
            g_battle_gte_rtpt_vxy2.vy = g_battle_gte_rtps_vxy0.vy = y + part->height;
        }
        gte_ldv3_split(&g_battle_gte_rtpt_vxy0, &g_battle_gte_rtpt_vxy1, &g_battle_gte_rtpt_vxy2);
        gte_rtpt();
        if ((flags | mode) & BATTLE_SPRITE_DISPLAY_SEMI_TRANSPARENT) {
            prim->code |= 2;
        } else {
            prim->code &= ~2;
        }
        prim->tpage = display->tpage | (mode & BATTLE_SPRITE_DISPLAY_BLEND_MASK);
        prim->clut = display->clut;
        gte_stsxy3_split(&prim->x0, &prim->x1, &prim->x2);
        gte_ldv0(&g_battle_gte_rtps_vxy0);
        gte_rtps();
        /* The retail color transfer combines red and green in one 16-bit value. */
        *(u16*)&prim->r0 = *(u16*)&display->color.r;
        prim->b0 = display->color.b;
        gte_stsxy(&prim->x3);
        prim->u0 = prim->u2 = part->u;
        prim->u1 = prim->u3 = prim->u0 + part->width;
        prim->v0 = prim->v1 = part->v;
        prim->v2 = prim->v3 = prim->v0 + part->height;
        if (flags & BATTLE_SPRITE_DISPLAY_NEARER) {
            if ((g_camera_render_state.vx & 0xfff) < 0x400) {
                addPrim(ot - ((g_camera_render_state.vx & 0xfff) - 0x12e) * g_camera_zoom.vz / 368640 - 1, prim);
            } else {
                addPrim(ot, prim);
            }
        } else {
            addPrim(ot, prim);
        }
    }
}
