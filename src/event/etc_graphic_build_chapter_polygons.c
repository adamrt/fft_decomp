#include "fft/event_etc.h"
#include "psx/types.h"

/* The retail build keeps its POLY_GT4 cursor 0x2a bytes into the primitive
 * (its b3 byte) and addresses every field relative to that; the view folds
 * back to those displacements. */
#define PRIM ((POLY_GT4*)(fields - 0x2a))

/* Build the 2x2 POLY_GT4 grid that shows a chapter title graphic. Each
 * dimension row is six s16 values: x, y, w, h, u-ish x offset, y offset. */
void etc_graphic_build_chapter_polygons(
    s32 graphic_id, s32 fade, s32 dimension_set, s32 layer, volatile s32 primitives, s32 color) {
    /* Locals the retail build keeps on the stack, one word apart. */
    volatile struct {
        s32 layer;
        s32 _unused_04;
        s32 dimensions;
        s32 _unused_0c;
        s32 texture_mode;
        s32 _unused_14;
        s32 tpage_bits;
        s32 _unused_1c;
        s32 tpage_shifted;
        s32 _unused_24;
        s32 tpage_right_alt;
        s32 _unused_2c;
        s32 tpage_right;
        s32 _unused_34;
        s32 tpage_left_alt;
        s32 _unused_3c;
        s32 tpage_left;
    } stack;
    /* Each register pin is load-bearing: unpinning any one changes the allocation,
       and unpinning all callee-saved ones reorders $s3-$s8. */
    register s32 row_height __asm__("$3");
    s16 clut;
    s32 clip_base;
    register s32 dimension_offset __asm__("$4");
    register s32 clip_end __asm__("$4");
    s32 fade_right;
    register s32 scratch __asm__("$8");
    s32 tpage_shifted;
    s32 value;
    s32 last_primitive;
    register s32 clip_height __asm__("$6");
    register s32 dimension_index __asm__("$19");
    register s32 one __asm__("$22");
    s32 row;
    s32 poly;
    s32 row_offset;
    register s32 column __asm__("$23");
    s32 y_offset;
    register s32 y_bottom __asm__("$3");
    etc_graphic_dimensions_t* dims_faded;
    etc_graphic_dimensions_t* dims;
    etc_graphic_dimensions_t* dims_clipped;
    u8* fields;

    dimension_index = dimension_set;
    row = 0;
    stack.layer = layer;
    scratch = g_etc_graphics[graphic_id].dimensions;
    stack.dimensions = scratch;
    dimension_offset = g_etc_graphics[graphic_id].texture_mode;
    poly = primitives;
    scratch = dimension_offset & 3;
    stack.tpage_bits = scratch;
    tpage_shifted = scratch << 9;
    stack.tpage_shifted = tpage_shifted;
    stack.texture_mode = dimension_offset;
    stack.tpage_right_alt = tpage_shifted | 0x86;
    one = 1;
    do {
        column = 0;
        row_offset = 0;
        scratch = stack.tpage_bits;
        fields = &((POLY_GT4*)poly)->b3;
        value = scratch << 7;
        stack.tpage_right = (scratch = value | 0x26);
        scratch = stack.tpage_shifted;
        stack.tpage_left = value | 0x46;
        stack.tpage_left_alt = scratch | 0x106;
        /* Label loop: a do-while's loop notes change the whole allocation. */
    next_column:
        SetPolyGT4(poly);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        PRIM->u0 = 0U;
        PRIM->v0 = 0U;
        PRIM->u1 = one;
        PRIM->v1 = 0U;
        PRIM->u2 = 0U;
        PRIM->v2 = one;
        PRIM->u3 = one;
        PRIM->v3 = one;
        PRIM->x0 = 0;
        PRIM->y0 = 0;
        PRIM->x1 = 0;
        PRIM->y1 = 0;
        PRIM->x2 = 0;
        PRIM->y2 = 0;
        PRIM->x3 = 0;
        PRIM->y3 = 0;
        if (column != 0) {
            if ((GetGraphType() == one) || (GetGraphType() == 2)) {
                PRIM->tpage = *(volatile u16*)&stack.tpage_right_alt;
            } else {
                PRIM->tpage = *(volatile u16*)&stack.tpage_right;
            }
        } else if ((GetGraphType() == one) || (GetGraphType() == 2)) {
            PRIM->tpage = *(volatile u16*)&stack.tpage_left_alt;
        } else {
            PRIM->tpage = *(volatile u16*)&stack.tpage_left;
        }
        clut = 0x7F80;
        scratch = stack.texture_mode;
        if (scratch == 0) {
            clut = 0xFDB;
        }
        PRIM->clut = clut;
        if (row == 0) {
            scratch = stack.layer;
            dimension_offset = dimension_index << 1;
            if (scratch != 0) {
                dimension_offset += dimension_index;
                scratch = stack.dimensions;
                dimension_offset <<= 2;
                dimension_offset += scratch;
                dims_faded = (etc_graphic_dimensions_t*)(row_offset + dimension_offset);
                PRIM->u0 = (u8)((dims_faded->u.bytes.low + dims_faded->width.bytes.low) - fade);
                PRIM->v0 = dims_faded->v.bytes.low;
                PRIM->u1 = (s8)(dims_faded->u.bytes.low + dims_faded->width.bytes.low);
                PRIM->v1 = dims_faded->v.bytes.low;
                PRIM->u2 = (u8)((dims_faded->u.bytes.low + dims_faded->width.bytes.low) - fade);
                PRIM->v2 = (s8)(dims_faded->v.bytes.low + dims_faded->height.bytes.low);
                PRIM->u3 = (s8)(dims_faded->u.bytes.low + dims_faded->width.bytes.low);
                PRIM->v3 = (s8)(dims_faded->v.bytes.low + dims_faded->height.bytes.low);
                PRIM->x0 = (s16)(((dims_faded->x.value + dims_faded->width.value) - fade) + 0x100);
                PRIM->y0 = (s16)(dims_faded->y.value + 0x80);
                PRIM->x1 = (s16)(dims_faded->x.value + dims_faded->width.value + 0x100);
                PRIM->y1 = (s16)(dims_faded->y.value + 0x80);
                PRIM->x2 = (s16)(((dims_faded->x.value + dims_faded->width.value) - fade) + 0x100);
                PRIM->y2 = (s16)(dims_faded->y.value + (dims_faded->height.value + 0x80));
                PRIM->x3 = (s16)(dims_faded->x.value + dims_faded->width.value + 0x100);
                y_offset = dims_faded->height.value;
                y_bottom = dims_faded->y.value;
                PRIM->r0 = (s8)color;
                PRIM->g0 = (s8)color;
                PRIM->b0 = (s8)color;
                PRIM->r1 = (s8)color;
                PRIM->g1 = (s8)color;
                PRIM->b1 = (s8)color;
                PRIM->r2 = (s8)color;
                PRIM->g2 = (s8)color;
                PRIM->b2 = (s8)color;
                /* Shared with the clipped-row branch below; a duplicated
                                   tail does not cross-jump back into one copy here. */
                goto finish_bottom_row;
            }
            dimension_offset += dimension_index;
            scratch = stack.dimensions;
            dimension_offset <<= 2;
            dimension_offset += scratch;
            dims = (etc_graphic_dimensions_t*)(row_offset + dimension_offset);
            PRIM->u0 = dims->u.bytes.low;
            PRIM->v0 = dims->v.bytes.low;
            PRIM->u1 = (s8)(dims->u.bytes.low + fade);
            PRIM->v1 = dims->v.bytes.low;
            PRIM->u2 = dims->u.bytes.low;
            PRIM->v2 = (s8)(dims->v.bytes.low + dims->height.bytes.low);
            PRIM->u3 = (s8)(dims->u.bytes.low + fade);
            PRIM->v3 = (s8)(dims->v.bytes.low + dims->height.bytes.low);
            PRIM->x0 = (s16)(dims->x.value + 0x100);
            PRIM->y0 = (s16)(dims->y.value + 0x80);
            fade_right = fade + 0x100;
            PRIM->x1 = (s16)(dims->x.value + fade_right);
            PRIM->y1 = (s16)(dims->y.value + 0x80);
            PRIM->x2 = (s16)(dims->x.value + 0x100);
            PRIM->y2 = (s16)(dims->y.value + (dims->height.value + 0x80));
            PRIM->x3 = (s16)(dims->x.value + fade_right);
            PRIM->y3 = (s16)(dims->y.value + (dims->height.value + 0x80));
            /* Keeps y3 ahead of the colors; sunk below them, the tail cross-jumps into finish_bottom_row. */
            __asm__ volatile("" : : : "memory");
            PRIM->r0 = (s8)color;
            PRIM->g0 = (s8)color;
            PRIM->b0 = (s8)color;
            PRIM->r1 = (s8)color;
            PRIM->g1 = (s8)color;
            PRIM->b1 = (s8)color;
            PRIM->r2 = (s8)color;
            PRIM->g2 = (s8)color;
            PRIM->b2 = (s8)color;
            PRIM->r3 = (s8)color;
            PRIM->g3 = (s8)color;
            PRIM->b3 = (s8)color;
        } else if (row == one) {
            scratch = stack.dimensions;
            dims_clipped = (etc_graphic_dimensions_t*)(row_offset + ((dimension_index * 0xC) + scratch));
            row_height = dims_clipped->width.signed_value;
            clip_height = row_height < (fade + 0x20) ? row_height - fade : 0x20;
            clip_base = clip_height - 0x100;
            scratch = stack.layer;
            if (scratch != 0) {
                PRIM->u0
                    = (u8)(((dims_clipped->u.bytes.low + (u8)dims_clipped->width.signed_value) - fade) - clip_height);
                PRIM->v0 = dims_clipped->v.bytes.low;
                PRIM->u1 = (s8)((dims_clipped->u.bytes.low + (u8)dims_clipped->width.signed_value) - fade);
                PRIM->v1 = dims_clipped->v.bytes.low;
                PRIM->u2
                    = (u8)(((dims_clipped->u.bytes.low + (u8)dims_clipped->width.signed_value) - fade) - clip_height);
                PRIM->v2 = (s8)(dims_clipped->v.bytes.low + dims_clipped->height.bytes.low);
                PRIM->u3 = (s8)((dims_clipped->u.bytes.low + (u8)dims_clipped->width.signed_value) - fade);
                PRIM->v3 = (s8)(dims_clipped->v.bytes.low + dims_clipped->height.bytes.low);
                PRIM->x0 = (s16)(((dims_clipped->x.value + (u16)dims_clipped->width.signed_value) - fade) - clip_base);
                PRIM->y0 = (s16)(dims_clipped->y.value + 0x80);
                value = ((dims_clipped->x.value + (u16)dims_clipped->width.signed_value) - fade) - clip_base;
                value += clip_height;
                PRIM->x1 = (s16)value;
                PRIM->y1 = (s16)(dims_clipped->y.value + 0x80);
                PRIM->x2 = (s16)(((dims_clipped->x.value + (u16)dims_clipped->width.signed_value) - fade) - clip_base);
                PRIM->y2 = (s16)(dims_clipped->y.value + (dims_clipped->height.value + 0x80));
                value = ((dims_clipped->x.value + (u16)dims_clipped->width.signed_value) - fade) - clip_base;
                value += clip_height;
                PRIM->x3 = (s16)value;
                y_offset = dims_clipped->height.value;
                y_bottom = dims_clipped->y.value;
                PRIM->r0 = 0;
                PRIM->g0 = 0;
                PRIM->b0 = 0;
                PRIM->r1 = (s8)color;
                PRIM->g1 = (s8)color;
                PRIM->b1 = (s8)color;
                PRIM->r2 = 0;
                PRIM->g2 = 0;
                PRIM->b2 = 0;
            finish_bottom_row:
                PRIM->r3 = (s8)color;
                PRIM->g3 = (s8)color;
                PRIM->b3 = (s8)color;
                /* Keeps the y3 sum after the r3/g3/b3 stores. */
                __asm__ volatile("" : : : "memory");
                y_offset += 0x80;
                y_bottom += y_offset;
                PRIM->y3 = (s16)y_bottom;
            } else {
                PRIM->u0 = (u8)(dims_clipped->u.bytes.low + fade);
                PRIM->v0 = dims_clipped->v.bytes.low;
                value = dims_clipped->u.bytes.low + fade;
                value += clip_height;
                PRIM->u1 = (s8)value;
                PRIM->v1 = dims_clipped->v.bytes.low;
                PRIM->u2 = (u8)(dims_clipped->u.bytes.low + fade);
                PRIM->v2 = (s8)(dims_clipped->v.bytes.low + dims_clipped->height.bytes.low);
                value = dims_clipped->u.bytes.low + fade;
                value += clip_height;
                PRIM->u3 = (s8)value;
                PRIM->v3 = (s8)(dims_clipped->v.bytes.low + dims_clipped->height.bytes.low);
                PRIM->x0 = (s16)(dims_clipped->x.value + fade + 0x100);
                PRIM->y0 = (s16)(dims_clipped->y.value + 0x80);
                clip_end = clip_height + 0x100;
                value = dims_clipped->x.value + fade;
                value += clip_end;
                PRIM->x1 = (s16)value;
                PRIM->y1 = (s16)(dims_clipped->y.value + 0x80);
                PRIM->x2 = (s16)(dims_clipped->x.value + fade + 0x100);
                PRIM->y2 = (s16)(dims_clipped->y.value + (dims_clipped->height.value + 0x80));
                value = dims_clipped->x.value + fade;
                value += clip_end;
                PRIM->x3 = (s16)value;
                PRIM->y3 = (s16)(dims_clipped->y.value + (dims_clipped->height.value + 0x80));
                PRIM->r0 = (s8)color;
                PRIM->g0 = (s8)color;
                PRIM->b0 = (s8)color;
                PRIM->r1 = 0;
                PRIM->g1 = 0;
                PRIM->b1 = 0;
                PRIM->r2 = (s8)color;
                PRIM->g2 = (s8)color;
                PRIM->b2 = (s8)color;
                PRIM->r3 = 0;
                PRIM->g3 = 0;
                PRIM->b3 = 0;
            }
        }
        fields += 0x34;
        poly += 0x34;
        column += 1;
        row_offset += 0xC;
        if (column < 2) {
            goto next_column;
        }
        row += 1;
    } while (row < 2);
    scratch = stack.layer;
    if (scratch != 0) {
        scratch = primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x34));
        battle_gfx_draw_or_append_gpu_primitive(primitives);
        scratch = primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x9C));
        scratch = primitives;
        last_primitive = scratch + 0x68;
    } else {
        scratch = primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x9C));
        scratch = primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x68));
        scratch = primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x34));
        last_primitive = primitives;
    }
    battle_gfx_draw_or_append_gpu_primitive(last_primitive);
}

#undef PRIM
