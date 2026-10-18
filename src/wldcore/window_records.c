/* World-map window record pools, animation and libgs sprite drawing. */

#include "fft/gfx.h"
#include "fft/system.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/types.h"

static void gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* source, RECT* rectangle);
static void window_draw_gs_image_record(window_gs_image_record_t* object, GsOT* ot);

/* Reset both world-map window pools and their allocation counters. */
void window_init_record_pools(void) {
    /* Member pointers preserve the separate color and CLUT address walks. */
    CVECTOR* color;
    point32_t* clut;
    s32 i;

    g_window_render_record_count = 0;
    g_window_record_count = 0;
    g_window_render_object_count = 0;
    g_window_aux_render_object_count = 0;
    for (i = 0; i < WINDOW_GS_RECORD_CAPACITY; i++) {
        g_window_records[i].flags = DISPLAY_OBJECT_ANIMATED;
        g_window_records[i].priority = 1;
        color = &g_window_records[i].color;
        color->b = color->g = color->r = 0x80;
    }

    for (i = 0; i < WINDOW_GS_RECORD_CAPACITY; i++) {
        g_window_gs_image_records[i].flags = DISPLAY_OBJECT_IMAGE;
        g_window_gs_image_records[i].priority = 1;
        clut = &g_window_gs_image_records[i].clut;
        clut->x = 0x3c0;
        clut->y = 0x1f0;
        color = &g_window_gs_image_records[i].color;
        color->b = color->g = color->r = 0x80;
    }
}

/* Sort visible animated and image records through libgs. */
void window_dispatch_gs_record_list(GsOT* otag, void** list, s32 count) {
    void* object;
    u32* flags;
    s32 i;

    if (g_system_flags & SYSTEM_FLAG_HIDE_DISPLAY_OBJECTS) {
        return;
    }
    for (i = 0; i < count; i++) {
        object = list[i];
        flags = object;
        if (*flags & (DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN)) {
            continue;
        }
        if (*flags & DISPLAY_OBJECT_ANIMATED) {
            display_object_animated_t* animated = object;
            window_draw_gs_animated_record(animated, otag);
        } else if (*flags & DISPLAY_OBJECT_IMAGE) {
            window_gs_image_record_t* image = object;
            window_draw_gs_image_record(image, otag);
        }
    }
}

/* Draw one animated display object: advance its sequence, then convert every
 * part of the current cel into a libgs sprite primitive and sort it into the
 * ordering table.
 *
 * UNTINTED uses the object's color as is, otherwise the global fade tint is
 * applied. The frame counter steps unless SYSTEM_FLAG_FREEZE_ANIMATION is
 * set, or always with ANIMATE_WHILE_PAUSED. SEMI_TRANSPARENT replaces each
 * part's own blend mode with the object's BLEND_RATE_MASK bits.
 *
 * Each part's first 32-bit word holds the display_object_part_t bit-fields.
 * hidden, semi_transparent and blend_rate go into the attribute as they are;
 * texture_flags moves down two bits to the flip and 8-bit attribute bits. The
 * part's own palette_index is used when the object has no palette or
 * use_part_palette is set. Retail adds the cel's CLUT x before shifting that
 * field down, so the shift drops the cel's x origin and the CLUT x is just
 * palette_index * 16 (OPEN's twin adds the origin after the shift).
 *
 * The parts are walked back to front as raw 32-bit element indices: the target spells the
 * subscript ((part_count - i) * 2) | 1 into the cel's 32-bit array, which is the
 * part's first 32-bit value, and reaches the part itself from there. Indexing a part
 * array of records instead produces a different address computation, so
 * the 32-bit element index is retained.
 *
 * Both high halves are masked before the shift. The code is the same as a
 * plain shift, but it matches the target's 0x88-byte frame: CSE shares the
 * 0xffff0000 constant between the two statements, so the first mask/shift
 * combine keeps that constant for the second and leaves its dead AND result
 * as a USE-only pseudo. That pseudo and the part loop's folded entry test
 * (combine turns it into `blez part_count`, keeps `i = 0`, and leaves the slt
 * pseudo only a USE) get the unreferenced reload slots at 0x50 and 0x58. */
void window_draw_gs_animated_record(display_object_animated_t* object, GsOT* ot) {
    GsSPRITE sprite;
    CVECTOR color;
    display_object_sequence_header_t* sequence;
    display_object_part_t* part;
    display_object_cel_header_t** cels;
    display_object_cel_header_t* cel;
    u32* words;
    s32 part_count;
    s32 i;
    s32 index;
    s32 mode;
    s32 tpage_x;
    s32 tpage_y;
    s32 clut_x;
    s32 clut_y;
    s32 palette;
    s32 px;

    sequence = g_window_anim_sequences[object->sequence];
    sprite.my = 0;
    sprite.mx = 0;
    sprite.scaley = ONE;
    sprite.scalex = ONE;
    sprite.rotate = 0;
    cels = g_window_anim_cels;
    if (object->flags & DISPLAY_OBJECT_UNTINTED) {
        color = object->color;
    } else {
        world_map_fade_tint_color(&object->color, &color);
    }

    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    if ((object->flags & DISPLAY_OBJECT_ANIMATE_WHILE_PAUSED) || !(g_system_flags & SYSTEM_FLAG_FREEZE_ANIMATION)) {
        object->anim_counter++;
    }

    /* A goto loop: as a for/while loop, GCC's loop pass hoists the 0xffff
     * constant into a register, which the target does not. Frame records follow the serialized header. Pointer addition
     * keeps the index-first address sum; array indexing reverses it. */
step_frame:
    index = ((display_object_frame_t*)(sequence + 1) + object->frame_index)->duration;
    if (index == object->anim_counter && index != 0xffff) {
        object->anim_counter = 0;
        object->frame_index++;
        index = sequence->frame_count;
        if (index == object->frame_index) {
            object->frame_index = 0;
        }
        goto step_frame;
    }

    index = ((display_object_frame_t*)(sequence + 1) + object->frame_index)->cel;
    cel = cels[index];
    /* Preserve the reverse 32-bit cursor; a part array changes address computation. */
    words = (u32*)cel;
    tpage_x = cel->packed_texture_page_origin & 0xffff;
    tpage_y = (cel->packed_texture_page_origin & 0xffff0000) >> 16;
    clut_x = cel->packed_clut_origin & 0xffff;
    part_count = cel->part_count;
    clut_y = (cel->packed_clut_origin & 0xffff0000) >> 16;
    for (i = 0; i < part_count; i++) {
        index = ((part_count - i) * 2) | 1;
        sprite.attribute = words[index] & 0x80000000;
        if (object->flags & DISPLAY_OBJECT_SEMI_TRANSPARENT) {
            sprite.attribute |= 0x40000000;
            sprite.attribute |= (object->flags & DISPLAY_OBJECT_BLEND_RATE_MASK) << 22;
            mode = (object->flags & DISPLAY_OBJECT_BLEND_RATE_MASK) >> 6;
        } else {
            sprite.attribute |= words[index] & 0x70000000;
            mode = (words[index] >> 28) & 3;
        }
        /* Pointer indexing reverses this address-add instruction's operands. */
        part = (display_object_part_t*)(index * 4 + (s32)words);
        /* These packed reads must stay 32 bits wide; named byte fields shorten the code. */
        sprite.attribute |= (words[index] & 0x07000000) >> 2;
        px = part->x_offset - 0x80;
        sprite.x = object->position.x + px;
        px = part->y_offset - 0x80;
        sprite.y = object->position.y + px;
        sprite.w = part->width;
        sprite.h = part->height;
        sprite.tpage = GetTPage(0, mode, tpage_x, tpage_y);
        sprite.u = part->u;
        sprite.v = (words[index + 1] & 0xff0000) >> 16;
        palette = object->palette;
        if (palette == 0 || (words[index] & 0x08000000)) {
            /* The cel's CLUT x is shifted out with the palette field (retail quirk). */
            sprite.cx = (clut_x + (words[index] & 0xff0000)) >> 12;
        } else {
            sprite.cx = clut_x + ((palette - 1) << 4);
        }
        sprite.cy = clut_y;
        if (sprite.attribute & 0x800000) {
            sprite.u++;
        }
        if (sprite.attribute & 0x400000) {
            sprite.v++;
        }
        GsSortFlipSprite(&sprite, ot, object->priority);
    }
}

/* Convert one display object into a libgs sprite primitive and sort it into the
 * ordering table handed down by window_dispatch_gs_record_list.
 *
 * 8BIT_CLUT selects the 8-bit texture mode (attribute 0x01000000). UNTINTED
 * uses the object's color as is, otherwise world_map_fade_tint_color
 * applies the global fade tint. A nonzero palette picks the CLUT
 * (palette - 1) * 16 pixels right of the record's. POP_IN draws the window
 * growing: gfx_calculate_scaled_rectangle gives the centered part shown at
 * step anim_counter / 2, and the flag clears itself after the full-size step.
 *
 * A field-to-field copy narrows the load to the store's mode at expand time;
 * routing the value through a named local of the field's own width does not,
 * and combine cannot re-narrow it afterwards. The target narrows exactly where
 * no local is involved (`sprite.u = object->bounds.x + fx` is `lbu`), so the
 * locals and their absence both follow the target's load widths.
 *
 * - The anim test is `>= 4`: the target puts the counter-step arm at the
 *   branch target and the clear-and-toggle arm in the fall-through.
 * - `frame.x` and `frame.y` need their own locals, or the `sb` sites re-load
 *   them narrowed instead of reusing the `lhu` value.
 * - `object->palette` needs one local shared by the `!= 0` test and the
 *   arithmetic, or the arithmetic re-loads it.
 * - `ox = object->base.x` must precede `fx = frame.x`: the three loads form one
 *   scheduler ready list ordered by source, and the other order transposes
 *   two of them.
 */
static void window_draw_gs_image_record(window_gs_image_record_t* object, GsOT* ot) {
    GsSPRITE sprite;
    RECT frame;
    CVECTOR color;
    s32 clut_x;
    s32 clut_y;
    s32 page;
    s32 palette;

    if (object->flags & DISPLAY_OBJECT_8BIT_CLUT) {
        sprite.attribute = 0x01000000;
    } else {
        sprite.attribute = 0;
    }
    clut_x = object->clut.x;
    sprite.cx = clut_x;
    clut_y = object->clut.y;
    sprite.cy = clut_y;
    palette = object->palette;
    if (palette != 0) {
        sprite.cx = object->clut.x + ((palette - 1) << 4);
    }
    if (object->flags & DISPLAY_OBJECT_UNTINTED) {
        color = object->color;
    } else {
        world_map_fade_tint_color(&object->color, &color);
    }
    sprite.r = color.r;
    sprite.g = color.g;
    sprite.b = color.b;
    page = object->tpage;
    sprite.tpage = page;
    if (object->flags & DISPLAY_OBJECT_POP_IN) {
        s16 fx;
        s16 fy;
        s32 ox;
        s32 oy;

        gfx_calculate_scaled_rectangle(object->anim_counter / 2, &object->bounds, &frame);
        ox = object->base.x;
        fx = frame.x;
        fy = frame.y;
        sprite.x = fx + ox;
        oy = object->base.y;
        sprite.y = fy + oy;
        sprite.u = object->bounds.x + fx;
        sprite.v = object->bounds.y + fy;
        sprite.w = frame.w;
        sprite.h = frame.h;
        if (object->anim_counter / 2 >= 4) {
            object->anim_counter = 0;
            object->flags ^= DISPLAY_OBJECT_POP_IN;
        } else {
            object->anim_counter++;
        }
    } else {
        s32 sx;
        s32 sy;
        s16 su;
        s16 sv;

        sx = object->base.x;
        sprite.x = sx;
        sy = object->base.y;
        sprite.y = sy;
        su = object->bounds.x;
        sprite.u = su;
        sv = object->bounds.y;
        sprite.v = sv;
        sprite.w = object->bounds.w;
        sprite.h = object->bounds.h;
    }
    GsSortFastSprite(&sprite, ot, object->priority);
}

/* Attach the current window record to the caller's list and reset its color. */
s32 window_append_animated_record(void** queue, s32* count) {
    CVECTOR* color;

    queue[*count] = &g_window_records[g_window_record_count];
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &g_window_records->color;
    g_window_records[g_window_record_count].palette = 0;
    g_window_records[g_window_record_count].flags
        &= ~(DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED);
    ((CVECTOR*)((u8*)color + g_window_record_count * sizeof(g_window_records[0])))->r = 0x80;
    ((CVECTOR*)((u8*)color + g_window_record_count * sizeof(g_window_records[0])))->g = 0x80;
    ((CVECTOR*)((u8*)color + g_window_record_count * sizeof(g_window_records[0])))->b = 0x80;
    (*count)++;
    return g_window_record_count++;
}

/* Attach the current render record to the caller's list and reset it.
 *
 * Sibling of window_append_animated_record, which performs the same append for the 36-byte
 * window records. */
s32 window_append_record(void** queue, s32* count) {
    CVECTOR* color;

    queue[*count] = &g_window_gs_image_records[g_window_render_record_count];
    /* Direct record.color access changes the column base and instruction scheduling. */
    color = &g_window_gs_image_records->color;
    g_window_gs_image_records[g_window_render_record_count].flags
        &= ~(DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED);
    ((CVECTOR*)((u8*)color + g_window_render_record_count * sizeof(g_window_gs_image_records[0])))->r = 0x80;
    ((CVECTOR*)((u8*)color + g_window_render_record_count * sizeof(g_window_gs_image_records[0])))->g = 0x80;
    ((CVECTOR*)((u8*)color + g_window_render_record_count * sizeof(g_window_gs_image_records[0])))->b = 0x80;
    g_window_gs_image_records[g_window_render_record_count].palette = 0;
    g_window_gs_image_records[g_window_render_record_count].anim_counter = 0;
    (*count)++;
    return g_window_render_record_count++;
}

/* Build the centered sub-rectangle of the source area for a scale mode
 * (20%, 50%, 80%, 90% or 100%), aligned to 4 pixels. */
static void gfx_calculate_scaled_rectangle(s32 scale_mode, const RECT* source, RECT* rectangle) {
    s32 scale;
    s32 source_width;
    s32 source_height;
    s32 half_width;
    s32 half_height;
    s32 width;
    s32 height;

    switch (scale_mode) {
    case 0:
        scale = 20;
        break;
    case 1:
        scale = 50;
        break;
    case 2:
        scale = 80;
        break;
    case 3:
        scale = 90;
        break;
    default:
        scale = 100;
        break;
    }

    source_width = source->w;
    source_height = source->h;
    half_width = (source_width / 2) & 0xfffc;
    half_height = (source_height / 2) & 0xfffc;
    width = (source_width * scale / 200) & 0xfffc;
    height = (source_height * scale / 200) & 0xfffc;
    if (half_width < width) {
        width = half_width;
    }
    if (half_height < height) {
        height = half_height;
    }
    rectangle->x = half_width - width;
    rectangle->y = half_height - height;
    rectangle->w = width * 2;
    rectangle->h = height * 2;
}
