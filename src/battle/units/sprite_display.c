/* Manage unit sprite displays and submit their parts as textured polygons. */

#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/sprite.h"
#include "fft/sprite_sheet.h"
#include "fft/unit.h"
#include "fft/unit_runtime.h"
#include "psx/gte_inline.h"
#include "psx/gte_inline_split.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

static MATRIX g_sprite_display_matrix;
extern s16 g_sprite_prim_index;
extern s16 g_sprite_prim_frame_start_index;

/* Cleared with the sprite counters at render init; nothing reads them. */
extern s16 g_sprite_unread_1;
extern s16 g_sprite_unread_2;

static SVECTOR g_gte_rtpt_vxy0 = { 0 };

static SVECTOR g_gte_rtpt_vxy1 = { 0 };

static SVECTOR g_gte_rtpt_vxy2 = { 0 };

static SVECTOR g_gte_rtps_vxy0 = { 0 };

/* Reset the unit sprite renderer at battle start: the identity third row and column of
 * the sprite display matrix, depth 0x200 in the GTE projection inputs, the frame
 * counters, and the sprite primitive pool, which begins just past the unit slots. */
void sprite_render_state_init(void) {
    g_sprite_display_matrix.m[2][2] = ONE;
    g_gte_rtps_vxy0.vz = 0x200;
    g_gte_rtpt_vxy2.vz = 0x200;
    g_gte_rtpt_vxy1.vz = 0x200;
    g_gte_rtpt_vxy0.vz = 0x200;
    g_sprite_display_matrix.m[2][1] = 0;
    g_sprite_display_matrix.m[2][0] = 0;
    g_sprite_display_matrix.m[1][2] = 0;
    g_sprite_display_matrix.m[0][2] = 0;
    g_sprite_display_matrix.t[2] = 0;
    g_sprite_prim_index = 0;
    g_sprite_prim_frame_start_index = 0;
    g_sprite_unread_2 = 0;
    g_sprite_unread_1 = 0;
    /* The primitive pool begins immediately after the allocation slots. */
    g_sprite_prim_pool = (POLY_FT4*)(g_unit_slot_states + UNIT_RUNTIME_SLOT_COUNT);
}

/* Reset a unit's own sprite display: neutral color, no flags on its eight
 * parts and none of them in use. The display initializers derive addresses from
 * slot bases because direct nested member addresses add instructions. */
sprite_display_t* sprite_display_init_unit(s32 runtime_id) {
    s32 body_part;
    sprite_display_t* display = (sprite_display_t*)((u8*)&g_unit_slot_states[runtime_id]
        + offsetof(unit_slot_state_t, unit.unit_sprite_display.header));

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
sprite_display_t* sprite_display_init_weapon(s32 runtime_id, s32 display_type) {
    s32 weapon_part;
    sprite_weapon_display_t* weapons = (sprite_weapon_display_t*)((u8*)&g_unit_slot_states[runtime_id]
        + offsetof(unit_slot_state_t, unit.weapon_sprite_displays));
    sprite_display_t* display = &weapons[display_type].header;

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
sprite_display_t* sprite_display_init_numeric(s32 runtime_id, s32 display_section) {
    sprite_display_t* display;
    s32 i;

    switch (display_section) {
    case 0:
        display = &g_unit_slot_states[runtime_id].unit.numeric_sprite_displays[0].header;
        break;
    case 1:
        display = &g_unit_slot_states[runtime_id].unit.numeric_sprite_displays[1].header;
        break;
    case 2:
        display = &g_unit_slot_states[runtime_id].unit.numeric_sprite_displays[2].header;
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
sprite_display_t* sprite_display_init_status_bubble(s32 runtime_id) {
    sprite_display_t* display = (sprite_display_t*)((u8*)&g_unit_slot_states[runtime_id]
        + offsetof(unit_slot_state_t, unit.status_bubble_sprite_display.header));
    s32 i;

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = SPRITE_TPAGE_FRAME;
    display->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_SPRITES, 7);
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
sprite_display_t* sprite_display_init_item(s32 runtime_id) {
    sprite_display_t* display = (sprite_display_t*)((u8*)&g_unit_slot_states[runtime_id]
        + offsetof(unit_slot_state_t, unit.item_sprite_display.header));
    s32 i;

    display->color.b = 0x80;
    display->color.g = 0x80;
    display->color.r = 0x80;
    display->scale_y = ONE;
    display->scale_x = ONE;
    display->tpage = SPRITE_TPAGE_ITEM;
    display->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_SPRITES, 7);
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

/* Empty. unit_remove passes the removed unit's sprite display, a member of its
 * unit payload that sprite_display_init_unit handed out. */
void sprite_display_release_unit_stub(sprite_display_t* unused_display) {
}

/* Starts a frame of sprite drawing: the GTE offset is reset to (0, 0) and the
 * primitive ring index reached last frame is recorded. */
void sprite_render_frame_init(void) {
    SetGeomOffset(0, 0);
    /* The target loads the counter unsigned (lhu). */
    g_sprite_prim_frame_start_index = g_sprite_prim_index;
}

/* Takes the next of the 256 primitives of g_sprite_prim_pool, used as
 * a ring, and returns its index. */
s16 sprite_primitive_take(void) {
    g_sprite_prim_index = (g_sprite_prim_index + 1) % 0x100;
    return g_sprite_prim_index;
}

/* Queue one POLY_FT4 per sprite part in [start, end) of a sprite display.
 *
 * The display's rotation, the caller's angle and scale, and position build
 * the GTE matrix. `mode` supplies the whole-sprite flags and blend selector:
 * a flip bit mirrors each part's place about the sprite's origin, and its
 * picture unless the part's own flip bit is set too. Either one's
 * semi-transparent bit applies, and the blend bits of `mode` are added to
 * the display's texture page. A part flagged SPRITE_DISPLAY_NEARER is
 * queued ahead of `ot` by the camera pitch and zoom while the pitch is below
 * a quarter turn.
 *
 * The unused VECTOR reproduces the target's 16-byte locals area. The nested
 * test with its duplicated `addPrim(ot, prim)` arm is the original shape: jump
 * optimization merges the arms, but the references counted before that merge
 * give `ot` the allocation priority that places it in $s5 ahead of `display`. */
void sprite_display_construct_polygon_data(
    sprite_display_t* display, s32 end, s32 start, SVECTOR* position, s16 angle, s16 mode, SVECTOR* scale, u32* ot) {
    VECTOR unused;
    sprite_display_part_t* part;
    POLY_FT4* prim;
    s32 i;
    s16 flags;
    s16 index;
    s16 x;
    s16 y;
    s16 cosine;
    s16 sine;

    cosine = rcos(angle + display->rotation);
    g_sprite_display_matrix.m[0][0] = ((cosine * scale->vx) >> 12) * display->scale_x >> 12;
    g_sprite_display_matrix.m[1][1] = ((cosine * scale->vy) >> 12) * display->scale_y >> 12;
    sine = rsin(angle + display->rotation);
    g_sprite_display_matrix.m[0][1] = ((-sine * scale->vy) >> 12) * display->scale_y >> 12;
    g_sprite_display_matrix.m[1][0] = ((sine * scale->vx) >> 12) * display->scale_x >> 12;
    g_sprite_display_matrix.t[0] = position->vx;
    g_sprite_display_matrix.t[1] = position->vy;
    SetRotMatrix(&g_sprite_display_matrix);
    SetTransMatrix(&g_sprite_display_matrix);
    for (i = start; i < end; i++) {
        part = &display->parts[i];
        flags = part->flags;
        index = sprite_primitive_take();
        flags ^= mode;
        prim = g_sprite_prim_pool + index;
        setlen(prim, 9);
        prim->code = 0x2c;
        if (mode & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            x = -part->x_shift - part->width;
        } else {
            x = part->x_shift;
        }
        if (mode & SPRITE_DISPLAY_VERTICAL_FLIP) {
            y = -part->y_shift - part->height;
        } else {
            y = part->y_shift;
        }
        if (flags & SPRITE_DISPLAY_HORIZONTAL_FLIP) {
            g_gte_rtpt_vxy1.vx = g_gte_rtps_vxy0.vx = x;
            g_gte_rtpt_vxy0.vx = g_gte_rtpt_vxy2.vx = x + part->width;
        } else {
            g_gte_rtpt_vxy0.vx = g_gte_rtpt_vxy2.vx = x;
            g_gte_rtpt_vxy1.vx = g_gte_rtps_vxy0.vx = x + part->width;
        }
        if (flags & SPRITE_DISPLAY_VERTICAL_FLIP) {
            g_gte_rtpt_vxy2.vy = g_gte_rtps_vxy0.vy = y;
            g_gte_rtpt_vxy0.vy = g_gte_rtpt_vxy1.vy = y + part->height;
        } else {
            g_gte_rtpt_vxy0.vy = g_gte_rtpt_vxy1.vy = y;
            g_gte_rtpt_vxy2.vy = g_gte_rtps_vxy0.vy = y + part->height;
        }
        gte_ldv3_split(&g_gte_rtpt_vxy0, &g_gte_rtpt_vxy1, &g_gte_rtpt_vxy2);
        gte_rtpt();
        if ((flags | mode) & SPRITE_DISPLAY_SEMI_TRANSPARENT) {
            prim->code |= 2;
        } else {
            prim->code &= ~2;
        }
        prim->tpage = display->tpage | (mode & SPRITE_DISPLAY_BLEND_MASK);
        prim->clut = display->clut;
        gte_stsxy3_split(&prim->x0, &prim->x1, &prim->x2);
        gte_ldv0(&g_gte_rtps_vxy0);
        gte_rtps();
        /* The retail color transfer combines red and green in one 16-bit value. */
        memcpy(&prim->r0, &display->color.r, sizeof(u16));
        prim->b0 = display->color.b;
        gte_stsxy(&prim->x3);
        prim->u0 = prim->u2 = part->u;
        prim->u1 = prim->u3 = prim->u0 + part->width;
        prim->v0 = prim->v1 = part->v;
        prim->v2 = prim->v3 = prim->v0 + part->height;
        if (flags & SPRITE_DISPLAY_NEARER) {
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

/* Fill in part `index` of a sprite display: its offset from the sprite origin, its texture
 * rectangle and its flags. */
void sprite_display_store_data(
    sprite_display_t* display, s32 index, s16 x_shift, s16 y_shift, s16 u, s16 v, s16 width, s16 height, s16 flags) {
    sprite_display_part_t* part;

    part = &display->parts[index];
    part->x_shift = x_shift;
    part->y_shift = y_shift;
    part->u = u;
    part->v = v;
    part->width = width;
    part->height = height;
    part->flags = flags;
}

/* Replace the two mirror bits of a sprite part (SPRITE_DISPLAY_HORIZONTAL_FLIP and
 * _VERTICAL_FLIP) with `flags`. Never called. */
void sprite_display_set_part_flip(sprite_display_t* display, s32 part_index, u32 flags) {
    display->parts[part_index].flags
        = (display->parts[part_index].flags & ~(SPRITE_DISPLAY_HORIZONTAL_FLIP | SPRITE_DISPLAY_VERTICAL_FLIP)) | flags;
}

/* Set or clear bit 0 of a sprite part's flags: the part is drawn semi-transparent while it is set. */
void sprite_display_set_part_semi_transparent(sprite_display_t* display, s32 part_index, s32 enabled) {
    if (enabled != 0) {
        display->parts[part_index].flags |= SPRITE_DISPLAY_SEMI_TRANSPARENT;
    } else {
        display->parts[part_index].flags &= ~SPRITE_DISPLAY_SEMI_TRANSPARENT;
    }
}
