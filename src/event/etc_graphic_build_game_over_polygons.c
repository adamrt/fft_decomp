#include "fft/event_etc.h"
#include "psx/types.h"

/* Build and draw the four POLY_GT4 tiles of an ETC graphic revealed up to `fade`
 * pixels, followed by a gradient edge of up to 32 pixels. */
void etc_graphic_build_game_over_polygons(
    s32 graphic_id, s32 fade, s32 dimension_set, s32 reverse_order, POLY_GT4* primitives, s32 color) {
    /* Locals the retail build keeps on the stack, one word apart. */
    volatile struct {
        s32 reverse_order;
        s32 _unused_04;
        s32 dimension_base;
        s32 _unused_0c;
        s32 first_tpage;
        s32 _unused_14;
        s32 second_tpage;
        s32 _unused_1c;
        s32 third_tpage;
    } stack;
    /* Pins: unpinning any one of these changes the target's register allocation. */
    register s32 graphic_height __asm__("$3");
    s16 clut;
    s32 first_fade_x;
    register s32 clip __asm__("$6");
    register void* current_primitive __asm__("$19");
    register s32 dimension_offset __asm__("$20");
    register s32 tile __asm__("$21");
    register s32 half __asm__("$23");
    u16 tpage;
    register u8* primitive_cursor __asm__("$16");
    register s32 enabled __asm__("$22");
    /* One $t0 temporary for the tpage constants, the stack reloads and the clut flag. */
    register s32 scratch __asm__("$8");
    register s32 dimensions __asm__("$4");
    s32 tpage_base;
    s32 graphic_index = graphic_id;
    s32 reveal = fade;
    s32 first_dimension_index = dimension_set;
    s32 brightness = color;

#define primitive ((POLY_GT4*)(primitive_cursor - 0x2a))
    /* Target-specific address arithmetic: ordinary pointer addition reverses
     * the addu operands at +0x190/+0x2e0. The row index is scaled and added to
     * the base as a 32-bit target address before the per-tile displacement;
     * field access itself stays typed. */
    half = 0;
    stack.reverse_order = reverse_order;
    dimensions = (s32)g_etc_graphics[graphic_index].dimensions;
    current_primitive = primitives;
    enabled = 1;
    stack.dimension_base = dimensions;
    do {
        tile = 0;
        scratch = 1;
        /* Hides the 1 so the tpage words are built with sll/ori, not folded to 0xa6/0xc6. */
        __asm__("" : "=r"(scratch) : "0"(scratch));
        tpage_base = scratch << 7;
        stack.first_tpage = (scratch = tpage_base | 0x26);
        scratch = 0x200;
        /* Hides the 0x200 so `li 0x200; ori 0x106` is not folded to `li 0x306`. */
        __asm__("" : "=r"(scratch) : "0"(scratch));
        scratch |= 0x106;
        tpage_base |= 0x46;
        dimension_offset = 0;
        primitive_cursor = (u8*)current_primitive + 0x2a;
        stack.second_tpage = scratch;
        stack.third_tpage = tpage_base;
        do {
            /* Keeps the index and fade unscaled in the loop instead of hoisted scaled forms. */
            __asm__ __volatile__("" : "=r"(first_dimension_index) : "0"(first_dimension_index));
            __asm__ __volatile__("" : "=r"(reveal) : "0"(reveal));
            SetPolyGT4((POLY_GT4*)current_primitive);
            SetShadeTex((POLY_GT4*)current_primitive, 0);
            SetSemiTrans((POLY_GT4*)current_primitive, 1);

            primitive->u0 = 0;
            primitive->v0 = 0;
            primitive->u1 = enabled;
            primitive->v1 = 0;
            primitive->u2 = 0;
            primitive->v2 = enabled;
            primitive->u3 = enabled;
            primitive->v3 = enabled;
            primitive->x0 = 0;
            primitive->y0 = 0;
            primitive->x1 = 0;
            primitive->y1 = 0;
            primitive->x2 = 0;
            primitive->y2 = 0;
            primitive->x3 = 0;
            primitive->y3 = 0;

            if (tile != 0) {
                if (GetGraphType() == enabled || GetGraphType() == 2) {
                    tpage = 0x286;
                } else {
                    tpage = *(volatile u16*)&stack.first_tpage;
                }
            } else if (GetGraphType() == enabled || GetGraphType() == 2) {
                tpage = *(volatile u16*)&stack.second_tpage;
            } else {
                tpage = *(volatile u16*)&stack.third_tpage;
            }
            scratch = 1;
            /* Hides the 1 so the clut test stays a runtime branch, with `li t0,1` in the jump delay slots. */
            __asm__("" : "=r"(scratch) : "0"(scratch));
            primitive->tpage = tpage;
            if (scratch != 0) {
                /* Keeps 0x7f80 as the jump-delay-slot arm; without it the two clut arms swap. */
                __asm__ __volatile__("");
                clut = 0x7f80;
            } else {
                clut = 0xfdb;
            }
            primitive->clut = clut;

            if (half == 0) {
                scratch = stack.reverse_order;
                if (scratch == 0) {
                    /* Pin: keeps the first-row address in $a0 through the final addition. */
                    register etc_graphic_dimensions_t* first_row_base __asm__("$4");
                    etc_graphic_dimensions_t* row;
                    scratch = stack.dimension_base;
                    first_row_base
                        = (etc_graphic_dimensions_t*)(first_dimension_index * (sizeof(etc_graphic_dimensions_t))
                            + (u32)scratch);
                    row = (etc_graphic_dimensions_t*)(dimension_offset + (u32)first_row_base);
                    primitive->u0 = row->u.bytes.low + row->width.bytes.low;
                    primitive->v0 = row->v.bytes.low;
                    primitive->u2 = row->u.bytes.low;
                    primitive->v2 = row->v.bytes.low;
                    primitive->u1 = row->u.bytes.low + row->width.bytes.low;
                    primitive->v1 = row->v.bytes.low + reveal;
                    primitive->u3 = row->u.bytes.low;
                    primitive->v3 = row->v.bytes.low + reveal;
                    primitive->x0 = row->y.value + 0x100;
                    primitive->y0 = row->x.value + 0x80;
                    first_fade_x = reveal + 0x100;
                    primitive->x1 = row->y.value + first_fade_x;
                    primitive->y1 = row->x.value + 0x80;
                    primitive->x2 = row->y.value + 0x100;
                    primitive->y2 = row->x.value + (row->width.value + 0x80);
                    primitive->x3 = row->y.value + first_fade_x;
                    primitive->y3 = row->x.value + (row->width.value + 0x80);
                    /* Keeps the y3 store ahead of the brightness stores. */
                    __asm__ __volatile__("");
                    primitive->r0 = brightness;
                    primitive->g0 = brightness;
                    primitive->b0 = brightness;
                    primitive->r1 = brightness;
                    primitive->g1 = brightness;
                    primitive->b1 = brightness;
                    primitive->r2 = brightness;
                    primitive->g2 = brightness;
                    primitive->b2 = brightness;
                    primitive->r3 = brightness;
                    primitive->g3 = brightness;
                    primitive->b3 = brightness;
                }
            } else if (half == enabled) {
                /* Pin: unpinned, the fade x and the row pointer swap $a0 and $a1. */
                register s32 second_fade_x __asm__("$4");
                etc_graphic_dimensions_t* row;

                scratch = stack.dimension_base;
                row = (etc_graphic_dimensions_t*)(dimension_offset
                    + (first_dimension_index * sizeof(etc_graphic_dimensions_t) + (u32)scratch));
                graphic_height = row->height.signed_value;
                clip = graphic_height < reveal + 0x20 ? graphic_height - reveal : 0x20;
                second_fade_x = reveal + 0x100;
                scratch = stack.reverse_order;
                if (scratch == 0) {
                    primitive->u0 = row->u.bytes.low + row->width.bytes.low;
                    primitive->v0 = row->v.bytes.low + reveal;
                    primitive->u2 = row->u.bytes.low;
                    primitive->v2 = row->v.bytes.low + reveal;
                    primitive->u1 = row->u.bytes.low + row->width.bytes.low;
                    {
                        s32 value = row->v.bytes.low + reveal;
                        value += clip;
                        primitive->v1 = value;
                    }
                    primitive->u3 = row->u.bytes.low;
                    {
                        s32 value = row->v.bytes.low + reveal;
                        value += clip;
                        primitive->v3 = value;
                    }
                    {
                        u16 x_offset = row->y.value;
                        primitive->x0 = x_offset + second_fade_x;
                    }
                    primitive->y0 = row->x.value + 0x80;
                    {
                        u16 x_offset = row->y.value;
                        s32 value = x_offset + second_fade_x;
                        value += clip;
                        primitive->x1 = value;
                    }
                    primitive->y1 = row->x.value + 0x80;
                    {
                        u16 x_offset = row->y.value;
                        primitive->x2 = x_offset + second_fade_x;
                    }
                    primitive->y2 = row->x.value + (row->width.value + 0x80);
                    {
                        u16 x_offset = row->y.value;
                        s32 value = x_offset + second_fade_x;
                        value += clip;
                        primitive->x3 = value;
                    }
                    primitive->y3 = row->x.value + (row->width.value + 0x80);
                    primitive->r0 = brightness;
                    primitive->g0 = brightness;
                    primitive->b0 = brightness;
                    primitive->r1 = 0;
                    primitive->g1 = 0;
                    primitive->b1 = 0;
                    primitive->r2 = brightness;
                    primitive->g2 = brightness;
                    primitive->b2 = brightness;
                    primitive->r3 = 0;
                    primitive->g3 = 0;
                    primitive->b3 = 0;
                }
            }
            primitive_cursor += 0x34;
            current_primitive += 0x34;
            tile++;
            dimension_offset += 0xc;
        } while (tile < 2);
        half++;
    } while (half < 2);

    scratch = stack.reverse_order;
    if (scratch != 0) {
        scratch = *(s32 volatile*)&primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x34));
        battle_gfx_draw_or_append_gpu_primitive(primitives);
        scratch = *(s32 volatile*)&primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x9c));
        scratch = *(s32 volatile*)&primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x68));
    } else {
        scratch = *(s32 volatile*)&primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x9c));
        scratch = *(s32 volatile*)&primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x68));
        scratch = *(s32 volatile*)&primitives;
        battle_gfx_draw_or_append_gpu_primitive((scratch + 0x34));
        battle_gfx_draw_or_append_gpu_primitive(primitives);
    }

#undef primitive
}
